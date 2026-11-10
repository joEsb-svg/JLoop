#pragma once

#include <JuceHeader.h>
#include <array>
#include <vector>
#include "SequencerEngine.h"
#include "ProjectIO.h"
#include "SampleStore.h"
#include "MusicTheory.h"
#include "Mastering.h"
#include "AutoTune.h"   // nearestInScale
#include "../Instruments/Instruments.h"

//==============================================================================
/**
    CONTROLLO DEL PROGETTO: JLoop ascolta la canzone (in una copia del progetto, offline, come l'esportazione)
    e cerca i problemi piu' comuni del mix, ognuno con una spiegazione, una guida passo passo e (quasi sempre)
    una correzione automatica.

    Cosa controlla:
    - Volume e distorsione: il master che va oltre 0 dB (il limiter di sicurezza schiaccia), la canzone troppo piano
      o troppo forte (LUFS), una traccia che copre tutte le altre.
    - Cassa e basso: un basso che suona insieme alla cassa sotto i 120 Hz senza sidechain; bassi stereo.
    - Note fuori tonalita': note del piano roll fuori dalla scala del progetto (o da quella che "si sente").
    - Tracce che si pestano: due tracce forti nella stessa zona di frequenze, nello stesso momento.

    Come ascolta: ogni traccia del mixer passa per 5 filtri (bassi, medio-bassi, medi, medio-alti, alti) e se ne
    misura l'energia ogni ~40 ms; del master si misurano i picchi prima del limiter e il volume percepito (LUFS).
*/
namespace doctor
{
    enum Severity { tip = 0, warn, red };
    enum Kind { clipping, tooQuiet, tooLoud, dominant, kickBass, stereoBass, offKey, crowding };
    enum Band { bLow = 0, bLowMid, bMid, bHighMid, bHigh, numBands };   // <120, 120-500, 500-2k, 2k-6k, >6k Hz

    inline const char* bandName (int b)
    {
        static const char* n[] { "sotto i 120 Hz", "tra 120 e 500 Hz", "tra 500 Hz e 2 kHz", "tra 2 e 6 kHz", "sopra i 6 kHz" };
        return n[juce::jlimit (0, numBands - 1, b)];
    }
    inline float bandCentre (int b)
    {
        static const float c[] { 70.0f, 250.0f, 1000.0f, 3500.0f, 9000.0f };
        return c[juce::jlimit (0, numBands - 1, b)];
    }

    inline const SequencerEngine::Channel& chan (const SequencerEngine& e, int c)   { return const_cast<SequencerEngine&> (e).getChannel (c); }

    struct OffNote { int pattern, channel, start, length, pitch, velocity, newPitch; };

    struct Issue
    {
        Kind kind;
        Severity severity;
        juce::String title, detail, fixText;
        int track = -1, track2 = -1, channel = -1, pattern = -1, band = -1;
        float amount = 0.0f;               // es. dB da togliere
        int fromBar = -1, toBar = -1;      // dove si sente (battute, da 0)
        std::vector<OffNote> notes;        // note fuori tonalita'
        int scaleRoot = 0, scaleType = 0;
        bool canFix = true;
    };

    struct Report
    {
        std::vector<Issue> issues;
        double seconds = 0.0;
        int bars = 0;
        bool song = false;
        float lufs = -70.0f, peakDb = -70.0f;
        int score() const
        {
            int s = 100;
            for (auto& i : issues) s -= i.severity == red ? 25 : i.severity == warn ? 12 : 5;
            return juce::jlimit (0, 100, s);
        }
        juce::String verdict() const
        {
            const int s = score();
            const int n = (int) issues.size();
            const juce::String what = n == 0 ? juce::String ("niente da sistemare")
                                    : n == 1 ? juce::String ("1 cosa da sistemare") : juce::String (n) + " cose da sistemare";
            bool anyRed = false;
            for (auto& i : issues) anyRed = anyRed || i.severity == red;
            return juce::String (s >= 85 && ! anyRed ? "Ottimo" : s >= 65 && ! anyRed ? "Buono" : s >= 40 ? "Da sistemare" : "Molti problemi") + ", " + what;
        }
    };

    //==========================================================================
    /** I numeri raccolti mentre la copia del progetto suona (thread di analisi). */
    class Ear : public SequencerEngine::AnalysisTap
    {
    public:
        static constexpr int numTracks = mixer::numTracks;

        void prepare (double sampleRate)
        {
            sr = sampleRate;
            frameLen = juce::jmax (256, (int) (sr * 0.04));
            for (auto& t : tracks) t = {};
            masterPeaks.clear();
            lufsBlocks.clear();
            setupFilters();
        }

        void track (int t, const float* L, const float* R, int n, float gl, float gr) noexcept override
        {
            if (t <= 0 || t >= numTracks) return;
            auto& tr = tracks[(size_t) t];
            if (! tr.used)
            {
                tr.used = true;
                tr.frames.reserve (8192);
                tr.filters = filters;
            }
            for (int i = 0; i < n; ++i)
            {
                const float l = L[i] * gl, r = R[i] * gr;
                const float m = 0.5f * (l + r);
                // bassi: L e R separati (per sapere se sono stereo), poi le altre bande sul mono
                const float lowL = tr.filters.lowL[1].process (tr.filters.lowL[0].process (l, 0), 0);
                const float lowR = tr.filters.lowL[1].process (tr.filters.lowL[0].process (r, 1), 1);
                tr.acc[bLow] += 0.25f * (lowL + lowR) * (lowL + lowR);
                tr.side += 0.25f * (lowL - lowR) * (lowL - lowR);
                for (int b = 1; b < numBands; ++b)
                {
                    float y = tr.filters.hp[b].process (m, 0);
                    if (b < numBands - 1) y = tr.filters.lp[b].process (y, 0);
                    tr.acc[b] += y * y;
                }
                tr.total += m * m;
                if (++tr.count >= frameLen)
                {
                    Frame f;
                    for (int b = 0; b < numBands; ++b) { f.band[b] = tr.acc[b] / (float) tr.count; tr.acc[b] = 0.0f; }
                    f.total = tr.total / (float) tr.count;
                    f.side = tr.side / (float) tr.count;
                    tr.total = tr.side = 0.0f;
                    tr.count = 0;
                    // le tracce partono quando arriva qualcosa: si riempie il buco iniziale con silenzio
                    while ((int) tr.frames.size() < frameIndex) tr.frames.push_back ({});
                    tr.frames.push_back (f);
                }
            }
        }

        void master (const float* L, const float* R, int n) noexcept override
        {
            for (int i = 0; i < n; ++i)
            {
                masterPeak = juce::jmax (masterPeak, std::abs (L[i]), std::abs (R[i]));
                // volume percepito (K-weighting, blocchi da 100 ms)
                const float kl = kHp.process (kShelf.process (L[i], 0), 0), kr = kHp.process (kShelf.process (R[i], 1), 1);
                lufsAcc += (double) kl * kl + (double) kr * kr;
                if (++lufsCount >= (int) (sr * 0.1))
                {
                    lufsBlocks.push_back ((float) (lufsAcc / lufsCount));
                    lufsAcc = 0.0; lufsCount = 0;
                }
                if (++masterCount >= frameLen)
                {
                    masterPeaks.push_back (masterPeak);
                    masterPeak = 0.0f; masterCount = 0;
                    ++frameIndex;
                }
            }
        }

        /** Volume integrato (LUFS) con le soglie standard: -70 assoluta e -10 relativa, su finestre da 400 ms. */
        float integratedLufs() const
        {
            std::vector<double> win;
            for (size_t i = 3; i < lufsBlocks.size(); ++i)
                win.push_back ((lufsBlocks[i] + lufsBlocks[i - 1] + lufsBlocks[i - 2] + lufsBlocks[i - 3]) * 0.25);
            auto loud = [] (double ms) { return -0.691 + 10.0 * std::log10 (juce::jmax (1.0e-12, ms)); };
            double sum = 0.0; int count = 0;
            for (double w : win) if (loud (w) > -70.0) { sum += w; ++count; }
            if (count == 0) return -70.0f;
            const double rel = loud (sum / count) - 10.0;
            sum = 0.0; count = 0;
            for (double w : win) if (loud (w) > rel && loud (w) > -70.0) { sum += w; ++count; }
            return count == 0 ? -70.0f : (float) loud (sum / count);
        }

        struct Frame { float band[numBands] {}; float total = 0.0f, side = 0.0f; };
        struct Filters { std::array<mixer::Biquad, 2> lowL; std::array<mixer::Biquad, numBands> hp, lp; };
        struct Track
        {
            bool used = false;
            Filters filters;
            std::vector<Frame> frames;
            float acc[numBands] {};
            float total = 0.0f, side = 0.0f;
            int count = 0;
        };
        std::array<Track, numTracks> tracks;
        std::vector<float> masterPeaks;   // un valore per frame
        double sr = 48000.0;
        int frameLen = 1920;

    private:
        void setupFilters()
        {
            auto lowpass = [] (mixer::Biquad& b, double sampleRate, double f)
            {
                const double w = juce::MathConstants<double>::twoPi * f / sampleRate, al = std::sin (w) / (2.0 * 0.7071), c = std::cos (w);
                b.set ((1 - c) / 2, 1 - c, (1 - c) / 2, 1 + al, -2 * c, 1 - al);
            };
            auto highpass = [] (mixer::Biquad& b, double sampleRate, double f, double q = 0.7071)
            {
                const double w = juce::MathConstants<double>::twoPi * f / sampleRate, al = std::sin (w) / (2.0 * q), c = std::cos (w);
                b.set ((1 + c) / 2, -(1 + c), (1 + c) / 2, 1 + al, -2 * c, 1 - al);
            };
            static constexpr double edges[] { 0.0, 120.0, 500.0, 2000.0, 6000.0, 0.0 };
            for (auto& b : filters.lowL) lowpass (b, sr, 120.0);
            for (int b = 1; b < numBands; ++b)
            {
                highpass (filters.hp[(size_t) b], sr, edges[b]);
                if (b < numBands - 1) lowpass (filters.lp[(size_t) b], sr, edges[b + 1]);
            }
            kShelf.setHighShelf (sr, 1681.0, 4.0);
            highpass (kHp, sr, 38.0, 0.5);
            kShelf.reset(); kHp.reset();
        }

        Filters filters;
        mixer::Biquad kShelf, kHp;
        float masterPeak = 0.0f;
        int masterCount = 0, frameIndex = 0, lufsCount = 0;
        double lufsAcc = 0.0;
        std::vector<float> lufsBlocks;
    };

    //==========================================================================
    // Riconoscere le tracce

    /** La traccia del mixer della cassa (-1 = nessuna). */
    inline int findKickTrack (const SequencerEngine& e, int* channelOut = nullptr)
    {
        for (int c = 0; c < e.getNumChannels(); ++c)
        {
            const auto& ch = chan (e, c);
            const auto name = (ch.name + " " + ch.sampleName).toLowerCase();
            const bool kick = (! e.isSynth (c) && ch.sampleName.isEmpty() && ch.drumType.load() == 0)
                           || name.contains ("kick") || name.contains ("cassa") || name.contains ("bd ") || name.startsWith ("bd");
            if (kick && e.getChannelTrack (c) > 0)
            {
                if (channelOut != nullptr) *channelOut = c;
                return e.getChannelTrack (c);
            }
        }
        return -1;
    }

    /** Lo slot con il sidechain dalla traccia "source" (-1 = non c'e'). */
    inline int sidechainSlot (const SequencerEngine& e, int track, int source)
    {
        const auto& tr = e.getTrack (track);
        for (int s = 0; s < mixer::numSlots; ++s)
            if (tr.slots[(size_t) s].type.load() == mixer::sidechain && ! tr.slots[(size_t) s].bypass.load()
                && juce::roundToInt (tr.slots[(size_t) s].params[0].load()) == source)
                return s;
        return -1;
    }

    inline int freeSlot (const SequencerEngine& e, int track)
    {
        const auto& tr = e.getTrack (track);
        for (int s = 0; s < mixer::numSlots; ++s)
            if (tr.slots[(size_t) s].type.load() == mixer::none) return s;
        return -1;
    }

    inline juce::String dB (float v)   { return juce::String (std::abs (v), 1).replace (".", ","); }

    //==========================================================================
    /** Raggruppa i frame "accesi" in battute: la zona piu' lunga (o la prima). */
    inline void barsOf (const std::vector<bool>& on, double frameSec, double secPerBar, int& from, int& to)
    {
        from = to = -1;
        int bestStart = -1, bestLen = 0, start = -1;
        for (size_t i = 0; i <= on.size(); ++i)
        {
            const bool v = i < on.size() && on[i];
            if (v && start < 0) start = (int) i;
            if (! v && start >= 0)
            {
                if ((int) i - start > bestLen) { bestLen = (int) i - start; bestStart = start; }
                start = -1;
            }
        }
        if (bestStart < 0) return;
        from = (int) std::floor (bestStart * frameSec / secPerBar);
        to = (int) std::floor ((bestStart + bestLen - 1) * frameSec / secPerBar);
    }

    //==========================================================================
    /** I controlli sull'audio ascoltato. */
    inline void checkAudio (const SequencerEngine& e, const Ear& lis, Report& rep)
    {
        const double frameSec = lis.frameLen / lis.sr;
        const double secPerBar = 4.0 * 60.0 / juce::jmax (20.0, e.getBpm());
        const bool masteringOn = e.getMastering().enabled.load();

        // --- 1. il master va oltre 0 dB
        {
            std::vector<bool> over (lis.masterPeaks.size(), false);
            int count = 0; float peak = 0.0f;
            for (size_t i = 0; i < lis.masterPeaks.size(); ++i)
            {
                peak = juce::jmax (peak, lis.masterPeaks[i]);
                if (lis.masterPeaks[i] > 1.0f) { over[i] = true; ++count; }
            }
            rep.peakDb = juce::Decibels::gainToDecibels (peak, -70.0f);
            if (count >= 3)
            {
                Issue is { clipping, count * frameSec > 4.0 ? red : warn };
                barsOf (over, frameSec, secPerBar, is.fromBar, is.toBar);
                is.amount = juce::jlimit (1.0f, 18.0f, rep.peakDb + 1.0f);
                is.title = "Il master va in rosso";
                is.detail = "Picco +" + dB (rep.peakDb) + " dB" + (is.fromBar >= 0 ? " alle battute " + juce::String (is.fromBar + 1) + "-" + juce::String (is.toBar + 1) : juce::String())
                          + ": il limiter di sicurezza schiaccia il suono e si sente meno pulito.";
                is.fixText = "Abbasso il volume del master di " + dB (is.amount) + " dB.";
                rep.issues.push_back (is);
            }
        }
        // --- 2. troppo piano / troppo forte (solo se il mastering e' spento: con il mastering il volume lo decide lui)
        rep.lufs = lis.integratedLufs();
        if (! masteringOn && rep.lufs > -69.0f)
        {
            if (rep.lufs < -17.0f)
            {
                Issue is { tooQuiet, warn };
                is.amount = juce::jlimit (0.0f, 15.0f, -9.5f - rep.lufs);
                is.title = "La canzone e' troppo piano";
                is.detail = "Volume percepito " + juce::String (juce::roundToInt (rep.lufs)) + " LUFS: le canzoni elettroniche stanno di solito tra -10 e -7. "
                            "Accanto ad altri brani sembrera' debole.";
                is.fixText = "Accendo il mastering (stile Club / EDM) e alzo il volume di " + dB (is.amount) + " dB.";
                rep.issues.push_back (is);
            }
            else if (rep.lufs > -5.5f && (rep.issues.empty() || rep.issues.back().kind != clipping))   // (se va in rosso, basta la prima correzione)
            {
                Issue is { tooLoud, warn };
                is.amount = juce::jlimit (1.0f, 10.0f, rep.lufs + 8.0f);
                is.title = "La canzone e' troppo forte";
                is.detail = "Volume percepito " + juce::String (juce::roundToInt (rep.lufs)) + " LUFS: oltre -6 il suono si schiaccia e stanca. "
                            "Spotify e YouTube lo abbassano comunque.";
                is.fixText = "Abbasso il volume del master di " + dB (is.amount) + " dB.";
                rep.issues.push_back (is);
            }
        }

        // energia media di ogni traccia (e per banda)
        struct Stat { int t; double total = 0, band[numBands] {}, side = 0; int frames = 0; };
        std::vector<Stat> stats;
        double sumAll = 0.0;
        for (int t = 1; t < Ear::numTracks; ++t)
        {
            const auto& tr = lis.tracks[(size_t) t];
            if (! tr.used || tr.frames.empty()) continue;
            Stat s; s.t = t;
            for (auto& f : tr.frames)
            {
                s.total += f.total; s.side += f.side;
                for (int b = 0; b < numBands; ++b) s.band[b] += f.band[b];
            }
            s.frames = (int) tr.frames.size();
            if (s.total / juce::jmax (1, s.frames) < 1.0e-7) continue;   // in pratica muta
            sumAll += s.total;
            stats.push_back (s);
        }
        auto levelDb = [] (double energy) { return (float) (10.0 * std::log10 (juce::jmax (1.0e-12, energy))); };
        auto frameOn = [&lis] (int t, size_t i, int band, double thr)
        {
            const auto& fr = lis.tracks[(size_t) t].frames;
            return i < fr.size() && (band < 0 ? fr[i].total : fr[i].band[band]) > thr;
        };

        // --- 3. una traccia copre tutte le altre
        if (stats.size() >= 3)
            for (auto& s : stats)
                if (s.total / sumAll > 0.6)
                {
                    Issue is { dominant, tip };
                    is.track = s.t; is.amount = 3.0f;
                    is.title = "\"" + e.getTrackName (s.t) + "\" copre le altre tracce";
                    is.detail = "Da sola fa " + juce::String (juce::roundToInt (s.total / sumAll * 100.0)) + "% del volume del mix: il resto si sente poco.";
                    is.fixText = "Abbasso il fader di \"" + e.getTrackName (s.t) + "\" di 3 dB.";
                    rep.issues.push_back (is);
                }

        // --- 4. cassa e basso
        const int kick = findKickTrack (e);
        const Stat* kickStat = nullptr;
        for (auto& s : stats) if (s.t == kick) kickStat = &s;
        for (auto& s : stats)
        {
            if (s.t == kick) continue;
            const double lowShare = s.band[bLow] / juce::jmax (1.0e-12, s.total);
            if (lowShare < 0.3) continue;   // non e' un basso
            const auto& fr = lis.tracks[(size_t) s.t].frames;
            // basso stereo
            if (s.side / juce::jmax (1.0e-12, s.band[bLow]) > 0.25)
            {
                Issue is { stereoBass, tip };
                is.track = s.t;
                is.title = "I bassi di \"" + e.getTrackName (s.t) + "\" sono stereo";
                is.detail = "Sotto i 120 Hz il suono e' diverso tra sinistra e destra: in mono (casse del club, telefono) i bassi si indeboliscono.";
                is.fixText = freeSlot (e, s.t) >= 0 ? "Aggiungo l'effetto Stereo con ampiezza 30% sulla traccia." : "Gli slot degli effetti sono pieni: segui la guida.";
                is.canFix = freeSlot (e, s.t) >= 0;
                rep.issues.push_back (is);
            }
            if (kickStat == nullptr || sidechainSlot (e, s.t, kick) >= 0) continue;
            const double kickAvg = kickStat->band[bLow] / juce::jmax (1, kickStat->frames);
            const double bassAvg = s.band[bLow] / juce::jmax (1, s.frames);
            if (levelDb (bassAvg) < levelDb (kickAvg) - 15.0f) continue;
            int kickOn = 0, both = 0;
            std::vector<bool> where (fr.size(), false);
            for (size_t i = 0; i < fr.size(); ++i)
                if (frameOn (kick, i, bLow, kickAvg * 0.6))
                {
                    ++kickOn;
                    if (fr[i].band[bLow] > bassAvg * 0.3) { ++both; where[i] = true; }
                }
            if (kickOn >= 8 && both > kickOn / 2)
            {
                Issue is { kickBass, warn };
                is.track = s.t; is.track2 = kick;
                barsOf (where, frameSec, secPerBar, is.fromBar, is.toBar);
                is.title = "Cassa e \"" + e.getTrackName (s.t) + "\" si coprono";
                is.detail = juce::String (juce::roundToInt (100.0 * both / kickOn)) + "% dei colpi di cassa arrivano mentre \"" + e.getTrackName (s.t)
                            + "\" suona forte sotto i 120 Hz: i bassi si impastano e la cassa perde botta.";
                is.canFix = freeSlot (e, s.t) >= 0;
                is.fixText = is.canFix ? "Aggiungo il sidechain dalla cassa (\"" + e.getTrackName (kick) + "\") sulla traccia del basso."
                                       : "Gli slot degli effetti sono pieni: segui la guida.";
                rep.issues.push_back (is);
            }
        }

        // --- 5. tracce che si pestano (bande medie), al massimo 3
        std::vector<Issue> crowd;
        for (int b = bLowMid; b <= bHighMid; ++b)
            for (size_t x = 0; x < stats.size(); ++x)
                for (size_t y = x + 1; y < stats.size(); ++y)
                {
                    const auto& A = stats[x]; const auto& B = stats[y];
                    if (A.t == kick || B.t == kick) continue;
                    const double shA = A.band[b] / juce::jmax (1.0e-12, A.total), shB = B.band[b] / juce::jmax (1.0e-12, B.total);
                    if (shA < 0.35 || shB < 0.35) continue;
                    const float la = levelDb (A.band[b] / juce::jmax (1, A.frames)), lb = levelDb (B.band[b] / juce::jmax (1, B.frames));
                    if (std::abs (la - lb) > 6.0f) continue;
                    const auto& fa = lis.tracks[(size_t) A.t].frames;
                    const double thrA = A.band[b] / juce::jmax (1, A.frames) * 0.4, thrB = B.band[b] / juce::jmax (1, B.frames) * 0.4;
                    int any = 0, both = 0;
                    std::vector<bool> where (fa.size(), false);
                    for (size_t i = 0; i < fa.size(); ++i)
                    {
                        const bool a = frameOn (A.t, i, b, thrA), c = frameOn (B.t, i, b, thrB);
                        if (a || c) ++any;
                        if (a && c) { ++both; where[i] = true; }
                    }
                    if (any < 10 || both < any / 2) continue;
                    const auto& quiet = la < lb ? A : B;
                    const auto& loud = la < lb ? B : A;
                    Issue is { crowding, tip };
                    is.track = quiet.t; is.track2 = loud.t; is.band = b; is.amount = 3.0f;
                    barsOf (where, frameSec, secPerBar, is.fromBar, is.toBar);
                    is.title = "\"" + e.getTrackName (A.t) + "\" e \"" + e.getTrackName (B.t) + "\" si pestano i piedi";
                    is.detail = "Tutte e due sono forti " + juce::String (bandName (b)) + " nello stesso momento: si coprono a vicenda.";
                    is.canFix = std::abs (e.getTrack (quiet.t).eqMid.load()) < 0.05f;
                    is.fixText = is.canFix ? "Scavo 3 dB a " + juce::String (juce::roundToInt (bandCentre (b))) + " Hz nell'EQ di \"" + e.getTrackName (quiet.t) + "\"."
                                           : "L'EQ di \"" + e.getTrackName (quiet.t) + "\" e' gia' usato: segui la guida.";
                    is.amount = (float) both / (float) any;   // per ordinarli
                    crowd.push_back (is);
                    juce::ignoreUnused (loud);
                }
        std::sort (crowd.begin(), crowd.end(), [] (const Issue& a, const Issue& b) { return a.amount > b.amount; });
        for (size_t i = 0; i < juce::jmin ((size_t) 3, crowd.size()); ++i) { crowd[i].amount = 3.0f; rep.issues.push_back (crowd[i]); }
    }

    //==========================================================================
    /** Note fuori tonalita' (senza ascoltare: si leggono le note dei synth nel piano roll). */
    inline void checkNotes (const SequencerEngine& e, Report& rep)
    {
        struct N { int pattern, channel; SequencerEngine::NoteData note; };
        std::vector<N> notes;
        for (int c = 0; c < e.getNumChannels(); ++c)
        {
            if (! e.isSynth (c)) continue;
            for (int p = 0; p < SequencerEngine::numPatterns; ++p)
                for (auto& n : e.getNotes (p, c))
                    notes.push_back ({ p, c, n });
        }
        if (notes.size() < 8) return;

        int root = e.getScaleRoot(), type = e.getScaleType();
        bool guessed = false;
        if (type <= 0)
        {
            // la tonalita' che "si sente": quella (maggiore o minore) che contiene piu' note, pesate per durata
            double best = -1.0;
            for (int t : { 2, 1 })
                for (int r = 0; r < 12; ++r)
                {
                    double in = 0.0;
                    for (auto& n : notes) if (theory::inScale (n.note.pitch, r, t)) in += n.note.length;
                    if (in > best + 1.0e-6) { best = in; root = r; type = t; }
                }
            double all = 0.0;
            for (auto& n : notes) all += n.note.length;
            if (best / all < 0.88) return;   // non si capisce in che tonalita' e': meglio non dire niente
            guessed = true;
        }
        // per ogni canale e pattern: le note fuori
        std::map<std::pair<int, int>, std::vector<OffNote>> off;
        for (auto& n : notes)
            if (! theory::inScale (n.note.pitch, root, type))
                off[{ n.channel, n.pattern }].push_back ({ n.pattern, n.channel, n.note.start, n.note.length, n.note.pitch, n.note.velocity,
                                                          autotune::nearestInScale (n.note.pitch, root, type) });
        int total = 0;
        for (auto& [k, v] : off) total += (int) v.size();
        if (total == 0 || total > (int) notes.size() / 4) return;   // tante note fuori = e' voluto (cromatismi)

        for (auto& [k, v] : off)
        {
            Issue is { offKey, warn };
            is.channel = k.first; is.pattern = k.second; is.notes = v;
            is.scaleRoot = root; is.scaleType = type;
            const auto key = theory::scaleLabel (root, type);
            juce::StringArray names;
            for (size_t i = 0; i < juce::jmin ((size_t) 4, v.size()); ++i) names.add (theory::noteName (v[i].pitch % 12));
            is.title = juce::String ((int) v.size()) + ((int) v.size() == 1 ? " nota fuori tonalita' in \"" : " note fuori tonalita' in \"")
                       + chan (e, k.first).name + "\"";
            is.detail = juce::String (guessed ? "La canzone sembra in " : "Il progetto e' in ") + key + ": nel pattern "
                        + juce::String (k.second + 1).paddedLeft ('0', 2) + " ci sono " + names.joinIntoString (", ") + (v.size() > 4 ? "..." : "") + ".";
            is.fixText = "Sposto ogni nota sulla nota della scala piu' vicina.";
            const int firstBar = v.front().start / (SequencerEngine::ticksPerStep * 16);
            is.fromBar = is.toBar = -1;
            juce::ignoreUnused (firstBar);
            rep.issues.push_back (is);
        }
    }

    //==========================================================================
    /** La correzione automatica (thread della UI). true = fatto. */
    inline bool applyFix (SequencerEngine& e, const Issue& is)
    {
        switch (is.kind)
        {
            case clipping: case tooLoud:
                e.setMasterVolume (e.getMasterVolume() * juce::Decibels::decibelsToGain (-is.amount));
                return true;
            case tooQuiet:
            {
                auto& m = e.getMastering();
                m.styleIndex.store (2);   // Club / EDM
                const auto& st = mastering::style (2);
                m.loudness.store (st.loudness);
                m.glue.store (st.glue); m.warmth.store (st.warmth); m.width.store (st.width);
                m.gainDb.store (is.amount);
                m.enabled.store (true);
                return true;
            }
            case dominant:
            {
                auto& tr = e.getTrack (is.track);
                tr.volume.store (tr.volume.load() * juce::Decibels::decibelsToGain (-3.0f));
                return true;
            }
            case kickBass:
            {
                const int slot = freeSlot (e, is.track);
                if (slot < 0) return false;
                e.setTrackEffect (is.track, slot, mixer::sidechain);
                auto& s = e.getTrack (is.track).slots[(size_t) slot];
                s.params[0].store ((float) is.track2);
                s.params[1].store (0.6f);
                s.params[2].store (180.0f);
                return true;
            }
            case stereoBass:
            {
                const int slot = freeSlot (e, is.track);
                if (slot < 0) return false;
                e.setTrackEffect (is.track, slot, mixer::stereo);
                e.getTrack (is.track).slots[(size_t) slot].params[0].store (0.3f);
                return true;
            }
            case crowding:
            {
                auto& tr = e.getTrack (is.track);
                if (std::abs (tr.eqMid.load()) > 0.05f) return false;
                tr.eqMidFreq.store (bandCentre (is.band));
                tr.eqMid.store (-3.0f);
                return true;
            }
            case offKey:
            {
                auto list = e.getNotes (is.pattern, is.channel);
                for (auto& n : list)
                    for (auto& o : is.notes)
                        if (n.start == o.start && n.pitch == o.pitch && n.length == o.length)
                            n.pitch = o.newPitch;
                e.setNotes (is.pattern, is.channel, list);
                return true;
            }
        }
        return false;
    }

    //==========================================================================
    /** La guida passo passo: cosa fare a mano, e dove (l'interfaccia sa aprire mixer, piano roll, mastering). */
    enum class Go { nothing, mixer, pianoRoll, mastering, listen };
    struct Step { juce::String text; Go go = Go::nothing; juce::String button; };

    inline std::vector<Step> guide (const SequencerEngine& e, const Issue& is)
    {
        std::vector<Step> s;
        const auto name = is.track >= 0 ? "\"" + e.getTrackName (is.track) + "\"" : juce::String();
        const auto listenText = is.fromBar >= 0 ? "Riascolta dalla battuta " + juce::String (is.fromBar + 1) + " e senti la differenza."
                                                : juce::String ("Riascolta e senti la differenza.");
        switch (is.kind)
        {
            case clipping:
                s.push_back ({ "Apri il mixer: la prima traccia a sinistra e' il Master.", Go::mixer, "Apri il mixer" });
                s.push_back ({ "Abbassa il fader del Master di circa " + dB (is.amount) + " dB, finche' durante la parte piu' forte la lucina rossa in alto non si accende piu'." });
                s.push_back ({ "Se poi la canzone ti sembra troppo piano, non rialzare il fader: accendi il Mastering, che alza il volume senza distorcere.", Go::mastering, "Apri il mastering" });
                s.push_back ({ listenText, Go::listen, "Ascolta" });
                break;
            case tooQuiet:
                s.push_back ({ "Apri il pannello Esporta: in alto c'e' il Mastering.", Go::mastering, "Apri il mastering" });
                s.push_back ({ "Accendi il Mastering e scegli lo stile \"Club / EDM\" (o quello piu' vicino alla tua musica)." });
                s.push_back ({ "Tocca \"Automatico\": JLoop ascolta la canzone e regola il volume per arrivare al livello dello stile." });
                s.push_back ({ listenText, Go::listen, "Ascolta" });
                break;
            case tooLoud:
                s.push_back ({ "Apri il mixer.", Go::mixer, "Apri il mixer" });
                s.push_back ({ "Abbassa il fader del Master di circa " + dB (is.amount) + " dB." });
                s.push_back ({ "Per avere comunque un volume forte e pulito usa il Mastering con lo stile \"Club / EDM\".", Go::mastering, "Apri il mastering" });
                break;
            case dominant:
                s.push_back ({ "Apri il mixer sulla traccia " + name + ".", Go::mixer, "Apri il mixer" });
                s.push_back ({ "Abbassa il suo fader di circa 3 dB." });
                s.push_back ({ "Ascolta: le altre tracce devono uscire meglio, senza che " + name + " sparisca.", Go::listen, "Ascolta" });
                break;
            case kickBass:
                s.push_back ({ "Apri il mixer sulla traccia del basso, " + name + ".", Go::mixer, "Apri il mixer" });
                s.push_back ({ "Tocca uno slot vuoto degli effetti della traccia e scegli \"Sidechain\"." });
                s.push_back ({ "In \"Sorgente\" scegli la traccia della cassa: \"" + e.getTrackName (is.track2) + "\" (la numero " + juce::String (is.track2) + ")." });
                s.push_back ({ "Metti \"Quantita'\" intorno al 60% e \"Rilascio\" tra 150 e 250 ms: a ogni colpo di cassa il basso si abbassa e lascia spazio." });
                s.push_back ({ listenText, Go::listen, "Ascolta" });
                break;
            case stereoBass:
                s.push_back ({ "Apri il mixer sulla traccia " + name + ".", Go::mixer, "Apri il mixer" });
                s.push_back ({ "Tocca uno slot vuoto degli effetti e scegli \"Stereo\"." });
                s.push_back ({ "Porta \"Ampiezza\" intorno al 30%: i bassi diventano quasi mono e restano forti su ogni impianto." });
                s.push_back ({ "In alternativa accendi il Mastering: mette in mono i bassi del mix intero.", Go::mastering, "Apri il mastering" });
                break;
            case crowding:
            {
                const auto other = "\"" + e.getTrackName (is.track2) + "\"";
                s.push_back ({ "Apri il mixer sulla traccia " + name + " (la meno importante delle due in quella zona).", Go::mixer, "Apri il mixer" });
                s.push_back ({ "Nell'EQ della traccia porta la frequenza del MID a circa " + juce::String (juce::roundToInt (bandCentre (is.band))) + " Hz." });
                s.push_back ({ "Abbassa il MID di 3 dB: scavi un po' di spazio per " + other + "." });
                s.push_back ({ "Ascolta: " + other + " deve sentirsi meglio senza che " + name + " sparisca. Se serve, prova 2 o 4 dB.", Go::listen, "Ascolta" });
                break;
            }
            case offKey:
            {
                s.push_back ({ "Apri il piano roll di \"" + chan (e, is.channel).name + "\" nel pattern " + juce::String (is.pattern + 1).paddedLeft ('0', 2) + ".",
                               Go::pianoRoll, "Apri il piano roll" });
                juce::StringArray lines;
                for (size_t i = 0; i < juce::jmin ((size_t) 6, is.notes.size()); ++i)
                {
                    const auto& n = is.notes[i];
                    const int step = n.start / SequencerEngine::ticksPerStep;
                    lines.add (juce::String (theory::noteName (n.pitch % 12)) + juce::String (n.pitch / 12 - 1) + " allo step " + juce::String (step + 1)
                               + juce::String (juce::CharPointer_UTF8 (" \xe2\x86\x92 ")) + theory::noteName (n.newPitch % 12) + juce::String (n.newPitch / 12 - 1));
                }
                s.push_back ({ "Sposta queste note (trascinale su o giu'): " + lines.joinIntoString ("; ") + (is.notes.size() > 6 ? "..." : "") + "." });
                s.push_back ({ "Nel piano roll le righe chiare sono le note di " + theory::scaleLabel (is.scaleRoot, is.scaleType) + ": se resti su quelle, sei sempre in tonalita'." });
                s.push_back ({ "Ascolta il pattern: se una nota \"fuori\" ti piaceva cosi', lasciala pure, a volte e' voluta.", Go::listen, "Ascolta" });
                break;
            }
        }
        return s;
    }

    //==========================================================================
    /** L'ascolto, in un thread a parte (come l'esportazione: una copia del progetto, l'app resta libera). */
    class Analyzer : public juce::Thread
    {
    public:
        Analyzer (const juce::var& projectJson, SampleStore& store)
            : juce::Thread ("JLoop controllo")
        {
            engine = std::make_unique<SequencerEngine>();
            ProjectIO::fromJson (*engine, projectJson);
            instruments = std::make_unique<Instruments::Host> (*engine);
            instruments->resolve();
            for (int c = 0; c < engine->getNumChannels(); ++c)
            {
                auto& ch = engine->getChannel (c);
                ch.sample.store (engine->isSynth (c) || ch.sampleName.isEmpty() ? nullptr : store.load (ch.sampleName));
            }
            engine->setMetronome (false);
            report.song = engine->getSongLengthSteps() > 0;
            engine->setSongMode (report.song);
            engine->setSongPosition (0);
        }
        ~Analyzer() override    { stopThread (10000); }

        void run() override
        {
            const double sr = 32000.0;
            const int steps = report.song ? engine->getSongLengthSteps() : engine->getPatternLength() * 4;
            report.seconds = report.song ? engine->getSongSeconds (steps) : steps * 60.0 / engine->getBpm() / 4.0;
            report.bars = (steps + 15) / 16;
            const auto total = (juce::int64) (juce::jmin (report.seconds, 6.0 * 60.0) * sr);
            listener.prepare (sr);
            engine->prepare (sr);
            engine->setAnalysisTap (&listener);
            engine->setPlaying (true);
            juce::AudioBuffer<float> buffer (2, 512);
            juce::int64 done = 0;
            while (done < total)
            {
                if (threadShouldExit()) { engine->setAnalysisTap (nullptr); return; }
                const int n = (int) juce::jmin ((juce::int64) 512, total - done);
                buffer.clear();
                engine->process (buffer.getArrayOfWritePointers(), 2, n);
                done += n;
                progress.store (0.95f * (float) ((double) done / (double) juce::jmax ((juce::int64) 1, total)));
            }
            engine->setAnalysisTap (nullptr);
            if (total > 0) checkAudio (*engine, listener, report);
            progress.store (1.0f);
            finished.store (true);
        }

        float getProgress() const       { return progress.load(); }
        bool isFinished() const         { return finished.load(); }
        /** Il risultato (dopo isFinished): i controlli sull'audio. Le note si controllano sul progetto vero. */
        Report takeReport()             { return std::move (report); }

    private:
        std::unique_ptr<SequencerEngine> engine;
        std::unique_ptr<Instruments::Host> instruments;   // distrutto prima del motore
        Ear listener;
        Report report;
        std::atomic<float> progress { 0.0f };
        std::atomic<bool> finished { false };

        JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (Analyzer)
    };

    /** Mette in ordine: prima i rossi, poi gli avvisi, poi i consigli. */
    inline void sortIssues (Report& r)
    {
        std::stable_sort (r.issues.begin(), r.issues.end(), [] (const Issue& a, const Issue& b) { return a.severity > b.severity; });
    }
}
