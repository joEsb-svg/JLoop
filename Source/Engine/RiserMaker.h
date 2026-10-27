#pragma once

#include <JuceHeader.h>
#include <map>
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
        bool down = false;  // DOWNER: l'effetto contrario, parte dal drop e scende (impatto all'inizio)
        int dropBar = 16;   // 0 = prima battuta
        Sound sound;                              // carattere di ogni ingrediente (preset o casuale)
        juce::String soundName { "Classico EDM" };
    };

    inline bool hasAudio (const Options& o)   { return o.noise || o.note || o.roll || o.impact; }

    inline double riserSeconds (const Options& o, double bpm)   { return o.bars * 4.0 * 60.0 / juce::jmax (20.0, bpm); }

    //==============================================================================
    // Ricetta: tutto quello che serve per rifare lo stesso riser a un altro tempo
    inline juce::var soundToVar (const Sound& s)
    {
        return juce::Array<juce::var> { s.noiseType, s.noiseFrom, s.noiseOctaves, s.noiseQ, s.noiseGain,
                                        s.toneType, s.rise, s.curve, s.toneGain, s.detune, s.octave, s.stutter,
                                        s.rollType, s.rollFrom, s.rollTo, s.rollStart, s.rollPitch, s.rollGain,
                                        s.impactType, s.gap, s.pump, s.reverseCymbal, s.seed };
    }
    inline Sound soundFromVar (const juce::var& v)
    {
        Sound s;
        auto* a = v.getArray();
        if (a == nullptr || a->size() < 23) return s;
        int i = 0;
        auto n = [&] { return (*a)[i++]; };
        s.noiseType = n(); s.noiseFrom = n(); s.noiseOctaves = n(); s.noiseQ = n(); s.noiseGain = n();
        s.toneType = n(); s.rise = n(); s.curve = n(); s.toneGain = n(); s.detune = n(); s.octave = n(); s.stutter = n();
        s.rollType = n(); s.rollFrom = n(); s.rollTo = n(); s.rollStart = n(); s.rollPitch = n(); s.rollGain = n();
        s.impactType = n(); s.gap = n(); s.pump = n(); s.reverseCymbal = n(); s.seed = n();
        return s;
    }
    inline juce::String recipeToString (const Options& o, double bpm, int root)
    {
        auto* d = new juce::DynamicObject();
        d->setProperty ("kind", o.down ? "downer" : "riser");
        d->setProperty ("bars", o.bars);
        d->setProperty ("noise", o.noise); d->setProperty ("note", o.note); d->setProperty ("roll", o.roll); d->setProperty ("impact", o.impact);
        d->setProperty ("name", o.soundName);
        d->setProperty ("sound", soundToVar (o.sound));
        d->setProperty ("bpm", bpm);
        d->setProperty ("root", root);
        return juce::JSON::toString (juce::var (d), true);
    }
    inline bool recipeFromString (const juce::String& text, Options& o, double& bpm, int& root)
    {
        const auto v = juce::JSON::parse (text);
        if (! v.isObject()) return false;
        o.down = v.getProperty ("kind", "riser").toString() == "downer";
        o.bars = juce::jlimit (1, 16, (int) v.getProperty ("bars", 4));
        o.noise = v.getProperty ("noise", true); o.note = v.getProperty ("note", true); o.roll = v.getProperty ("roll", true); o.impact = v.getProperty ("impact", true);
        o.soundName = v.getProperty ("name", "Classico EDM").toString();
        o.sound = soundFromVar (v.getProperty ("sound", {}));
        bpm = (double) v.getProperty ("bpm", 120.0);
        root = (int) v.getProperty ("root", 0);
        return true;
    }

    /** L'audio del riser (mono). La salita dura 'bars' battute; l'impatto (se c'e') comincia subito dopo.
        DOWNER: tutto al contrario. L'impatto e' all'inizio, rumore e nota scendono, la rullata rallenta. */
    inline juce::AudioBuffer<float> render (const Options& o, double bpm, double sr, int rootPitchClass)
    {
        const auto& s = o.sound;
        const int T = (int) (riserSeconds (o, bpm) * sr);
        const int tail = o.down ? juce::jmax (0, (o.impact ? (int) (3.0 * sr) : 0) - T) + (int) (0.05 * sr)
                                : o.impact ? (int) (3.0 * sr) : (int) (0.05 * sr);
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
        if (o.down)   // la salita al contrario e' una discesa
        {
            auto* b = bed.getWritePointer (0);
            std::reverse (b, b + T);
        }
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
        if (o.roll) detail::rollLayer (y, T, sr, bpm, o.bars * 4.0, s, rnd, o.down);

        if (o.down)
        {
            const int fadeIn = juce::jmin (T, (int) (0.002 * sr));
            for (int i = 0; i < fadeIn; ++i) y[i] *= (float) i / (float) fadeIn;
            // la coda sfuma fino al silenzio alla fine della discesa
            const int fade = juce::jmin (T, (int) (0.04 * T));
            for (int i = 0; i < fade; ++i) y[T - fade + i] *= 1.0f - (float) i / (float) fade;
            if (o.impact)
            {
                juce::AudioBuffer<float> hit (1, (int) (3.0 * sr));
                hit.clear();
                detail::impactLayer (hit.getWritePointer (0), hit.getNumSamples(), sr, s.impactType, rnd);
                out.addFrom (0, 0, hit, 0, 0, juce::jmin (hit.getNumSamples(), out.getNumSamples()));
            }
        }
        else
        {
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
        }

        const float peak = out.getMagnitude (0, 0, out.getNumSamples());
        if (peak > 0.0001f) out.applyGain (0.89f / peak);
        return out;
    }

    /** Scrive il riser/downer come campione (nome con durata e BPM) e lo carica. Ritorna il nome del file, vuoto se fallisce. */
    inline juce::String writeSample (SampleStore& store, const Options& o, double bpm, int root, SampleData*& data)
    {
        data = nullptr;
        auto audio = render (o, bpm, 44100.0, root);
        auto folder = SampleStore::getFolder();
        folder.createDirectory();
        const juce::String bpmText = std::abs (bpm - std::round (bpm)) < 0.05 ? juce::String (juce::roundToInt (bpm)) : juce::String (bpm, 1);
        const auto base = juce::File::createLegalFileName (juce::String (o.down ? "Downer " : "Riser ") + o.soundName + " " + juce::String (o.bars)
                                                           + (o.bars == 1 ? " battuta " : " battute ") + bpmText + " BPM");
        juce::File file = folder.getChildFile (base + ".wav");
        for (int i = 2; file.exists(); ++i) file = folder.getChildFile (base + " (" + juce::String (i) + ").wav");
        {
            juce::WavAudioFormat wav;
            std::unique_ptr<juce::OutputStream> os = std::make_unique<juce::FileOutputStream> (file);
            auto w = wav.createWriterFor (os, juce::AudioFormatWriterOptions{}.withSampleRate (44100.0).withNumChannels (1).withBitsPerSample (16));
            if (w == nullptr || ! w->writeFromAudioSampleBuffer (audio, 0, audio.getNumSamples()))
                return {};
        }
        data = store.load (file.getFileName());
        return data != nullptr ? file.getFileName() : juce::String();
    }

    /** I BPM sono cambiati: rifa' a tempo tutti i riser e downer del progetto (stessa ricetta, stessa durata in battute).
        Ritorna quanti ne ha rifatti. Thread della UI. */
    inline int retimeAll (SequencerEngine& engine, SampleStore& store)
    {
        const double bpm = engine.getBpm();
        int count = 0;
        std::map<juce::String, juce::String> made;   // vecchio nome -> nuovo (canali che usano lo stesso riser)
        for (int c = 0; c < engine.getNumChannels(); ++c)
        {
            auto& ch = engine.getChannel (c);
            auto it = engine.sampleRecipes.find (ch.sampleName);
            if (it == engine.sampleRecipes.end()) continue;
            Options o; double madeAt = 0.0; int root = 0;
            if (! recipeFromString (it->second, o, madeAt, root) || std::abs (madeAt - bpm) < 0.05) continue;

            juce::String newName;
            if (auto m = made.find (ch.sampleName); m != made.end()) newName = m->second;
            else
            {
                // c'e' gia' un file fatto con questa ricetta a questo tempo? si riusa
                const auto wanted = recipeToString (o, bpm, root);
                for (auto& [name, recipe] : engine.sampleRecipes)
                    if (recipe == wanted && SampleStore::getFolder().getChildFile (name).existsAsFile()) { newName = name; break; }
                if (newName.isEmpty())
                {
                    SampleData* data = nullptr;
                    newName = writeSample (store, o, bpm, root, data);
                    if (newName.isEmpty()) continue;
                    engine.sampleRecipes[newName] = wanted;
                }
                made[ch.sampleName] = newName;
            }
            if (auto* data = store.load (newName))
            {
                ch.sampleName = newName;
                ch.sample.store (data);
                ch.touchSound();
                ++count;
            }
        }
        return count;
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
        // il riser finisce sul drop; il downer parte dal drop
        const int startBar = o.down ? o.dropBar : o.dropBar - o.bars;
        const int endBar = startBar + o.bars;
        if (! hasAudio (o) && ! (o.filter && o.toPlaylist))
        {
            r.error = "Scegli almeno un ingrediente.";
            return r;
        }
        if (o.toPlaylist && ! o.down && startBar < 0)
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
                        if (s < (endBar + (o.down ? 0 : 1)) * SequencerEngine::stepsPerBar && e > startBar * SequencerEngine::stepsPerBar) freeTrack = false;
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

            // --- il campione (con la ricetta, per rifarlo se cambiano i BPM)
            const double bpm = engine.getBpm();
            const int root = engine.getScaleRoot();
            SampleData* data = nullptr;
            const auto fileName = writeSample (store, o, bpm, root, data);
            if (fileName.isEmpty()) { r.error = "Non riesco a salvare il campione."; return r; }
            engine.sampleRecipes[fileName] = recipeToString (o, bpm, root);

            // --- il canale
            r.channel = engine.addChannel();
            if (r.channel < 0) { r.error = "Hai raggiunto il massimo di canali."; return r; }
            auto& ch = engine.getChannel (r.channel);
            ch.type.store (SequencerEngine::drumOrSample);
            ch.sampleName = fileName;
            ch.sample.store (data);
            ch.colour.store (o.down ? 0xffff8a4fu : 0xff4fd8ffu);
            ch.name = (o.down ? "Downer " : "Riser ") + o.soundName;
            ch.customName = true;
            ch.touchSound();
            r.sampleName = fileName;

            // --- il pattern: lungo quanto il riser, un colpo sul primo step (il campione suona tutto)
            auto& pat = engine.getPattern (r.pattern);
            pat.length.store (juce::jmin (SequencerEngine::maxSteps, o.bars * SequencerEngine::stepsPerBar));   // lungo quanto la salita
            pat.velocity[(size_t) r.channel][0].store ((uint8_t) 110);
            pat.probability[(size_t) r.channel][0].store ((uint8_t) 100);
            pat.ratchet[(size_t) r.channel][0].store ((uint8_t) 1);
            engine.setPatternName (r.pattern, (o.down ? "Downer " : "Riser ") + o.soundName + " (" + juce::String (o.bars) + ")");

            if (o.toPlaylist)
            {
                SequencerEngine::Clip c;
                c.track = r.track; c.bar = startBar; c.pattern = r.pattern; c.length = 0;   // la clip copre tutta la salita
                clips.push_back (c);
                engine.setClips (clips);
                r.startBar = startBar;
            }
        }

        // --- filtro sul master. Riser: passa-alto che sale per tutta la salita e si riapre sul drop.
        //     Downer: passa-basso che si chiude per tutta la discesa e si riapre alla fine.
        if (o.filter && o.toPlaylist)
        {
            auto& master = engine.getTrack (0);
            const float type = o.down ? 0.0f : 1.0f;
            int slot = -1;
            for (int s = 0; s < mixer::numSlots && slot < 0; ++s)
                if (master.slots[(size_t) s].type.load() == mixer::filter && std::abs (master.slots[(size_t) s].params[0].load() - type) < 0.5f) slot = s;
            for (int s = 0; s < mixer::numSlots && slot < 0; ++s)
                if (master.slots[(size_t) s].type.load() == mixer::none) slot = s;
            if (slot >= 0)
            {
                if (master.slots[(size_t) slot].type.load() != mixer::filter)
                    engine.setTrackEffect (0, slot, mixer::filter);
                auto& sl = master.slots[(size_t) slot];
                const float open = o.down ? 20000.0f : 20.0f, closed = o.down ? 250.0f : 1200.0f;
                sl.params[0].store (type);
                sl.params[1].store (open);
                sl.params[2].store (0.25f);
                sl.params[3].store (0.0f);
                const auto& cutDef = mixer::info (mixer::filter).params[1];
                automation::Target t { automation::effectParam, 0, slot, 1 };
                const int lane = engine.addLane (t);
                if (lane >= 0)
                {
                    constexpr int ticksPerBar = 192;
                    auto pts = engine.getLanePoints (lane);
                    const int a = startBar * ticksPerBar, b = endBar * ticksPerBar;
                    pts.erase (std::remove_if (pts.begin(), pts.end(), [a, b] (const automation::Point& p) { return p.tick >= a && p.tick <= b; }), pts.end());
                    pts.push_back ({ a, mixer::toNormalised (cutDef, open) });
                    pts.push_back ({ b - 2, mixer::toNormalised (cutDef, closed) });
                    pts.push_back ({ b, mixer::toNormalised (cutDef, open) });
                    std::sort (pts.begin(), pts.end(), [] (const automation::Point& x, const automation::Point& y) { return x.tick < y.tick; });
                    engine.setLanePoints (lane, pts);
                }
            }
        }

        r.ok = true;
        return r;
    }
}
