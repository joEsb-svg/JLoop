#pragma once

#include <JuceHeader.h>
#include <functional>
#include "../Engine/VocalMaker.h"
#include "SoundBrowser.h"
#include "Theme.h"
#include "Widgets.h"

//==============================================================================
/**
    La forma d'onda della tua voce, con l'inizio e la fine trascinabili (il taglio) e le dissolvenze.
*/
class WaveTrimView : public juce::Component
{
public:
    std::function<void()> onChanged;

    void setSource (const SampleData* d, vocals::MyVoice* v)
    {
        data = d; voice = v;
        peaks.clear();
        repaint();
    }

    void paint (juce::Graphics& g) override
    {
        auto r = getLocalBounds().toFloat();
        Theme::drawSunken (g, r, Theme::lcdBg, 6.0f);
        if (data == nullptr || voice == nullptr || data->audio.getNumSamples() == 0)
        {
            g.setColour (Theme::textDim);
            g.setFont (Theme::font (12.0f));
            g.drawText ("Scegli una voce", r, juce::Justification::centred);
            return;
        }
        r = r.reduced (4.0f, 6.0f);
        buildPeaks ((int) r.getWidth());
        const float mid = r.getCentreY(), h = r.getHeight() * 0.5f;
        const float xa = xOf (voice->start), xb = xOf (endSec());
        for (int x = 0; x < (int) peaks.size(); ++x)
        {
            const float px = r.getX() + (float) x;
            const bool inside = px >= xa && px <= xb;
            g.setColour (inside ? juce::Colour (0xffe36fb8) : Theme::textDim.withAlpha (0.35f));
            const float p = peaks[(size_t) x];
            g.drawVerticalLine (juce::roundToInt (px), mid - p * h, mid + p * h + 1.0f);
        }
        // fuori dal taglio: scuro
        g.setColour (juce::Colours::black.withAlpha (0.45f));
        g.fillRect (juce::Rectangle<float> (r.getX(), r.getY(), xa - r.getX(), r.getHeight()));
        g.fillRect (juce::Rectangle<float> (xb, r.getY(), r.getRight() - xb, r.getHeight()));
        // dissolvenze (in secondi del file: la voce viene stirata, quindi si riportano indietro)
        const double toSrc = engineBpm > 0.0 ? engineBpm / voice->srcBpm : 1.0;
        g.setColour (Theme::cyan.withAlpha (0.8f));
        if (voice->fadeIn > 0.0f)
            g.drawLine (xa, r.getBottom(), juce::jmin (xb, xOf (voice->start + voice->fadeIn * 0.001 * toSrc)), r.getY(), 1.5f);
        if (voice->fadeOut > 0.0f)
            g.drawLine (juce::jmax (xa, xOf (endSec() - voice->fadeOut * 0.001 * toSrc)), r.getY(), xb, r.getBottom(), 1.5f);
        // maniglie
        for (float x : { xa, xb })
        {
            g.setColour (Theme::accent);
            g.fillRect (x - 1.5f, r.getY() - 4.0f, 3.0f, r.getHeight() + 8.0f);
            g.fillRoundedRectangle (juce::Rectangle<float> (x - 7.0f, r.getY() - 6.0f, 14.0f, 12.0f), 3.0f);
        }
        g.setColour (Theme::text);
        g.setFont (Theme::font (11.0f));
        g.drawText (juce::String (voice->start, 2) + " s", juce::Rectangle<float> (xa + 9.0f, r.getY(), 60.0f, 14.0f), juce::Justification::left);
        g.drawText (juce::String (endSec(), 2) + " s", juce::Rectangle<float> (xb - 69.0f, r.getBottom() - 14.0f, 60.0f, 14.0f), juce::Justification::right);
    }

    void mouseDown (const juce::MouseEvent& e) override
    {
        if (data == nullptr || voice == nullptr) return;
        dragging = std::abs (e.position.x - xOf (voice->start)) <= std::abs (e.position.x - xOf (endSec())) ? 0 : 1;
        mouseDrag (e);
    }

    void mouseDrag (const juce::MouseEvent& e) override
    {
        if (data == nullptr || voice == nullptr || dragging < 0) return;
        const double t = juce::jlimit (0.0, length(), secAt (e.position.x));
        if (dragging == 0) voice->start = juce::jmin (t, endSec() - 0.05);
        else
        {
            const double v = juce::jmax (t, voice->start + 0.05);
            voice->end = v >= length() - 0.005 ? 0.0 : v;
        }
        repaint();
    }

    void mouseUp (const juce::MouseEvent&) override   { if (dragging >= 0 && onChanged) onChanged(); dragging = -1; }

    void mouseDoubleClick (const juce::MouseEvent&) override
    {
        if (voice == nullptr) return;
        voice->start = 0.0; voice->end = 0.0;   // doppio tocco: tutta la voce
        repaint();
        if (onChanged) onChanged();
    }

    double engineBpm = 120.0;

private:
    double length() const     { return data != nullptr ? data->audio.getNumSamples() / data->sampleRate : 1.0; }
    double endSec() const     { return voice != nullptr && voice->end > voice->start ? voice->end : length(); }
    float xOf (double sec) const
    {
        const auto r = getLocalBounds().toFloat().reduced (4.0f, 6.0f);
        return r.getX() + (float) (sec / length()) * r.getWidth();
    }
    double secAt (float x) const
    {
        const auto r = getLocalBounds().toFloat().reduced (4.0f, 6.0f);
        return (x - r.getX()) / juce::jmax (1.0f, r.getWidth()) * length();
    }

    void buildPeaks (int width)
    {
        if ((int) peaks.size() == width || width <= 0) return;
        peaks.assign ((size_t) width, 0.0f);
        const int n = data->audio.getNumSamples();
        const float* d = data->audio.getReadPointer (0);
        float maxPeak = 1.0e-6f;
        for (int x = 0; x < width; ++x)
        {
            const int a = (int) ((juce::int64) x * n / width), b = juce::jmax (a + 1, (int) ((juce::int64) (x + 1) * n / width));
            float p = 0.0f;
            for (int i = a; i < b && i < n; ++i) p = juce::jmax (p, std::abs (d[i]));
            peaks[(size_t) x] = p;
            maxPeak = juce::jmax (maxPeak, p);
        }
        for (auto& p : peaks) p /= maxPeak;
    }

    const SampleData* data = nullptr;
    vocals::MyVoice* voice = nullptr;
    std::vector<float> peaks;
    int dragging = -1;
};

//==============================================================================
/**
    "Vocals a tempo": scegli una frase (hook, grida, build-up, strane), la voce, l'ottava e l'effetto,
    ascoltala e aggiungila. Viene cantata al BPM e nella tonalita' del progetto e si rifa' se cambi i BPM.
    "Le tue": una voce importata, tagliata, intonata e con le dissolvenze, stirata al tempo del progetto.
    Con editChannel >= 0 si modifica la voce di un canale che c'e' gia'.
*/
class VocalPanel : public juce::Component
{
public:
    VocalPanel (SequencerEngine& e, SampleStore& s, int editChannel = -1) : engine (e), store (s), channelToEdit (editChannel)
    {
        addAndMakeVisible (categorySwitch);
        categorySwitch.onChange = [this] (int c) { showCategory (c); };
        for (auto* sc : { &voiceSwitch, &octaveSwitch, &fxSwitch })
            addAndMakeVisible (sc);
        voiceSwitch.onChange = [this] (int v) { options.voice = v; changed(); };
        octaveSwitch.onChange = [this] (int o) { options.octave = o - 1; changed(); };
        fxSwitch.onChange = [this] (int f) { if (mine()) myVoice.fx = f; else options.fx = f; changed(); };
        octaveSwitch.setSelected (1);

        // --- "Le tue"
        for (auto* b : { &bpmMinus, &bpmPlus, &bpmGuess, &semiMinus, &semiPlus })
            addChildComponent (b);
        bpmMinus.onClick = [this] { setSrcBpm (myVoice.srcBpm - 1.0); };
        bpmPlus.onClick  = [this] { setSrcBpm (myVoice.srcBpm + 1.0); };
        bpmGuess.onClick = [this] { guessSrcBpm(); };
        semiMinus.onClick = [this] { myVoice.semis = juce::jmax (-12.0f, myVoice.semis - 1.0f); changed(); };
        semiPlus.onClick  = [this] { myVoice.semis = juce::jmin (12.0f, myVoice.semis + 1.0f); changed(); };
        auto setupSlider = [this] (juce::Slider& sl, double lo, double hi, double step, std::function<void (double)> set)
        {
            sl.setSliderStyle (juce::Slider::LinearHorizontal);
            sl.setTextBoxStyle (juce::Slider::NoTextBox, false, 0, 0);
            sl.setRange (lo, hi, step);
            sl.setColour (juce::Slider::trackColourId, juce::Colour (0xffe36fb8));
            sl.setColour (juce::Slider::thumbColourId, Theme::textBright);
            sl.onValueChange = [this, &sl, set] { set (sl.getValue()); wave.repaint(); repaint(); };
            sl.onDragEnd = [this] { changed(); };
            addChildComponent (sl);
        };
        setupSlider (gainSlider, -24.0, 12.0, 0.5, [this] (double v) { myVoice.gainDb = (float) v; });
        setupSlider (fadeInSlider, 0.0, 2000.0, 10.0, [this] (double v) { myVoice.fadeIn = (float) v; });
        setupSlider (fadeOutSlider, 0.0, 2000.0, 10.0, [this] (double v) { myVoice.fadeOut = (float) v; });
        reverseToggle.onClick = [this] { myVoice.reverse = reverseToggle.getToggleState(); changed(); };
        addChildComponent (reverseToggle);
        addChildComponent (wave);
        wave.onChanged = [this] { changed(); };

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

        // Modifica di un canale che ha gia' una voce: si parte da come e' fatta
        const auto recipe = vocals::recipeOfChannel (engine, channelToEdit);
        double madeAt = 0.0; int root = 0, scale = 0;
        if (recipe.isNotEmpty() && vocals::stretchFromString (recipe, myVoice, madeAt))
        {
            source = myVoice.source;
            loadMyVoiceControls();
            categorySwitch.setSelected (4);
            showCategory (4);
        }
        else if (recipe.isNotEmpty() && vocals::recipeFromString (recipe, options, madeAt, root, scale))
        {
            const int c = vocals::phrases()[(size_t) options.phrase].category;
            const auto keep = options;
            categorySwitch.setSelected (c);
            showCategory (c);
            options = keep;
            voiceSwitch.setSelected (vocals::voiceOf (options));
            octaveSwitch.setSelected (options.octave + 1);
            fxSwitch.setSelected (options.fx);
        }
        else
        {
            channelToEdit = -1;
            showCategory (0);
        }
        if (channelToEdit >= 0)
        {
            addButton.setButtonText ("Applica al canale");
            playlistToggle.setVisible (false);
        }
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
        g.drawFittedText (channelToEdit >= 0 ? "Modifica la voce: " + engine.getChannel (channelToEdit).name : juce::String ("Vocals a tempo"),
                          titleArea, juce::Justification::centredLeft, 1);

        if (mine())
        {
            Theme::drawCaption (g, "A che BPM era la tua voce?", voiceCaption, juce::Justification::centredLeft);
            g.setColour (Theme::lcdText);
            g.setFont (Theme::font (20.0f, true));
            g.drawFittedText (juce::String (myVoice.srcBpm, 1), bpmValueArea, juce::Justification::centred, 1);
            g.setColour (Theme::text);
            g.setFont (Theme::font (12.5f, true));
            g.drawFittedText ("Intonazione", semiLabel, juce::Justification::centredLeft, 1);
            g.setColour (Theme::lcdText);
            g.setFont (Theme::font (16.0f, true));
            g.drawFittedText ((myVoice.semis > 0.0f ? "+" : "") + juce::String (juce::roundToInt (myVoice.semis)) + " st", semiValue, juce::Justification::centred, 1);
            g.setColour (Theme::text);
            g.setFont (Theme::font (11.5f, true));
            g.drawFittedText ("Volume " + juce::String (myVoice.gainDb, 1) + " dB", gainLabel, juce::Justification::centredLeft, 1);
            g.drawFittedText ("Fade in " + juce::String (juce::roundToInt (myVoice.fadeIn)) + " ms", fadeInLabel, juce::Justification::centredLeft, 1);
            g.drawFittedText ("Fade out " + juce::String (juce::roundToInt (myVoice.fadeOut)) + " ms", fadeOutLabel, juce::Justification::centredLeft, 1);

            g.setColour (Theme::textDim);
            g.setFont (Theme::font (11.5f));
            juce::String info;
            if (auto* src = sourceData())
            {
                const double secs = (myVoice.end > myVoice.start ? myVoice.end : src->audio.getNumSamples() / src->sampleRate) - myVoice.start;
                const double newSecs = secs * myVoice.srcBpm / engine.getBpm();
                info << "Taglio di " << juce::String (secs, 1) << " s: a " << juce::roundToInt (engine.getBpm()) << " BPM dura "
                     << juce::String (newSecs, 1) << " s (" << juce::String (newSecs * engine.getBpm() / 240.0, 1) << " battute). "
                     << "Trascina le maniglie per tagliare, doppio tocco = tutta.";
            }
            else
                info << "Scegli una voce importata (un'acapella...) o \"+ Importa\": JLoop la mette a tempo senza cambiarne l'intonazione.";
            g.drawFittedText (info, infoArea, juce::Justification::topLeft, 3);
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
        if (narrow)
        {
            right = r.removeFromBottom (mine() ? juce::jmin (r.getHeight() * 60 / 100, 340) : juce::jmin (r.getHeight() / 2, 300));
            r.removeFromBottom (6);
            left = r;
        }
        else { left = r.removeFromLeft (r.getWidth() * 52 / 100); r.removeFromLeft (12); right = r; }

        categorySwitch.setOptions (left.getWidth() < 470 ? juce::StringArray { "Hook", "Grida", "Build", "Strane", "Tue" }
                                                          : juce::StringArray { "Hook cantati", "Grida", "Build-up", "Strane", "Le tue" });
        categorySwitch.setBounds (left.removeFromTop (32));
        left.removeFromTop (6);
        if (mine())
        {
            wave.setBounds (left.removeFromBottom (juce::jlimit (70, 150, left.getHeight() * 45 / 100)));
            left.removeFromBottom (6);
        }
        listView.setBounds (left);
        list.setLayout (1, 44);
        list.setVisibleWidth (left.getWidth() - 8);

        auto buttons = right.removeFromBottom (44);
        listenButton.setBounds (buttons.removeFromLeft (buttons.getWidth() * 2 / 5).reduced (0, 2));
        buttons.removeFromLeft (8);
        addButton.setBounds (buttons.reduced (0, 2));
        right.removeFromBottom (6);
        if (channelToEdit < 0)
        {
            playlistToggle.setBounds (right.removeFromBottom (28));
            right.removeFromBottom (4);
        }

        fxSwitch.setOptions (right.getWidth() < 330 || mine() ? juce::StringArray { "Secca", "Spazio", "Eco", "Radio" }
                                                              : juce::StringArray { "Asciutta", "Spazio", "Eco a tempo", "Radio" });
        if (mine())
        {
            const bool tight = right.getHeight() < 250;
            voiceCaption = right.removeFromTop (16).toFloat();
            auto bpmRow = right.removeFromTop (tight ? 34 : 40);
            bpmMinus.setBounds (bpmRow.removeFromLeft (44).reduced (2));
            bpmValueArea = bpmRow.removeFromLeft (72);
            bpmPlus.setBounds (bpmRow.removeFromLeft (44).reduced (2));
            bpmRow.removeFromLeft (6);
            bpmGuess.setBounds (bpmRow.removeFromLeft (juce::jmin (110, bpmRow.getWidth())).reduced (2));
            right.removeFromTop (4);
            auto semiRow = right.removeFromTop (tight ? 30 : 34);
            semiLabel = semiRow.removeFromLeft (96);
            semiMinus.setBounds (semiRow.removeFromLeft (40).reduced (2));
            semiValue = semiRow.removeFromLeft (60);
            semiPlus.setBounds (semiRow.removeFromLeft (40).reduced (2));
            right.removeFromTop (4);
            auto gainRow = right.removeFromTop (26);
            gainLabel = gainRow.removeFromLeft (juce::jmin (130, gainRow.getWidth() / 2));
            gainSlider.setBounds (gainRow);
            auto fadeRow = right.removeFromTop (tight ? 40 : 44);
            auto fi = fadeRow.removeFromLeft (fadeRow.getWidth() / 2).withTrimmedRight (6), fo = fadeRow.withTrimmedLeft (6);
            fadeInLabel = fi.removeFromTop (16); fadeInSlider.setBounds (fi);
            fadeOutLabel = fo.removeFromTop (16); fadeOutSlider.setBounds (fo);
            right.removeFromTop (4);
            auto fxRow = right.removeFromTop (32);
            reverseToggle.setBounds (fxRow.removeFromLeft (juce::jmin (130, fxRow.getWidth() / 3)));
            fxRow.removeFromLeft (6);
            fxSwitch.setBounds (fxRow);
            right.removeFromTop (4);
            infoArea = right;
            return;
        }

        const bool compact = right.getHeight() < 260;
        voiceSwitch.setOptions (right.getWidth() < 360 ? juce::StringArray { "Donna", "Uomo", "Coro", "Robot", "Sussur." }
                                                       : juce::StringArray { "Femminile", "Maschile", "Coro", "Robot", "Sussurro" });
        voiceCaption = right.removeFromTop (16).toFloat();
        voiceSwitch.setBounds (right.removeFromTop (32));
        right.removeFromTop (compact ? 4 : 8);
        auto row = right.removeFromTop (16 + 32);
        auto octArea = row.removeFromLeft (juce::jmin (150, row.getWidth() / 3));
        octaveCaption = octArea.removeFromTop (16).toFloat();
        octaveSwitch.setBounds (octArea);
        row.removeFromLeft (10);
        fxCaption = row.removeFromTop (16).toFloat();
        fxSwitch.setBounds (row);
        right.removeFromTop (compact ? 4 : 10);
        infoArea = right;
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
        for (auto* c : std::initializer_list<juce::Component*> { &voiceSwitch, &octaveSwitch })
            c->setVisible (! mine());
        for (auto* c : std::initializer_list<juce::Component*> { &bpmMinus, &bpmPlus, &bpmGuess, &semiMinus, &semiPlus, &gainSlider,
                                                                 &fadeInSlider, &fadeOutSlider, &reverseToggle, &wave })
            c->setVisible (mine());
        fxSwitch.setSelected (mine() ? myVoice.fx : options.fx);
        addButton.setEnabled (! mine() || source.isNotEmpty());
        listenButton.setEnabled (! mine() || source.isNotEmpty());
        resized();
        repaint();
    }

    //==========================================================================
    // "Le tue": voci importate

    SampleData* sourceData()   { return source.isNotEmpty() ? store.load (source) : nullptr; }

    void loadMyVoiceControls()
    {
        gainSlider.setValue (myVoice.gainDb, juce::dontSendNotification);
        fadeInSlider.setValue (myVoice.fadeIn, juce::dontSendNotification);
        fadeOutSlider.setValue (myVoice.fadeOut, juce::dontSendNotification);
        reverseToggle.setToggleState (myVoice.reverse, juce::dontSendNotification);
        fxSwitch.setSelected (myVoice.fx);
        wave.engineBpm = engine.getBpm();
        wave.setSource (sourceData(), &myVoice);
    }

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
        loadMyVoiceControls();
        updateVisibility();
    }

    void chooseMine (int i)
    {
        stopPreview();
        if (i == 0) { importFile(); return; }
        if (i - 1 >= mineNames.size()) return;
        list.setSelected (i);
        if (source != mineNames[i - 1])
        {
            // voce nuova: si riparte pulito (stesse modifiche solo se e' la stessa voce)
            const int fx = myVoice.fx;
            myVoice = {};
            myVoice.fx = fx;
            source = myVoice.source = mineNames[i - 1];
            guessSrcBpm();
        }
        loadMyVoiceControls();
        updateVisibility();
    }

    void guessSrcBpm()
    {
        if (auto* src = sourceData())
            setSrcBpm (stretch::guessBpm (src->audio.getNumSamples() / src->sampleRate, engine.getBpm()));
    }

    void setSrcBpm (double b)
    {
        myVoice.srcBpm = juce::jlimit (40.0, 240.0, b);
        changed();
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
            safe->myVoice = {};
            safe->source = safe->myVoice.source = name;
            safe->guessSrcBpm();
            safe->showMine();
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
        wave.repaint();
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
            data->audio = vocals::renderMine (*src, myVoice, engine.getBpm());
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
        vocals::Result r;
        if (channelToEdit >= 0)
        {
            const auto recipe = mine() ? vocals::stretchRecipe (myVoice, 0.0)
                                       : vocals::recipeToString (options, 0.0, engine.getScaleRoot(), engine.getScaleType());
            r.ok = vocals::replaceOnChannel (engine, store, channelToEdit, recipe);
            r.channel = channelToEdit;
            if (! r.ok) r.error = "Non riesco a rifare la voce.";
        }
        else
        {
            const int bar = playlistToggle.getToggleState() ? engine.getSongStartBar() : -1;
            r = mine() ? vocals::applyStretched (engine, store, myVoice, bar) : vocals::apply (engine, store, options, bar);
        }
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
    int channelToEdit = -1;
    vocals::Options options;
    vocals::MyVoice myVoice;
    std::vector<int> ids;
    int category = 0;
    juce::String source;
    juce::StringArray mineNames;
    std::unique_ptr<juce::FileChooser> chooser;
    bool listening = false;

    SegmentedControl categorySwitch { { "Hook cantati", "Grida", "Build-up", "Strane", "Le tue" } };
    SegmentedControl voiceSwitch { { "Femminile", "Maschile", "Coro", "Robot", "Sussurro" } };
    SegmentedControl octaveSwitch { { "-1", "0", "+1" } };
    SegmentedControl fxSwitch { { "Asciutta", "Spazio", "Eco a tempo", "Radio" } };
    juce::Viewport listView;
    TileList list;
    WaveTrimView wave;
    juce::TextButton bpmMinus { "-" }, bpmPlus { "+" }, bpmGuess { "Indovina" }, semiMinus { "-" }, semiPlus { "+" };
    juce::Slider gainSlider, fadeInSlider, fadeOutSlider;
    juce::ToggleButton reverseToggle { "Al contrario" };
    juce::TextButton closeButton { "Chiudi" }, listenButton { juce::String (juce::CharPointer_UTF8 ("\xe2\x96\xb6 Ascolta")) }, addButton { "Aggiungi al progetto" };
    juce::ToggleButton playlistToggle { "Mettila anche nella playlist (dal cursore)" };
    juce::Rectangle<int> box, titleArea, infoArea, bpmValueArea, semiLabel, semiValue, gainLabel, fadeInLabel, fadeOutLabel;
    juce::Rectangle<float> voiceCaption, octaveCaption, fxCaption;
};
