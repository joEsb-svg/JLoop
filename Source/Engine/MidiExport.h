#pragma once

#include <JuceHeader.h>
#include "SequencerEngine.h"

//==============================================================================
/**
    Esporta in MIDI (.mid, formato 1): una traccia per canale, con il suo nome.
    - Il pattern attuale, oppure tutta la canzone (le clip della playlist, con ripetizioni, tagli e lunghezze)
    - Step: una nota da un sedicesimo (con i ratchet divisi); batteria sintetizzata sulle note General MIDI
      (kick 36, snare 38, clap 39, hi-hat 42, open hat 46, tom 45) sul canale MIDI 10
    - Note del piano roll cosi' come sono; i canali spenti (mute) non vengono esportati
    Risoluzione: 48 tick per quarto (= quella del sequencer: 12 tick per sedicesimo).
*/
namespace midiexport
{
    inline constexpr int ppq = 48;
    static_assert (SequencerEngine::ticksPerStep * 4 == ppq, "un sedicesimo = 12 tick");

    /** Nota suonata da uno step del canale (prima dell'altezza dello step). */
    inline int stepNote (SequencerEngine& engine, int c)
    {
        auto& ch = engine.getChannel (c);
        if (engine.isSynth (c))
            return ch.stepNote.load();
        if (ch.sample.load() != nullptr)
            return 60;
        static constexpr int gm[] { 36, 38, 39, 42, 46, 45 };
        return gm[juce::jlimit (0, 5, ch.drumType.load())];
    }

    inline bool isDrums (SequencerEngine& engine, int c)
    {
        return ! engine.isSynth (c) && engine.getChannel (c).sample.load() == nullptr;
    }

    /** Aggiunge alla sequenza un pezzo del pattern: step [from, from+len) del pattern ripetuto, a partire dal tick 'at'. */
    inline void addPatternPart (SequencerEngine& engine, juce::MidiMessageSequence& seq, int c, int midiChannel,
                                int pattern, int fromStep, int lenSteps, int at)
    {
        auto& pat = engine.getPattern (pattern);
        const int patLen = juce::jmax (1, pat.length.load());
        const int tps = SequencerEngine::ticksPerStep;
        const int base = stepNote (engine, c);

        // Step
        for (int k = 0; k < lenSteps; ++k)
        {
            const int s = (fromStep + k) % patLen;
            const int vel = pat.velocity[(size_t) c][(size_t) s].load();
            if (vel <= 0) continue;
            const int semis = (int) (int8_t) pat.semis[(size_t) c][(size_t) s].load();
            const int ratchet = juce::jmax (1, (int) pat.ratchet[(size_t) c][(size_t) s].load());
            const int note = juce::jlimit (0, 127, base + semis);
            const int sub = tps / ratchet;
            for (int r = 0; r < ratchet; ++r)
            {
                const int t = at + k * tps + r * sub;
                seq.addEvent (juce::MidiMessage::noteOn (midiChannel, note, (juce::uint8) juce::jlimit (1, 127, vel)), t);
                seq.addEvent (juce::MidiMessage::noteOff (midiChannel, note), t + juce::jmax (1, sub - 1));
            }
        }

        // Note del piano roll (il pattern si ripete finche' dura il pezzo)
        const auto notes = engine.getNotes (pattern, c);
        if (notes.empty()) return;
        const int startTick = fromStep * tps, endTick = startTick + lenSteps * tps, patTicks = patLen * tps;
        for (int rep = startTick / patTicks; rep * patTicks < endTick; ++rep)
            for (auto& n : notes)
            {
                const int on = n.start + rep * patTicks;
                if (on < startTick || on >= endTick) continue;
                const int off = juce::jmin (on + juce::jmax (1, n.length), endTick);
                seq.addEvent (juce::MidiMessage::noteOn (midiChannel, juce::jlimit (0, 127, n.pitch), (juce::uint8) juce::jlimit (1, 127, n.velocity)), at + on - startTick);
                seq.addEvent (juce::MidiMessage::noteOff (midiChannel, juce::jlimit (0, 127, n.pitch)), at + off - startTick);
            }
    }

    /** Scrive il file. song = tutta la playlist, altrimenti il pattern attuale. Ritorna false (con l'errore) se non c'e' niente. */
    inline bool write (SequencerEngine& engine, bool song, const juce::File& file, juce::String& error)
    {
        juce::MidiFile midi;
        midi.setTicksPerQuarterNote (ppq);

        juce::MidiMessageSequence tempoTrack;
        tempoTrack.addEvent (juce::MidiMessage::tempoMetaEvent (juce::roundToInt (60000000.0 / juce::jmax (20.0, engine.getBpm()))), 0);
        tempoTrack.addEvent (juce::MidiMessage::timeSignatureMetaEvent (4, 4), 0);
        midi.addTrack (tempoTrack);

        const auto clips = engine.getClips();
        if (song && clips.empty())
        {
            error = "La playlist e' vuota: non c'e' una canzone da esportare.";
            return false;
        }

        int written = 0;
        for (int c = 0; c < engine.getNumChannels(); ++c)
        {
            auto& ch = engine.getChannel (c);
            if (ch.muted.load()) continue;
            const int midiChannel = isDrums (engine, c) ? 10 : 1;
            juce::MidiMessageSequence seq;
            seq.addEvent (juce::MidiMessage::textMetaEvent (3, ch.name.isNotEmpty() ? ch.name : "Canale " + juce::String (c + 1)), 0);

            if (song)
                for (auto& clip : clips)
                    addPatternPart (engine, seq, c, midiChannel, clip.pattern, clip.offset, engine.getClipLength (clip),
                                    clip.startStep() * SequencerEngine::ticksPerStep);
            else
            {
                const int p = engine.getCurrentPattern();
                addPatternPart (engine, seq, c, midiChannel, p, 0, engine.getPattern (p).length.load(), 0);
            }

            if (seq.getNumEvents() <= 1) continue;   // solo il nome: canale senza note
            seq.updateMatchedPairs();
            seq.sort();
            midi.addTrack (seq);
            ++written;
        }

        if (written == 0)
        {
            error = song ? "Nella canzone non ci sono note da esportare." : "Il pattern e' vuoto: non ci sono note da esportare.";
            return false;
        }

        file.deleteFile();
        juce::FileOutputStream out (file);
        if (! out.openedOk() || ! midi.writeTo (out))
        {
            error = "Non riesco a scrivere il file MIDI.";
            return false;
        }
        return true;
    }
}
