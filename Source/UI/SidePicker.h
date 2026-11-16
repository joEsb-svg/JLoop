#pragma once

#include <JuceHeader.h>
#include <functional>
#include <vector>
#include "Theme.h"

//==============================================================================
/**
    Scelta da un elenco lungo, nello stile del menu principale: un pannello che esce da sinistra,
    a sinistra i gruppi, a destra le voci del gruppo (con una riga di spiegazione), a due colonne
    quando c'e' spazio. Tocca una voce per sceglierla; tocca fuori, Indietro o Esc per chiudere.

    Si usa per gli effetti del mixer (e va bene per altri elenchi simili).
    Il pannello si mette sopra la finestra intera (chi lo apre lo aggiunge al componente principale).
*/
class SidePicker : public juce::Component,
                   private juce::Timer
{
public:
    struct Entry { juce::String name, description; int id = 0; bool current = false; };
    struct Group { juce::String name; std::vector<Entry> entries; };

    juce::String title;                         // es. "Effetto - slot 1"
    std::vector<Group> groups;
    juce::String extraAction;                   // es. "Togli l'effetto" (vuoto = niente)
    int extraId = 0;
    bool extraEnabled = true;
    std::function<void (int)> onPick;           // dopo la chiusura
    std::function<void()> onClosed;

    SidePicker()
    {
        setWantsKeyboardFocus (true);
        viewport.setViewedComponent (&content, false);
        viewport.setScrollBarsShown (true, false);
        viewport.setScrollBarThickness (6);
        viewport.setScrollOnDragMode (juce::Viewport::ScrollOnDragMode::all);
        addAndMakeVisible (viewport);
    }

    /** Apre il pannello sul gruppo che contiene la voce scelta (o sul primo). */
    void open (bool animate = true)
    {
        group = 0;
        for (int g = 0; g < (int) groups.size(); ++g)
            for (auto& e : groups[(size_t) g].entries)
                if (e.current) group = g;
        showGroup (group);
        shown = animate ? 0.0f : 1.0f;
        closing = false;
        if (animate) startTimerHz (60);
        resized();
        grabKeyboardFocus();
    }

    void close()
    {
        if (closing) return;
        closing = true;
        startTimerHz (60);
    }

    void showGroupForTest (int g)   { showGroup (juce::jlimit (0, (int) groups.size() - 1, g)); repaint(); }

    //==========================================================================
    void paint (juce::Graphics& g) override
    {
        g.fillAll (juce::Colours::black.withAlpha (0.62f * shown));
        const auto d = drawerBounds().toFloat();
        g.setColour (juce::Colours::black.withAlpha (0.5f * shown));
        g.fillRect (d.withX (d.getRight()).withWidth (14.0f));
        g.setColour (cPanel);
        g.fillRect (d);
        g.setColour (cLine);
        g.drawVerticalLine ((int) d.getRight() - 1, d.getY(), d.getBottom());

        auto rail = railBounds().toFloat();
        g.setColour (cRail);
        g.fillRect (rail);
        g.setColour (juce::Colour (0xff1b2733));
        g.drawVerticalLine ((int) rail.getRight() - 1, rail.getY(), rail.getBottom());

        // titolo in cima alla colonna
        g.setColour (cDim);
        g.setFont (Theme::font (9.5f, true).withExtraKerningFactor (0.14f));
        g.drawFittedText (title.toUpperCase(), rail.reduced (12.0f, 0.0f).withHeight (44.0f).toNearestInt(), juce::Justification::centredLeft, 2, 0.8f);

        for (int i = 0; i < (int) groups.size(); ++i)
        {
            const auto r = groupRect (i).toFloat();
            const bool sel = i == group;
            if (sel)
            {
                g.setColour (cCyanBg);
                g.fillRoundedRectangle (r, 8.0f);
                g.setColour (cCyan.withAlpha (0.35f));
                g.drawRoundedRectangle (r.reduced (0.5f), 8.0f, 1.0f);
            }
            bool hasCurrent = false;
            for (auto& e : groups[(size_t) i].entries) hasCurrent = hasCurrent || e.current;
            auto tr = r.reduced (narrow() ? 6.0f : 12.0f, 0.0f);
            if (hasCurrent)
            {
                g.setColour (cOrange);
                g.fillEllipse (tr.removeFromRight (7.0f).withSizeKeepingCentre (7.0f, 7.0f));
            }
            g.setColour (sel ? cCyan : cText);
            g.setFont (Theme::font (narrow() ? 11.0f : 13.5f, true));
            g.drawFittedText (groups[(size_t) i].name, tr.toNearestInt(), juce::Justification::centredLeft, 2, 0.75f);
        }

        auto outside = getLocalBounds().withLeft ((int) d.getRight() + 14);
        if (outside.getWidth() > 70 && shown > 0.9f)
        {
            g.setColour (juce::Colour (0xff9aa6b2));
            g.setFont (Theme::font (10.5f));
            g.drawFittedText ("tocca qui fuori per chiudere", outside.removeFromBottom (44).reduced (6, 0), juce::Justification::centred, 2, 0.8f);
        }
    }

    void resized() override
    {
        viewport.setBounds (drawerBounds().withTrimmedLeft (railBounds().getWidth()).reduced (12, 10));
        layoutContent();
    }

    void mouseDown (const juce::MouseEvent& e) override
    {
        const auto p = e.getPosition();
        if (! drawerBounds().contains (p)) { close(); return; }
        for (int i = 0; i < (int) groups.size(); ++i)
            if (groupRect (i).contains (p)) { showGroup (i); repaint(); return; }
    }

    bool keyPressed (const juce::KeyPress& k) override
    {
        if (k == juce::KeyPress::escapeKey || k == juce::KeyPress::backspaceKey) { close(); return true; }
        return false;
    }

private:
    inline static const juce::Colour cPanel { 0xff0f161e }, cRail { 0xff0b1117 }, cCard { 0xff141c25 }, cLine { 0xff233140 },
                                     cText { 0xffdfe6ee }, cDim { 0xff7f8c99 }, cCyan { 0xff46d9f0 }, cCyanBg { 0xff0d2229 }, cOrange { 0xfff0922b };

    struct Tile : public juce::Component
    {
        SidePicker* owner = nullptr;
        juce::String name, description;
        int id = 0;
        bool current = false, enabled = true, plain = false;

        void paint (juce::Graphics& g) override
        {
            auto r = getLocalBounds().toFloat().reduced (0.5f);
            g.setColour (isMouseButtonDown() ? juce::Colour (0xff1c2733) : current ? cCyanBg : cCard);
            g.fillRoundedRectangle (r, 8.0f);
            g.setColour (current ? cCyan : cLine);
            g.drawRoundedRectangle (r, 8.0f, current ? 1.5f : 1.0f);
            auto in = r.reduced (12.0f, 6.0f);
            const float a = enabled ? 1.0f : 0.4f;
            if (plain || description.isEmpty())
            {
                g.setColour ((current ? cCyan : cText).withAlpha (a));
                g.setFont (Theme::font (13.0f, true));
                g.drawFittedText (name, in.toNearestInt(), juce::Justification::centredLeft, 1, 0.8f);
                return;
            }
            g.setColour ((current ? cCyan : cText).withAlpha (a));
            g.setFont (Theme::font (13.5f, true));
            g.drawFittedText (name + (current ? "   (questo)" : ""), in.removeFromTop (18.0f).toNearestInt(), juce::Justification::centredLeft, 1, 0.8f);
            g.setColour (cDim.withAlpha (a));
            g.setFont (Theme::font (10.0f));
            g.drawFittedText (description, in.toNearestInt(), juce::Justification::topLeft, 2, 0.9f);
        }

        void mouseDown (const juce::MouseEvent&) override   { repaint(); }
        void mouseUp (const juce::MouseEvent& e) override
        {
            repaint();
            if (! enabled || e.getDistanceFromDragStart() > 8 || ! getLocalBounds().contains (e.getPosition())) return;   // era uno scorrimento
            juce::Component::SafePointer<SidePicker> safe (owner);
            const int pick = id;
            juce::MessageManager::callAsync ([safe, pick] { if (safe != nullptr) { safe->picked = pick; safe->close(); } });
        }
    };

    bool narrow() const   { return drawerBounds().getWidth() < 560; }
    juce::Rectangle<int> drawerBounds() const
    {
        const int w = juce::jmin (740, getWidth() - (getWidth() < 600 ? 40 : 100));
        const float e = 1.0f - std::pow (1.0f - juce::jlimit (0.0f, 1.0f, shown), 3.0f);
        return { (int) std::round (-(float) w * (1.0f - e)), 0, w, getHeight() };
    }
    juce::Rectangle<int> railBounds() const   { return drawerBounds().removeFromLeft (narrow() ? 96 : 160); }
    juce::Rectangle<int> groupRect (int i) const
    {
        auto r = railBounds().reduced (8, 0);
        r.removeFromTop (50);
        const int n = juce::jmax (1, (int) groups.size());
        const int h = juce::jmin (42, (getHeight() - 60) / n - 2);
        return r.withY (r.getY() + i * (h + 2)).withHeight (h);
    }

    void showGroup (int g)
    {
        group = g;
        tiles.clear();
        if (g >= 0 && g < (int) groups.size())
            for (auto& e : groups[(size_t) g].entries)
            {
                auto* t = tiles.add (new Tile());
                t->owner = this; t->name = e.name; t->description = e.description; t->id = e.id; t->current = e.current;
                content.addAndMakeVisible (t);
            }
        if (extraAction.isNotEmpty())
        {
            auto* t = tiles.add (new Tile());
            t->owner = this; t->name = extraAction; t->id = extraId; t->enabled = extraEnabled; t->plain = true;
            content.addAndMakeVisible (t);
        }
        layoutContent();
        viewport.setViewPosition (0, 0);
    }

    void layoutContent()
    {
        const int w = viewport.getWidth() - 8;
        if (w <= 0) return;
        const int cols = w >= 420 ? 2 : 1, gap = 6, h = 56;
        const int cw = (w - gap * (cols - 1)) / cols;
        int y = 0;
        for (int i = 0; i < tiles.size(); ++i)
        {
            auto* t = tiles[i];
            if (t->plain)
            {
                if (i % cols != 0) y += h + gap;
                y += 6;
                t->setBounds (0, y, cols == 2 ? cw : w, 38);
                y += 38 + gap;
                continue;
            }
            t->setBounds ((i % cols) * (cw + gap), y, cw, h);
            if (i % cols == cols - 1 || i == tiles.size() - 1) y += h + gap;
        }
        content.setSize (w, juce::jmax (1, y));
    }

    void timerCallback() override
    {
        const float step = 1.0f / (0.18f * 60.0f);
        shown = juce::jlimit (0.0f, 1.0f, shown + (closing ? -step : step));
        resized();
        repaint();
        if (! closing && shown >= 1.0f) stopTimer();
        if (closing && shown <= 0.0f)
        {
            stopTimer();
            auto done = onClosed;
            auto pick = onPick;
            const int id = picked;
            juce::MessageManager::callAsync ([done, pick, id]
            {
                if (done) done();
                if (id != 0 && pick) pick (id);
            });
        }
    }

    int group = 0, picked = 0;
    float shown = 0.0f;
    bool closing = false;
    juce::Viewport viewport;
    juce::Component content;
    juce::OwnedArray<Tile> tiles;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (SidePicker)
};
