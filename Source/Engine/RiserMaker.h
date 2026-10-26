#pragma once

#include <JuceHeader.h>
#include "SequencerEngine.h"
#include "SampleStore.h"
#include "RiserSound.h"

//==============================================================================
/**
    RISER AUTOMATICO: prepara la salita prima di un drop, a tempo col progetto.
    Ingredienti (si possono combinare):
    - rumore che sale (filtro che si apre e volume che cresce)
    - nota che sale (supersaw che sale di due ottave, nella tonalita' del progetto)
    - rullata che accelera (1/4 -> 1/8 -> 1/16 -> 1/32)
    - impatto alla fine (botta + coda, proprio sul drop)
    - filtro sul master (passa-alto che sale e si riapre di colpo sul drop: automazione nella playlist)
    L'audio diventa un campione ("Riser 4 battute 128 BPM.wav") su un canale nuovo, in un pattern nuovo;
    se si sceglie la playlist, il pattern viene piazzato in modo che il riser finisca sulla battuta del drop.
*/
namespace riser
{
    struct Options
    {
        int bars = 4;
        bool noise = true, note = true, roll = true, impact = true, filter = false;
        bool toPlaylist = true;
        int dropBar = 16;   // 0 = prima battuta
        Sound sound;                              // carattere di ogni ingrediente (preset o casuale)
        juce::String soundName { "Classico EDM" };
    };

    inline bool hasAudio (const Options& o)   { return o.noise || o.note || o.roll || o.impact; }

    inline double riserSeconds (const Options& o, double bpm)   { return o.bars * 4.0 * 60.0 / juce::jmax (20.0, bpm); }

    /** L'audio del riser (mono). La salita dura 'bars' battute; l'impatto (se c'e') comincia subito dopo. */
    inline juce::AudioBuffer<float> render (const Options& o, double bpm, double sr, int rootPitchClass)
    {
        const auto& s = o.sound;
        const int T = (int) (riserSeconds (o, bpm) * sr);
        const int tail = o.impact ? (int) (3.0 * sr) : (int) (0.05 * sr);
        juce::AudioBuffer<float> out (1, T + tail);
        out.clear();
        auto* y = out.getWritePointer (0);
        juce::Random rnd (s.seed);

        // rumore e nota (su cui agisce la "pompa" del sidechain)
        juce::AudioBuffer<float> bed (1, T);
        bed.clear();
        if (o.noise) detail::noiseLayer (bed.getWritePointer (0), T, sr, s, rnd);
        if (o.note && s.toneGain > 0.0f) detail::toneLayer (bed.getWritePointer (0), T, sr, bpm, s, rootPitchClass, rnd);
        if (o.noise && s.reverseCymbal) detail::reverseCymbalLayer (bed.getWritePointer (0), T, sr, bpm, rnd);
        if (s.pump)
        {
            const double beat = 60.0 / bpm * sr;
            auto* b = bed.getWritePointer (0);
            for (int i = 0; i < T; ++i)
            {
                const double t = std::fmod ((double) i, beat) / sr;
                b[i] *= (float) (1.0 - 0.85 * std::exp (-t / 0.09));
            }
        }
        out.addFrom (0, 0, bed, 0, 0, T);
        if (o.roll) detail::rollLayer (y, T, sr, bpm, o.bars * 4.0, s, rnd);

        // pausa prima del drop: l'ultimo mezzo quarto tace (con 3 ms di sfumatura)
        if (s.gap && (o.noise || o.note || o.roll))
        {
            const int gapLen = juce::jmin (T, (int) (0.5 * 60.0 / bpm * sr));
            const int fadeLen = (int) (0.003 * sr);
            for (int i = T - gapLen - fadeLen; i < T; ++i)
                if (i >= 0) y[i] *= i < T - gapLen ? 1.0f - (float) (i - (T - gapLen - fadeLen)) / (float) fadeLen : 0.0f;
        }

        if (o.impact) detail::impactLayer (y + T, tail, sr, s.impactType, rnd);

        // il riser si chiude di netto sul drop (2 ms di sfumatura per non fare click)
        const int fade = juce::jmin (T, (int) (0.002 * sr));
        if (o.noise || o.note || o.roll)
            for (int i = 0; i < fade; ++i)
                y[T - fade + i] *= 1.0f - (float) i / (float) fade;

        const float peak = out.getMagnitude (0, 0, out.getNumSamples());
        if (peak > 0.0001f) out.applyGain (0.89f / peak);
        return out;
    }

    /** Battuta del drop proposta: dove nella playlist entrano piu' clip insieme (o la fine della canzone). */
    inline int suggestDropBar (const SequencerEngine& engine, int bars)
    {
        const auto clips = engine.getClips();
        const int songBars = (engine.getSongLengthSteps() + SequencerEngine::stepsPerBar - 1) / SequencerEngine::stepsPerBar;
        if (clips.empty()) return bars;
        auto active = [&] (int bar)
        {
            int n = 0;
            for (auto& c : clips)
            {
                const int s = c.startStep(), e = s + engine.getClipLength (c);
                if (bar * SequencerEngine::stepsPerBar >= s && bar * SequencerEngine::stepsPerBar < e) ++n;
            }
            return n;
        };
        int best = -1, bestScore = 0;
        for (int b = juce::jmax (1, bars); b < songBars; ++b)
        {
            const int score = active (b) - active (b - 1);
            if (score > bestScore) { bestScore = score; best = b; }
        }
        return best >= 0 ? best : juce::jmax (bars, songBars);
    }

    struct Result
    {
        bool ok = false;
        juce::String error, sampleName;
        int channel = -1, pattern = -1, track = -1, startBar = -1;
    };

    /** Crea tutto (campione, canale, pattern, clip, automazione). Va chiamato dal thread della UI. */
    inline Result apply (SequencerEngine& engine, SampleStore& store, const Options& o)
    {
        Result r;
        const int startBar = o.dropBar - o.bars;
        if (! hasAudio (o) && ! (o.filter && o.toPlaylist))
        {
            r.error = "Scegli almeno un ingrediente.";
            return r;
        }
        if (o.toPlaylist && startBar < 0)
        {
            r.error = "Il riser dura " + juce::String (o.bars) + " battute: il drop deve essere almeno alla battuta " + juce::String (o.bars + 1) + ".";
            return r;
        }

        // Traccia libera della playlist per tutta la salita (e il drop)
        auto clips = engine.getClips();
        if (o.toPlaylist && hasAudio (o))
        {
            for (int t = 0; t < SequencerEngine::numTracks && r.track < 0; ++t)
            {
                bool freeTrack = true;
                for (auto& c : clips)
                    if (c.track == t)
                    {
                        const int s = c.startStep(), e = s + engine.getClipLength (c);
                        if (s < (o.dropBar + 1) * SequencerEngine::stepsPerBar && e > startBar * SequencerEngine::stepsPerBar) freeTrack = false;
                    }
                if (freeTrack) r.track = t;
            }
            if (r.track < 0)
            {
                r.error = "Nella playlist non c'e' una traccia libera in quelle battute.";
                return r;
            }
        }

        if (hasAudio (o))
        {
            r.pattern = engine.getFirstFreePattern();
            if (r.pattern < 0) { r.error = "Tutti i pattern sono gia' usati."; return r; }

            // --- il campione
            const double bpm = engine.getBpm();
            auto audio = render (o, bpm, 44100.0, engine.getScaleRoot());
            auto folder = SampleStore::getFolder();
            folder.createDirectory();
            const auto base = juce::File::createLegalFileName ("Riser " + o.soundName + " " + juce::String (o.bars) + (o.bars == 1 ? " battuta " : " battute ")
                                                               + juce::String (juce::roundToInt (bpm)) + " BPM");
            juce::File file = folder.getChildFile (base + ".wav");
            for (int i = 2; file.exists(); ++i) file = folder.getChildFile (base + " (" + juce::String (i) + ").wav");
            {
                juce::WavAudioFormat wav;
                std::unique_ptr<juce::OutputStream> os = std::make_unique<juce::FileOutputStream> (file);
                auto w = wav.createWriterFor (os, juce::AudioFormatWriterOptions{}.withSampleRate (44100.0).withNumChannels (1).withBitsPerSample (16));
                if (w == nullptr || ! w->writeFromAudioSampleBuffer (audio, 0, audio.getNumSamples()))
                {
                    r.error = "Non riesco a salvare il riser.";
                    return r;
                }
            }
            auto* data = store.load (file.getFileName());
            if (data == nullptr) { r.error = "Non riesco a caricare il riser."; return r; }

            // --- il canale
            r.channel = engine.addChannel();
            if (r.channel < 0) { r.error = "Hai raggiunto il massimo di canali."; return r; }
            auto& ch = engine.getChannel (r.channel);
            ch.type.store (SequencerEngine::drumOrSample);
            ch.sampleName = file.getFileName();
            ch.sample.store (data);
            ch.colour.store (0xff4fd8ff);
            ch.name = "Riser " + o.soundName;
            ch.customName = true;
            ch.touchSound();
            r.sampleName = file.getFileName();

            // --- il pattern: lungo quanto il riser, un colpo sul primo step (il campione suona tutto)
            auto& pat = engine.getPattern (r.pattern);
            pat.length.store (juce::jmin (SequencerEngine::maxSteps, o.bars * SequencerEngine::stepsPerBar));   // lungo quanto la salita
            pat.velocity[(size_t) r.channel][0].store ((uint8_t) 110);
            pat.probability[(size_t) r.channel][0].store ((uint8_t) 100);
            pat.ratchet[(size_t) r.channel][0].store ((uint8_t) 1);
            engine.setPatternName (r.pattern, "Riser " + o.soundName + " (" + juce::String (o.bars) + ")");

            if (o.toPlaylist)
            {
                SequencerEngine::Clip c;
                c.track = r.track; c.bar = startBar; c.pattern = r.pattern; c.length = 0;   // la clip copre tutta la salita
                clips.push_back (c);
                engine.setClips (clips);
                r.startBar = startBar;
            }
        }

        // --- filtro sul master: passa-alto che sale per tutta la salita e si riapre sul drop
        if (o.filter && o.toPlaylist)
        {
            auto& master = engine.getTrack (0);
            int slot = -1;
            for (int s = 0; s < mixer::numSlots && slot < 0; ++s)
                if (master.slots[(size_t) s].type.load() == mixer::filter) slot = s;
            for (int s = 0; s < mixer::numSlots && slot < 0; ++s)
                if (master.slots[(size_t) s].type.load() == mixer::none) slot = s;
            if (slot >= 0)
            {
                if (master.slots[(size_t) slot].type.load() != mixer::filter)
                    engine.setTrackEffect (0, slot, mixer::filter);
                auto& sl = master.slots[(size_t) slot];
                sl.params[0].store (1.0f);     // passa-alto
                sl.params[1].store (20.0f);
                sl.params[2].store (0.25f);
                sl.params[3].store (0.0f);
                const auto& cutDef = mixer::info (mixer::filter).params[1];
                automation::Target t { automation::effectParam, 0, slot, 1 };
                const int lane = engine.addLane (t);
                if (lane >= 0)
                {
                    constexpr int ticksPerBar = 192;
                    auto pts = engine.getLanePoints (lane);
                    const int a = startBar * ticksPerBar, b = o.dropBar * ticksPerBar;
                    pts.erase (std::remove_if (pts.begin(), pts.end(), [a, b] (const automation::Point& p) { return p.tick >= a && p.tick <= b; }), pts.end());
                    pts.push_back ({ a, mixer::toNormalised (cutDef, 20.0f) });
                    pts.push_back ({ b - 2, mixer::toNormalised (cutDef, 1200.0f) });
                    pts.push_back ({ b, mixer::toNormalised (cutDef, 20.0f) });
                    std::sort (pts.begin(), pts.end(), [] (const automation::Point& x, const automation::Point& y) { return x.tick < y.tick; });
                    engine.setLanePoints (lane, pts);
                }
            }
        }

        r.ok = true;
        return r;
    }
}
