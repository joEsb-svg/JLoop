#pragma once

#include <JuceHeader.h>
#include <functional>
#include "../Engine/VocalMaker.h"
#include "SoundBrowser.h"
#include "Theme.h"
#include "Widgets.h"

//==============================================================================
/**
    "Vocals a tempo": scegli una frase (hook, grida, build-up, strane), la voce, l'ottava e l'effetto,
    ascoltala e aggiungila. Viene cantata al BPM e nella tonalita' del progetto e si rifa' se cambi i BPM.
*/
class VocalPanel : public juce::Component
{
public:
    VocalPanel (SequencerEngine& e, SampleStore& s) : engine (e), store (s)
    {
        addAndMakeVisible (categorySwitch);
        categorySwitch.onChange = [this] (int c) { showCategory (c); };
        for (auto* sc : { &voiceSwitch, &octaveSwitch, &fxSwitch })
            addAndMakeVisible (sc);
        voiceSwitch.onChange = [this] (int v) { options.voice = v; changed(); };
        octaveSwitch.onChange = [this] (int o) { options.octave = o - 1; changed(); };
        fxSwitch.onChange = [this] (int f) { options.fx = f; changed(); };
        octaveSwitch.setSelected (1);
        for (auto* b : { &bpmMinus, &bpmPlus, &bpmGuess })
            addChildComponent (b);
        bpmMinus.onClick = [this] { setSrcBpm (srcBpm - 1.0); };
        bpmPlus.onClick  = [this] { setSrcBpm (srcBpm + 1.0); };
        bpmGuess.onClick = [this] { guessSrcBpm(); };

        listView.setScrollOnDragMode (juce::Viewport::ScrollOnDragMode::all);
        listView.setScrollBarsShown (true, false);
        listView.setScrollBarThickness (6);
        listView.setViewedComponent (&list, false);
        addAndMakeVisible (listView);
        list.onTap = [this] (int i) { choose (i); };

        for (auto* b : { &closeButton, &listenButton, &addButton })
            addAndMakeVisible (b);
        playlistToggle.setToggleState (false, juce::dontSendNotification);
        addAndMakeVisible (playlistToggle);
        closeButton.onClick = [this] { close(); };
        listenButton.onClick = [this] { if (listening) stopPreview(); else startPreview(); };
        addButton.onClick = [this] { make(); };

        showCategory (0);
    }

    ~VocalPanel() override   { stopPreview(); }

    std::function<void (const vocals::Result&)> onCreated;
    std::function<void()> onClose;

    void paint (juce::Graphics& g) override
    {
        g.fillAll (juce::Colours::black.withAlpha (0.55f));
        g.setColour (Theme::panel);
        g.fillRoundedRectangle (box.toFloat(), Theme::corner);
        g.setColour (Theme::border);
        g.drawRoundedRectangle (box.toFloat().reduced (0.5f), Theme::corner, 1.0f);

        g.setColour (Theme::textBright);
        g.setFont (Theme::font (16.0f, true));
        g.drawFittedText ("Vocals a tempo", titleArea, juce::Justification::centredLeft, 1);

        if (mine())
        {
            Theme::drawCaption (g, "A che BPM e' la tua voce?", voiceCaption, juce::Justification::centredLeft);
            g.setColour (Theme::lcdText);
            g.setFont (Theme::font (22.0f, true));
            g.drawFittedText (juce::String (srcBpm, 1), bpmValueArea, juce::Justification::centred, 1);
            g.setColour (Theme::textDim);
            g.setFont (Theme::font (12.0f));
            juce::String info;
            if (auto* src = sourceData())
            {
                const double secs = src->audio.getNumSamples() / src->sampleRate;
                const double newSecs = secs * srcBpm / engine.getBpm();
                info << "Dura " << juce::String (secs, 1) << " s. A " << juce::roundToInt (engine.getBpm()) << " BPM diventa "
                     << juce::String (newSecs, 1) << " s (" << juce::String (newSecs * engine.getBpm() / 240.0, 1) << " battute), con la stessa intonazione."
                     << " Se cambi i BPM si rifa' da sola.";
            }
            else
                info << "Scegli una voce che hai importato (un'acapella, un vocal scaricato...) oppure \"+ Importa\". "
                        "Poi dimmi a che BPM era: JLoop la stira al tempo del progetto senza cambiarne l'intonazione.";
            g.drawFittedText (info, infoArea, juce::Justification::topLeft, 5);
            return;
        }

        Theme::drawCaption (g, "Voce", voiceCaption, juce::Justification::centredLeft);
        Theme::drawCaption (g, "Ottava", octaveCaption, juce::Justification::centredLeft);
        Theme::drawCaption (g, "Effetto", fxCaption, juce::Justification::centredLeft);

        const auto& ph = vocals::phrases()[(size_t) options.phrase];
        g.setColour (Theme::textDim);
        g.setFont (Theme::font (12.0f));
        const juce::String key = engine.getScaleType() > 0 ? theory::scaleLabel (engine.getScaleRoot(), engine.getScaleType())
                                                            : juce::String (theory::noteName (engine.getScaleRoot())) + " minore";
        juce::String info = juce::String (ph.bars) + (ph.bars == 1 ? " battuta" : " battute") + " a " + juce::String (juce::roundToInt (engine.getBpm())) + " BPM";
        if (ph.pitched) info << ", in " << key;
        info << ". Se cambi i BPM si rifa' da sola a tempo.";
        g.drawFittedText (info, infoArea, juce::Justification::topLeft, 3);
    }

    void resized() override
    {
        box = getLocalBounds().reduced (getWidth() < 500 ? 6 : 16, getHeight() < 450 ? 6 : 16);
        if (box.getWidth() > 1000) box = box.withSizeKeepingCentre (1000, box.getHeight());
        auto r = box.reduced (10, 8);
        auto top = r.removeFromTop (34);
        closeButton.setBounds (top.removeFromRight (90).reduced (0, 2));
        titleArea = top;
        r.removeFromTop (6);

        const bool narrow = r.getWidth() < 620;
        juce::Rectangle<int> left, right;
        if (narrow) { right = r.removeFromBottom (juce::jmin (r.getHeight() / 2, 300)); r.removeFromBottom (6); left = r; }
        else        { left = r.removeFromLeft (r.getWidth() * 55 / 100); r.removeFromLeft (12); right = r; }

        categorySwitch.setOptions (left.getWidth() < 470 ? juce::StringArray { "Hook", "Grida", "Build", "Strane", "Tue" }
                                                          : juce::StringArray { "Hook cantati", "Grida", "Build-up", "Strane", "Le tue" });
        categorySwitch.setBounds (left.removeFromTop (32));
        left.removeFromTop (6);
        listView.setBounds (left);
        list.setLayout (1, 44);
        list.setVisibleWidth (left.getWidth() - 8);

        const bool compact = right.getHeight() < 260;
        voiceSwitch.setOptions (right.getWidth() < 360 ? juce::StringArray { "Donna", "Uomo", "Coro", "Robot", "Sussur." }
                                                       : juce::StringArray { "Femminile", "Maschile", "Coro", "Robot", "Sussurro" });
        voiceCaption = right.removeFromTop (16).toFloat();
        {
            // "Le tue": BPM originale  [-] 128.0 [+]  [Indovina]
            auto bpmRow = right.withHeight (40);
            bpmMinus.setBounds (bpmRow.removeFromLeft (48).reduced (2));
            bpmValueArea = bpmRow.removeFromLeft (80);
            bpmPlus.setBounds (bpmRow.removeFromLeft (48).reduced (2));
            bpmRow.removeFromLeft (8);
            bpmGuess.setBounds (bpmRow.removeFromLeft (juce::jmin (120, bpmRow.getWidth())).reduced (2));
        }
        voiceSwitch.setBounds (right.removeFromTop (32));
        right.removeFromTop (compact ? 4 : 8);
        auto row = right.removeFromTop (16 + 32);
        auto octArea = row.removeFromLeft (juce::jmin (150, row.getWidth() / 3));
        octaveCaption = octArea.removeFromTop (16).toFloat();
        octaveSwitch.setBounds (octArea);
        row.removeFromLeft (10);
        fxCaption = row.removeFromTop (16).toFloat();
        fxSwitch.setOptions (row.getWidth() < 300 ? juce::StringArray { "Secca", "Spazio", "Eco", "Radio" }
                                                  : juce::StringArray { "Asciutta", "Spazio", "Eco a tempo", "Radio" });
        fxSwitch.setBounds (row);
        right.removeFromTop (compact ? 4 : 10);

        auto buttons = right.removeFromBottom (44);
        listenButton.setBounds (buttons.removeFromLeft (buttons.getWidth() * 2 / 5).reduced (0, 2));
        buttons.removeFromLeft (8);
        addButton.setBounds (buttons.reduced (0, 2));
        right.removeFromBottom (6);
        playlistToggle.setBounds (right.removeFromBottom (30));
        right.removeFromBottom (4);
        infoArea = right;
        if (mine()) infoArea = infoArea.withTrimmedTop (16);   // sotto i tasti dei BPM
    }

    void mouseUp (const juce::MouseEvent& e) override
    {
        if (! box.contains (e.getPosition())) close();
    }

    // Test
    void selectForTest (int category, int index)   { categorySwitch.setSelected (category); showCategory (category); choose (index); }

private:
    bool mine() const   { return category == 4; }

    void updateVisibility()
    {
        for (auto* c : std::initializer_list<juce::Component*> { &voiceSwitch, &octaveSwitch, &fxSwitch })
            c->setVisible (! mine());
        for (auto* c : std::initializer_list<juce::Component*> { &bpmMinus, &bpmPlus, &bpmGuess })
            c->setVisible (mine());
        addButton.setEnabled (! mine() || source.isNotEmpty());
        listenButton.setEnabled (! mine() || source.isNotEmpty());
        resized();
        repaint();
    }

    //==========================================================================
    // "Le tue": voci importate

    SampleData* sourceData()   { return source.isNotEmpty() ? store.load (source) : nullptr; }

    void showMine()
    {
        mineNames = store.listSamples();
        std::vector<TileList::Item> items;
        items.push_back ({ "+ Importa una voce...", "WAV, AIFF, FLAC, OGG, MP3", Theme::accent, true });
        int selected = -1;
        for (int i = 0; i < mineNames.size(); ++i)
        {
            if (mineNames[i] == source) selected = i + 1;
            items.push_back ({ SampleStore::displayName (mineNames[i]), {}, juce::Colour (0xffc49360), true });
        }
        list.setItems (items, selected);
        updateVisibility();
    }

    void chooseMine (int i)
    {
        stopPreview();
        if (i == 0) { importFile(); return; }
        if (i - 1 >= mineNames.size()) return;
        list.setSelected (i);
        source = mineNames[i - 1];
        guessSrcBpm();
        updateVisibility();
    }

    void guessSrcBpm()
    {
        if (auto* src = sourceData())
            setSrcBpm (stretch::guessBpm (src->audio.getNumSamples() / src->sampleRate, engine.getBpm()));
    }

    void setSrcBpm (double b)
    {
        srcBpm = juce::jlimit (40.0, 240.0, b);
        if (listening) startPreview();
        repaint();
    }

    void importFile()
    {
        chooser = std::make_unique<juce::FileChooser> ("Scegli una voce", juce::File(), "*.wav;*.aif;*.aiff;*.flac;*.ogg;*.mp3");
        juce::Component::SafePointer<VocalPanel> safe (this);
        chooser->launchAsync (juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectFiles, [safe] (const juce::FileChooser& fc)
        {
            if (safe == nullptr) return;
            const auto url = fc.getURLResult();
            if (url.isEmpty()) return;
            juce::String error;
            const auto name = safe->store.importFromUrl (url, error);
            if (name.isEmpty())
            {
                juce::AlertWindow::showAsync (juce::MessageBoxOptions().withTitle ("Vocals").withMessage (error).withButton ("OK"), nullptr);
                return;
            }
            safe->source = name;
            safe->showMine();
            safe->guessSrcBpm();
            safe->updateVisibility();
        });
    }

    void showCategory (int c)
    {
        stopPreview();
        category = c;
        if (mine()) { showMine(); return; }
        ids.clear();
        std::vector<TileList::Item> items;
        int selected = -1;
        const auto& all = vocals::phrases();
        for (int i = 0; i < (int) all.size(); ++i)
            if (all[(size_t) i].category == c)
            {
                if (i == options.phrase) selected = (int) ids.size();
                ids.push_back (i);
                const auto& p = all[(size_t) i];
                items.push_back ({ p.name, juce::String (p.bars) + (p.bars == 1 ? " battuta, " : " battute, ") + vocal::voiceName (p.voice),
                                   juce::Colour (c == 0 ? 0xffe36fb8u : c == 1 ? 0xffff8a4fu : c == 2 ? 0xff4fd8ffu : 0xffb07cffu), true });
            }
        list.setItems (items, selected);
        if (selected < 0) choose (0);
        updateVisibility();
    }

    void choose (int i)
    {
        if (mine()) { chooseMine (i); return; }
        if (i < 0 || i >= (int) ids.size()) return;
        list.setSelected (i);
        options.phrase = ids[(size_t) i];
        options.voice = -1;
        voiceSwitch.setSelected (vocals::voiceOf (options));
        changed();
    }

    void changed()
    {
        if (listening) startPreview();
        repaint();
    }

    void startPreview()
    {
        SampleData::Ptr data = new SampleData();
        data->name = "vocal-preview";
        data->sampleRate = 44100.0;
        if (mine())
        {
            auto* src = sourceData();
            if (src == nullptr) return;
            data->sampleRate = src->sampleRate;
            data->audio = stretch::process (src->audio, src->sampleRate, srcBpm / engine.getBpm());
        }
        else
            data->audio = vocals::render (options, engine.getBpm(), 44100.0, engine.getScaleRoot(), engine.getScaleType());
        keepAlive().push_back (data);
        while (keepAlive().size() > 3) keepAlive().erase (keepAlive().begin());
        engine.previewSample (data.get());
        listening = true;
        listenButton.setButtonText (juce::String (juce::CharPointer_UTF8 ("\xe2\x96\xa0 Ferma")));
    }

    void stopPreview()
    {
        if (! listening) return;
        engine.previewSample (nullptr);
        listening = false;
        listenButton.setButtonText (juce::String (juce::CharPointer_UTF8 ("\xe2\x96\xb6 Ascolta")));
    }

    static std::vector<SampleData::Ptr>& keepAlive()   { static std::vector<SampleData::Ptr> v; return v; }

    void make()
    {
        stopPreview();
        const int bar = playlistToggle.getToggleState() ? engine.getSongStartBar() : -1;
        const auto r = mine() ? vocals::applyStretched (engine, store, source, srcBpm, bar) : vocals::apply (engine, store, options, bar);
        if (! r.ok)
        {
            juce::AlertWindow::showAsync (juce::MessageBoxOptions().withTitle ("Vocals").withMessage (r.error).withButton ("OK")
                                                                   .withAssociatedComponent (this), nullptr);
            return;
        }
        if (onCreated) onCreated (r);
        close();
    }

    void close()
    {
        stopPreview();
        setVisible (false);
        if (onClose) onClose();
    }

    SequencerEngine& engine;
    SampleStore& store;
    vocals::Options options;
    std::vector<int> ids;
    int category = 0;
    juce::String source;
    juce::StringArray mineNames;
    double srcBpm = 120.0;
    std::unique_ptr<juce::FileChooser> chooser;
    juce::TextButton bpmMinus { "-" }, bpmPlus { "+" }, bpmGuess { "Indovina" };
    juce::Rectangle<int> bpmValueArea;
    bool listening = false;

    SegmentedControl categorySwitch { { "Hook cantati", "Grida", "Build-up", "Strane", "Le tue" } };
    SegmentedControl voiceSwitch { { "Femminile", "Maschile", "Coro", "Robot", "Sussurro" } };
    SegmentedControl octaveSwitch { { "-1", "0", "+1" } };
    SegmentedControl fxSwitch { { "Asciutta", "Spazio", "Eco a tempo", "Radio" } };
    juce::Viewport listView;
    TileList list;
    juce::TextButton closeButton { "Chiudi" }, listenButton { juce::String (juce::CharPointer_UTF8 ("\xe2\x96\xb6 Ascolta")) }, addButton { "Aggiungi al progetto" };
    juce::ToggleButton playlistToggle { "Mettila anche nella playlist (dal cursore)" };
    juce::Rectangle<int> box, titleArea, infoArea;
    juce::Rectangle<float> voiceCaption, octaveCaption, fxCaption;
};
