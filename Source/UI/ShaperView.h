#pragma once

#include <JuceHeader.h>
#include <functional>
#include <vector>
#include "../Engine/SequencerEngine.h"
#include "Theme.h"
#include "ChannelEditor.h"   // LabeledKnob

//==============================================================================
/** La finestra del Curvatore (effetto del mixer, tipo WaveShaper).

    A sinistra la curva: ingresso in orizzontale, uscita in verticale. Tocca un punto vuoto per
    aggiungere un punto, trascinalo, doppio tocco per toglierlo; il pallino azzurro a meta' di ogni
    tratto lo piega. La zona azzurrina e' fin dove arriva il suono in questo momento.
    A destra l'onda prima (grigio) e dopo (arancio), le curve di partenza, "Speculare" e le manopole.

    Con "Speculare" si disegna solo la meta' destra: la sinistra e' la stessa rovesciata (la curva
    passa per il centro e il suono resta simmetrico, come la modalita' "bipolar" dei waveshaper).
*/
class ShaperView : public juce::Component,
                   private juce::Timer
{
public:
    struct Pt { float x, y, b; };

    ShaperView (SequencerEngine& e, int trackIndex, int slotIndex)
        : engine (e), track (trackIndex), slot (slotIndex)
    {
        using namespace mixer::shaperParam;
        const int ids[] { pre, post, mix, tone };
        const char* names[] { "Prima", "Dopo", "Mix", "Tono" };
        for (int i = 0; i < 4; ++i)
        {
            const int idx = ids[i];
            auto* k = knobs.add (new LabeledKnob (names[i], 0.0, 1.0, 0.5, [idx] (double n)
            {
                const float v = mixer::fromNormalised (def (idx), (float) n);
                if (idx == tone) return std::abs (v) < 0.02f ? juce::String ("neutro") : (v > 0 ? "chiaro " : "scuro ") + juce::String (juce::roundToInt (std::abs (v) * 100.0f)) + "%";
                return mixer::formatValue (def (idx), v);
            }));
            k->onChange = [this, idx] (double n) { set (idx, mixer::fromNormalised (def (idx), (float) n)); curve.repaint(); };
            k->onRelease = [this] { changed(); };
            k->setColour (Theme::accent);
            addAndMakeVisible (k);
        }
        const char* presetNames[] { "Morbida", "Dura", "Piega", "Gradini", "Raddrizza", "Seno", "Trash", "Dritta" };
        for (int i = 0; i < numPresets; ++i)
        {
            auto* b = presetButtons.add (new Chip (presetNames[i]));
            b->onClick = [this, i] { applyPreset (i); };
            addAndMakeVisible (b);
        }
        mirrorButton.onClick = [this] { setMirror (! mirrored()); };
        qualityButton.onClick = [this]
        {
            set (quality, (float) ((juce::roundToInt (get (quality)) + 1) % 3));
            refresh();
            changed();
        };
        presetMenuButton.onClick = [this] { showPresetMenu(); };
        closeButton.onClick = [this] { if (onClose) onClose(); };
        for (auto* b : { &mirrorButton, &qualityButton, &presetMenuButton, &closeButton })
            addAndMakeVisible (b);

        curve.owner = this;
        waveView.owner = this;
        addAndMakeVisible (curve);
        addAndMakeVisible (waveView);
        setOpaque (true);
        refresh();
        startTimerHz (25);
    }

    std::function<void()> onChanged, onClose;

    /** Per le foto di prova: un seno al posto del suono vero (qui non suona niente). */
    void showTestSignal (float amp)
    {
        testSignal = true;
        shownPeak = amp;
        waveView.before.clear();
        for (int i = 0; i < 1024; ++i) waveView.before.push_back (amp * std::sin (juce::MathConstants<float>::twoPi * 2.0f * (float) i / 1024.0f));
        curveChanged();
    }

    static constexpr int numPresets = 8;

    /** Mette una curva di partenza: 0 morbida, 1 dura, 2 piega, 3 gradini, 4 raddrizza, 5 seno, 6 trash, 7 dritta. */
    void applyPreset (int i)
    {
        auto half = [this] (float centre, std::vector<Pt> right) { centreBend = centre; writeMirrored (right); set (mixer::shaperParam::mirror, 1.0f); };
        switch (i)
        {
            case 0:  half (-0.55f, { { 0.4f, 0.72f, -0.3f }, { 1.0f, 0.97f, 0.0f } }); break;
            case 1:  half (0.0f, { { 0.33f, 0.8f, 0.0f }, { 1.0f, 0.8f, 0.0f } }); break;
            case 2:  half (-0.2f, { { 0.38f, 0.95f, 0.1f }, { 0.7f, -0.55f, -0.1f }, { 1.0f, 0.75f, 0.0f } }); break;
            case 3:  half (0.0f, { { 0.22f, 0.0f, 0.0f }, { 0.25f, 0.5f, 0.0f }, { 0.6f, 0.5f, 0.0f }, { 0.63f, 1.0f, 0.0f }, { 1.0f, 1.0f, 0.0f } }); break;
            case 4:  writePoints ({ { -1.0f, 0.9f, 0.0f }, { 0.0f, -0.6f, 0.0f }, { 1.0f, 0.9f, 0.0f } }); set (mixer::shaperParam::mirror, 0.0f); break;
            case 5:  half (-0.35f, { { 0.33f, 1.0f, 0.35f }, { 0.66f, 0.0f, -0.35f }, { 1.0f, -1.0f, 0.0f } }); break;
            case 6:  writePoints ({ { -1.0f, -1.0f, 0.0f }, { -0.55f, -0.85f, 0.3f }, { -0.2f, -0.15f, -0.4f }, { 0.15f, 0.55f, -0.5f },
                                    { 0.45f, 0.35f, 0.2f }, { 0.75f, 0.95f, -0.3f }, { 1.0f, 1.0f, 0.0f } });
                     set (mixer::shaperParam::mirror, 0.0f); break;
            default: half (0.0f, { { 1.0f, 1.0f, 0.0f } }); break;
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
        g.drawText ("CURVATORE", titleArea.withTrimmedLeft (12), juce::Justification::centredLeft, false);
        if (! hintArea.isEmpty())
            Theme::drawCaption (g, "Tocca per aggiungere un punto, trascinalo, doppio tocco lo toglie. Il pallino azzurro piega il tratto",
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
            // le curve di partenza (a righe) + speculare e qualita'
            const int chipH = 30, minChipW = 74;
            const int perRow = juce::jmax (3, c.getWidth() / minChipW);
            const int items = numPresets + 2;
            const int rowsNeeded = (items + perRow - 1) / perRow;
            auto chips = c.removeFromTop (rowsNeeded * (chipH + 4));
            const int cw = chips.getWidth() / perRow;
            std::vector<juce::Component*> all;
            for (auto* b : presetButtons) all.push_back (b);
            all.push_back (&mirrorButton);
            all.push_back (&qualityButton);
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
            const int side = juce::jmin (r.getHeight(), r.getWidth() * 45 / 100);
            curve.setBounds (r.removeFromLeft (side).withSizeKeepingCentre (side, side));
            r.removeFromLeft (10);
            waveView.setBounds (r.removeFromTop (juce::jlimit (60, 160, r.getHeight() * 32 / 100)));
            r.removeFromTop (6);
            layoutControls (r);
        }
        else
        {
            hintArea = r.removeFromTop (14);
            r.removeFromTop (2);
            const int side = juce::jmin (r.getWidth(), r.getHeight() * 46 / 100);
            curve.setBounds (r.removeFromTop (side).withSizeKeepingCentre (side, side));
            r.removeFromTop (6);
            waveView.setBounds (r.removeFromTop (juce::jlimit (50, 110, r.getHeight() / 4)));
            r.removeFromTop (6);
            layoutControls (r);
        }
    }

private:
    //==========================================================================
    /** Pulsantino piatto (curve di partenza, speculare, qualita'). */
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

    //==========================================================================
    /** L'editor della curva. */
    struct Curve : public juce::Component
    {
        ShaperView* owner = nullptr;
        int dragPoint = -1, dragBend = -1;
        float startBend = 0.0f;
        bool moved = false;

        juce::Rectangle<float> plot() const   { return getLocalBounds().toFloat().reduced (8.0f); }
        juce::Point<float> toScreen (float x, float y) const
        {
            const auto p = plot();
            return { p.getX() + (x + 1.0f) * 0.5f * p.getWidth(), p.getBottom() - (y + 1.0f) * 0.5f * p.getHeight() };
        }
        juce::Point<float> toCurve (juce::Point<float> s) const
        {
            const auto p = plot();
            return { juce::jlimit (-1.0f, 1.0f, (s.x - p.getX()) / p.getWidth() * 2.0f - 1.0f),
                     juce::jlimit (-1.0f, 1.0f, (p.getBottom() - s.y) / p.getHeight() * 2.0f - 1.0f) };
        }
        juce::Point<float> bendHandle (const std::vector<Pt>& pts, int i) const
        {
            const auto& a = pts[(size_t) i];
            const auto& b = pts[(size_t) i + 1];
            const float y = a.y + (b.y - a.y) * mixer::shaperParam::bendT (0.5f, a.b);
            return toScreen ((a.x + b.x) * 0.5f, y);
        }

        void paint (juce::Graphics& g) override
        {
            const auto full = getLocalBounds().toFloat();
            Theme::drawSunken (g, full, Theme::lcdBg, 8.0f);
            const auto p = plot();
            // griglia, diagonale (curva "dritta") e zona dove arriva il suono
            g.setColour (juce::Colour (0xff101c24));
            for (int i = 1; i < 8; ++i)
            {
                g.drawVerticalLine ((int) (p.getX() + p.getWidth() * (float) i / 8.0f), p.getY(), p.getBottom());
                g.drawHorizontalLine ((int) (p.getY() + p.getHeight() * (float) i / 8.0f), p.getX(), p.getRight());
            }
            g.setColour (juce::Colour (0xff1f3a48));
            g.drawVerticalLine ((int) p.getCentreX(), p.getY(), p.getBottom());
            g.drawHorizontalLine ((int) p.getCentreY(), p.getX(), p.getRight());
            const float zone = juce::jlimit (0.0f, 1.0f, owner->shownPeak);
            if (zone > 0.01f)
            {
                const auto l = toScreen (-zone, 0.0f), rr = toScreen (zone, 0.0f);
                g.setColour (Theme::cyan.withAlpha (0.07f));
                g.fillRect (juce::Rectangle<float> (l.x, p.getY(), rr.x - l.x, p.getHeight()));
            }
            {
                juce::Path diag;
                diag.startNewSubPath (toScreen (-1.0f, -1.0f));
                diag.lineTo (toScreen (1.0f, 1.0f));
                float dashes[] { 4.0f, 4.0f };
                juce::PathStrokeType (1.0f).createDashedStroke (diag, diag, dashes, 2);
                g.setColour (juce::Colour (0xff2a3a48));
                g.fillPath (diag);
            }
            g.setColour (Theme::textDim.withAlpha (0.7f));
            g.setFont (Theme::font (9.5f));
            g.drawText (juce::String (juce::CharPointer_UTF8 ("ingresso \xe2\x86\x92")), p.reduced (4.0f).removeFromBottom (12.0f), juce::Justification::centredRight, false);
            g.drawText (juce::String (juce::CharPointer_UTF8 ("\xe2\x86\x91 uscita")), p.reduced (4.0f).removeFromTop (12.0f), juce::Justification::centredLeft, false);

            // la curva
            std::vector<float> table ((size_t) 512);
            owner->buildTable (table.data(), (int) table.size());
            juce::Path c;
            for (int i = 0; i < (int) table.size(); ++i)
            {
                const auto s = toScreen (-1.0f + 2.0f * (float) i / (float) (table.size() - 1), table[(size_t) i]);
                if (i == 0) c.startNewSubPath (s); else c.lineTo (s);
            }
            g.setColour (Theme::accent.withAlpha (0.25f));
            g.strokePath (c, juce::PathStrokeType (7.0f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
            g.setColour (Theme::accent);
            g.strokePath (c, juce::PathStrokeType (2.2f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));

            // punti e maniglie (con "Speculare" si toccano solo quelli a destra)
            const auto pts = owner->points();
            const bool mir = owner->mirrored();
            for (int i = 0; i + 1 < (int) pts.size(); ++i)
            {
                if (mir && pts[(size_t) i].x < -1.0e-4f) continue;
                g.setColour (Theme::cyan.withAlpha (dragBend == i ? 1.0f : 0.85f));
                g.fillEllipse (juce::Rectangle<float> (dragBend == i ? 11.0f : 8.0f, dragBend == i ? 11.0f : 8.0f).withCentre (bendHandle (pts, i)));
            }
            for (int i = 0; i < (int) pts.size(); ++i)
            {
                const bool editable = ! mir || pts[(size_t) i].x > 1.0e-4f;
                const auto s = toScreen (pts[(size_t) i].x, pts[(size_t) i].y);
                const float rad = dragPoint == i ? 8.0f : 6.0f;
                g.setColour (dragPoint == i ? juce::Colours::white : Theme::lcdBg);
                g.fillEllipse (juce::Rectangle<float> (rad * 2.0f, rad * 2.0f).withCentre (s));
                g.setColour (Theme::accent.withAlpha (editable ? 1.0f : 0.35f));
                g.drawEllipse (juce::Rectangle<float> (rad * 2.0f, rad * 2.0f).withCentre (s), 2.0f);
            }
        }

        int hitPoint (juce::Point<float> s, const std::vector<Pt>& pts) const
        {
            int best = -1;
            float bestD = 16.0f;
            for (int i = 0; i < (int) pts.size(); ++i)
            {
                if (owner->mirrored() && pts[(size_t) i].x < 1.0e-4f) continue;
                const float d = toScreen (pts[(size_t) i].x, pts[(size_t) i].y).getDistanceFrom (s);
                if (d < bestD) { bestD = d; best = i; }
            }
            return best;
        }

        int hitBend (juce::Point<float> s, const std::vector<Pt>& pts) const
        {
            for (int i = 0; i + 1 < (int) pts.size(); ++i)
            {
                if (owner->mirrored() && pts[(size_t) i].x < -1.0e-4f) continue;
                if (bendHandle (pts, i).getDistanceFrom (s) < 13.0f) return i;
            }
            return -1;
        }

        void mouseDown (const juce::MouseEvent& e) override
        {
            auto pts = owner->points();
            moved = false;
            dragPoint = hitPoint (e.position, pts);
            dragBend = dragPoint < 0 ? hitBend (e.position, pts) : -1;
            if (dragBend >= 0) { startBend = pts[(size_t) dragBend].b; repaint(); return; }
            if (dragPoint >= 0) { repaint(); return; }
            // un punto nuovo dove hai toccato
            const auto c = toCurve (e.position);
            dragPoint = owner->addPoint (c.x, c.y);
            moved = dragPoint >= 0;
            repaint();
        }

        void mouseDrag (const juce::MouseEvent& e) override
        {
            if (dragBend >= 0)
            {
                // su = il tratto si gonfia verso l'alto, giu' = verso il basso
                const float b = juce::jlimit (-1.0f, 1.0f, startBend + (float) e.getDistanceFromDragStartY() / 90.0f);
                owner->setBend (dragBend, b);
                moved = true;
            }
            else if (dragPoint >= 0 && e.getDistanceFromDragStart() > 2)
            {
                const auto c = toCurve (e.position);
                dragPoint = owner->movePoint (dragPoint, c.x, c.y);
                moved = true;
            }
            owner->curveChanged();
        }

        void mouseUp (const juce::MouseEvent&) override
        {
            if (moved) owner->changed();
            dragPoint = dragBend = -1;
            repaint();
        }

        void mouseDoubleClick (const juce::MouseEvent& e) override
        {
            const auto pts = owner->points();
            const int i = hitPoint (e.position, pts);
            if (i > 0 && i < (int) pts.size() - 1) { owner->removePoint (i); owner->changed(); }
            else if (const int b = hitBend (e.position, pts); b >= 0) { owner->setBend (b, 0.0f); owner->changed(); }   // tratto di nuovo dritto
            dragPoint = dragBend = -1;
            owner->curveChanged();
        }
    };

    //==========================================================================
    /** L'onda prima (grigio) e dopo (arancio) la curva. */
    struct Wave : public juce::Component
    {
        ShaperView* owner = nullptr;
        std::vector<float> before;

        void paint (juce::Graphics& g) override
        {
            const auto r = getLocalBounds().toFloat();
            Theme::drawSunken (g, r, Theme::lcdBg, 8.0f);
            g.setColour (Theme::textDim);
            g.setFont (Theme::font (9.5f));
            g.drawText ("grigio: prima   arancio: dopo", r.reduced (8.0f, 4.0f), juce::Justification::topLeft, false);
            if (before.empty()) return;
            std::vector<float> table ((size_t) 1025);
            owner->buildTable (table.data(), (int) table.size());
            auto shape = [&table] (float x)
            {
                const float pos = (juce::jlimit (-1.0f, 1.0f, x) + 1.0f) * 0.5f * (float) (table.size() - 1);
                const int i0 = juce::jmin ((int) pos, (int) table.size() - 2);
                return table[(size_t) i0] + (table[(size_t) i0 + 1] - table[(size_t) i0]) * (pos - (float) i0);
            };
            const auto inner = r.reduced (4.0f, 8.0f);
            juce::Path a, b;
            const int n = (int) before.size();
            for (int i = 0; i < n; ++i)
            {
                const float x = inner.getX() + inner.getWidth() * (float) i / (float) (n - 1);
                const float v = before[(size_t) i];
                const float ya = inner.getCentreY() - juce::jlimit (-1.2f, 1.2f, v) * inner.getHeight() * 0.42f;
                const float yb = inner.getCentreY() - shape (v) * inner.getHeight() * 0.42f;
                if (i == 0) { a.startNewSubPath (x, ya); b.startNewSubPath (x, yb); }
                else        { a.lineTo (x, ya); b.lineTo (x, yb); }
            }
            g.setColour (juce::Colour (0xff5a6a78));
            g.strokePath (a, juce::PathStrokeType (1.2f));
            g.setColour (Theme::accent);
            g.strokePath (b, juce::PathStrokeType (1.8f));
        }
    };

    //==========================================================================
    static const mixer::ParamDef& def (int p)   { return mixer::info (mixer::shaper).params[(size_t) p]; }
    mixer::Slot& theSlot() const                { return engine.getTrack (track).slots[(size_t) slot]; }
    float get (int p) const                     { return theSlot().params[(size_t) p].load(); }
    void set (int p, float v)                   { theSlot().params[(size_t) p].store (juce::jlimit (def (p).minValue, def (p).maxValue, v)); }
    void changed()                              { if (onChanged) onChanged(); }
    bool mirrored() const                       { return get (mixer::shaperParam::mirror) > 0.5f; }

    void buildTable (float* t, int n) const
    {
        mixer::shaperParam::buildTable ([this] (int i) { return get (i); }, t, n);
    }

    std::vector<Pt> points() const
    {
        using namespace mixer::shaperParam;
        const int n = juce::jlimit (2, (int) maxPoints, juce::roundToInt (get (count)));
        std::vector<Pt> pts;
        for (int i = 0; i < n; ++i) pts.push_back ({ get (px (i)), get (py (i)), get (bend (i)) });
        std::stable_sort (pts.begin(), pts.end(), [] (const Pt& a, const Pt& b) { return a.x < b.x; });
        return pts;
    }

    void writePoints (std::vector<Pt> pts)
    {
        using namespace mixer::shaperParam;
        std::stable_sort (pts.begin(), pts.end(), [] (const Pt& a, const Pt& b) { return a.x < b.x; });
        if (pts.size() > (size_t) maxPoints) pts.resize ((size_t) maxPoints);
        pts.front().x = -1.0f;
        pts.back().x = 1.0f;
        set (count, (float) pts.size());
        for (int i = 0; i < (int) maxPoints; ++i)
        {
            const Pt p = i < (int) pts.size() ? pts[(size_t) i] : Pt { 0.0f, 0.0f, 0.0f };
            set (px (i), p.x); set (py (i), p.y); set (bend (i), p.b);
        }
    }

    /** "Speculare": dalla meta' destra (x > 0, al massimo 5 punti) la curva intera, che passa per il centro. */
    void writeMirrored (std::vector<Pt> right)
    {
        std::stable_sort (right.begin(), right.end(), [] (const Pt& a, const Pt& b) { return a.x < b.x; });
        right.erase (std::remove_if (right.begin(), right.end(), [] (const Pt& p) { return p.x <= 1.0e-3f; }), right.end());
        if (right.size() > 5) right.resize (5);
        if (right.empty() || right.back().x < 1.0f) right.push_back ({ 1.0f, right.empty() ? 1.0f : right.back().y, 0.0f });
        right.back().x = 1.0f;
        std::vector<Pt> all;
        // a sinistra: gli stessi punti rovesciati; il tratto rovesciato ha la piega opposta
        for (int i = (int) right.size() - 1; i >= 0; --i)
        {
            const float bendOfSegment = -(i == 0 ? centreBend : right[(size_t) i - 1].b);
            all.push_back ({ -right[(size_t) i].x, -right[(size_t) i].y, bendOfSegment });
        }
        all.push_back ({ 0.0f, 0.0f, centreBend });
        for (auto& p : right) all.push_back (p);
        writePoints (all);
    }

    std::vector<Pt> rightHalf()
    {
        const auto pts = points();
        std::vector<Pt> r;
        for (auto& p : pts)
        {
            if (std::abs (p.x) < 1.0e-4f) centreBend = p.b;
            if (p.x > 1.0e-4f) r.push_back (p);
        }
        return r;
    }

    void setMirror (bool on)
    {
        set (mixer::shaperParam::mirror, on ? 1.0f : 0.0f);
        if (on) { centreBend = 0.0f; writeMirrored (rightHalf()); }
        presetShown = -1;
        refresh();
        changed();
    }

    /** Aggiunge un punto (restituisce l'indice per trascinarlo subito, o -1 se non c'e' posto). */
    int addPoint (float x, float y)
    {
        using namespace mixer::shaperParam;
        presetShown = -1;
        if (mirrored())
        {
            auto r = rightHalf();
            if (r.size() >= 5 || std::abs (x) < 0.03f || std::abs (x) > 0.97f) return -1;
            if (x < 0.0f) { x = -x; y = -y; }
            r.push_back ({ x, y, 0.0f });
            writeMirrored (r);
        }
        else
        {
            auto pts = points();
            if ((int) pts.size() >= (int) maxPoints || std::abs (x) > 0.97f) return -1;
            pts.push_back ({ x, y, 0.0f });
            writePoints (pts);
        }
        curveChanged();
        const auto pts = points();
        for (int i = 0; i < (int) pts.size(); ++i)
            if (std::abs (pts[(size_t) i].x - x) < 1.0e-5f && std::abs (pts[(size_t) i].y - y) < 1.0e-5f) return i;
        return -1;
    }

    /** Sposta un punto; i punti alle estremita' si muovono solo in su e giu'. Restituisce il suo indice nuovo. */
    int movePoint (int i, float x, float y)
    {
        auto pts = points();
        if (i < 0 || i >= (int) pts.size()) return -1;
        presetShown = -1;
        const bool edge = i == 0 || i == (int) pts.size() - 1;
        if (edge) x = pts[(size_t) i].x;
        else x = juce::jlimit (pts[(size_t) i - 1].x + 0.005f, pts[(size_t) i + 1].x - 0.005f, x);
        if (mirrored())
        {
            if (pts[(size_t) i].x <= 1.0e-4f) return i;
            x = juce::jmax (0.01f, x);
            pts[(size_t) i].x = x; pts[(size_t) i].y = y;
            centreBend = 0.0f;
            for (auto& p : pts) if (std::abs (p.x) < 1.0e-4f) centreBend = p.b;
            std::vector<Pt> r;
            for (auto& p : pts) if (p.x > 1.0e-4f) r.push_back (p);
            writeMirrored (r);
        }
        else
        {
            pts[(size_t) i].x = x; pts[(size_t) i].y = y;
            writePoints (pts);
        }
        return i;
    }

    void removePoint (int i)
    {
        auto pts = points();
        if (i <= 0 || i >= (int) pts.size() - 1) return;
        presetShown = -1;
        if (mirrored())
        {
            if (pts[(size_t) i].x <= 1.0e-4f) return;
            pts.erase (pts.begin() + i);
            centreBend = 0.0f;
            std::vector<Pt> r;
            for (auto& p : pts) { if (std::abs (p.x) < 1.0e-4f) centreBend = p.b; if (p.x > 1.0e-4f) r.push_back (p); }
            writeMirrored (r);
        }
        else
        {
            pts.erase (pts.begin() + i);
            writePoints (pts);
        }
    }

    void setBend (int seg, float b)
    {
        auto pts = points();
        if (seg < 0 || seg + 1 >= (int) pts.size()) return;
        presetShown = -1;
        pts[(size_t) seg].b = b;
        if (mirrored())
        {
            centreBend = 0.0f;
            std::vector<Pt> r;
            for (auto& p : pts) { if (std::abs (p.x) < 1.0e-4f) centreBend = p.b; if (p.x > 1.0e-4f) r.push_back (p); }
            writeMirrored (r);
        }
        else writePoints (pts);
        curveChanged();
    }

    void curveChanged()
    {
        curve.repaint();
        waveView.repaint();
    }

    void refresh()
    {
        using namespace mixer::shaperParam;
        const int ids[] { pre, post, mix, tone };
        for (int i = 0; i < 4; ++i)
            if (! knobs[i]->slider.isMouseButtonDown())
                knobs[i]->setValue (mixer::toNormalised (def (ids[i]), get (ids[i])));
        mirrorButton.on = mirrored();
        qualityButton.setButtonText (juce::String (juce::CharPointer_UTF8 ("Qualit\xc3\xa0 ")) + mixer::formatValue (def (quality), get (quality)));
        for (int i = 0; i < presetButtons.size(); ++i) presetButtons[i]->on = i == presetShown;
        for (auto* c : getChildren()) c->repaint();
    }

    void showPresetMenu()
    {
        juce::PopupMenu menu;
        const char* names[] { "Morbida", "Dura", "Piega", "Gradini", "Raddrizza", "Seno", "Trash", "Dritta (azzera)" };
        for (int i = 0; i < numPresets; ++i) menu.addItem (i + 1, names[i]);
        juce::Component::SafePointer<ShaperView> safe (this);
        menu.showMenuAsync (juce::PopupMenu::Options().withTargetComponent (&presetMenuButton), [safe] (int r)
        {
            if (safe != nullptr && r > 0) safe->applyPreset (r - 1);
        });
    }

    void timerCallback() override
    {
        auto& sl = theSlot();
        if (sl.type.load() != mixer::shaper) return;
        if (auto* fx = testSignal ? nullptr : dynamic_cast<mixer::Shaper*> (sl.owned.get()))
        {
            const float pk = fx->inputPeak.exchange (0.0f);
            shownPeak = juce::jmax (pk, shownPeak * 0.85f);
            // l'ultimo pezzo di suono, da un passaggio per lo zero in salita (l'onda sta ferma)
            const int n = mixer::Shaper::scopeSize, pos = fx->scopePos.load();
            std::vector<float> buf ((size_t) n);
            for (int i = 0; i < n; ++i) buf[(size_t) i] = fx->scope[(size_t) ((pos + i) & (n - 1))];
            int start = 0;
            for (int i = 1; i < n / 2; ++i)
                if (buf[(size_t) i - 1] <= 0.0f && buf[(size_t) i] > 0.0f) { start = i; break; }
            waveView.before.assign (buf.begin() + start, buf.begin() + start + n / 2);
        }
        // valori cambiati altrove (annulla, automazione)
        bool different = false;
        for (int p = 0; p < mixer::info (mixer::shaper).numParams; ++p)
            if (const float v = get (p); v != seen[(size_t) p]) { seen[(size_t) p] = v; different = true; }
        bool busy = curve.dragPoint >= 0 || curve.dragBend >= 0;
        for (auto* k : knobs) busy = busy || k->slider.isMouseButtonDown();
        if (different && ! busy) refresh();
        curve.repaint();
        waveView.repaint();
    }

    SequencerEngine& engine;
    const int track, slot;
    int presetShown = -1;
    bool testSignal = false;
    float centreBend = 0.0f, shownPeak = 0.0f;
    Curve curve;
    Wave waveView;
    juce::OwnedArray<LabeledKnob> knobs;
    juce::OwnedArray<Chip> presetButtons;
    Chip mirrorButton { "Speculare" }, qualityButton { "Qualita'" }, presetMenuButton { juce::String (juce::CharPointer_UTF8 ("Preset \xe2\x96\xbe")) }, closeButton { "Effetti" };
    juce::Rectangle<int> titleArea, hintArea;
    std::array<float, mixer::maxParams> seen {};

    friend struct Curve;
    friend struct Wave;
};
