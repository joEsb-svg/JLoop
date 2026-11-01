#pragma once

#include <JuceHeader.h>
#include "../Engine/AutoFx.h"
#include "../Engine/RiserMaker.h"
#include "SoundBrowser.h"
#include "Theme.h"
#include "Widgets.h"

//==============================================================================
/**
    "Effetti pronti" delle automazioni: scegli l'effetto, dove (Master o una traccia del mixer),
    da che battuta e per quanto. JLoop mette l'effetto nel mixer e disegna le curve.
*/
class AutoFxPanel : public juce::Component
{
public:
    explicit AutoFxPanel (SequencerEngine& e) : engine (e)
    {
        for (auto* b : { &cancelButton, &applyButton, &minusButton, &plusButton })
            addAndMakeVisible (b);
        cancelButton.onClick = [this] { setVisible (false); };
        applyButton.onClick = [this] { apply(); };
        minusButton.onClick = [this] { manualStart = true; setStart (startBar - 1); };
        plusButton.onClick  = [this] { manualStart = true; setStart (startBar + 1); };
        addAndMakeVisible (lengthSwitch);
        lengthSwitch.onChange = [this] (int) { bars = lengths[(size_t) lengthSwitch.getSelected()]; suggestStart(); repaint(); };

        for (auto* vp : { &fxView, &whereView })
        {
            vp->setScrollOnDragMode (juce::Viewport::ScrollOnDragMode::all);
            vp->setScrollBarsShown (true, false);
            vp->setScrollBarThickness (6);
            addAndMakeVisible (vp);
        }
        fxView.setViewedComponent (&fxList, false);
        whereView.setViewedComponent (&whereList, false);
        fxList.onTap = [this] (int i) { chooseFx (i); };
        whereList.onTap = [this] (int i) { whereList.setSelected (i); repaint(); };
    }

    /** Fatto: l'effetto e' stato messo (per ridisegnare le curve). */
    std::function<void()> onApplied;

    void open()
    {
        // Effetti, per categoria
        fxIds.clear();
        std::vector<TileList::Item> items;
        for (int c = 0; c < autofx::numCategories; ++c)
            for (int id = 0; id < autofx::numIds; ++id)
                if (autofx::info (id).category == c)
                {
                    fxIds.push_back (id);
                    items.push_back ({ autofx::info (id).name, autofx::info (id).detail, juce::Colour (autofx::info (id).colour), true });
                }
        fxList.setItems (items, -1);

        // Dove: Master e le tracce usate
        tracks.clear();
        std::vector<TileList::Item> where;
        tracks.push_back (0);
        where.push_back ({ "Master", "Tutta la canzone", Theme::accent, true });
        for (int t = 1; t < mixer::numTracks; ++t)
            if (engine.isTrackUsed (t))
            {
                tracks.push_back (t);
                where.push_back ({ "Mixer " + juce::String (t) + ": " + engine.getTrackName (t), {}, engine.getTrackColour (t), true });
            }
        whereList.setItems (where, 0);

        manualStart = false;
        chooseFx (0);
        setVisible (true);
        toFront (true);
    }

    void paint (juce::Graphics& g) override
    {
        g.setColour (Theme::panel);
        g.fillRoundedRectangle (getLocalBounds().toFloat(), Theme::corner);
        g.setColour (Theme::textBright);
        g.setFont (Theme::font (16.0f, true));
        g.drawFittedText (getWidth() < 640 ? "Effetti pronti" : "Effetti pronti: JLoop mette l'effetto e disegna la curva", titleArea, juce::Justification::centredLeft, 1);

        Theme::drawCaption (g, "1. Effetto", fxCaption, juce::Justification::centredLeft);
        Theme::drawCaption (g, "2. Dove", whereCaption, juce::Justification::centredLeft);
        Theme::drawCaption (g, "3. Quando", whenCaption, juce::Justification::centredLeft);
        const auto& inf = autofx::info (currentFx());
        if (inf.usesLength)
            Theme::drawCaption (g, "Quante battute", lengthCaption, juce::Justification::centredLeft);
        g.setColour (Theme::text);
        g.setFont (Theme::font (13.0f, true));
        g.drawFittedText ("Dalla battuta", startLabel.toNearestInt(), juce::Justification::centredLeft, 1);
        g.setColour (Theme::lcdText);
        g.setFont (Theme::font (22.0f, true));
        g.drawFittedText (juce::String (startBar + 1), startValue.toNearestInt(), juce::Justification::centred, 1);

        g.setColour (Theme::textDim);
        g.setFont (Theme::font (12.0f));
        juce::String when = inf.usesLength
            ? "Battute " + juce::String (startBar + 1) + " - " + juce::String (startBar + bars)
                + "  (fino al drop della battuta " + juce::String (startBar + bars + 1) + ")"
            : "Nella battuta " + juce::String (startBar + 1) + ", subito prima della battuta " + juce::String (startBar + 2);
        g.drawFittedText (when, summaryArea.toNearestInt(), juce::Justification::topLeft, 3);
    }

    void resized() override
    {
        auto r = getLocalBounds().reduced (8, 6);
        auto top = r.removeFromTop (34);
        cancelButton.setBounds (top.removeFromRight (90).reduced (0, 2));
        titleArea = top;
        r.removeFromTop (6);
        const bool narrow = r.getWidth() < 640;

        juce::Rectangle<int> fxArea, whereArea, whenArea;
        if (narrow)
        {
            whenArea = r.removeFromBottom (190);
            r.removeFromBottom (6);
            whereArea = r.removeFromBottom (juce::jmin (150, r.getHeight() / 3));
            r.removeFromBottom (6);
            fxArea = r;
        }
        else
        {
            fxArea = r.removeFromLeft (r.getWidth() * 45 / 100);
            r.removeFromLeft (10);
            whereArea = r.removeFromLeft (r.getWidth() / 2);
            r.removeFromLeft (10);
            whenArea = r;
        }

        fxCaption = fxArea.removeFromTop (18).toFloat();
        fxView.setBounds (fxArea);
        fxList.setLayout (1, 50);
        fxList.setVisibleWidth (fxArea.getWidth() - 8);

        whereCaption = whereArea.removeFromTop (18).toFloat();
        whereView.setBounds (whereArea);
        whereList.setLayout (narrow ? juce::jmax (1, whereArea.getWidth() / 170) : 1, 40);
        whereList.setVisibleWidth (whereArea.getWidth() - 8);

        whenCaption = whenArea.removeFromTop (18).toFloat();
        auto row = whenArea.removeFromTop (44);
        startLabel = row.removeFromLeft (100).toFloat();
        minusButton.setBounds (row.removeFromLeft (44).reduced (2));
        startValue = row.removeFromLeft (56).toFloat();
        plusButton.setBounds (row.removeFromLeft (44).reduced (2));
        whenArea.removeFromTop (6);
        lengthCaption = whenArea.removeFromTop (16).toFloat();
        lengthSwitch.setBounds (whenArea.removeFromTop (34).withWidth (juce::jmin (whenArea.getWidth(), 330)));
        whenArea.removeFromTop (6);
        applyButton.setBounds (whenArea.removeFromBottom (44).reduced (0, 2));
        summaryArea = whenArea.toFloat();
    }

    // Test
    void selectForTest (int fxIndex, int whereIndex)   { chooseFx (fxIndex); whereList.setSelected (whereIndex); repaint(); }

private:
    int currentFx() const   { const int i = fxList.getSelected(); return i >= 0 && i < (int) fxIds.size() ? fxIds[(size_t) i] : 0; }

    void chooseFx (int i)
    {
        if (i < 0 || i >= (int) fxIds.size()) return;
        fxList.setSelected (i);
        const auto& inf = autofx::info (fxIds[(size_t) i]);
        bars = inf.defaultBars;
        for (int k = 0; k < (int) lengths.size(); ++k)
            if (lengths[(size_t) k] == bars) lengthSwitch.setSelected (k);
        lengthSwitch.setVisible (inf.usesLength);
        if (! inf.usesLength) bars = 1;
        // Sidechain: il Master non va bene, parte dalla prima traccia
        if (fxIds[(size_t) i] == autofx::sidechainUp && whereList.getSelected() == 0 && tracks.size() > 1)
        {
            const int kick = autofx::detail::findKickTrack (engine, -1);
            for (int k = 1; k < (int) tracks.size(); ++k)
                if (tracks[(size_t) k] != kick) { whereList.setSelected (k); break; }
        }
        suggestStart();
        repaint();
    }

    /** Dove partire: gli effetti da build-up finiscono sul drop; gli altri partono dal cursore della canzone. */
    void suggestStart()
    {
        if (manualStart) { repaint(); return; }
        const auto& inf = autofx::info (currentFx());
        const bool endsOnDrop = inf.category == autofx::buildAndDrop || inf.category == autofx::space;
        if (endsOnDrop && ! engine.getClips().empty())
            setStart (riser::suggestDropBar (engine, bars) - bars);
        else
            setStart (engine.getSongStartBar());
    }

    void setStart (int b)    { startBar = juce::jlimit (0, SequencerEngine::maxBars - bars, b); repaint(); }

    void apply()
    {
        const int w = whereList.getSelected();
        const int track = w >= 0 && w < (int) tracks.size() ? tracks[(size_t) w] : 0;
        const auto r = autofx::apply (engine, currentFx(), track, startBar, bars);
        if (! r.ok)
        {
            juce::AlertWindow::showAsync (juce::MessageBoxOptions().withTitle ("Effetti pronti").withMessage (r.error)
                                                                   .withButton ("OK").withAssociatedComponent (this), nullptr);
            return;
        }
        setVisible (false);
        if (onApplied) onApplied();
    }

    SequencerEngine& engine;
    std::vector<int> fxIds, tracks;
    int startBar = 0, bars = 4;
    bool manualStart = false;
    const std::array<int, 5> lengths { 1, 2, 4, 8, 16 };

    juce::Viewport fxView, whereView;
    TileList fxList, whereList;
    SegmentedControl lengthSwitch { { "1", "2", "4", "8", "16" } };
    juce::TextButton cancelButton { "Annulla" }, applyButton { "Metti l'effetto" }, minusButton { "-" }, plusButton { "+" };
    juce::Rectangle<int> titleArea;
    juce::Rectangle<float> fxCaption, whereCaption, whenCaption, startLabel, startValue, summaryArea, lengthCaption;
};
