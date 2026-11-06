#pragma once

#include <JuceHeader.h>
#include <array>
#include <cmath>
#include <string>
#include <vector>

//==============================================================================
/**
    VOCI SINTETICHE per le frasi vocali a tempo: un piccolo "cantante" a formanti (stile Klatt).

    Sorgente: impulso glottale (Rosenberg) + respiro, con vibrato, piccole imprecisioni e portamento.
    Tratto vocale: 5 risonatori in cascata che si spostano da una vocale all'altra.
    Consonanti: rumore filtrato (s, sh, f...), esplosive (t, k, p, d, g, b), nasali (m, n), scivolate (l, r, w, y).
    Si lavora a 2x la frequenza di uscita (meno aliasing sulle note alte), poi si dimezza.

    Una frase e' una lista di sillabe: fonemi separati da spazi ("h ey", "l eh t s", "g ow"),
    su che battito comincia, quanto dura (in battiti), che nota (grado della scala) e quanto e' accentata.
*/
namespace vocal
{
    enum Voice { female = 0, male, choir, robot, whisper, numVoices };
    enum Fx { dry = 0, space, echo, radio, numFx };

    inline const char* voiceName (int v)
    {
        static const char* names[] { "Femminile", "Maschile", "Coro", "Robot", "Sussurro" };
        return names[juce::jlimit (0, numVoices - 1, v)];
    }
    inline const char* fxName (int f)
    {
        static const char* names[] { "Asciutta", "Spazio (riverbero)", "Eco a tempo", "Radio" };
        return names[juce::jlimit (0, numFx - 1, f)];
    }

    struct Syl
    {
        const char* text;     // fonemi separati da spazi
        double beat, len;     // inizio e durata in battiti (quarti)
        float degree = 0.0f;  // grado della scala (0 = tonica, 7 = ottava sopra); nelle frasi parlate: semitoni dalla voce normale
        float accent = 1.0f;  // 0..1
        float bend = 0.0f;    // semitoni di scivolata verso la fine della sillaba
    };

    //==========================================================================
    namespace detail
    {
        enum Kind { vowel, glide, nasal, fric, stop, aspir };

        struct Ph
        {
            Kind kind = vowel;
            std::array<float, 3> f { 500, 1500, 2500 }, f2 { 0, 0, 0 };   // f2[0] > 0: dittongo (va da f a f2)
            float voice = 1.0f, noise = 0.0f, nfc = 4000.0f, nbw = 2000.0f, dur = 0.06f;
            bool voicedStop = false;
            bool fromNext = false;   // h: prende le formanti della vocale che segue
        };

        /** Formanti (F1, F2, F3) delle vocali: valori medi di Peterson & Barney, uomo e donna. */
        inline bool vowelFormants (const std::string& c, bool fem, std::array<float, 3>& a, std::array<float, 3>& b)
        {
            struct V { const char* code; float m[3]; float w[3]; };
            static const V table[] {
                { "i",  { 270, 2290, 3010 }, { 310, 2790, 3310 } },
                { "ii", { 270, 2290, 3010 }, { 310, 2790, 3310 } },
                { "ih", { 390, 1990, 2550 }, { 430, 2480, 3070 } },
                { "e",  { 530, 1840, 2480 }, { 610, 2330, 2990 } },
                { "eh", { 530, 1840, 2480 }, { 610, 2330, 2990 } },
                { "ae", { 660, 1720, 2410 }, { 860, 2050, 2850 } },
                { "a",  { 730, 1090, 2440 }, { 850, 1220, 2810 } },
                { "aa", { 730, 1090, 2440 }, { 850, 1220, 2810 } },
                { "o",  { 500,  850, 2450 }, { 560,  950, 2750 } },
                { "oo", { 440, 1020, 2240 }, { 470, 1160, 2680 } },
                { "u",  { 300,  870, 2240 }, { 370,  950, 2670 } },
                { "uh", { 640, 1190, 2390 }, { 760, 1400, 2780 } },
                { "er", { 490, 1350, 1690 }, { 500, 1640, 1960 } },
            };
            auto find = [fem] (const std::string& code, std::array<float, 3>& out)
            {
                for (auto& v : table)
                    if (code == v.code) { for (int k = 0; k < 3; ++k) out[(size_t) k] = fem ? v.w[k] : v.m[k]; return true; }
                return false;
            };
            b = { 0, 0, 0 };
            if (find (c, a)) return true;
            // dittonghi
            struct D { const char* code; const char* from; const char* to; };
            static const D diph[] { { "ey", "e", "ih" }, { "ay", "a", "ih" }, { "ow", "o", "oo" }, { "oy", "o", "ih" }, { "aw", "a", "oo" } };
            for (auto& d : diph)
                if (c == d.code) { find (d.from, a); find (d.to, b); return true; }
            return false;
        }

        inline bool parse (const std::string& token, bool fem, Ph& p)
        {
            std::array<float, 3> a {}, b {};
            if (vowelFormants (token, fem, a, b))
            {
                p.kind = vowel; p.f = a; p.f2 = b; p.voice = 1.0f; p.noise = 0.0f;
                return true;
            }
            const float k = fem ? 1.15f : 1.0f;   // le consonanti di una voce femminile hanno formanti un po' piu' alte
            auto F = [k] (float a1, float a2, float a3) { return std::array<float, 3> { a1 * k, a2 * k, a3 * k }; };
            auto fr = [&p] (float voice, float noise, float fc, float bw, float dur) { p.kind = fric; p.voice = voice; p.noise = noise; p.nfc = fc; p.nbw = bw; p.dur = dur; };
            auto st = [&p] (bool voiced, float noise, float fc, float bw, float dur) { p.kind = stop; p.voicedStop = voiced; p.voice = voiced ? 0.25f : 0.0f; p.noise = noise; p.nfc = fc; p.nbw = bw; p.dur = dur; };
            p.f2 = { 0, 0, 0 };
            if (token == "h")       { p.kind = aspir; p.voice = 0.0f; p.noise = 0.5f; p.dur = 0.07f; p.fromNext = true; }
            else if (token == "s")  fr (0.0f, 0.24f, 6500, 2600, 0.10f);
            else if (token == "z")  fr (0.30f, 0.25f, 6200, 2600, 0.08f);
            else if (token == "sh") fr (0.0f, 0.42f, 3100, 1700, 0.11f);
            else if (token == "zh") fr (0.30f, 0.30f, 3000, 1700, 0.08f);
            else if (token == "f" || token == "th") fr (0.0f, 0.14f, 7000, 5000, 0.08f);
            else if (token == "v")  fr (0.40f, 0.10f, 6000, 5000, 0.06f);
            else if (token == "t")  st (false, 0.55f, 4500, 3000, 0.07f);
            else if (token == "d")  st (true,  0.30f, 3600, 2600, 0.05f);
            else if (token == "k")  st (false, 0.50f, 2100, 1500, 0.08f);
            else if (token == "g")  st (true,  0.30f, 2000, 1500, 0.05f);
            else if (token == "p")  st (false, 0.35f, 1100, 2500, 0.07f);
            else if (token == "b")  st (true,  0.25f, 900,  2000, 0.05f);
            else if (token == "ch") { st (false, 0.5f, 3200, 1800, 0.11f); }
            else if (token == "j")  { st (true, 0.35f, 3000, 1800, 0.09f); }
            else if (token == "m")  { p.kind = nasal; p.f = F (250, 1000, 2250); p.voice = 0.45f; p.dur = 0.07f; }
            else if (token == "n")  { p.kind = nasal; p.f = F (250, 1600, 2600); p.voice = 0.45f; p.dur = 0.07f; }
            else if (token == "ng") { p.kind = nasal; p.f = F (250, 2000, 2700); p.voice = 0.45f; p.dur = 0.07f; }
            else if (token == "l")  { p.kind = glide; p.f = F (360, 1300, 2700); p.voice = 0.7f; p.dur = 0.06f; }
            else if (token == "r")  { p.kind = glide; p.f = F (420, 1300, 1650); p.voice = 0.7f; p.dur = 0.07f; }
            else if (token == "w")  { p.kind = glide; p.f = F (300, 650, 2200);  p.voice = 0.6f; p.dur = 0.06f; }
            else if (token == "y")  { p.kind = glide; p.f = F (280, 2250, 2950); p.voice = 0.6f; p.dur = 0.06f; }
            else return false;
            return true;
        }

        /** Risonatore di Klatt (guadagno 1 in continua): y = a x + b y1 + c y2. */
        struct Reson
        {
            double a = 1, b = 0, c = 0, y1 = 0, y2 = 0;
            void set (double f, double bw, double sr)
            {
                const double T = 1.0 / sr;
                c = -std::exp (-2.0 * juce::MathConstants<double>::pi * bw * T);
                b = 2.0 * std::exp (-juce::MathConstants<double>::pi * bw * T) * std::cos (2.0 * juce::MathConstants<double>::pi * f * T);
                a = 1.0 - b - c;
            }
            float process (float x) noexcept
            {
                const double y = a * x + b * y1 + c * y2;
                y2 = y1; y1 = y;
                return (float) y;
            }
        };

        struct Seg
        {
            double start = 0, end = 0;
            Ph ph;
            std::array<float, 3> fNext {};   // formanti della vocale che segue (per h)
            float pitchA = 60, pitchB = 60, accent = 1;
            bool isVowel = false;
        };

        inline std::vector<std::string> tokens (const char* text)
        {
            std::vector<std::string> out;
            std::string cur;
            for (const char* c = text; *c != 0; ++c)
            {
                if (*c == ' ') { if (! cur.empty()) out.push_back (cur); cur.clear(); }
                else cur += *c;
            }
            if (! cur.empty()) out.push_back (cur);
            return out;
        }
    }

    /** Come canta uno dei cantanti (il coro ne usa diversi). */
    struct Singer
    {
        bool fem = true;
        int style = 0;            // 0 naturale, 1 robot, 2 sussurro
        float detuneCents = 0.0f, offsetSec = 0.0f, formantScale = 1.0f;
        float vibratoCents = 28.0f, breath = 0.08f, tension = 0.0f;   // tension: voce gridata (piu' brillante)
        juce::uint32 seed = 1;
    };

    /** Mette in fila le sillabe: consonanti iniziali prima del battito, vocale sul battito, consonanti finali alla fine. */
    inline std::vector<detail::Seg> layout (const std::vector<Syl>& syls, double bpm, const std::vector<std::pair<float, float>>& pitches, bool fem)
    {
        using namespace detail;
        std::vector<Seg> segs;
        const double spb = 60.0 / juce::jmax (20.0, bpm);
        for (size_t si = 0; si < syls.size(); ++si)
        {
            const auto& s = syls[si];
            const auto toks = tokens (s.text);
            std::vector<Ph> phs;
            for (auto& t : toks) { Ph p; if (parse (t, fem, p)) phs.push_back (p); }
            int firstV = -1, lastV = -1;
            for (int i = 0; i < (int) phs.size(); ++i)
                if (phs[(size_t) i].kind == vowel) { if (firstV < 0) firstV = i; lastV = i; }
            if (firstV < 0) continue;

            const double T = s.beat * spb, E = (s.beat + s.len) * spb;
            const float pa = pitches[si].first, pb = pitches[si].second;
            // formanti della prima vocale (per le h)
            const auto vf = phs[(size_t) firstV].f;

            double onsetDur = 0.0;
            for (int i = 0; i < firstV; ++i) onsetDur += phs[(size_t) i].dur;
            double codaDur = 0.0;
            for (int i = lastV + 1; i < (int) phs.size(); ++i) codaDur += phs[(size_t) i].dur;

            double t = juce::jmax (0.0, T - onsetDur);
            if (! segs.empty()) t = juce::jmax (t, segs.back().end);   // non si accavalla alla sillaba prima
            for (int i = 0; i < firstV; ++i)
            {
                Seg g; g.ph = phs[(size_t) i]; g.start = t; g.end = t + g.ph.dur; g.fNext = vf;
                g.pitchA = g.pitchB = pa; g.accent = s.accent;
                segs.push_back (g); t = g.end;
            }
            // vocali (anche piu' di una: si dividono il tempo)
            const double vowelEnd = juce::jmax (t + 0.05, E - codaDur - 0.012);
            const int nV = lastV - firstV + 1;
            const double each = (vowelEnd - t) / nV;
            for (int i = firstV; i <= lastV; ++i)
            {
                Seg g; g.ph = phs[(size_t) i]; g.start = t; g.end = t + each; g.isVowel = true;
                const float k0 = (float) (i - firstV) / nV, k1 = (float) (i - firstV + 1) / nV;
                g.pitchA = pa + (pb - pa) * k0; g.pitchB = pa + (pb - pa) * k1; g.accent = s.accent;
                segs.push_back (g); t = g.end;
            }
            for (int i = lastV + 1; i < (int) phs.size(); ++i)
            {
                Seg g; g.ph = phs[(size_t) i]; g.start = t; g.end = t + g.ph.dur; g.fNext = vf;
                g.pitchA = g.pitchB = pb; g.accent = s.accent;
                segs.push_back (g); t = g.end;
            }
        }
        return segs;
    }

    /** Un cantante: audio mono a sr (gia' dimezzato dall'oversampling), lungo 'length' campioni. */
    inline void sing (const std::vector<detail::Seg>& segsIn, const Singer& who, double sr, juce::AudioBuffer<float>& out)
    {
        using namespace detail;
        const int os = 2;
        const double sr2 = sr * os;
        const int n = out.getNumSamples();
        const int n2 = n * os;
        std::vector<float> buf ((size_t) n2, 0.0f);

        auto segs = segsIn;
        for (auto& s : segs)
        {
            s.start += who.offsetSec; s.end += who.offsetSec;
            for (auto* f : { &s.ph.f, &s.ph.f2, &s.fNext })
                for (auto& x : *f) x *= who.formantScale;
        }

        Reson cascade[5], noiseRes, fricRes;
        std::array<float, 5> F { 500, 1500, 2500, who.fem ? 4100.0f : 3500.0f, who.fem ? 4900.0f : 4500.0f };
        const float bw[5] { 70, 100, 140, 220, 280 };
        F[3] *= who.formantScale; F[4] *= who.formantScale;
        cascade[3].set (F[3], bw[3], sr2);
        cascade[4].set (F[4], bw[4], sr2);

        juce::Random rnd ((juce::int64) who.seed);
        double phase = rnd.nextDouble(), vibPhase = rnd.nextDouble();
        float pitch = segs.empty() ? 60.0f : segs.front().pitchA;
        float voiceAmp = 0.0f, noiseAmp = 0.0f, aspAmp = 0.0f, prevG = 0.0f, tilt = 0.0f, jitter = 0.0f, jitterTarget = 0.0f;
        double vowelTime = 0.0;
        float fricFc = -1.0f, fricBw = -1.0f;
        size_t si = 0;
        const float formantCoef = 1.0f - std::exp (-1.0f / (0.018f * (float) sr2));
        const float fastCoef = 1.0f - std::exp (-1.0f / (0.006f * (float) sr2));
        const float attackCoef = 1.0f - std::exp (-1.0f / (0.012f * (float) sr2));
        const float releaseCoef = 1.0f - std::exp (-1.0f / (0.045f * (float) sr2));
        const float glideCoef = who.style == 1 ? 1.0f : 1.0f - std::exp (-1.0f / (0.035f * (float) sr2));
        const float oq = juce::jlimit (0.35f, 0.7f, 0.62f - who.tension * 0.2f);   // quota aperta: piu' bassa = voce piu' tesa e brillante
        const float tp = oq * 0.68f;
        const float tiltCoef = 1.0f - std::exp (-juce::MathConstants<float>::twoPi * (who.tension > 0.3f ? 14000.0f : 9000.0f) / (float) sr2);
        float lastX = 0.0f, hpState = 0.0f;
        const float hpCoef = 1.0f - std::exp (-juce::MathConstants<float>::twoPi * 4500.0f / (float) sr2);

        int formantUpdate = 0;
        for (int i = 0; i < n2; ++i)
        {
            const double t = i / sr2;
            while (si < segs.size() && segs[si].end <= t) ++si;
            const Seg* cur = (si < segs.size() && segs[si].start <= t) ? &segs[si] : nullptr;

            // --- obiettivi
            float tv = 0.0f, tn = 0.0f, ta = 0.0f;
            std::array<float, 3> tf { F[0], F[1], F[2] };
            float tpitch = pitch;
            bool vowelNow = false, fast = false;
            if (cur != nullptr)
            {
                const auto& p = cur->ph;
                const double prog = (t - cur->start) / juce::jmax (1.0e-4, cur->end - cur->start);
                tpitch = cur->pitchA + (cur->pitchB - cur->pitchA) * (float) juce::jlimit (0.0, 1.0, (prog - 0.35) / 0.65);
                switch (p.kind)
                {
                    case vowel:
                    {
                        vowelNow = true;
                        const float d = p.f2[0] > 0.0f ? (float) juce::jlimit (0.0, 1.0, (prog - 0.3) / 0.6) : 0.0f;
                        for (int k = 0; k < 3; ++k) tf[(size_t) k] = p.f[(size_t) k] + (p.f2[0] > 0.0f ? (p.f2[(size_t) k] - p.f[(size_t) k]) * d : 0.0f);
                        tv = 1.0f;
                        break;
                    }
                    case glide: case nasal:
                        tf = p.f; tv = p.voice; break;
                    case aspir:
                        tf = cur->fNext; ta = p.noise; break;
                    case fric:
                        tv = p.voice; tn = p.noise; break;
                    case stop:
                    {
                        // chiusura (silenzio o voce soffocata), poi lo scoppio e un po' di soffio
                        fast = true;
                        if (prog < 0.45) tv = p.voicedStop ? 0.18f : 0.0f;
                        else if (prog < 0.62) tn = p.noise * 1.4f;
                        else { tn = p.noise * 0.35f; ta = p.voicedStop ? 0.0f : 0.25f; tv = p.voicedStop ? 0.6f : 0.0f; tf = cur->fNext; }
                        break;
                    }
                }
                const float acc = 0.55f + 0.45f * cur->accent;
                tv *= acc; tn *= acc; ta *= acc;
                if (p.kind == fric || p.kind == stop)
                    if (p.nfc != fricFc || p.nbw != fricBw) { fricFc = p.nfc; fricBw = p.nbw; fricRes.set (juce::jmin (p.nfc * who.formantScale, (float) sr * 0.45f), p.nbw, sr2); }
            }
            if (who.style == 2) { ta = juce::jmax (ta, tv * 0.75f); tv = 0.0f; }   // sussurro: solo fiato

            // --- inseguimento (morbido)
            voiceAmp += (tv - voiceAmp) * (tv > voiceAmp ? attackCoef : releaseCoef);
            noiseAmp += (tn - noiseAmp) * fastCoef;
            aspAmp += (ta - aspAmp) * (fast ? fastCoef : attackCoef);
            const float fc = fast ? fastCoef : formantCoef;
            for (int k = 0; k < 3; ++k) F[(size_t) k] += (tf[(size_t) k] - F[(size_t) k]) * fc;
            if (--formantUpdate <= 0)
            {
                formantUpdate = 16;
                for (int k = 0; k < 3; ++k) cascade[k].set (juce::jmin (F[(size_t) k], (float) sr * 0.45f), bw[k], sr2);
            }
            pitch += (tpitch - pitch) * glideCoef;

            // --- intonazione: vibrato che arriva dopo un po' sulla vocale, piccole imprecisioni
            vowelTime = vowelNow ? vowelTime + 1.0 / sr2 : 0.0;
            float cents = who.detuneCents;
            if (who.style != 1)
            {
                const float vibAmount = (float) juce::jlimit (0.0, 1.0, (vowelTime - 0.22) / 0.35);
                vibPhase += 5.4 / sr2;
                cents += who.vibratoCents * vibAmount * std::sin ((float) (vibPhase * juce::MathConstants<double>::twoPi));
                if ((i & 1023) == 0) jitterTarget = (rnd.nextFloat() * 2.0f - 1.0f) * 9.0f;
                jitter += (jitterTarget - jitter) * 0.0005f;
                cents += jitter;
            }
            const double f0 = 440.0 * std::pow (2.0, (pitch - 69.0 + cents / 100.0) / 12.0);
            phase += f0 / sr2;
            if (phase >= 1.0) phase -= 1.0;

            // --- sorgente
            float src;
            if (who.style == 1)
                src = (float) (2.0 * phase - 1.0) * 0.6f;   // robot: dente di sega pieno
            else
            {
                const float p = (float) phase;
                const float g = p < tp ? 0.5f * (1.0f - std::cos (juce::MathConstants<float>::pi * p / tp))
                              : p < oq ? std::cos (juce::MathConstants<float>::halfPi * (p - tp) / (oq - tp)) : 0.0f;
                src = (g - prevG) * (float) (sr2 / juce::jmax (60.0, f0)) * 0.35f;   // derivata (radiazione delle labbra)
                // respiro modulato dall'apertura della glottide
                src += (rnd.nextFloat() * 2.0f - 1.0f) * who.breath * g;
                prevG = g;
            }
            tilt += (src - tilt) * tiltCoef;
            float x = tilt * voiceAmp + (rnd.nextFloat() * 2.0f - 1.0f) * aspAmp * 0.6f;
            for (auto& r : cascade) x = r.process (x);
            // sopra la quinta formante la cascata taglia troppo: un filo di sorgente "pulita" sugli acuti (aria)
            hpState += (tilt - hpState) * hpCoef;
            x += (tilt - hpState) * voiceAmp * 0.05f;
            float fricOut = 0.0f;
            if (noiseAmp > 0.0005f)
                fricOut = fricRes.process ((rnd.nextFloat() * 2.0f - 1.0f) * noiseAmp) * 1.6f;
            else
                fricRes.process (0.0f);
            // presenza: un po' di enfasi sugli acuti (la cascata da sola e' troppo scura)
            const float presence = x + (x - lastX) * (sr2 > 60000.0 ? 2.2f : 1.2f);
            lastX = x;
            buf[(size_t) i] = presence * 0.5f + fricOut;
        }

        // Dimezza (filtro passa-basso FIR a mezza banda, poi un campione si' e uno no)
        static const std::array<float, 31> fir = []
        {
            std::array<float, 31> h {};
            float sum = 0.0f;
            for (int k = 0; k < 31; ++k)
            {
                const float m = (float) k - 15.0f;
                const float sinc = m == 0.0f ? 1.0f : std::sin (juce::MathConstants<float>::pi * 0.45f * m) / (juce::MathConstants<float>::pi * 0.45f * m);
                const float w = 0.42f - 0.5f * std::cos (juce::MathConstants<float>::twoPi * k / 30.0f) + 0.08f * std::cos (2.0f * juce::MathConstants<float>::twoPi * k / 30.0f);
                h[(size_t) k] = sinc * w;
                sum += h[(size_t) k];
            }
            for (auto& v : h) v /= sum;
            return h;
        }();
        float* o = out.getWritePointer (0);
        for (int j = 0; j < n; ++j)
        {
            float acc = 0.0f;
            const int c = j * os;
            for (int k = 0; k < 31; ++k)
            {
                const int idx = c + k - 15;
                if (idx >= 0 && idx < n2) acc += fir[(size_t) k] * buf[(size_t) idx];
            }
            o[j] += acc;
        }
    }

    /** Ultimi ritocchi: niente continua, picco a -1 dB, poi l'effetto (asciutta, riverbero, eco a tempo, radio). */
    inline void finish (juce::AudioBuffer<float>& audio, double sr, double bpm, int fx)
    {
        float* d = audio.getWritePointer (0);
        const int n = audio.getNumSamples();
        // via la continua + un taglio sotto gli 80 Hz
        float x1 = 0.0f, y1 = 0.0f;
        const float r = 1.0f - juce::MathConstants<float>::twoPi * 80.0f / (float) sr;
        for (int i = 0; i < n; ++i) { const float y = d[i] - x1 + r * y1; x1 = d[i]; y1 = y; d[i] = y; }

        auto normalise = [&] (float target)
        {
            const float peak = audio.getMagnitude (0, 0, n);
            if (peak > 1.0e-6f) audio.applyGain (target / peak);
        };
        // aria e presenza: la cascata a formanti da sola e' scura, le voci nei pezzi elettronici sono brillanti
        {
            juce::IIRFilter shelf, air;
            shelf.setCoefficients (juce::IIRCoefficients::makeHighShelf (sr, 2200.0, 0.7, 3.2));
            air.setCoefficients (juce::IIRCoefficients::makeHighShelf (sr, 7000.0, 0.7, 2.0));
            shelf.processSamples (d, n);
            air.processSamples (d, n);
        }
        normalise (0.89f);

        if (fx == radio)
        {
            juce::IIRFilter hp, lp;
            hp.setCoefficients (juce::IIRCoefficients::makeHighPass (sr, 450.0, 0.9));
            lp.setCoefficients (juce::IIRCoefficients::makeLowPass (sr, 3200.0, 1.1));
            hp.processSamples (d, n);
            lp.processSamples (d, n);
            for (int i = 0; i < n; ++i) d[i] = std::tanh (d[i] * 2.5f);
        }
        else if (fx == echo)
        {
            // eco di un ottavo puntato (3/16), che si spegne piano
            const int delay = juce::jmax (1, (int) std::round (0.75 * 60.0 / juce::jmax (20.0, bpm) * sr));
            std::vector<float> line ((size_t) delay, 0.0f);
            float lp = 0.0f;
            for (int i = 0, w = 0; i < n; ++i, w = (w + 1) % delay)
            {
                const float y = line[(size_t) w];
                lp += (y - lp) * 0.35f;   // ogni ripetizione un po' piu' scura
                line[(size_t) w] = d[i] + lp * 0.42f;
                d[i] += y * 0.38f;
            }
        }
        else if (fx == space)
        {
            juce::Reverb rv;
            rv.setSampleRate (sr);
            juce::Reverb::Parameters p;
            p.roomSize = 0.72f; p.damping = 0.45f; p.wetLevel = 0.28f; p.dryLevel = 0.85f; p.width = 0.0f;
            rv.setParameters (p);
            rv.processMono (d, n);
        }
        if (fx != dry) normalise (0.89f);
    }
}
