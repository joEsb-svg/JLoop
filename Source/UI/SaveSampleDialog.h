#pragma once

#include <JuceHeader.h>
#include <functional>
#include "../Engine/SequencerEngine.h"
#include "../Engine/SampleStore.h"
#include "../Engine/MusicTheory.h"
#include "../Instruments/Instruments.h"
#include "Theme.h"
#include "WaveView.h"

//==============================================================================
/**
    "Salva come campione": il suono del synth del canale, cosi' com'e' adesso (manopole ed effetti
    del synth compresi), suonato su una nota per una certa durata e registrato in un file audio
    dentro "I miei campioni". Il file resta per sempre e si usa come qualsiasi campione
    (batteria, Slicer, canali campione), anche in altri progetti.

    Il synth del canale non viene toccato: se ne fa una copia (stesso stato) e si registra quella.
*/
class SaveSampleDialog : public juce::Component
{
public:
    SaveSampleDialog (SequencerEngine& e, SampleStore& store, int channelIndex)
        : engine (e), samples (store), channel (channelIndex)
    {
        title.setText ("Salva il suono come campione", juce::dontSendNotification);
        title.setFont (Theme::font (16.0f, true));
        title.setColour (juce::Label::textColourId, Theme::textBright);
        addAndMakeVisible (title);

        nameEditor.setFont (Theme::font (15.0f));
        nameEditor.setText (suggestedName(), juce::dontSendNotification);
        nameEditor.setSelectAllWhenFocused (true);
        addAndMakeVisible (nameEditor);

        lowerButton.onClick  = [this] { setNote (note - 1); };
        higherButton.onClick = [this] { setNote (note + 1); };
        octDownButton.onClick = [this] { setNote (note - 12); };
        octUpButton.onClick   = [this] { setNote (note + 12); };
        for (int i = 0; i < numLengths; ++i)
        {
            auto* b = lengthButtons.add (new juce::TextButton (lengthName (i)));
            b->setClickingTogglesState (true);
            b->setRadioGroupId (4711);
            b->setToggleState (i == lengthIndex, juce::dontSendNotification);
            b->onClick = [this, i] { lengthIndex = i; rendered = nullptr; updateWave(); };
            addAndMakeVisible (b);
        }
        listenButton.onClick = [this] { if (auto* s = render()) engine.previewSample (s); };
        cancelButton.onClick = [this] { if (onDone) onDone(); };
        saveButton.onClick   = [this] { save(); };
        saveButton.setColour (juce::TextButton::buttonColourId, Theme::accent.darker (0.35f));
        for (auto* b : { &lowerButton, &higherButton, &octDownButton, &octUpButton, &listenButton, &cancelButton, &saveButton })
            addAndMakeVisible (b);
        addAndMakeVisible (noteLabel);
        noteLabel.setJustificationType (juce::Justification::centred);
        noteLabel.setFont (Theme::font (14.0f, true));
        wave.setEmptyText ("Tocca Ascolta per sentirlo e vederne l'onda");
        addAndMakeVisible (wave);
        setNote (isDrumKit() ? 36 : 48);
    }

    std::function<void()> onDone;
    /** Dopo il salvataggio: il nome del file dentro "I miei campioni". */
    std::function<void (const juce::String&)> onSaved;

    int preferredHeight (int) const   { return 330; }

    /** Il nome di una nota come nel piano roll (60 = Do4). */
    static juce::String noteText (int n)   { return juce::String (theory::noteName (n)) + juce::String (n / 12 - 1); }

    /** Per le prove: registra senza la finestra. */
    const SampleData* renderForTest (int n, int len)   { note = n; lengthIndex = len; rendered = nullptr; return render(); }

    void paint (juce::Graphics& g) override
    {
        g.fillAll (Theme::panel);
        g.setColour (Theme::textDim);
        g.setFont (Theme::font (12.0f));
        for (auto& [r, t] : captions)
            g.drawText (t, r, juce::Justification::bottomLeft, false);
    }

    void resized() override
    {
        captions.clear();
        auto r = getLocalBounds().reduced (14, 10);
        title.setBounds (r.removeFromTop (26));
        auto caption = [this, &r] (const juce::String& t) { captions.push_back ({ r.removeFromTop (18), t }); };
        caption ("Nome");
        nameEditor.setBounds (r.removeFromTop (32));
        r.removeFromTop (4);

        auto row = r.removeFromTop (18 + 34);
        auto left = row.removeFromLeft (row.getWidth() * 45 / 100);
        captions.push_back ({ left.removeFromTop (18), "Nota" });
        captions.push_back ({ row.removeFromTop (18).withTrimmedLeft (10), "Lunghezza" });
        const int bw = juce::jmax (30, left.getWidth() / 6);
        octDownButton.setBounds (left.removeFromLeft (bw).reduced (1, 2));
        lowerButton.setBounds (left.removeFromLeft (bw).reduced (1, 2));
        octUpButton.setBounds (left.removeFromRight (bw).reduced (1, 2));
        higherButton.setBounds (left.removeFromRight (bw).reduced (1, 2));
        noteLabel.setBounds (left);
        row.removeFromLeft (10);
        const int lw = row.getWidth() / numLengths;
        for (auto* b : lengthButtons) b->setBounds (row.removeFromLeft (lw).reduced (1, 2));

        r.removeFromTop (6);
        caption ("Anteprima (effetti del synth e coda compresi)");
        auto w = r.removeFromTop (juce::jmax (50, r.getHeight() - 46));
        listenButton.setBounds (w.removeFromRight (90).reduced (4, 6));
        wave.setBounds (w);
        r.removeFromTop (6);
        auto buttons = r.removeFromTop (36);
        saveButton.setBounds (buttons.removeFromRight (juce::jmin (220, buttons.getWidth() / 2)));
        buttons.removeFromRight (8);
        cancelButton.setBounds (buttons.removeFromRight (100));
    }

private:
    static constexpr int numLengths = 5;
    static juce::String lengthName (int i)   { static const char* n[] { "1/8", "1/4", "1/2", "1 batt.", "2 batt." }; return n[juce::jlimit (0, numLengths - 1, i)]; }
    static double lengthBeats (int i)        { static const double b[] { 0.5, 1.0, 2.0, 4.0, 8.0 }; return b[juce::jlimit (0, numLengths - 1, i)]; }

    bool isDrumKit() const   { return engine.getChannel (channel).instrumentKind == juce::String ("drumforge"); }

    juce::String suggestedName() const
    {
        auto* proc = engine.getInstrument (channel);
        auto preset = Instruments::presetName (proc);
        return preset.isNotEmpty() ? preset : engine.getChannel (channel).name;
    }

    void setNote (int n)
    {
        note = juce::jlimit (12, 108, n);
        noteLabel.setText (noteText (note), juce::dontSendNotification);
        rendered = nullptr;
        updateWave();
    }

    void updateWave()   { wave.setSample (rendered.get(), juce::Colour (engine.getChannel (channel).colour.load())); }

    /** Un'"ora" finta per i synth che seguono il tempo (LFO a tempo, arpeggi interni): il BPM del progetto. */
    struct FixedPlayHead : public juce::AudioPlayHead
    {
        double bpm = 120.0, sr = 44100.0;
        juce::int64 samples = 0;
        juce::Optional<PositionInfo> getPosition() const override
        {
            PositionInfo info;
            info.setBpm (bpm);
            info.setIsPlaying (true);
            info.setTimeInSamples (samples);
            info.setPpqPosition ((double) samples / sr * bpm / 60.0);
            info.setTimeSignature (juce::AudioPlayHead::TimeSignature { 4, 4 });
            return info;
        }
    };

    /** Registra la nota con una copia del synth del canale. Mono, picco a -1 dB, senza silenzio in coda. */
    const SampleData* render()
    {
        if (rendered != nullptr) return rendered.get();
        auto* source = engine.getInstrument (channel);
        if (source == nullptr) return nullptr;
        auto copy = Instruments::create (engine.getChannel (channel).instrumentKind);
        if (copy == nullptr) return nullptr;
        {
            juce::MemoryBlock state;
            source->getStateInformation (state);
            copy->setStateInformation (state.getData(), (int) state.getSize());
        }
        const double sr = 44100.0;
        const int block = 512;
        FixedPlayHead head;
        head.bpm = juce::jlimit (40.0, 300.0, engine.getBpm());
        head.sr = sr;
        copy->setPlayHead (&head);
        copy->setPlayConfigDetails (0, 2, sr, block);
        copy->prepareToPlay (sr, block);

        const int noteOff = juce::jmax (block, (int) (lengthBeats (lengthIndex) * 60.0 / head.bpm * sr));
        const int maxTotal = noteOff + (int) (6.0 * sr);   // coda: al massimo 6 secondi dopo il rilascio
        juce::AudioBuffer<float> buf (2, block);
        juce::AudioBuffer<float> mono (1, maxTotal);
        mono.clear();
        int quietBlocks = 0, pos = 0;
        for (; pos < maxTotal; pos += block)
        {
            const int n = juce::jmin (block, maxTotal - pos);
            juce::MidiBuffer m;
            if (pos == 0) m.addEvent (juce::MidiMessage::noteOn (1, note, 0.85f), 0);
            if (noteOff >= pos && noteOff < pos + n) m.addEvent (juce::MidiMessage::noteOff (1, note), noteOff - pos);
            buf.setSize (2, n, false, false, true);
            buf.clear();
            copy->processBlock (buf, m);
            head.samples += n;
            float pk = 0.0f;
            for (int i = 0; i < n; ++i)
            {
                const float v = 0.5f * (buf.getSample (0, i) + buf.getSample (1, i));
                mono.setSample (0, pos + i, v);
                pk = juce::jmax (pk, std::abs (v));
            }
            // dopo il rilascio: ci si ferma quando e' silenzio da ~0.2 s
            if (pos > noteOff) { quietBlocks = pk < 0.0005f ? quietBlocks + 1 : 0; if (quietBlocks * block > (int) (0.2 * sr)) { pos += n; break; } }
        }
        copy->releaseResources();
        copy->setPlayHead (nullptr);

        int end = juce::jmin (pos, maxTotal);
        while (end > block && std::abs (mono.getSample (0, end - 1)) < 0.0005f) --end;
        const float peak = mono.getMagnitude (0, 0, end);
        if (peak < 0.0005f)
        {
            wave.setEmptyText ("Questo synth da solo non fa suono (gli serve un campione o una voce)");
            return nullptr;
        }
        SampleData::Ptr out (new SampleData());
        out->name = "anteprima";
        out->sampleRate = sr;
        out->audio.setSize (1, end);
        out->audio.copyFrom (0, 0, mono, 0, 0, end);
        out->audio.applyGain (0.89f / peak);
        const int fade = juce::jmin (end / 4, (int) (0.005 * sr));   // fine senza "click"
        out->audio.applyGainRamp (0, end - fade, fade, 1.0f, 0.0f);
        rendered = out;
        updateWave();
        return rendered.get();
    }

    void save()
    {
        auto* s = render();
        if (s == nullptr) return;
        auto base = juce::File::createLegalFileName (nameEditor.getText().trim());
        if (base.isEmpty()) base = "Campione";
        auto folder = SampleStore::getFolder();
        folder.createDirectory();
        auto file = folder.getChildFile (base + ".wav");
        for (int k = 2; file.exists(); ++k)   // non si sovrascrive mai un campione che c'e' gia'
            file = folder.getChildFile (base + " " + juce::String (k) + ".wav");
        {
            std::unique_ptr<juce::OutputStream> os = std::make_unique<juce::FileOutputStream> (file);
            juce::WavAudioFormat wav;
            auto w = wav.createWriterFor (os, juce::AudioFormatWriterOptions{}.withSampleRate (s->sampleRate).withNumChannels (1).withBitsPerSample (24)
                                                     .withMetadataValues ({ { "MidiUnityNote", juce::String (note) } }));   // la nota registrata: il piano roll suona giusto
            if (w == nullptr || ! w->writeFromAudioSampleBuffer (s->audio, 0, s->audio.getNumSamples()))
            {
                wave.setEmptyText ("Non riesco a salvare il file");
                return;
            }
        }
        if (onSaved) onSaved (file.getFileName());
        if (onDone) onDone();
    }

    SequencerEngine& engine;
    SampleStore& samples;
    const int channel;
    int note = 48, lengthIndex = 1;
    SampleData::Ptr rendered;
    juce::Label title, noteLabel;
    juce::TextEditor nameEditor;
    juce::TextButton octDownButton { "-ott" }, lowerButton { "-" }, higherButton { "+" }, octUpButton { "+ott" };
    juce::OwnedArray<juce::TextButton> lengthButtons;
    juce::TextButton listenButton { "Ascolta" }, cancelButton { "Annulla" }, saveButton { "Salva in \"I miei campioni\"" };
    WaveView wave;
    std::vector<std::pair<juce::Rectangle<int>, juce::String>> captions;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (SaveSampleDialog)
};
