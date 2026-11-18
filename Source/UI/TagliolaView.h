#pragma once

#include <JuceHeader.h>
#include <functional>
#include <vector>
#include "../Engine/SequencerEngine.h"
#include "Theme.h"
#include "ChannelEditor.h"   // LabeledKnob

//==============================================================================
/** La finestra della TAGLIOLA (effetto del mixer: volume, filtro e pan disegnati a tempo).

    A sinistra si sceglie la curva (Volume, Filtro, Pan); al centro la si disegna: in orizzontale il
    giro (lungo "Durata"), in verticale quanto passa. Tocca per aggiungere un punto, trascinalo,
    doppio tocco per toglierlo; il pallino azzurro a meta' di un tratto lo piega. Due punti sulla stessa
    riga verticale fanno un taglio netto. I punti si agganciano alla griglia (si puo' togliere).
    Disegnare su una curva spenta la accende.
*/
class TagliolaView : public juce::Component,
                     private juce::Timer
{
public:
    struct Pt { float x, y, b; };

    TagliolaView (SequencerEngine& e, int trackIndex, int slotIndex)
        : engine (e), track (trackIndex), slot (slotIndex)
    {
        using namespace mixer::tagliolaParam;
        const int ids[] { amount, smooth, resonance, mix };
        const char* names[] { "Quanto", "Morbido", "Risonanza", "Mix" };
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
        for (int i = 0; i < numPresets; ++i)
        {
            auto* b = presetButtons.add (new Chip (presetName (i)));
            b->onClick = [this, i] { applyPreset (i); };
            addAndMakeVisible (b);
        }
        for (int l = 0; l < numLanes; ++l)
        {
            auto* t = laneTabs.add (new LaneTab (laneName (l), laneColour (l)));
            t->onClick = [this, l] { lane = l; refresh(); };
            addAndMakeVisible (t);
        }
        onButton.onClick = [this]
        {
            set (laneOn (lane), laneIsOn (lane) ? 0.0f : 1.0f);
            presetShown = -1;
            refresh();
            changed();
        };
        lengthButton.onClick = [this] { cycle (length, 5); };
        gridButton.onClick = [this] { cycle (grid, 4); };
        typeButton.onClick = [this] { cycle (filterType, 2); };
        presetMenuButton.onClick = [this] { showPresetMenu(); };
        closeButton.onClick = [this] { if (onClose) onClose(); };
        for (auto* b : { &onButton, &lengthButton, &gridButton, &typeButton, &presetMenuButton, &closeButton })
            addAndMakeVisible (b);

        editor.owner = this;
        addAndMakeVisible (editor);
        setOpaque (true);
        refresh();
        startTimerHz (30);
    }

    std::function<void()> onChanged, onClose;

    static constexpr int numPresets = 8;
    static const char* presetName (int i)
    {
        static const char* names[] { "Pompa", "Gate 1/16", "Stutter", "Trance gate", "Wobble", "Sale", "Ping-pong", "Dritta" };
        return names[juce::jlimit (0, numPresets - 1, i)];
    }

    /** Per le foto di prova: quale curva si vede e dove sta "suonando". */
    void selectLane (int l)          { lane = juce::jlimit (0, (int) mixer::tagliolaParam::numLanes - 1, l); refresh(); }
    void showTestPlayhead (float ph) { testPhase = ph; editor.repaint(); }

    /** Mette una forma di partenza su tutte e tre le curve (e la durata giusta). */
    void applyPreset (int i)
    {
        using namespace mixer::tagliolaParam;
        const std::vector<Pt> flatVol { { 0.0f, 1.0f, 0.0f }, { 1.0f, 1.0f, 0.0f } };
        const std::vector<Pt> flatCut { { 0.0f, 1.0f, 0.0f }, { 1.0f, 1.0f, 0.0f } };
        const std::vector<Pt> flatPan { { 0.0f, 0.5f, 0.0f }, { 1.0f, 0.5f, 0.0f } };
        auto lanes = [this] (std::vector<Pt> v, bool vOn, std::vector<Pt> c, bool cOn, std::vector<Pt> p, bool pOn)
        {
            writePoints (volume, v); set (laneOn (volume), vOn ? 1.0f : 0.0f);
            writePoints (cutoff, c); set (laneOn (cutoff), cOn ? 1.0f : 0.0f);
            writePoints (pan, p);    set (laneOn (pan), pOn ? 1.0f : 0.0f);
        };
        switch (i)
        {
            case 0:  set (length, 0.0f); lanes ({ { 0.0f, 0.0f, -0.6f }, { 0.55f, 1.0f, 0.0f }, { 1.0f, 1.0f, 0.0f } }, true, flatCut, false, flatPan, false); lane = volume; break;
            case 1:  set (length, 1.0f); lanes (steps ("10101010"), true, flatCut, false, flatPan, false); lane = volume; break;
            case 2:  set (length, 0.0f); lanes (steps ("10101010"), true, flatCut, false, flatPan, false); lane = volume; break;
            case 3:  set (length, 1.0f); lanes (steps ("11011010"), true, flatCut, false, flatPan, false); lane = volume; break;
            case 4:  set (length, 0.0f); set (resonance, 0.65f); set (filterType, 0.0f);
                     lanes (flatVol, false, { { 0.0f, 0.12f, -0.4f }, { 0.25f, 0.85f, 0.4f }, { 0.5f, 0.12f, -0.4f }, { 0.75f, 0.85f, 0.4f }, { 1.0f, 0.12f, 0.0f } }, true, flatPan, false);
                     lane = cutoff; break;
            case 5:  set (length, 4.0f); set (resonance, 0.45f); set (filterType, 0.0f);
                     lanes (flatVol, false, { { 0.0f, 0.1f, 0.5f }, { 1.0f, 1.0f, 0.0f } }, true, flatPan, false); lane = cutoff; break;
            case 6:  set (length, 1.0f); lanes (flatVol, false, flatCut, false, { { 0.0f, 0.1f, 0.0f }, { 0.5f, 0.9f, 0.0f }, { 1.0f, 0.1f, 0.0f } }, true); lane = pan; break;
            default: lanes (flatVol, true, flatCut, false, flatPan, false); lane = volume; break;
        }
        presetShown = i;
        refresh();
        changed();
    }

    //==========================================================================
    void paint (juce::Graphics& g) override
    {
        g.fillAll (Theme::panel);
        g.setColour (Theme::accent);
        g.fillRoundedRectangle (titleArea.toFloat().removeFromLeft (4.0f).reduced (0.0f, 5.0f), 2.0f);
        g.setColour (Theme::textBright);
        g.setFont (Theme::font (17.0f, true).withExtraKerningFactor (0.08f));
        g.drawText ("TAGLIOLA", titleArea.withTrimmedLeft (12), juce::Justification::centredLeft, false);
        if (! hintArea.isEmpty())
            Theme::drawCaption (g, "Tocca per aggiungere un punto, trascinalo, doppio tocco lo toglie. Due punti sulla stessa riga = taglio netto",
                                hintArea.toFloat(), juce::Justification::centredLeft);
    }

    void resized() override
    {
        auto r = getLocalBounds().reduced (8, 6);
        auto head = r.removeFromTop (32);
        closeButton.setBounds (head.removeFromRight (90).reduced (2));
        presetMenuButton.setBounds (head.removeFromRight (90).reduced (2));
        titleArea = head;
        r.removeFromTop (4);
        const bool wide = r.getWidth() > r.getHeight() * 1.2f;

        auto layoutControls = [this] (juce::Rectangle<int> c)
        {
            const int chipH = 30, minChipW = 78;
            const int perRow = juce::jmax (3, c.getWidth() / minChipW);
            std::vector<juce::Component*> all { &onButton, &lengthButton, &gridButton, &typeButton };
            for (auto* b : presetButtons) all.push_back (b);
            const int rowsNeeded = ((int) all.size() + perRow - 1) / perRow;
            auto chips = c.removeFromTop (rowsNeeded * (chipH + 4));
            const int cw = chips.getWidth() / perRow;
            for (size_t i = 0; i < all.size(); ++i)
                all[i]->setBounds (chips.getX() + (int) (i % (size_t) perRow) * cw, chips.getY() + (int) (i / (size_t) perRow) * (chipH + 4), cw - 4, chipH);
            c.removeFromTop (4);
            const int kw = c.getWidth() / 4;
            auto row = c.removeFromTop (juce::jmin (c.getHeight(), 96));
            for (auto* k : knobs) k->setBounds (row.removeFromLeft (kw).reduced (2, 0));
        };

        if (wide)
        {
            hintArea = {};
            auto tabs = r.removeFromLeft (juce::jmin (96, r.getWidth() / 8));
            const int th = juce::jmin (46, tabs.getHeight() / 3);
            for (auto* t : laneTabs) { t->setBounds (tabs.removeFromTop (th).reduced (0, 2)); }
            r.removeFromLeft (6);
            auto controls = r.removeFromRight (juce::jmax (260, r.getWidth() * 40 / 100));
            r.removeFromRight (8);
            editor.setBounds (r);
            layoutControls (controls);
        }
        else
        {
            hintArea = r.removeFromTop (14);
            r.removeFromTop (2);
            auto tabs = r.removeFromTop (36);
            const int tw = tabs.getWidth() / 3;
            for (auto* t : laneTabs) t->setBounds (tabs.removeFromLeft (tw).reduced (2, 0));
            r.removeFromTop (6);
            editor.setBounds (r.removeFromTop (juce::jmin (r.getWidth(), r.getHeight() * 55 / 100)));
            r.removeFromTop (8);
            layoutControls (r);
        }
    }

private:
    //==========================================================================
    struct Chip : public juce::Button
    {
        explicit Chip (const juce::String& t) : juce::Button (t) {}
        bool on = false;
        void paintButton (juce::Graphics& g, bool over, bool down) override
        {
            auto r = getLocalBounds().toFloat().reduced (0.5f);
            g.setColour (on ? juce::Colour (0xff2a1a0c) : down ? Theme::raisedHi : over ? Theme::raised.brighter (0.05f) : Theme::raised);
            g.fillRoundedRectangle (r, 6.0f);
            g.setColour (on ? Theme::accent : Theme::edgeLight.withAlpha (0.5f));
            g.drawRoundedRectangle (r, 6.0f, on ? 1.5f : 1.0f);
            g.setColour (on ? Theme::accent : Theme::text);
            g.setFont (Theme::font (12.0f, true));
            g.drawFittedText (getButtonText(), getLocalBounds().reduced (3, 0), juce::Justification::centred, 1, 0.7f);
        }
    };

    /** La linguetta di una curva: il nome e una lucina (accesa = la curva lavora). */
    struct LaneTab : public juce::Button
    {
        LaneTab (const juce::String& t, juce::Colour c) : juce::Button (t), colour (c) {}
        juce::Colour colour;
        bool selected = false, active = false;
        void paintButton (juce::Graphics& g, bool over, bool down) override
        {
            auto r = getLocalBounds().toFloat().reduced (0.5f);
            g.setColour (selected ? juce::Colour (0xff0d2229) : down ? Theme::raisedHi : over ? Theme::raised.brighter (0.05f) : Theme::raised);
            g.fillRoundedRectangle (r, 7.0f);
            g.setColour (selected ? Theme::cyan.withAlpha (0.6f) : Theme::edgeLight.withAlpha (0.4f));
            g.drawRoundedRectangle (r, 7.0f, selected ? 1.5f : 1.0f);
            auto in = r.reduced (9.0f, 0.0f);
            const auto dot = in.removeFromLeft (9.0f).withSizeKeepingCentre (9.0f, 9.0f);
            g.setColour (active ? colour : Theme::textDim.withAlpha (0.35f));
            g.fillEllipse (dot);
            in.removeFromLeft (6.0f);
            g.setColour (selected ? Theme::cyan : Theme::text);
            g.setFont (Theme::font (13.0f, true));
            g.drawFittedText (getButtonText(), in.toNearestInt(), juce::Justification::centredLeft, 1, 0.7f);
        }
    };

    //==========================================================================
    /** L'editor della curva scelta: il giro in orizzontale, quanto passa in verticale. */
    struct Editor : public juce::Component
    {
        TagliolaView* owner = nullptr;
        int dragPoint = -1, dragBend = -1;
        float startBend = 0.0f;
        bool moved = false;

        juce::Rectangle<float> plot() const   { return getLocalBounds().toFloat().reduced (8.0f, 10.0f); }
        juce::Point<float> toScreen (float x, float y) const
        {
            const auto p = plot();
            return { p.getX() + x * p.getWidth(), p.getBottom() - y * p.getHeight() };
        }
        juce::Point<float> toCurve (juce::Point<float> s) const
        {
            const auto p = plot();
            return { juce::jlimit (0.0f, 1.0f, (s.x - p.getX()) / p.getWidth()), juce::jlimit (0.0f, 1.0f, (p.getBottom() - s.y) / p.getHeight()) };
        }
        juce::Point<float> bendHandle (const std::vector<Pt>& pts, int i) const
        {
            const auto& a = pts[(size_t) i];
            const auto& b = pts[(size_t) i + 1];
            return toScreen ((a.x + b.x) * 0.5f, a.y + (b.y - a.y) * mixer::shaperParam::bendT (0.5f, a.b));
        }
        bool hasHandle (const std::vector<Pt>& pts, int i) const
        {
            const auto a = toScreen (pts[(size_t) i].x, pts[(size_t) i].y), b = toScreen (pts[(size_t) i + 1].x, pts[(size_t) i + 1].y);
            return b.x - a.x > 18.0f && std::abs (b.y - a.y) > 6.0f;
        }

        juce::Path curvePath (int lane) const
        {
            std::vector<float> table ((size_t) 513);
            owner->buildTable (lane, table.data(), (int) table.size());
            juce::Path c;
            for (int i = 0; i < (int) table.size(); ++i)
            {
                const auto s = toScreen ((float) i / (float) (table.size() - 1), table[(size_t) i]);
                if (i == 0) c.startNewSubPath (s); else c.lineTo (s);
            }
            return c;
        }

        void paint (juce::Graphics& g) override
        {
            Theme::drawSunken (g, getLocalBounds().toFloat(), Theme::lcdBg, 8.0f);
            const auto p = plot();
            using namespace mixer::tagliolaParam;

            // griglia: righe dei sedicesimi (o della griglia scelta), piu' forti sui quarti e sulle battute
            const double beats = lengthBeats[juce::jlimit (0, 4, juce::roundToInt (owner->get (length)))];
            const int perBeat = owner->gridPerBeat();
            const int lines = (int) std::round (beats * juce::jmax (4, perBeat));
            for (int i = 1; i < lines; ++i)
            {
                const float x = p.getX() + p.getWidth() * (float) i / (float) lines;
                const int perB = juce::jmax (4, perBeat);
                const bool beat = i % perB == 0, bar = i % (perB * 4) == 0;
                if (lines > 96 && ! beat) continue;
                g.setColour (bar ? juce::Colour (0xff2c4252) : beat ? juce::Colour (0xff1d3040) : juce::Colour (0xff111d26));
                g.drawVerticalLine ((int) x, p.getY(), p.getBottom());
            }
            g.setColour (juce::Colour (0xff111d26));
            for (int i = 1; i < 4; ++i) g.drawHorizontalLine ((int) (p.getY() + p.getHeight() * (float) i / 4.0f), p.getX(), p.getRight());

            g.setFont (Theme::font (9.5f));
            g.setColour (Theme::textDim.withAlpha (0.7f));
            const char* top[] { "passa tutto", "filtro aperto", "destra" };
            const char* bottom[] { "silenzio", "filtro chiuso", "sinistra" };
            g.drawText (top[owner->lane], p.reduced (4.0f, 2.0f).removeFromTop (12.0f), juce::Justification::centredLeft, false);
            g.drawText (bottom[owner->lane], p.reduced (4.0f, 2.0f).removeFromBottom (12.0f), juce::Justification::centredLeft, false);

            // le altre curve accese, sbiadite dietro
            for (int l = 0; l < numLanes; ++l)
                if (l != owner->lane && owner->laneIsOn (l))
                {
                    g.setColour (owner->laneColour (l).withAlpha (0.28f));
                    g.strokePath (curvePath (l), juce::PathStrokeType (1.5f));
                }

            const bool on = owner->laneIsOn (owner->lane);
            const auto col = owner->laneColour (owner->lane).withAlpha (on ? 1.0f : 0.4f);
            auto c = curvePath (owner->lane);
            {
                juce::Path fill (c);
                fill.lineTo (p.getRight(), p.getBottom());
                fill.lineTo (p.getX(), p.getBottom());
                fill.closeSubPath();
                g.setColour (col.withAlpha (on ? 0.12f : 0.05f));
                g.fillPath (fill);
            }
            g.setColour (col.withAlpha (on ? 0.25f : 0.1f));
            g.strokePath (c, juce::PathStrokeType (7.0f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
            g.setColour (col);
            g.strokePath (c, juce::PathStrokeType (2.2f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));

            const auto pts = owner->points (owner->lane);
            for (int i = 0; i + 1 < (int) pts.size(); ++i)
                if (hasHandle (pts, i))
                {
                    g.setColour (Theme::cyan.withAlpha (dragBend == i ? 1.0f : 0.85f));
                    g.fillEllipse (juce::Rectangle<float> (dragBend == i ? 11.0f : 8.0f, dragBend == i ? 11.0f : 8.0f).withCentre (bendHandle (pts, i)));
                }
            for (int i = 0; i < (int) pts.size(); ++i)
            {
                const auto s = toScreen (pts[(size_t) i].x, pts[(size_t) i].y);
                const float rad = dragPoint == i ? 8.0f : 6.0f;
                g.setColour (dragPoint == i ? juce::Colours::white : Theme::lcdBg);
                g.fillEllipse (juce::Rectangle<float> (rad * 2.0f, rad * 2.0f).withCentre (s));
                g.setColour (col);
                g.drawEllipse (juce::Rectangle<float> (rad * 2.0f, rad * 2.0f).withCentre (s), 2.0f);
            }

            if (! on)
            {
                g.setColour (Theme::textDim);
                g.setFont (Theme::font (12.0f, true));
                g.drawText ("curva spenta: disegna per accenderla", p.withTrimmedTop (p.getHeight() * 0.4f).removeFromTop (20.0f), juce::Justification::centred, false);
            }

            // dove sta suonando
            if (owner->playhead >= 0.0f)
            {
                const float x = p.getX() + p.getWidth() * owner->playhead;
                g.setColour (Theme::cyan.withAlpha (0.9f));
                g.fillRect (x - 1.0f, p.getY(), 2.0f, p.getHeight());
            }
        }

        int hitPoint (juce::Point<float> s, const std::vector<Pt>& pts) const
        {
            int best = -1;
            float bestD = 16.0f;
            for (int i = 0; i < (int) pts.size(); ++i)
            {
                const float d = toScreen (pts[(size_t) i].x, pts[(size_t) i].y).getDistanceFrom (s);
                if (d < bestD) { bestD = d; best = i; }
            }
            return best;
        }

        int hitBend (juce::Point<float> s, const std::vector<Pt>& pts) const
        {
            for (int i = 0; i + 1 < (int) pts.size(); ++i)
                if (hasHandle (pts, i) && bendHandle (pts, i).getDistanceFrom (s) < 13.0f) return i;
            return -1;
        }

        void mouseDown (const juce::MouseEvent& e) override
        {
            auto pts = owner->points (owner->lane);
            moved = false;
            dragPoint = hitPoint (e.position, pts);
            dragBend = dragPoint < 0 ? hitBend (e.position, pts) : -1;
            if (dragBend >= 0) { startBend = pts[(size_t) dragBend].b; repaint(); return; }
            if (dragPoint >= 0) { repaint(); return; }
            const auto c = toCurve (e.position);
            dragPoint = owner->addPoint (owner->snapX (c.x), c.y);
            moved = dragPoint >= 0;
            repaint();
        }

        void mouseDrag (const juce::MouseEvent& e) override
        {
            if (dragBend >= 0)
            {
                const float b = juce::jlimit (-1.0f, 1.0f, startBend + (float) e.getDistanceFromDragStartY() / 90.0f);
                owner->setBend (dragBend, b);
                moved = true;
            }
            else if (dragPoint >= 0 && e.getDistanceFromDragStart() > 2)
            {
                const auto c = toCurve (e.position);
                dragPoint = owner->movePoint (dragPoint, owner->snapX (c.x), c.y);
                moved = true;
            }
            repaint();
        }

        void mouseUp (const juce::MouseEvent&) override
        {
            if (moved) owner->changed();
            dragPoint = dragBend = -1;
            repaint();
        }

        void mouseDoubleClick (const juce::MouseEvent& e) override
        {
            const auto pts = owner->points (owner->lane);
            const int i = hitPoint (e.position, pts);
            if (i > 0 && i < (int) pts.size() - 1) { owner->removePoint (i); owner->changed(); }
            else if (const int b = hitBend (e.position, pts); b >= 0) { owner->setBend (b, 0.0f); owner->changed(); }
            dragPoint = dragBend = -1;
            repaint();
        }
    };

    //==========================================================================
    static const mixer::ParamDef& def (int p)   { return mixer::info (mixer::tagliola).params[(size_t) p]; }
    static const char* laneName (int l)         { static const char* n[] { "Volume", "Filtro", "Pan" }; return n[juce::jlimit (0, 2, l)]; }
    static juce::Colour laneColour (int l)      { return l == 0 ? Theme::accent : l == 1 ? Theme::cyan : juce::Colour (0xffb58cff); }
    mixer::Slot& theSlot() const                { return engine.getTrack (track).slots[(size_t) slot]; }
    float get (int p) const                     { return theSlot().params[(size_t) p].load(); }
    void set (int p, float v)                   { theSlot().params[(size_t) p].store (juce::jlimit (def (p).minValue, def (p).maxValue, v)); }
    void changed()                              { if (onChanged) onChanged(); }
    bool laneIsOn (int l) const                 { return get (mixer::tagliolaParam::laneOn (l)) > 0.5f; }

    int gridPerBeat() const
    {
        static constexpr int per[] { 0, 2, 4, 8 };
        return per[juce::jlimit (0, 3, juce::roundToInt (get (mixer::tagliolaParam::grid)))];
    }

    float snapX (float x) const
    {
        using namespace mixer::tagliolaParam;
        const int per = gridPerBeat();
        if (per <= 0) return x;
        const double beats = lengthBeats[juce::jlimit (0, 4, juce::roundToInt (get (length)))];
        const float steps = (float) (beats * per);
        return juce::jlimit (0.0f, 1.0f, std::round (x * steps) / steps);
    }

    void buildTable (int l, float* t, int n) const
    {
        mixer::tagliolaParam::buildTable ([this] (int i) { return get (i); }, l, t, n);
    }

    std::vector<Pt> points (int l) const
    {
        using namespace mixer::tagliolaParam;
        const int n = juce::jlimit (2, maxPoints, juce::roundToInt (get (laneCount (l))));
        std::vector<Pt> pts;
        for (int i = 0; i < n; ++i) pts.push_back ({ get (px (l, i)), get (py (l, i)), get (bend (l, i)) });
        std::stable_sort (pts.begin(), pts.end(), [] (const Pt& a, const Pt& b) { return a.x < b.x; });
        return pts;
    }

    void writePoints (int l, std::vector<Pt> pts)
    {
        using namespace mixer::tagliolaParam;
        std::stable_sort (pts.begin(), pts.end(), [] (const Pt& a, const Pt& b) { return a.x < b.x; });
        if (pts.size() > (size_t) maxPoints) pts.resize ((size_t) maxPoints);
        if (pts.size() < 2) pts = { { 0.0f, 1.0f, 0.0f }, { 1.0f, 1.0f, 0.0f } };
        pts.front().x = 0.0f;
        pts.back().x = 1.0f;
        set (laneCount (l), (float) pts.size());
        for (int i = 0; i < maxPoints; ++i)
        {
            const Pt p = i < (int) pts.size() ? pts[(size_t) i] : Pt { 0.0f, 0.0f, 0.0f };
            set (px (l, i), p.x); set (py (l, i), p.y); set (bend (l, i), p.b);
        }
    }

    /** Un ritmo a scalini ("1" = passa, "0" = silenzio) su tutto il giro, con i tagli netti. */
    static std::vector<Pt> steps (const char* pattern)
    {
        const int n = (int) std::strlen (pattern);
        std::vector<Pt> pts;
        float prev = pattern[0] == '1' ? 1.0f : 0.0f;
        pts.push_back ({ 0.0f, prev, 0.0f });
        for (int i = 1; i < n; ++i)
        {
            const float v = pattern[i] == '1' ? 1.0f : 0.0f;
            if (v == prev) continue;
            const float x = (float) i / (float) n;
            pts.push_back ({ x, prev, 0.0f });
            pts.push_back ({ x, v, 0.0f });
            prev = v;
        }
        pts.push_back ({ 1.0f, prev, 0.0f });
        return pts;
    }

    void turnOnLane()
    {
        if (! laneIsOn (lane)) { set (mixer::tagliolaParam::laneOn (lane), 1.0f); refresh(); }
    }

    int addPoint (float x, float y)
    {
        auto pts = points (lane);
        if ((int) pts.size() >= mixer::tagliolaParam::maxPoints || x <= 0.0f || x >= 1.0f) return -1;
        presetShown = -1;
        turnOnLane();
        pts.push_back ({ x, y, 0.0f });
        writePoints (lane, pts);
        const auto now = points (lane);
        for (int i = (int) now.size() - 1; i >= 0; --i)   // (con un salto, il nuovo e' l'ultimo con quella x)
            if (std::abs (now[(size_t) i].x - x) < 1.0e-5f && std::abs (now[(size_t) i].y - y) < 1.0e-5f) return i;
        return -1;
    }

    int movePoint (int i, float x, float y)
    {
        auto pts = points (lane);
        if (i < 0 || i >= (int) pts.size()) return -1;
        presetShown = -1;
        turnOnLane();
        const bool edge = i == 0 || i == (int) pts.size() - 1;
        if (edge) x = pts[(size_t) i].x;
        else x = juce::jlimit (pts[(size_t) i - 1].x, pts[(size_t) i + 1].x, x);   // puo' toccare il vicino: taglio netto
        pts[(size_t) i].x = x; pts[(size_t) i].y = y;
        writePoints (lane, pts);
        return i;
    }

    void removePoint (int i)
    {
        auto pts = points (lane);
        if (i <= 0 || i >= (int) pts.size() - 1) return;
        presetShown = -1;
        pts.erase (pts.begin() + i);
        writePoints (lane, pts);
    }

    void setBend (int seg, float b)
    {
        auto pts = points (lane);
        if (seg < 0 || seg + 1 >= (int) pts.size()) return;
        presetShown = -1;
        turnOnLane();
        pts[(size_t) seg].b = b;
        writePoints (lane, pts);
    }

    void cycle (int param, int options)
    {
        set (param, (float) ((juce::roundToInt (get (param)) + 1) % options));
        refresh();
        changed();
    }

    void refresh()
    {
        using namespace mixer::tagliolaParam;
        const int ids[] { amount, smooth, resonance, mix };
        for (int i = 0; i < 4; ++i)
            if (! knobs[i]->slider.isMouseButtonDown())
                knobs[i]->setValue (mixer::toNormalised (def (ids[i]), get (ids[i])));
        for (int l = 0; l < laneTabs.size(); ++l) { laneTabs[l]->selected = l == lane; laneTabs[l]->active = laneIsOn (l); }
        onButton.on = laneIsOn (lane);
        onButton.setButtonText (laneIsOn (lane) ? juce::String (juce::CharPointer_UTF8 ("Accesa \xe2\x9c\x93")) : juce::String ("Spenta"));
        lengthButton.setButtonText ("Durata " + mixer::formatValue (def (length), get (length)));
        gridButton.setButtonText ("Griglia " + mixer::formatValue (def (grid), get (grid)));
        typeButton.setButtonText (get (filterType) > 0.5f ? "Passa alto" : "Passa basso");
        for (int i = 0; i < presetButtons.size(); ++i) presetButtons[i]->on = i == presetShown;
        for (auto* c : getChildren()) c->repaint();
    }

    void showPresetMenu()
    {
        juce::PopupMenu menu;
        for (int i = 0; i < numPresets; ++i) menu.addItem (i + 1, i == numPresets - 1 ? juce::String ("Dritta (azzera)") : juce::String (presetName (i)));
        juce::Component::SafePointer<TagliolaView> safe (this);
        menu.showMenuAsync (juce::PopupMenu::Options().withTargetComponent (&presetMenuButton), [safe] (int r)
        {
            if (safe != nullptr && r > 0) safe->applyPreset (r - 1);
        });
    }

    void timerCallback() override
    {
        auto& sl = theSlot();
        if (sl.type.load() != mixer::tagliola) return;
        float ph = testPhase;
        if (ph < 0.0f)
            if (auto* fx = dynamic_cast<mixer::Tagliola*> (sl.owned.get()))
            {
                const float v = fx->phaseShown.load();
                if (v != lastPhase) { lastPhase = v; stillFrames = 0; }
                else ++stillFrames;
                ph = stillFrames < 10 ? v : -1.0f;   // fermo da un po' (traccia muta): niente riga
            }
        if (ph != playhead) { playhead = ph; editor.repaint(); }

        bool different = false;
        for (int p = 0; p < mixer::info (mixer::tagliola).numParams; ++p)
            if (const float v = get (p); v != seen[(size_t) p]) { seen[(size_t) p] = v; different = true; }
        bool busy = editor.dragPoint >= 0 || editor.dragBend >= 0;
        for (auto* k : knobs) busy = busy || k->slider.isMouseButtonDown();
        if (different && ! busy) refresh();
    }

    SequencerEngine& engine;
    const int track, slot;
    int lane = 0, presetShown = -1, stillFrames = 100;
    float playhead = -1.0f, lastPhase = -2.0f, testPhase = -1.0f;
    Editor editor;
    juce::OwnedArray<LabeledKnob> knobs;
    juce::OwnedArray<Chip> presetButtons;
    juce::OwnedArray<LaneTab> laneTabs;
    Chip onButton { "Accesa" }, lengthButton { "Durata" }, gridButton { "Griglia" }, typeButton { "Passa basso" },
         presetMenuButton { juce::String (juce::CharPointer_UTF8 ("Preset \xe2\x96\xbe")) }, closeButton { "Effetti" };
    juce::Rectangle<int> titleArea, hintArea;
    std::array<float, mixer::maxParams> seen {};

    friend struct Editor;
};
