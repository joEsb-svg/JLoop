#pragma once

#include <JuceHeader.h>
#include <functional>
#include <vector>
#include "../Engine/SequencerEngine.h"
#include "Theme.h"
#include "ChannelEditor.h"     // LabeledKnob
#include "AutomationView.h"    // AutomationText, AutomationTargetPicker

//==============================================================================
/** La finestra di SEGUI-VOLUME (effetto del mixer, tipo Peak Controller).

    In alto: quale traccia ascoltare e il suo volume nel tempo (la linea azzurra).
    In mezzo: i controlli che si muovono col volume (fino a 4). Per ognuno "Da" e' il valore di partenza
    (col silenzio) e "Quanto" di quanto si sposta quando il suono e' forte (a sinistra = scende).
    Scorciatoia: "Abbassa questa traccia" = sidechain sul volume della traccia dove sta l'effetto.
*/
class FollowView : public juce::Component,
                   private juce::Timer
{
public:
    FollowView (SequencerEngine& e, int trackIndex, int slotIndex)
        : engine (e), track (trackIndex), slot (slotIndex), picker (e)
    {
        using namespace mixer::followParam;
        const int ids[] { amount, attack, release, gain };
        const char* names[] { "Quanto", "Attacco", "Ritorno", "Sensibilita'" };
        for (int i = 0; i < 4; ++i)
        {
            const int idx = ids[i];
            auto* k = knobs.add (new LabeledKnob (names[i], 0.0, 1.0, mixer::toNormalised (def (idx), def (idx).defaultValue), [idx] (double n)
            {
                return mixer::formatValue (def (idx), mixer::fromNormalised (def (idx), (float) n));
            }));
            k->onChange = [this, idx] (double n) { set (idx, mixer::fromNormalised (def (idx), (float) n)); };
            k->onRelease = [this] { changed(); };
            k->setColour (Theme::accent);
            addAndMakeVisible (k);
        }
        for (int i = 0; i < maxTargets; ++i)
        {
            auto* r = rows.add (new Row (*this, i));
            rowsContent.addChildComponent (r);
        }
        sourceButton.onClick = [this] { showSourceMenu(); };
        addButton.onClick = [this]
        {
            picker.setBounds (getLocalBounds());
            picker.open();
        };
        duckButton.onClick = [this] { addDuck(); };
        closeButton.onClick = [this] { if (onClose) onClose(); };
        for (auto* b : { &sourceButton, &addButton, &duckButton, &closeButton })
            addAndMakeVisible (b);

        picker.title = "Cosa vuoi far muovere col volume?";
        picker.includeSong = false;
        picker.onChoose = [this] (automation::Target t) { addTarget (t, 0.5f); };
        addChildComponent (picker);

        rowsView.setViewedComponent (&rowsContent, false);
        rowsView.setScrollBarsShown (true, false);
        rowsView.setScrollBarThickness (8);
        rowsView.setScrollOnDragMode (juce::Viewport::ScrollOnDragMode::never);   // le barrette si trascinano col dito
        addAndMakeVisible (rowsView);
        meter.owner = this;
        addAndMakeVisible (meter);
        setOpaque (true);
        refresh();
        startTimerHz (30);
    }

    std::function<void()> onChanged, onClose;

    /** Aggiunge un controllo da muovere (parte dal valore che ha adesso). */
    void addTarget (const automation::Target& t, float depth)
    {
        using namespace mixer::followParam;
        int free = -1;
        for (int i = 0; i < maxTargets; ++i)
        {
            if (targetOf (i) == t) { free = i; break; }   // gia' c'e': lo si aggiorna
            if (free < 0 && targetOf (i).kind == automation::none) free = i;
        }
        if (free < 0) return;
        set (target (free, tA), (float) t.a);
        set (target (free, tB), (float) t.b);
        set (target (free, tC), (float) t.c);
        set (target (free, tBase), AutomationText::current (engine, t));
        set (target (free, tDepth), depth);
        set (target (free, tKind), (float) t.kind);
        refresh();
        changed();
    }

    /** Sidechain: il volume di questa traccia scende quando suona quella ascoltata. */
    void addDuck()
    {
        const automation::Target t = track == 0 ? automation::Target { automation::master, 0, 0, 0 }
                                                : automation::Target { automation::trackParam, track, automation::trVolume, 0 };
        addTarget (t, -0.85f);
    }

    /** Per le foto di prova: un livello finto che pulsa. */
    void showTestLevel()   { testLevel = true; }

    //==========================================================================
    void paint (juce::Graphics& g) override
    {
        g.fillAll (Theme::panel);
        g.setColour (Theme::accent);
        g.fillRoundedRectangle (titleArea.toFloat().removeFromLeft (4.0f).reduced (0.0f, 5.0f), 2.0f);
        g.setColour (Theme::textBright);
        g.setFont (Theme::font (17.0f, true).withExtraKerningFactor (0.08f));
        g.drawText ("SEGUI-VOLUME", titleArea.withTrimmedLeft (12), juce::Justification::centredLeft, false);

        g.setColour (Theme::text);
        g.setFont (Theme::font (13.0f, true));
        g.drawText ("Ascolta", listenLabel, juce::Justification::centredLeft, false);
        if (! emptyArea.isEmpty())
        {
            g.setColour (Theme::textDim);
            g.setFont (Theme::font (12.5f));
            g.drawFittedText ("Ancora niente da muovere. Tocca \"Abbassa questa traccia\" per il sidechain, oppure \"+ Muovi un controllo\" "
                              "per scegliere una manopola qualsiasi (filtro, distorsione, volume di un altro canale...).",
                              emptyArea.reduced (6), juce::Justification::centred, 3, 0.9f);
        }
    }

    void resized() override
    {
        auto r = getLocalBounds().reduced (8, 6);
        auto head = r.removeFromTop (32);
        closeButton.setBounds (head.removeFromRight (90).reduced (2));
        titleArea = head;
        r.removeFromTop (6);
        const bool wide = r.getWidth() > r.getHeight() * 1.2f;

        auto top = r.removeFromTop (34);
        listenLabel = top.removeFromLeft (64);
        sourceButton.setBounds (top.removeFromLeft (juce::jmin (260, top.getWidth())).reduced (0, 2));
        r.removeFromTop (6);

        juce::Rectangle<int> left = r, right;
        if (wide)
        {
            right = r.removeFromRight (juce::jmax (250, r.getWidth() * 36 / 100));
            r.removeFromRight (10);
            left = r;
            // a destra: i due pulsanti sopra le manopole
            auto b = right.removeFromTop (34);
            duckButton.setBounds (b.removeFromLeft (b.getWidth() / 2).reduced (0, 2).withTrimmedRight (3));
            addButton.setBounds (b.reduced (0, 2).withTrimmedLeft (3));
            right.removeFromTop (6);
            meter.setBounds (left.removeFromTop (juce::jlimit (40, 90, left.getHeight() / 4)));
        }
        else
        {
            meter.setBounds (left.removeFromTop (juce::jlimit (50, 110, left.getHeight() / 5)));
            left.removeFromTop (8);
            auto b = left.removeFromTop (34);
            duckButton.setBounds (b.removeFromLeft (b.getWidth() / 2).reduced (0, 2).withTrimmedRight (3));
            addButton.setBounds (b.reduced (0, 2).withTrimmedLeft (3));
        }
        left.removeFromTop (6);

        const int rowH = 50;
        int used = 0;
        for (auto* row : rows)
        {
            row->setVisible (targetOf (row->index).kind != automation::none);
            if (row->isVisible())
                row->setBounds (0, used++ * rowH, juce::jmax (100, left.getWidth() - 8), rowH - 4);
        }
        juce::Rectangle<int> rowsArea;
        if (wide) rowsArea = left;
        else      rowsArea = left.removeFromTop (juce::jmin (left.getHeight() - 100, used * rowH));
        rowsView.setBounds (rowsArea);
        rowsContent.setSize (juce::jmax (100, rowsArea.getWidth() - 8), juce::jmax (1, used * rowH));
        rowsView.setVisible (used > 0);
        emptyArea = used == 0 ? (wide ? left.removeFromTop (juce::jmin (90, left.getHeight())) : left.removeFromTop (90)) : juce::Rectangle<int>();
        if (! wide) left.removeFromTop (6);

        auto knobArea = wide ? right : left;
        if (wide)
        {
            const int kw = knobArea.getWidth() / 2, kh = juce::jmin (100, knobArea.getHeight() / 2);
            for (int i = 0; i < knobs.size(); ++i)
                knobs[i]->setBounds (knobArea.getX() + (i % 2) * kw, knobArea.getY() + (i / 2) * kh, kw, kh);
        }
        else
        {
            const int kw = knobArea.getWidth() / 4;
            auto row = knobArea.removeFromTop (juce::jmin (knobArea.getHeight(), 96));
            for (auto* k : knobs) k->setBounds (row.removeFromLeft (kw).reduced (2, 0));
        }
        if (picker.isVisible()) picker.setBounds (getLocalBounds());
    }

private:
    static constexpr int maxTargets = mixer::followParam::maxTargets;

    struct Chip : public juce::Button
    {
        explicit Chip (const juce::String& t) : juce::Button (t) {}
        bool accent = false;
        void paintButton (juce::Graphics& g, bool over, bool down) override
        {
            auto r = getLocalBounds().toFloat().reduced (0.5f);
            g.setColour (down ? Theme::raisedHi : over ? Theme::raised.brighter (0.05f) : Theme::raised);
            g.fillRoundedRectangle (r, 6.0f);
            g.setColour (accent ? Theme::cyan.withAlpha (0.7f) : Theme::edgeLight.withAlpha (0.5f));
            g.drawRoundedRectangle (r, 6.0f, accent ? 1.5f : 1.0f);
            g.setColour (accent ? Theme::cyan : Theme::text);
            g.setFont (Theme::font (12.5f, true));
            g.drawFittedText (getButtonText(), getLocalBounds().reduced (6, 0), juce::Justification::centred, 1, 0.7f);
        }
    };

    /** Barretta orizzontale da trascinare (0..1 o -1..1). */
    struct Bar : public juce::Component
    {
        juce::String label;
        bool bipolar = false;
        float value = 0.0f, startValue = 0.0f;
        std::function<juce::String (float)> text;
        std::function<void (float)> onMove;
        std::function<void()> onDone;

        void paint (juce::Graphics& g) override
        {
            auto r = getLocalBounds().toFloat().reduced (0.5f);
            g.setColour (Theme::lcdBg);
            g.fillRoundedRectangle (r, 5.0f);
            const float lo = bipolar ? -1.0f : 0.0f;
            const float x0 = r.getX() + r.getWidth() * (bipolar ? 0.5f : 0.0f);
            const float x1 = r.getX() + r.getWidth() * (value - lo) / (1.0f - lo);
            g.setColour ((bipolar && value < 0.0f ? Theme::cyan : Theme::accent).withAlpha (0.45f));
            g.fillRect (juce::Rectangle<float>::leftTopRightBottom (juce::jmin (x0, x1), r.getY() + 1.0f, juce::jmax (x0, x1), r.getBottom() - 1.0f));
            if (bipolar) { g.setColour (Theme::textDim.withAlpha (0.5f)); g.drawVerticalLine ((int) x0, r.getY(), r.getBottom()); }
            g.setColour (Theme::edgeLight.withAlpha (0.4f));
            g.drawRoundedRectangle (r, 5.0f, 1.0f);
            g.setColour (Theme::text);
            g.setFont (Theme::font (11.0f, true));
            g.drawText (label, r.reduced (7.0f, 0.0f), juce::Justification::centredLeft, false);
            g.setColour (Theme::textBright);
            g.drawText (text ? text (value) : juce::String (value, 2), r.reduced (7.0f, 0.0f), juce::Justification::centredRight, false);
        }
        void mouseDown (const juce::MouseEvent&) override   { startValue = value; }
        void mouseDrag (const juce::MouseEvent& e) override
        {
            const float range = bipolar ? 2.0f : 1.0f;
            value = juce::jlimit (bipolar ? -1.0f : 0.0f, 1.0f, startValue + range * (float) e.getDistanceFromDragStartX() / (float) juce::jmax (40, getWidth()));
            if (onMove) onMove (value);
            repaint();
        }
        void mouseUp (const juce::MouseEvent&) override       { if (onDone) onDone(); }
        void mouseDoubleClick (const juce::MouseEvent&) override
        {
            value = bipolar ? 0.0f : value;
            if (onMove) onMove (value);
            if (onDone) onDone();
            repaint();
        }
    };

    /** Un controllo che si muove: nome, "Da", "Quanto", togli. */
    struct Row : public juce::Component
    {
        Row (FollowView& o, int i) : owner (o), index (i)
        {
            using namespace mixer::followParam;
            from.label = "Da";
            from.text = [this] (float v) { return AutomationText::value (owner.engine, owner.targetOf (index), v); };
            from.onMove = [this] (float v) { owner.set (target (index, tBase), v); };
            from.onDone = [this] { owner.changed(); };
            depth.label = "Quanto";
            depth.bipolar = true;
            depth.text = [] (float v) { const int pc = juce::roundToInt (v * 100.0f); return (pc > 0 ? "+" : "") + juce::String (pc) + "%"; };
            depth.onMove = [this] (float v) { owner.set (target (index, tDepth), v); };
            depth.onDone = [this] { owner.changed(); };
            removeButton.onClick = [this] { juce::MessageManager::callAsync ([safe = juce::Component::SafePointer<FollowView> (&owner), i = index] { if (safe != nullptr) safe->removeTarget (i); }); };
            for (auto* c : std::initializer_list<juce::Component*> { &from, &depth, &removeButton }) addAndMakeVisible (c);
        }

        void update()
        {
            using namespace mixer::followParam;
            if (! from.isMouseButtonDown())  from.value = owner.get (target (index, tBase));
            if (! depth.isMouseButtonDown()) depth.value = owner.get (target (index, tDepth));
            repaint();
            from.repaint();
            depth.repaint();
        }

        void paint (juce::Graphics& g) override
        {
            auto r = getLocalBounds().toFloat();
            g.setColour (Theme::raised);
            g.fillRoundedRectangle (r, 7.0f);
            const auto t = owner.targetOf (index);
            g.setColour (AutomationText::colour (owner.engine, t));
            g.fillRoundedRectangle (r.removeFromLeft (4.0f).reduced (0.0f, 6.0f), 2.0f);
            auto txt = nameArea.toFloat();
            g.setColour (Theme::textBright);
            g.setFont (Theme::font (12.5f, true));
            g.drawFittedText (AutomationText::parameter (owner.engine, t), txt.removeFromTop (txt.getHeight() * 0.55f).toNearestInt(), juce::Justification::bottomLeft, 1, 0.7f);
            g.setColour (Theme::textDim);
            g.setFont (Theme::font (10.5f));
            g.drawFittedText (AutomationText::source (owner.engine, t), txt.toNearestInt(), juce::Justification::topLeft, 1, 0.7f);
        }

        void resized() override
        {
            auto r = getLocalBounds().reduced (10, 6);
            removeButton.setBounds (r.removeFromRight (34));
            r.removeFromRight (6);
            nameArea = r.removeFromLeft (juce::jmax (90, r.getWidth() * 34 / 100));
            r.removeFromLeft (6);
            const int w = (r.getWidth() - 6) / 2;
            from.setBounds (r.removeFromLeft (w));
            r.removeFromLeft (6);
            depth.setBounds (r);
        }

        FollowView& owner;
        const int index;
        Bar from, depth;
        Chip removeButton { juce::String (juce::CharPointer_UTF8 ("\xc3\x97")) };
        juce::Rectangle<int> nameArea;
    };

    /** Il volume ascoltato negli ultimi secondi (azzurro) e quanto si muovono i controlli. */
    struct Meter : public juce::Component
    {
        FollowView* owner = nullptr;
        std::vector<float> history = std::vector<float> (120, 0.0f);
        void push (float v)   { history.erase (history.begin()); history.push_back (v); repaint(); }
        void paint (juce::Graphics& g) override
        {
            const auto r = getLocalBounds().toFloat();
            Theme::drawSunken (g, r, Theme::lcdBg, 8.0f);
            const auto in = r.reduced (6.0f, 8.0f);
            juce::Path p;
            for (size_t i = 0; i < history.size(); ++i)
            {
                const float x = in.getX() + in.getWidth() * (float) i / (float) (history.size() - 1);
                const float y = in.getBottom() - in.getHeight() * juce::jlimit (0.0f, 1.0f, history[i]);
                if (i == 0) p.startNewSubPath (x, y); else p.lineTo (x, y);
            }
            juce::Path fill (p);
            fill.lineTo (in.getRight(), in.getBottom()); fill.lineTo (in.getX(), in.getBottom()); fill.closeSubPath();
            g.setColour (Theme::cyan.withAlpha (0.12f));
            g.fillPath (fill);
            g.setColour (Theme::cyan);
            g.strokePath (p, juce::PathStrokeType (2.0f));
            g.setColour (Theme::textDim);
            g.setFont (Theme::font (9.5f));
            g.drawText ("volume ascoltato", r.reduced (8.0f, 4.0f), juce::Justification::topLeft, false);
        }
    };

    //==========================================================================
    static const mixer::ParamDef& def (int p)   { return mixer::info (mixer::follow).params[(size_t) p]; }
    mixer::Slot& theSlot() const                { return engine.getTrack (track).slots[(size_t) slot]; }
    float get (int p) const                     { return theSlot().params[(size_t) p].load(); }
    void set (int p, float v)                   { theSlot().params[(size_t) p].store (juce::jlimit (def (p).minValue, def (p).maxValue, v)); }
    void changed()                              { if (onChanged) onChanged(); }

    automation::Target targetOf (int i) const
    {
        using namespace mixer::followParam;
        return { juce::roundToInt (get (target (i, tKind))), juce::roundToInt (get (target (i, tA))),
                 juce::roundToInt (get (target (i, tB))), juce::roundToInt (get (target (i, tC))) };
    }

    void removeTarget (int i)
    {
        using namespace mixer::followParam;
        const auto t = targetOf (i);
        if (t.kind != automation::none)
            engine.applyTargetValue (t.kind, t.a, t.b, t.c, get (target (i, tBase)));   // torna dov'era
        set (target (i, tKind), 0.0f);
        refresh();
        changed();
    }

    juce::String sourceName (int src) const
    {
        if (src <= 0 || src == track) return track == 0 ? juce::String ("Questa traccia (Master)") : "Questa traccia (" + engine.getTrackName (track) + ")";
        return "Mixer " + juce::String (src) + ": " + engine.getTrackName (src);
    }

    void showSourceMenu()
    {
        juce::PopupMenu menu;
        const int current = juce::roundToInt (get (mixer::followParam::source));
        menu.addItem (1000, sourceName (0), true, current <= 0);
        for (int t = 1; t < mixer::numTracks; ++t)
            if (t != track && engine.isTrackUsed (t))
                menu.addItem (t, "Mixer " + juce::String (t) + ": " + engine.getTrackName (t), true, current == t);
        juce::Component::SafePointer<FollowView> safe (this);
        menu.showMenuAsync (juce::PopupMenu::Options().withTargetComponent (&sourceButton), [safe] (int r)
        {
            if (safe == nullptr || r <= 0) return;
            safe->set (mixer::followParam::source, r == 1000 ? 0.0f : (float) r);
            safe->refresh();
            safe->changed();
        });
    }

    void refresh()
    {
        using namespace mixer::followParam;
        const int ids[] { amount, attack, release, gain };
        for (int i = 0; i < 4; ++i)
            if (! knobs[i]->slider.isMouseButtonDown())
                knobs[i]->setValue (mixer::toNormalised (def (ids[i]), get (ids[i])));
        sourceButton.setButtonText (sourceName (juce::roundToInt (get (source))) + juce::String (juce::CharPointer_UTF8 ("  \xe2\x96\xbe")));
        int count = 0;
        for (auto* r : rows) { r->update(); count += targetOf (r->index).kind != automation::none ? 1 : 0; }
        addButton.setEnabled (count < maxTargets);
        resized();
        repaint();
    }

    void timerCallback() override
    {
        auto& sl = theSlot();
        if (sl.type.load() != mixer::follow) return;
        float lvl = 0.0f;
        if (testLevel)
        {
            testT += 1.0f / 30.0f;
            const float ph = std::fmod (testT * 2.0f, 1.0f);
            lvl = std::exp (-ph * 7.0f) * 0.9f;
        }
        else if (auto* fx = dynamic_cast<mixer::Follow*> (sl.owned.get()))
            lvl = fx->shownLevel.load();
        meter.push (lvl);

        bool different = false;
        for (int p = 0; p < mixer::info (mixer::follow).numParams; ++p)
            if (const float v = get (p); v != seen[(size_t) p]) { seen[(size_t) p] = v; different = true; }
        bool busy = false;
        for (auto* k : knobs) busy = busy || k->slider.isMouseButtonDown();
        for (auto* r : rows) busy = busy || r->from.isMouseButtonDown() || r->depth.isMouseButtonDown();
        if (different && ! busy) refresh();
    }

    SequencerEngine& engine;
    const int track, slot;
    bool testLevel = false;
    float testT = 0.0f;
    Meter meter;
    juce::Viewport rowsView;
    juce::Component rowsContent;
    juce::OwnedArray<LabeledKnob> knobs;
    juce::OwnedArray<Row> rows;
    Chip sourceButton { "Questa traccia" }, addButton { "+ Muovi un controllo" }, duckButton { "Abbassa questa traccia" }, closeButton { "Effetti" };
    AutomationTargetPicker picker;
    juce::Rectangle<int> titleArea, listenLabel, emptyArea;
    std::array<float, mixer::maxParams> seen {};

    friend struct Row;
};
