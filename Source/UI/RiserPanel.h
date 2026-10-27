#pragma once

#include <JuceHeader.h>
#include <functional>
#include "../Engine/RiserMaker.h"
#include "Theme.h"

//==============================================================================
/**
    Finestra "Riser automatico": durata, ingredienti, dove metterlo; sotto, l'anteprima della salita.
    "Crea" fa tutto (campione a tempo, canale, pattern, clip nella playlist, automazione del filtro).
*/
class RiserPanel : public juce::Component
{
public:
    RiserPanel (SequencerEngine& e, SampleStore& s, bool startAsDowner = false) : engine (e), store (s)
    {
        // Riser (sale fino al drop) o Downer (parte dal drop e scende)
        for (auto* b : { &modeRiser, &modeDowner })
        {
            b->setClickingTogglesState (true);
            b->setRadioGroupId (8103);
            addAndMakeVisible (*b);
        }
        modeRiser.onClick = [this] { setDown (false); };
        modeDowner.onClick = [this] { setDown (true); };

        static const int lengths[] { 1, 2, 4, 8, 16 };
        for (int b : lengths)
        {
            auto* btn = lengthButtons.add (new juce::TextButton (b == 1 ? juce::String ("1 battuta") : juce::String (b)));
            btn->setClickingTogglesState (true);
            btn->setRadioGroupId (8101);
            btn->setToggleState (b == options.bars, juce::dontSendNotification);
            btn->onClick = [this, b] { options.bars = b; if (! dropEdited) options.dropBar = riser::suggestDropBar (engine, b); options.dropBar = juce::jmax (options.dropBar, minDrop()); refresh(); };
            addAndMakeVisible (btn);
        }

        addIngredient ("Rumore che sale", options.noise);
        addIngredient ("Nota che sale", options.note);
        addIngredient ("Rullata", options.roll);
        addIngredient ("Filtro sul master", options.filter);
        addIngredient ("Impatto alla fine", options.impact);

        // --- Suono: preset (◂ nome ▸, tocca il nome per la lista), casuale, ascolta
        for (auto* b : { &prevSound, &soundButton, &nextSound, &randomButton, &listenButton })
            addAndMakeVisible (*b);
        prevSound.onClick = [this] { pickPreset ((presetIndex < 0 ? 0 : presetIndex) - 1); };
        nextSound.onClick = [this] { pickPreset ((presetIndex < 0 ? -1 : presetIndex) + 1); };
        soundButton.onClick = [this] { showSoundMenu(); };
        randomButton.onClick = [this]
        {
            const int seed = juce::Random::getSystemRandom().nextInt ({ 1000, 9999 });
            presetIndex = -1;
            options.sound = riser::randomSound (seed);
            options.soundName = "Casuale " + juce::String (seed);
            soundChanged();
        };
        listenButton.onClick = [this] { toggleListen(); };
        pickPreset (0, false);

        for (auto* b : { &toPlaylist, &toPattern })
        {
            b->setClickingTogglesState (true);
            b->setRadioGroupId (8102);
            addAndMakeVisible (*b);
        }
        toPlaylist.setToggleState (true, juce::dontSendNotification);
        toPlaylist.onClick = [this] { options.toPlaylist = true; refresh(); };
        toPattern.onClick = [this] { options.toPlaylist = false; refresh(); };
        for (auto* b : { &earlier, &later })
            addAndMakeVisible (*b);
        earlier.onClick = [this] { dropEdited = true; options.dropBar = juce::jmax (minDrop(), options.dropBar - 1); refresh(); };
        later.onClick = [this] { dropEdited = true; options.dropBar = juce::jmin (SequencerEngine::maxBars - 1, options.dropBar + 1); refresh(); };

        create.setButtonText ("Crea");
        create.onClick = [this] { make(); };
        addAndMakeVisible (create);

        options.dropBar = juce::jmax (options.bars, riser::suggestDropBar (engine, options.bars));
        setDown (startAsDowner);
    }

    ~RiserPanel() override   { stopPreview(); }

    std::function<void (const riser::Result&)> onCreated;
    std::function<void()> onClose;

    void paint (juce::Graphics& g) override
    {
        g.fillAll (Theme::panel);
        g.setColour (Theme::textBright);
        g.setFont (Theme::font (17.0f, true));
        g.drawText (options.down ? "Downer automatico" : "Riser automatico", titleArea, juce::Justification::centredLeft, false);
        Theme::drawCaption (g, "Durata", lengthCaption.toFloat(), juce::Justification::centredLeft);
        Theme::drawCaption (g, "Ingredienti", ingredientCaption.toFloat(), juce::Justification::centredLeft);
        Theme::drawCaption (g, "Suono", soundCaption.toFloat(), juce::Justification::centredLeft);
        Theme::drawCaption (g, "Dove", whereCaption.toFloat(), juce::Justification::centredLeft);

        // anteprima: curva della salita, colpi della rullata, impatto
        auto r = previewArea.toFloat();
        Theme::drawSunken (g, r);
        auto area = r.reduced (6.0f, 6.0f).withTrimmedRight (create.getWidth() + 8.0f);
        g.saveState();
        if (options.down)   // il downer e' la salita vista allo specchio
            g.addTransform (juce::AffineTransform::scale (-1.0f, 1.0f, area.getCentreX(), 0.0f));
        const bool any = riser::hasAudio (options) || options.filter;
        if (any)
        {
            juce::Path p;
            p.startNewSubPath (area.getX(), area.getBottom());
            for (int i = 0; i <= 40; ++i)
            {
                const float x = (float) i / 40.0f;
                p.lineTo (area.getX() + x * area.getWidth(), area.getBottom() - (0.1f + 0.85f * std::pow (x, juce::jlimit (1.0f, 3.0f, options.sound.curve))) * area.getHeight());
            }
            auto fill = p;
            fill.lineTo (area.getRight(), area.getBottom());
            fill.closeSubPath();
            g.setColour (Theme::cyan.withAlpha (0.12f));
            g.fillPath (fill);
            g.setColour (Theme::cyan);
            g.strokePath (p, juce::PathStrokeType (2.0f));
        }
        if (options.roll)
        {
            // gli stessi colpi del suono vero: da dove parte la rullata, divisioni dalla prima all'ultima
            static constexpr float every[4] { 1.0f, 0.5f, 0.25f, 0.125f };
            const auto& snd = options.sound;
            const float total = (float) options.bars * 4.0f;
            const int from = juce::jlimit (0, 3, snd.rollFrom), to = juce::jlimit (from, 3, snd.rollTo), sections = to - from + 1;
            const float startBeat = std::floor (total * juce::jlimit (0.0f, 0.95f, snd.rollStart));
            const float stride = juce::jmax (1.0f, total / 16.0f);   // con tante battute si disegna un colpo ogni tanto
            g.setColour (juce::Colour (0xffff6a5a));
            int k = 0;
            for (float b = startBeat; b < total - 1.0e-4f; ++k)
            {
                const float x = (b - startBeat) / juce::jmax (1.0e-4f, total - startBeat);
                if (k % (int) stride == 0)
                    g.fillRect (area.getX() + b / total * area.getWidth() - 1.0f, area.getBottom() - 14.0f, 2.0f, 14.0f);
                b += every[from + juce::jlimit (0, sections - 1, (int) (x * (float) sections))];
            }
        }
        if (options.sound.gap)
        {
            const float w = area.getWidth() * 0.5f / ((float) options.bars * 4.0f);
            g.setColour (Theme::lcdBg);
            g.fillRect (area.getRight() - w - 3.0f, area.getY(), w, area.getHeight());
        }
        if (options.impact)
        {
            g.setColour (Theme::textBright);
            g.fillRect (area.getRight() - 3.0f, area.getY(), 3.0f, area.getHeight());
        }
        g.restoreState();
        g.setColour (Theme::textDim);
        g.setFont (Theme::font (12.0f));
        g.drawText (status, area.withTrimmedLeft (options.down ? 10.0f : 2.0f).removeFromTop (16.0f), juce::Justification::topLeft, false);
    }

    void resized() override
    {
        auto r = getLocalBounds().reduced (14, 10);
        titleArea = r.removeFromTop (28);
        {
            auto modes = titleArea.withTrimmedLeft (200).removeFromLeft (220);
            modeRiser.setBounds (modes.removeFromLeft (110).reduced (2, 0));
            modeDowner.setBounds (modes.removeFromLeft (110).reduced (2, 0));
        }
        const int rowH = juce::jlimit (30, 40, getHeight() / 11);

        lengthCaption = r.removeFromTop (18);
        {
            auto row = r.removeFromTop (rowH);
            int x = row.getX();
            for (int i = 0; i < lengthButtons.size(); ++i)
            {
                const int w = i == 0 ? 110 : 52;
                lengthButtons[i]->setBounds (x, row.getY(), w, rowH - 4);
                x += w + 6;
            }
        }
        r.removeFromTop (4);
        ingredientCaption = r.removeFromTop (18);
        {
            const int perRow = getWidth() > 620 ? 5 : 3;
            const int rows = (ingredientButtons.size() + perRow - 1) / perRow;
            auto area = r.removeFromTop (rows * rowH);
            const int w = area.getWidth() / perRow;
            for (int i = 0; i < ingredientButtons.size(); ++i)
                ingredientButtons[i]->setBounds (juce::Rectangle<int> (area.getX() + (i % perRow) * w, area.getY() + (i / perRow) * rowH, w, rowH).reduced (3, 2));
        }
        r.removeFromTop (4);
        soundCaption = r.removeFromTop (18);
        {
            auto row = r.removeFromTop (rowH);
            prevSound.setBounds (row.removeFromLeft (rowH).reduced (2));
            soundButton.setBounds (row.removeFromLeft (juce::jmin (230, row.getWidth() / 2)).reduced (2));
            nextSound.setBounds (row.removeFromLeft (rowH).reduced (2));
            row.removeFromLeft (8);
            randomButton.setBounds (row.removeFromLeft (juce::jmin (120, row.getWidth() / 2)).reduced (2));
            listenButton.setBounds (row.removeFromLeft (juce::jmin (120, row.getWidth())).reduced (2));
        }
        r.removeFromTop (4);
        whereCaption = r.removeFromTop (18);
        {
            auto row = r.removeFromTop (rowH);
            const int w = juce::jmin (300, row.getWidth() / 2);
            auto pl = row.removeFromLeft (w);
            toPlaylist.setBounds (pl.reduced (2));
            row.removeFromLeft (4);
            earlier.setBounds (row.removeFromLeft (rowH).reduced (2));
            later.setBounds (row.removeFromLeft (rowH).reduced (2));
            row.removeFromLeft (8);
            toPattern.setBounds (row.removeFromLeft (juce::jmin (170, row.getWidth())).reduced (2));
        }
        r.removeFromTop (8);
        previewArea = r.removeFromTop (juce::jmax (56, juce::jmin (90, r.getHeight())));
        create.setBounds (previewArea.withSizeKeepingCentre (previewArea.getWidth(), previewArea.getHeight() - 12).removeFromRight (110).reduced (4, 0));
    }

    int preferredHeight (int width) const
    {
        const int rowH = 36, rows = width > 620 ? 1 : 2;
        return 20 + 28 + 18 + rowH + 4 + 18 + rows * rowH + 4 + 18 + rowH + 4 + 18 + rowH + 8 + 84 + 10;
    }

private:
    void addIngredient (const juce::String& name, bool& flag)
    {
        auto* b = ingredientButtons.add (new juce::TextButton (name));
        b->setClickingTogglesState (true);
        b->setToggleState (flag, juce::dontSendNotification);
        b->onClick = [this, b, &flag] { flag = b->getToggleState(); if (listening) startPreview(); refresh(); };
        addAndMakeVisible (b);
    }

    int minDrop() const   { return options.down ? 0 : options.bars; }

    void setDown (bool down)
    {
        options.down = down;
        modeRiser.setToggleState (! down, juce::dontSendNotification);
        modeDowner.setToggleState (down, juce::dontSendNotification);
        static const char* riserNames[] { "Rumore che sale", "Nota che sale", "Rullata", "Filtro sul master", "Impatto alla fine" };
        static const char* downerNames[] { "Rumore che scende", "Nota che scende", "Rullata che rallenta", "Filtro sul master", "Impatto all'inizio" };
        for (int i = 0; i < ingredientButtons.size(); ++i)
            ingredientButtons[i]->setButtonText (down ? downerNames[i] : riserNames[i]);
        options.dropBar = juce::jmax (options.dropBar, minDrop());
        if (listening) startPreview();
        refresh();
    }

    void refresh()
    {
        toPlaylist.setButtonText (options.down ? "Dalla battuta " + juce::String (options.dropBar + 1) + " (playlist)"
                                               : "Prima della battuta " + juce::String (options.dropBar + 1) + " (playlist)");
        earlier.setEnabled (options.toPlaylist);
        later.setEnabled (options.toPlaylist);
        const double secs = riser::riserSeconds (options, engine.getBpm());
        status = juce::String (options.bars) + (options.bars == 1 ? " battuta" : " battute") + " a " + juce::String (juce::roundToInt (engine.getBpm()))
               + " BPM = " + juce::String (secs, 1) + " s";
        if (options.filter && ! options.toPlaylist) status << "  (il filtro sul master funziona solo nella playlist)";
        if (options.toPlaylist && options.dropBar < minDrop()) status = "Il drop deve essere dopo la battuta " + juce::String (options.bars);
        status << "  -  si adatta da solo se cambi i BPM";
        create.setEnabled (riser::hasAudio (options) || (options.filter && options.toPlaylist));
        repaint();
    }

    void pickPreset (int index, bool notify = true)
    {
        const auto& list = riser::soundPresets();
        presetIndex = (index % (int) list.size() + (int) list.size()) % (int) list.size();
        options.sound = list[(size_t) presetIndex].sound;
        options.soundName = list[(size_t) presetIndex].name;
        if (notify) soundChanged();
        else soundButton.setButtonText (options.soundName + juce::String (juce::CharPointer_UTF8 ("  \xe2\x96\xbe")));
    }

    void showSoundMenu()
    {
        juce::PopupMenu m;
        const auto& list = riser::soundPresets();
        for (int i = 0; i < (int) list.size(); ++i)
            m.addItem (i + 1, list[(size_t) i].name, true, i == presetIndex);
        juce::Component::SafePointer<RiserPanel> safe (this);
        m.showMenuAsync (juce::PopupMenu::Options().withTargetComponent (&soundButton), [safe] (int r) { if (safe != nullptr && r > 0) safe->pickPreset (r - 1); });
    }

    void soundChanged()
    {
        soundButton.setButtonText (options.soundName + juce::String (juce::CharPointer_UTF8 ("  \xe2\x96\xbe")));
        if (listening) startPreview();   // si sente subito il suono nuovo
        refresh();
    }

    /** "Ascolta": calcola il riser cosi' com'e' adesso e lo fa sentire (tocca di nuovo per fermarlo). */
    void toggleListen()
    {
        if (listening) { stopPreview(); return; }
        startPreview();
    }

    void startPreview()
    {
        if (! riser::hasAudio (options)) return;
        SampleData::Ptr data = new SampleData();
        data->name = "riser-preview";
        data->sampleRate = 44100.0;
        data->audio = riser::render (options, engine.getBpm(), 44100.0, engine.getScaleRoot());
        keepAlive().push_back (data);                  // il thread audio puo' usarlo ancora per un attimo:
        while (keepAlive().size() > 3) keepAlive().erase (keepAlive().begin());   // si tengono gli ultimi
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
        const auto result = riser::apply (engine, store, options);
        if (! result.ok)
        {
            status = result.error;
            repaint();
            return;
        }
        if (onCreated) onCreated (result);
        if (onClose) onClose();
    }

    SequencerEngine& engine;
    SampleStore& store;
    riser::Options options;
    bool dropEdited = false;
    juce::OwnedArray<juce::TextButton> lengthButtons, ingredientButtons;
    juce::TextButton toPlaylist, toPattern { "Nuovo pattern" }, create;
    juce::TextButton modeRiser { "Riser" }, modeDowner { "Downer" };
    juce::TextButton prevSound { juce::String (juce::CharPointer_UTF8 ("\xe2\x97\x80")) }, nextSound { juce::String (juce::CharPointer_UTF8 ("\xe2\x96\xb6")) };
    juce::TextButton soundButton, randomButton { "Casuale" }, listenButton { juce::String (juce::CharPointer_UTF8 ("\xe2\x96\xb6 Ascolta")) };
    int presetIndex = 0;
    bool listening = false;
    juce::TextButton earlier { juce::String (juce::CharPointer_UTF8 ("\xe2\x97\x80")) }, later { juce::String (juce::CharPointer_UTF8 ("\xe2\x96\xb6")) };
    juce::Rectangle<int> titleArea, lengthCaption, ingredientCaption, soundCaption, whereCaption, previewArea;
    juce::String status;
};

//==============================================================================
/** La finestra al centro dello schermo (sfondo scuro; tocca fuori per chiudere). Si cancella da sola. */
class RiserOverlay : public juce::Component
{
public:
    explicit RiserOverlay (std::unique_ptr<RiserPanel> p) : panel (std::move (p))
    {
        viewport.setViewedComponent (panel.get(), false);
        viewport.setScrollBarsShown (true, false);
        addAndMakeVisible (viewport);
        panel->onClose = [this] { close(); };
    }

    void paint (juce::Graphics& g) override
    {
        g.fillAll (juce::Colours::black.withAlpha (0.55f));
        g.setColour (Theme::border);
        g.drawRect (viewport.getBounds().expanded (1), 1);
    }

    void resized() override
    {
        const int w = juce::jmin (720, getWidth() - 24);
        panel->setSize (w, panel->preferredHeight (w));
        const int h = juce::jmin (panel->getHeight(), getHeight() - 16);
        viewport.setBounds (juce::Rectangle<int> (w, h).withCentre (getLocalBounds().getCentre()));
    }

    void mouseUp (const juce::MouseEvent& e) override
    {
        if (! viewport.getBounds().contains (e.getPosition()))
            close();
    }

    void parentSizeChanged() override
    {
        if (auto* p = getParentComponent())
            setBounds (p->getLocalBounds());
    }

    void close()
    {
        setVisible (false);
        juce::Component::SafePointer<RiserOverlay> safe (this);
        juce::MessageManager::callAsync ([safe] { delete safe.getComponent(); });
    }

private:
    std::unique_ptr<RiserPanel> panel;
    juce::Viewport viewport;
};
