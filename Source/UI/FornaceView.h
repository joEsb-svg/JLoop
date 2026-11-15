#pragma once

#include <JuceHeader.h>
#include <functional>
#include "../Engine/SequencerEngine.h"
#include "Theme.h"
#include "ChannelEditor.h"   // LabeledKnob

//==============================================================================
/** La finestra di FORNACE, il saturatore a bande del mixer.

    In alto il grafico: le fasce di frequenza (colorate), lo spettro del suono che entra, le linee
    dei tagli da trascinare. Tocchi una banda e sotto compaiono le sue manopole (stile, drive, mix,
    livello, tono, dinamica, feedback). In fondo: mix totale, uscita, numero di bande e qualita'.
*/
class FornaceView : public juce::Component,
                    private juce::Timer
{
public:
    FornaceView (SequencerEngine& e, int trackIndex, int slotIndex)
        : engine (e), track (trackIndex), slot (slotIndex)
    {
        using namespace mixer::fornaceParam;
        struct K { const char* name; int what; };
        const K defs[] { { "Drive", drive }, { "Mix", bandMix }, { "Livello", level }, { "Tono", tone },
                         { "Dinamica", dynamics }, { "Feedback", feedback }, { "Freq. FB", fbFreq } };
        for (auto& d : defs)
        {
            const int what = d.what;
            auto* k = bandKnobs.add (new LabeledKnob (d.name, 0.0, 1.0, 0.5, [this, what] (double n) { return bandText (what, n); }));
            k->onChange = [this, what] (double n)
            {
                const int idx = at (band, what);
                set (idx, mixer::fromNormalised (def (idx), (float) n));
                display.repaint();
            };
            k->onRelease = [this] { changed(); };
            addAndMakeVisible (k);
        }
        for (int i = 0; i < 2; ++i)
        {
            const int idx = i == 0 ? (int) mix : (int) out;
            auto* k = globalKnobs.add (new LabeledKnob (i == 0 ? "Mix totale" : "Uscita", 0.0, 1.0, 0.5,
                                                         [idx] (double n) { return mixer::formatValue (def (idx), mixer::fromNormalised (def (idx), (float) n)); }));
            k->onChange = [this, idx] (double n) { set (idx, mixer::fromNormalised (def (idx), (float) n)); };
            k->onRelease = [this] { changed(); };
            k->setColour (Theme::accent);
            addAndMakeVisible (k);
        }

        styleButton.onClick = [this] { showStyleMenu(); };
        soloButton.onClick = [this] { toggleState (2); };
        muteButton.onClick = [this] { toggleState (1); };
        bandsButton.onClick = [this] { showChoiceMenu (bands, bandsButton); };
        qualityButton.onClick = [this] { showChoiceMenu (quality, qualityButton); };
        presetButton.onClick = [this] { showPresetMenu(); };
        closeButton.onClick = [this] { if (onClose) onClose(); };
        for (auto* b : { &styleButton, &soloButton, &muteButton, &bandsButton, &qualityButton, &presetButton, &closeButton })
            addAndMakeVisible (b);

        display.owner = this;
        addAndMakeVisible (display);
        setOpaque (true);
        refresh();
        startTimerHz (20);
    }

    std::function<void()> onChanged, onClose;

    static juce::Colour bandColour (int b)
    {
        static const juce::uint32 c[4] { 0xffff5b5b, 0xffff9a2e, 0xffffd84a, 0xff4fd6e8 };
        return juce::Colour (c[juce::jlimit (0, 3, b)]);
    }

    void selectBand (int b)
    {
        band = juce::jlimit (0, numBands() - 1, b);
        refresh();
    }

    /** Mette un preset (0 = predefinito). */
    void applyPreset (int index)
    {
        using namespace mixer::fornaceParam;
        const auto& inf = mixer::info (mixer::fornace);
        for (int p = 0; p < inf.numParams; ++p)
            set (p, inf.params[(size_t) p].defaultValue);
        auto b = [this] (int bnd, int st, float dr, std::initializer_list<std::pair<int, float>> more = {})
        {
            set (at (bnd, style), (float) st); set (at (bnd, drive), dr);
            for (auto& m : more) set (at (bnd, m.first), m.second);
        };
        auto cross = [this] (float a, float c, float d) { set (cross1, a); set (cross2, c); set (cross3, d); };
        switch (index)
        {
            case 1:  b (0, warmTube, 0.45f); b (1, tape, 0.4f); b (2, clean, 0.0f); b (3, clean, 0.0f); break;
            case 2:  cross (150.0f, 700.0f, 3500.0f); b (0, clean, 0.0f); b (1, crunch, 0.5f);
                     b (2, meanAmp, 0.75f, { { tone, 0.3f } }); b (3, hotTube, 0.3f); break;
            case 3:  cross (120.0f, 600.0f, 3000.0f); b (0, warmTube, 0.2f); b (1, fold, 0.55f, { { feedback, 0.5f }, { fbFreq, 300.0f } });
                     b (2, meanAmp, 0.6f, { { feedback, 0.4f }, { fbFreq, 1200.0f } }); b (3, tape, 0.3f); break;
            case 4:  set (bands, 0.0f); b (0, tape, 0.5f, { { dynamics, 0.3f } }); break;
            case 5:  set (bands, 2.0f); cross (140.0f, 2500.0f, 6000.0f); b (0, transformer, 0.4f, { { dynamics, 0.4f } });
                     b (1, warmTube, 0.35f); b (2, tape, 0.3f); break;
            case 6:  cross (300.0f, 1200.0f, 3500.0f); b (0, clean, 0.0f, { { state, 1.0f } }); b (1, crush, 0.6f);
                     b (2, destroy, 0.4f); b (3, clean, 0.0f, { { state, 1.0f } }); break;
            case 7:  set (bands, 1.0f); cross (250.0f, 2000.0f, 6000.0f); b (0, clean, 0.0f);
                     b (1, diode, 0.8f, { { feedback, 0.7f }, { fbFreq, 900.0f }, { tone, 0.4f } }); break;
            case 8:  cross (200.0f, 1500.0f, 5000.0f); b (0, clean, 0.0f); b (1, clean, 0.0f); b (2, warmTube, 0.4f);
                     b (3, tape, 0.3f, { { dynamics, 0.3f } }); break;
            default: break;
        }
        band = juce::jmin (band, numBands() - 1);
        refresh();
        changed();
    }

    static juce::StringArray presetNames()
    {
        return { "Predefinito", "Basso caldo, alti puliti", "Complextro: medi cattivi", "Growl che parla", "Nastro su tutto",
                 "Batteria che spinge", "Telefono rotto", "Acid che urla", "Voce presente" };
    }

    //==========================================================================
    void refresh()
    {
        using namespace mixer::fornaceParam;
        band = juce::jlimit (0, numBands() - 1, band);
        const int whats[] { drive, bandMix, level, tone, dynamics, feedback, fbFreq };
        for (int i = 0; i < bandKnobs.size(); ++i)
        {
            auto* k = bandKnobs[i];
            k->setColour (bandColour (band));
            if (! k->slider.isMouseButtonDown())
                k->setValue (mixer::toNormalised (def (at (band, whats[i])), get (at (band, whats[i]))));
        }
        for (int i = 0; i < 2; ++i)
            if (! globalKnobs[i]->slider.isMouseButtonDown())
                globalKnobs[i]->setValue (mixer::toNormalised (def (i == 0 ? (int) mix : (int) out), get (i == 0 ? (int) mix : (int) out)));

        styleButton.setButtonText ("Stile: " + mixer::formatValue (def (at (band, style)), get (at (band, style))));
        const int st = juce::roundToInt (get (at (band, state)));
        soloButton.setToggleState (st == 2, juce::dontSendNotification);
        muteButton.setToggleState (st == 1, juce::dontSendNotification);
        soloButton.setColour (juce::TextButton::buttonOnColourId, Theme::accent.withAlpha (0.8f));
        muteButton.setColour (juce::TextButton::buttonOnColourId, juce::Colour (0xffb04545));
        bandsButton.setButtonText ("Bande: " + juce::String (numBands()));
        qualityButton.setButtonText (juce::String (juce::CharPointer_UTF8 ("Qualit\xc3\xa0 ")) + mixer::formatValue (def (quality), get (quality)));
        repaint();
    }

    void paint (juce::Graphics& g) override
    {
        using namespace mixer::fornaceParam;
        g.fillAll (Theme::panel);

        g.setColour (Theme::accent);
        g.fillRoundedRectangle (titleArea.toFloat().removeFromLeft (4.0f).reduced (0.0f, 5.0f), 2.0f);
        g.setColour (Theme::textBright);
        g.setFont (Theme::font (17.0f, true).withExtraKerningFactor (0.12f));
        g.drawText ("FORNACE", titleArea.withTrimmedLeft (12), juce::Justification::centredLeft, false);

        if (! hintArea.isEmpty())
            Theme::drawCaption (g, "Trascina le linee dei tagli - tocca una banda",
                                hintArea.toFloat(), juce::Justification::centredLeft);

        // Le etichette delle bande
        const int nb = numBands();
        const char* names[4][4] { { "TUTTO", "", "", "" }, { "BASSI", "ALTI", "", "" }, { "BASSI", "MEDI", "ALTI", "" }, { "BASSI", "MEDIO-BASSI", "MEDIO-ALTI", "ALTI" } };
        for (int b = 0; b < nb; ++b)
        {
            auto r = chipRect (b).toFloat().reduced (2.0f, 1.0f);
            const auto c = bandColour (b);
            g.setColour (b == band ? c.withAlpha (0.14f) : Theme::raised.withAlpha (0.5f));
            g.fillRoundedRectangle (r, 5.0f);
            g.setColour (b == band ? c : Theme::edgeLight.withAlpha (0.4f));
            g.drawRoundedRectangle (r, 5.0f, b == band ? 1.5f : 1.0f);
            const bool twoLines = r.getHeight() >= 30.0f;
            auto top = twoLines ? r.removeFromTop (r.getHeight() * 0.5f) : r;
            g.setColour (c);
            g.setFont (Theme::font (10.5f, true));
            g.drawFittedText (juce::String (b + 1) + " " + names[nb - 1][b], top.toNearestInt().reduced (2, 0), juce::Justification::centred, 1, 0.7f);
            if (twoLines)
            {
                const int st = juce::roundToInt (get (at (b, state)));
                g.setColour (st == 1 ? juce::Colour (0xffb04545) : Theme::textDim);
                g.setFont (Theme::font (10.0f));
                g.drawFittedText (st == 1 ? juce::String ("muta") : st == 2 ? juce::String ("solo") : mixer::formatValue (def (at (b, style)), get (at (b, style))),
                                  r.toNearestInt().reduced (2, 0), juce::Justification::centred, 1, 0.7f);
            }
        }

        // Il riquadro della banda scelta
        const auto c = bandColour (band);
        g.setColour (Theme::bg.withAlpha (0.5f));
        g.fillRoundedRectangle (panelArea.toFloat(), 8.0f);
        g.setColour (c.withAlpha (0.4f));
        g.drawRoundedRectangle (panelArea.toFloat().reduced (0.5f), 8.0f, 1.0f);
        g.setColour (c);
        g.setFont (Theme::font (14.0f, true));
        g.drawFittedText ("Banda " + juce::String (band + 1) + (nb > 1 ? juce::String (" - ") + juce::String (names[nb - 1][band]).toLowerCase() : juce::String()),
                          bandTitleArea, juce::Justification::centredLeft, 1, 0.7f);
    }

    void resized() override
    {
        auto r = getLocalBounds().reduced (6);
        const bool wide = r.getWidth() > r.getHeight() * 1.25f;
        auto head = r.removeFromTop (wide ? 30 : 34);
        closeButton.setBounds (head.removeFromRight (wide ? 96 : 84).reduced (2));
        presetButton.setBounds (head.removeFromRight (wide ? 96 : 84).reduced (2));
        if (wide)
        {
            qualityButton.setBounds (head.removeFromRight (110).reduced (2));
            bandsButton.setBounds (head.removeFromRight (96).reduced (2));
        }
        titleArea = head;
        r.removeFromTop (4);

        juce::Rectangle<int> knobRows;
        if (wide)
        {
            hintArea = {};
            auto left = r.removeFromLeft (r.getWidth() * 46 / 100);
            r.removeFromLeft (8);
            chipsArea = left.removeFromBottom (juce::jlimit (22, 38, left.getHeight() / 6));
            left.removeFromBottom (4);
            display.setBounds (left);

            panelArea = r;
            auto p = r.reduced (8, 6);
            auto top = p.removeFromTop (30);
            muteButton.setBounds (top.removeFromRight (64).reduced (2));
            soloButton.setBounds (top.removeFromRight (64).reduced (2));
            styleButton.setBounds (top.removeFromRight (juce::jmin (220, top.getWidth() * 55 / 100)).reduced (2));
            bandTitleArea = top;
            p.removeFromTop (4);
            const int rowH = juce::jmin (130, p.getHeight() / 2);
            auto row1 = p.removeFromTop (rowH), row2 = p.removeFromTop (rowH);
            const int w = row1.getWidth() / 5;
            for (int i = 0; i < 5; ++i) bandKnobs[i]->setBounds (row1.removeFromLeft (w).reduced (8, 2));
            bandKnobs[5]->setBounds (row2.removeFromLeft (w).reduced (8, 2));
            bandKnobs[6]->setBounds (row2.removeFromLeft (w).reduced (8, 2));
            row2.removeFromLeft (w);
            globalKnobs[0]->setBounds (row2.removeFromLeft (w).reduced (8, 2));
            globalKnobs[1]->setBounds (row2.removeFromLeft (w).reduced (8, 2));
        }
        else
        {
            hintArea = r.removeFromTop (14);
            r.removeFromTop (2);
            display.setBounds (r.removeFromTop (juce::jlimit (100, 240, r.getHeight() * 30 / 100)));
            r.removeFromTop (5);
            chipsArea = r.removeFromTop (40);
            r.removeFromTop (5);

            auto bottom = r.removeFromBottom (juce::jlimit (62, 86, r.getHeight() / 5));
            const int bw = bottom.getWidth() / 4;
            globalKnobs[0]->setBounds (bottom.removeFromLeft (bw).reduced (2, 0));
            globalKnobs[1]->setBounds (bottom.removeFromLeft (bw).reduced (2, 0));
            bandsButton.setBounds (bottom.removeFromLeft (bw).withSizeKeepingCentre (bw - 6, 34));
            qualityButton.setBounds (bottom.withSizeKeepingCentre (bottom.getWidth() - 6, 34));
            r.removeFromBottom (5);

            panelArea = r;
            auto p = r.reduced (8, 6);
            auto top = p.removeFromTop (32);
            muteButton.setBounds (top.removeFromRight (62).reduced (2));
            soloButton.setBounds (top.removeFromRight (62).reduced (2));
            bandTitleArea = top;
            p.removeFromTop (3);
            styleButton.setBounds (p.removeFromTop (36).reduced (2));
            p.removeFromTop (4);
            const int row1H = juce::jmin (150, p.getHeight() * 55 / 100);
            auto row1 = p.removeFromTop (row1H);
            auto row2 = p.removeFromTop (juce::jmin (120, p.getHeight()));
            const int w1 = row1.getWidth() / 3, w2 = row2.getWidth() / 4;
            for (int i = 0; i < 3; ++i) bandKnobs[i]->setBounds (row1.removeFromLeft (w1).reduced (4, 0));
            for (int i = 3; i < 7; ++i) bandKnobs[i]->setBounds (row2.removeFromLeft (w2).reduced (2, 0));
        }
    }

    void mouseDown (const juce::MouseEvent& e) override
    {
        for (int b = 0; b < numBands(); ++b)
            if (chipRect (b).contains (e.getPosition()))
                selectBand (b);
    }

private:
    //==========================================================================
    /** Il grafico: bande, spettro, tagli. */
    struct Display : public juce::Component
    {
        FornaceView* owner = nullptr;
        std::array<float, 96> spectrum {};
        std::array<float, 4> level {};
        int dragLine = -1, dragBand = -1;
        float dragStartDrive = 0.0f;
        bool dragging = false;

        static constexpr float fMin = 30.0f, fMax = 18000.0f;
        float xFor (float f) const   { return (float) getWidth() * std::log (juce::jlimit (fMin, fMax, f) / fMin) / std::log (fMax / fMin); }
        float fFor (float x) const   { return fMin * std::pow (fMax / fMin, juce::jlimit (0.0f, 1.0f, x / (float) juce::jmax (1, getWidth()))); }

        /** I bordi (in pixel) delle bande accese. */
        std::array<float, 5> edges() const
        {
            using namespace mixer::fornaceParam;
            const int nb = owner->numBands();
            std::array<float, 5> e {};
            e[0] = 0.0f;
            for (int j = 1; j < nb; ++j) e[(size_t) j] = xFor (owner->crossover (j - 1));
            for (int j = nb; j < 5; ++j) e[(size_t) j] = (float) getWidth();
            return e;
        }

        void paint (juce::Graphics& g) override
        {
            using namespace mixer::fornaceParam;
            const auto full = getLocalBounds().toFloat();
            Theme::drawSunken (g, full, Theme::lcdBg, 8.0f);
            g.saveState();
            juce::Path clip; clip.addRoundedRectangle (full.reduced (1.0f), 7.0f);
            g.reduceClipRegion (clip);

            const int nb = owner->numBands();
            const auto e = edges();
            const float h = full.getHeight(), base = h - 17.0f, room = juce::jmax (10.0f, base - 20.0f);
            for (int b = 0; b < nb; ++b)
            {
                const auto c = FornaceView::bandColour (b);
                const float x0 = e[(size_t) b], x1 = e[(size_t) b + 1];
                const bool sel = b == owner->band;
                const int st = juce::roundToInt (owner->get (at (b, state)));
                const float dim = st == 1 ? 0.35f : 1.0f;
                g.setColour (c.withAlpha ((sel ? 0.16f : 0.06f) * dim));
                g.fillRect (x0, 0.0f, x1 - x0, h);
                const float d = owner->get (at (b, drive));
                const float barH = juce::jmax (3.0f, d * room);
                const juce::Rectangle<float> bar (x0 + 3.0f, base - barH, juce::jmax (2.0f, x1 - x0 - 6.0f), barH);
                g.setColour (c.withAlpha ((0.22f + 0.5f * juce::jlimit (0.0f, 1.0f, level[(size_t) b])) * dim));
                g.fillRoundedRectangle (bar, 4.0f);
                g.setColour (c.withAlpha (dim));
                g.fillRoundedRectangle (bar.withHeight (3.0f), 1.5f);
                g.setFont (Theme::font (10.0f, true));
                g.drawText (juce::String (b + 1), juce::Rectangle<float> (x0, 2.0f, x1 - x0, 13.0f), juce::Justification::centred, false);
                if (sel)
                {
                    g.setColour (c);
                    g.drawRoundedRectangle (juce::Rectangle<float> (x0 + 1.0f, 1.0f, x1 - x0 - 2.0f, h - 2.0f), 5.0f, 1.5f);
                }
            }

            // Lo spettro del suono che entra
            juce::Path sp;
            for (size_t i = 0; i < spectrum.size(); ++i)
            {
                const float x = full.getWidth() * (float) i / (float) (spectrum.size() - 1);
                const float y = base - juce::jlimit (0.0f, 1.0f, spectrum[i]) * room;
                if (i == 0) sp.startNewSubPath (x, y); else sp.lineTo (x, y);
            }
            g.setColour (juce::Colours::white.withAlpha (0.55f));
            g.strokePath (sp, juce::PathStrokeType (1.3f, juce::PathStrokeType::curved));

            // I tagli
            g.setFont (Theme::font (10.0f, true));
            for (int j = 0; j < nb - 1; ++j)
            {
                const float x = e[(size_t) j + 1];
                g.setColour (juce::Colours::white.withAlpha (dragLine == j ? 1.0f : 0.7f));
                g.fillRect (x - 0.8f, 0.0f, 1.6f, h);
                const float f = owner->crossover (j);
                const auto text = f >= 1000.0f ? juce::String (f / 1000.0f, 1) + " kHz" : juce::String (juce::roundToInt (f)) + " Hz";
                auto tag = juce::Rectangle<float> (46.0f, 14.0f).withCentre ({ juce::jlimit (24.0f, full.getWidth() - 24.0f, x), h - 9.0f });
                g.setColour (Theme::raised);
                g.fillRoundedRectangle (tag, 3.0f);
                g.setColour (Theme::textBright);
                g.drawText (text, tag, juce::Justification::centred, false);
            }
            g.restoreState();
        }

        void mouseDown (const juce::MouseEvent& ev) override
        {
            using namespace mixer::fornaceParam;
            const auto e = edges();
            const int nb = owner->numBands();
            dragLine = dragBand = -1;
            dragging = false;
            float best = 16.0f;
            for (int j = 0; j < nb - 1; ++j)
                if (const float d = std::abs (ev.position.x - e[(size_t) j + 1]); d < best) { best = d; dragLine = j; }
            if (dragLine >= 0) { repaint(); return; }
            for (int b = 0; b < nb; ++b)
                if (ev.position.x >= e[(size_t) b] && ev.position.x < e[(size_t) b + 1]) dragBand = b;
            if (dragBand >= 0)
            {
                owner->selectBand (dragBand);
                dragStartDrive = owner->get (at (dragBand, drive));
            }
        }

        void mouseDrag (const juce::MouseEvent& ev) override
        {
            using namespace mixer::fornaceParam;
            if (dragLine >= 0)
            {
                // ogni taglio resta tra i suoi vicini (almeno un terzo di ottava di distanza)
                const int nb = owner->numBands();
                const float lo = dragLine == 0 ? 30.0f : owner->crossover (dragLine - 1) * 1.3f;
                const float hi = dragLine >= nb - 2 ? 18000.0f : owner->crossover (dragLine + 1) / 1.3f;
                owner->set (cross1 + dragLine, juce::jlimit (lo, juce::jmax (lo, hi), fFor (ev.position.x)));
                dragging = true;
                owner->repaint();
            }
            else if (dragBand >= 0 && (dragging || std::abs (ev.getDistanceFromDragStartY()) > 6))
            {
                dragging = true;
                const float room = juce::jmax (10.0f, (float) getHeight() - 37.0f);
                owner->set (at (dragBand, drive), juce::jlimit (0.0f, 1.0f, dragStartDrive - (float) ev.getDistanceFromDragStartY() / room));
                owner->refresh();
            }
            repaint();
        }

        void mouseUp (const juce::MouseEvent&) override
        {
            if (dragging) owner->changed();
            dragLine = dragBand = -1;
            dragging = false;
            repaint();
        }
    };

    //==========================================================================
    static const mixer::ParamDef& def (int p)   { return mixer::info (mixer::fornace).params[(size_t) p]; }
    mixer::Slot& theSlot() const                { return engine.getTrack (track).slots[(size_t) slot]; }
    float get (int p) const                     { return theSlot().params[(size_t) p].load(); }
    void set (int p, float v)                   { theSlot().params[(size_t) p].store (juce::jlimit (def (p).minValue, def (p).maxValue, v)); }
    void changed()                              { if (onChanged) onChanged(); }
    int numBands() const                        { return juce::jlimit (1, 4, 1 + juce::roundToInt (get (mixer::fornaceParam::bands))); }

    /** La frequenza di un taglio come la usa il motore (i tagli non si scavalcano). */
    float crossover (int j) const
    {
        using namespace mixer::fornaceParam;
        float f = juce::jlimit (30.0f, 18000.0f, get (cross1));
        for (int k = 1; k <= j; ++k) f = juce::jmax (f * 1.3f, get (cross1 + k));
        return juce::jmin (18000.0f, f);
    }

    juce::Rectangle<int> chipRect (int b) const
    {
        const int nb = numBands();
        const int w = chipsArea.getWidth() / juce::jmax (1, nb);
        return { chipsArea.getX() + b * w, chipsArea.getY(), w, chipsArea.getHeight() };
    }

    juce::String bandText (int what, double n) const
    {
        using namespace mixer::fornaceParam;
        const auto& d = def (at (0, what));
        const float v = mixer::fromNormalised (d, (float) n);
        if (what == tone)
            return std::abs (v) < 0.02f ? juce::String ("neutro") : (v > 0.0f ? "chiaro " : "scuro ") + juce::String (juce::roundToInt (std::abs (v) * 100.0f)) + "%";
        if (what == dynamics)
            return std::abs (v) < 0.02f ? juce::String ("spenta") : (v > 0.0f ? "comprime " : "espande ") + juce::String (juce::roundToInt (std::abs (v) * 100.0f)) + "%";
        return mixer::formatValue (d, v);
    }

    void toggleState (int which)
    {
        using namespace mixer::fornaceParam;
        const int idx = at (band, state);
        set (idx, juce::roundToInt (get (idx)) == which ? 0.0f : (float) which);
        refresh();
        changed();
    }

    void showStyleMenu()
    {
        using namespace mixer::fornaceParam;
        const int idx = at (band, style);
        const int current = juce::roundToInt (get (idx));
        juce::PopupMenu menu;
        struct Group { const char* name; std::initializer_list<int> styles; };
        const Group groups[] { { "Pulito", { clean } }, { "Caldi", { warmTube, tape, transformer } },
                               { "Cattivi", { hotTube, crunch, meanAmp, diode } }, { "Strani", { fold, rectify, crush, destroy } } };
        for (auto& gr : groups)
        {
            menu.addSectionHeader (gr.name);
            for (int s : gr.styles)
                menu.addItem (s + 1, mixer::formatValue (def (idx), (float) s), true, s == current);
        }
        juce::Component::SafePointer<FornaceView> safe (this);
        menu.showMenuAsync (juce::PopupMenu::Options().withTargetComponent (&styleButton), [safe, idx] (int result)
        {
            if (safe == nullptr || result <= 0) return;
            safe->set (idx, (float) (result - 1));
            safe->refresh();
            safe->changed();
        });
    }

    void showChoiceMenu (int idx, juce::Component& target)
    {
        juce::PopupMenu menu;
        const int current = juce::roundToInt (get (idx));
        for (int v = juce::roundToInt (def (idx).minValue); v <= juce::roundToInt (def (idx).maxValue); ++v)
            menu.addItem (v + 1, mixer::formatValue (def (idx), (float) v), true, v == current);
        juce::Component::SafePointer<FornaceView> safe (this);
        menu.showMenuAsync (juce::PopupMenu::Options().withTargetComponent (&target), [safe, idx] (int result)
        {
            if (safe == nullptr || result <= 0) return;
            safe->set (idx, (float) (result - 1));
            safe->refresh();
            safe->resized();
            safe->changed();
        });
    }

    void showPresetMenu()
    {
        juce::PopupMenu menu;
        const auto names = presetNames();
        for (int i = 0; i < names.size(); ++i)
            menu.addItem (i + 1, names[i]);
        juce::Component::SafePointer<FornaceView> safe (this);
        menu.showMenuAsync (juce::PopupMenu::Options().withTargetComponent (&presetButton), [safe] (int result)
        {
            if (safe != nullptr && result > 0) safe->applyPreset (result - 1);
        });
    }

    void timerCallback() override
    {
        auto& sl = theSlot();
        if (sl.type.load() != mixer::fornace)
            return;
        // owned cambia solo su questo thread (quello della UI): si puo' leggere senza lucchetto
        if (auto* fx = static_cast<mixer::Fornace*> (sl.owned.get()))
        {
            for (size_t b = 0; b < 4; ++b)
            {
                const float pk = fx->meter[b].exchange (0.0f);
                const float shown = juce::jlimit (0.0f, 1.0f, (juce::Decibels::gainToDecibels (pk, -60.0f) + 48.0f) / 48.0f);
                display.level[b] = juce::jmax (shown, display.level[b] * 0.8f);
            }
            // spettro: l'ultimo pezzo di suono, finestra di Hann, FFT
            const int n = mixer::Fornace::scopeSize;
            const int pos = fx->scopePos.load();
            for (int i = 0; i < n; ++i)
                fftData[(size_t) i] = fx->scope[(size_t) ((pos + i) & (n - 1))] * (0.5f - 0.5f * std::cos (juce::MathConstants<float>::twoPi * (float) i / (float) (n - 1)));
            std::fill (fftData.begin() + n, fftData.end(), 0.0f);
            fft.performFrequencyOnlyForwardTransform (fftData.data());
            const double rate = fx->scopeRate.load();
            for (size_t i = 0; i < display.spectrum.size(); ++i)
            {
                const float f0 = Display::fMin * std::pow (Display::fMax / Display::fMin, ((float) i - 0.5f) / (float) (display.spectrum.size() - 1));
                const float f1 = Display::fMin * std::pow (Display::fMax / Display::fMin, ((float) i + 0.5f) / (float) (display.spectrum.size() - 1));
                const int b0 = juce::jlimit (1, n / 2 - 1, (int) (f0 * n / rate)), b1 = juce::jlimit (b0, n / 2 - 1, (int) (f1 * n / rate));
                float m = 0.0f;
                for (int k = b0; k <= b1; ++k) m = juce::jmax (m, fftData[(size_t) k]);
                const float db = juce::Decibels::gainToDecibels (m / (float) (n / 4), -90.0f) + 3.0f * std::log2 (juce::jmax (1.0f, f0 / 100.0f));   // un po' di pendenza: gli alti si vedono
                const float shown = juce::jlimit (0.0f, 1.0f, (db + 66.0f) / 66.0f);
                display.spectrum[i] = shown > display.spectrum[i] ? shown : display.spectrum[i] * 0.85f + shown * 0.15f;
            }
        }
        // valori cambiati altrove (annulla, automazione): manopole aggiornate
        bool moving = display.dragging;
        for (auto* k : bandKnobs) moving = moving || k->slider.isMouseButtonDown();
        for (auto* k : globalKnobs) moving = moving || k->slider.isMouseButtonDown();
        bool different = false;
        for (int p = 0; p < mixer::info (mixer::fornace).numParams; ++p)
            if (const float v = get (p); v != seen[(size_t) p]) { seen[(size_t) p] = v; different = true; }
        if (different && ! moving) refresh();
        display.repaint();
    }

    SequencerEngine& engine;
    const int track, slot;
    int band = 0;
    Display display;
    juce::OwnedArray<LabeledKnob> bandKnobs, globalKnobs;
    juce::TextButton styleButton, soloButton { "Solo" }, muteButton { "Muta" }, bandsButton, qualityButton, presetButton { "Preset" }, closeButton { "Effetti" };
    juce::Rectangle<int> titleArea, hintArea, chipsArea, panelArea, bandTitleArea;
    juce::dsp::FFT fft { 11 };
    std::array<float, mixer::Fornace::scopeSize * 2> fftData {};
    std::array<float, mixer::maxParams> seen {};
};
