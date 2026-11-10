#include "MateriaEditor.h"

namespace
{
    using namespace materia;

    const juce::Colour colBg { 0xff0e141b }, colPanel { 0xff161f2a }, colCard { 0xff1a2430 }, colLine { 0xff2c3a4a }, colDark { 0xff0b1016 };
    const juce::Colour colText { 0xffdfe8f1 }, colDim { 0xff8497ab }, colBrand { 0xffc9a2ff }, colMove { 0xffffe066 };

    juce::String suffixOf (int group, int slot, const char* s)
    {
        return group == 0 ? sourceId (slot, s) : group == 1 ? shaperId (slot, s) : moverId (slot, s);
    }
    const ModuleInfo& infoOf (int group, int type)
    {
        return group == 0 ? sourceInfo (type) : group == 1 ? shaperInfo (type) : moverInfo (type);
    }
    float raw (MateriaAudioProcessor& p, const juce::String& id)
    {
        if (auto* v = p.apvts.getRawParameterValue (id)) return v->load();
        return 0.0f;
    }
    void setParam (MateriaAudioProcessor& p, const juce::String& id, float value)
    {
        if (auto* prm = p.apvts.getParameter (id))
        {
            prm->beginChangeGesture();
            prm->setValueNotifyingHost (prm->convertTo0to1 (value));
            prm->endChangeGesture();
        }
    }
    /** C'e' un movimento collegato a questa destinazione? */
    bool isMoved (MateriaAudioProcessor& p, int dest)
    {
        for (int m = 0; m < numMovers; ++m)
            if ((int) raw (p, moverId (m, "Type")) != mvEmpty)
                for (int k = 0; k < numTargets; ++k)
                    if ((int) raw (p, moverId (m, "T") + juce::String (k + 1)) == dest
                        && std::abs (raw (p, moverId (m, "Q") + juce::String (k + 1))) > 0.001f)
                        return true;
        return false;
    }

    //==========================================================================
    /** Pulsante piatto (aggiungi, togli, su, giu'). */
    class FlatButton : public juce::Button
    {
    public:
        FlatButton (const juce::String& text, juce::Colour c) : juce::Button (text), colour (c) { setButtonText (text); }
        void paintButton (juce::Graphics& g, bool over, bool down) override
        {
            auto r = getLocalBounds().toFloat().reduced (0.5f);
            g.setColour (down ? colLine : over ? colCard.brighter (0.08f) : colDark);
            g.fillRoundedRectangle (r, 4.0f);
            g.setColour (colLine);
            g.drawRoundedRectangle (r, 4.0f, 1.0f);
            g.setColour (colour);
            g.setFont (frui::sans (juce::jlimit (10.0f, 12.5f, r.getHeight() * 0.55f), true));
            g.drawFittedText (getButtonText(), getLocalBounds().reduced (3, 0), juce::Justification::centred, 1, 0.8f);
        }
        juce::Colour colour;
    };

    //==========================================================================
    /** Manopola: arco colorato, nome sotto, valore al centro mentre si muove. */
    class Dial : public juce::Component
    {
    public:
        Dial (juce::RangedAudioParameter& p, juce::String name, juce::Colour c, std::function<juce::String (float)> textFn = {})
            : param (p), label (std::move (name)), colour (c), text (std::move (textFn)),
              attachment (p, [this] (float v) { norm = param.convertTo0to1 (v); repaint(); })
        {
            attachment.sendInitialUpdate();
            setRepaintsOnMouseActivity (true);
        }

        void paint (juce::Graphics& g) override
        {
            auto r = getLocalBounds().toFloat();
            const float labelH = juce::jlimit (11.0f, 15.0f, r.getHeight() * 0.2f);
            auto lab = r.removeFromBottom (labelH);
            const float d = juce::jmin (r.getWidth(), r.getHeight()) - 4.0f;
            auto k = r.withSizeKeepingCentre (d, d);
            const float a0 = juce::MathConstants<float>::pi * 1.25f, a1 = juce::MathConstants<float>::pi * 2.75f;
            const float th = juce::jmax (3.0f, d * 0.13f);
            juce::Path back, arc;
            back.addCentredArc (k.getCentreX(), k.getCentreY(), d * 0.5f - th * 0.5f, d * 0.5f - th * 0.5f, 0.0f, a0, a1, true);
            g.setColour (colLine);
            g.strokePath (back, juce::PathStrokeType (th, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
            const bool bipolar = param.getNormalisableRange().start < 0.0f;
            const float from = bipolar ? a0 + (a1 - a0) * 0.5f : a0, to = a0 + (a1 - a0) * norm;
            arc.addCentredArc (k.getCentreX(), k.getCentreY(), d * 0.5f - th * 0.5f, d * 0.5f - th * 0.5f, 0.0f, juce::jmin (from, to), juce::jmax (from, to), true);
            g.setColour (colour);
            g.strokePath (arc, juce::PathStrokeType (th, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
            auto inner = k.reduced (th + 2.0f);
            g.setColour (colCard.brighter (0.06f));
            g.fillEllipse (inner);
            g.setColour (colLine.brighter (0.15f));
            g.drawEllipse (inner, 1.0f);
            if (moved)   // un movimento la sta muovendo: puntino giallo
            {
                g.setColour (colMove);
                g.fillEllipse (k.getRight() - 7.0f, k.getY(), 7.0f, 7.0f);
            }
            const bool showValue = isMouseOverOrDragging() || dragging;
            g.setColour (showValue ? colText : colDim);
            g.setFont (frui::sans (juce::jlimit (9.0f, 11.5f, labelH * 0.8f), true));
            g.drawFittedText (showValue ? valueText() : label, lab.toNearestInt(), juce::Justification::centred, 1, 0.7f);
        }

        void mouseDown (const juce::MouseEvent&) override    { dragging = true; startNorm = norm; attachment.beginGesture(); repaint(); }
        void mouseDrag (const juce::MouseEvent& e) override
        {
            const float delta = (float) (-e.getDistanceFromDragStartY() + e.getDistanceFromDragStartX() * 0.5f) / (e.mods.isShiftDown() ? 600.0f : 170.0f);
            attachment.setValueAsPartOfGesture (param.convertFrom0to1 (juce::jlimit (0.0f, 1.0f, startNorm + delta)));
        }
        void mouseUp (const juce::MouseEvent&) override       { dragging = false; attachment.endGesture(); repaint(); }
        void mouseDoubleClick (const juce::MouseEvent&) override   { attachment.setValueAsCompleteGesture (param.convertFrom0to1 (param.getDefaultValue())); }
        void mouseWheelMove (const juce::MouseEvent&, const juce::MouseWheelDetails& w) override
        {
            attachment.setValueAsCompleteGesture (param.convertFrom0to1 (juce::jlimit (0.0f, 1.0f, norm + w.deltaY * 0.15f)));
        }
        juce::String valueText() const      { return text ? text (param.convertFrom0to1 (norm)) : param.getText (norm, 12); }
        bool moved = false;

    private:
        juce::RangedAudioParameter& param;
        juce::String label;
        juce::Colour colour;
        std::function<juce::String (float)> text;
        float norm = 0.0f, startNorm = 0.0f;
        bool dragging = false;
        juce::ParameterAttachment attachment;
    };

    //==========================================================================
    /** Pulsante-scelta: mostra la voce attuale, al tocco apre il menu. */
    class ChoiceButton : public juce::Component
    {
    public:
        ChoiceButton (juce::RangedAudioParameter& p, juce::StringArray options, juce::String prefixText = {})
            : param (p), names (std::move (options)), prefix (std::move (prefixText)),
              attachment (p, [this] (float v) { index = juce::roundToInt (v); repaint(); })
        {
            attachment.sendInitialUpdate();
        }
        void paint (juce::Graphics& g) override
        {
            auto r = getLocalBounds().toFloat().reduced (0.5f, 1.5f);
            g.setColour (colDark);
            g.fillRoundedRectangle (r, 4.0f);
            g.setColour (colLine);
            g.drawRoundedRectangle (r, 4.0f, 1.0f);
            g.setColour (colText);
            g.setFont (frui::sans (juce::jlimit (10.0f, 12.5f, r.getHeight() * 0.52f), false));
            const auto name = juce::isPositiveAndBelow (index, names.size()) ? names[index] : juce::String (index);
            g.drawFittedText (prefix + name + juce::String (juce::CharPointer_UTF8 (" \xe2\x96\xbe")), r.reduced (5.0f, 0.0f).toNearestInt(),
                              juce::Justification::centred, 1, 0.7f);
        }
        void mouseDown (const juce::MouseEvent&) override
        {
            juce::PopupMenu m;
            for (int i = 0; i < names.size(); ++i) m.addItem (i + 1, names[i], true, i == index);
            juce::Component::SafePointer<ChoiceButton> safe (this);
            m.showMenuAsync (juce::PopupMenu::Options().withTargetComponent (this), [safe] (int r)
            {
                if (safe != nullptr && r > 0) safe->attachment.setValueAsCompleteGesture ((float) (r - 1));
            });
        }
    private:
        juce::RangedAudioParameter& param;
        juce::StringArray names;
        juce::String prefix;
        int index = 0;
        juce::ParameterAttachment attachment;
    };

    //==========================================================================
    // I disegni dei blocchi

    juce::Path plot (juce::Rectangle<float> r, int points, const std::function<float (float)>& f)
    {
        juce::Path p;
        for (int i = 0; i < points; ++i)
        {
            const float x = (float) i / (float) (points - 1);
            const float y = juce::jlimit (-1.0f, 1.0f, f (x));
            const juce::Point<float> pt (r.getX() + x * r.getWidth(), r.getCentreY() - y * r.getHeight() * 0.46f);
            if (i == 0) p.startNewSubPath (pt); else p.lineTo (pt);
        }
        return p;
    }

    juce::Path envPath (juce::Rectangle<float> r, float a, float d, float s, float rel)
    {
        const float total = a + d + rel, hold = total * 0.25f, w = r.getWidth() / (total + hold);
        juce::Path p;
        float x = r.getX();
        p.startNewSubPath (x, r.getBottom());
        x += a * w; p.lineTo (x, r.getY());
        x += d * w; p.lineTo (x, r.getBottom() - s * r.getHeight());
        x += hold * w; p.lineTo (x, r.getBottom() - s * r.getHeight());
        p.lineTo (r.getRight(), r.getBottom());
        return p;
    }

    void paintDisplay (juce::Graphics& g, juce::Rectangle<float> area, MateriaAudioProcessor& proc, int group, int slot, bool mini)
    {
        g.setColour (colDark);
        g.fillRoundedRectangle (area, 4.0f);
        g.setColour (colLine.darker (0.2f));
        g.drawRoundedRectangle (area, 4.0f, 1.0f);
        auto r = area.reduced (mini ? 3.0f : 6.0f, mini ? 3.0f : 5.0f);
        const float stroke = mini ? 1.3f : 1.8f;
        auto K = [&] (const char* s) { return raw (proc, suffixOf (group, slot, s)); };
        const int points = mini ? 48 : 160;

        if (group == 3)
        {
            auto q = [&] (const char* id, float maxV) { return std::pow (raw (proc, id) / maxV, 0.4f); };
            g.setColour (colBrand);
            g.strokePath (envPath (r, q ("ampA", 4.0f) + 0.02f, q ("ampD", 6.0f) + 0.02f, raw (proc, "ampS"), q ("ampR", 8.0f) + 0.02f), juce::PathStrokeType (stroke));
            return;
        }
        const int type = (int) K ("Type");
        const auto colour = juce::Colour (infoOf (group, type).colour);
        g.setColour (colour);
        const float twoPi = juce::MathConstants<float>::twoPi;

        if (group == 0)
        {
            if (type == srcOsc || type == srcWave)
            {
                auto& bank = prisma::Bank::get();
                const int table = type == srcOsc ? 0 : (int) K ("Sel");
                bank.ensure (table);
                const float pos = K ("A"), warp = K ("B"), bend = K ("C");
                const int mode = type == srcOsc ? 1 : juce::jlimit (0, 5, (int) K ("Mode")) + 1;
                g.strokePath (plot (r, points, [&] (float x)
                {
                    double ph = juce::jmin (0.9999, (double) x);
                    if (warp > 0.0005f) ph = prisma::warpPhase (mode, warp, ph);
                    if (bend > 0.0005f) ph = prisma::warpPhase (2, bend, ph);
                    return bank.read (table, pos, ph, mini ? 3 : 1);
                }), juce::PathStrokeType (stroke));
            }
            else if (type == srcNoise)
            {
                juce::Random rnd (7 + slot);
                const float c = K ("A");
                float lp = 0.0f;
                const float k = c < 0.5f ? 0.08f + c * 1.8f : 1.0f;
                g.strokePath (plot (r, points, [&] (float)
                {
                    const float wn = rnd.nextFloat() * 2.0f - 1.0f;
                    lp += k * (wn - lp);
                    return (c > 0.5f ? wn - lp * (c - 0.5f) * 1.6f : lp * (1.0f + (0.5f - c) * 3.0f)) * 0.8f;
                }), juce::PathStrokeType (juce::jmax (1.0f, stroke - 0.5f)));
            }
            else if (type == srcString)
            {
                const float life = K ("B"), bow = K ("D"), hard = K ("A");
                g.strokePath (plot (r, points, [&] (float x)
                {
                    const float env = bow + (1.0f - bow) * std::exp (-x * (9.0f - life * 8.0f));
                    return env * (std::sin (x * twoPi * 9.0f) * 0.7f + hard * 0.3f * std::sin (x * twoPi * 27.0f) * std::exp (-x * 6.0f));
                }), juce::PathStrokeType (stroke));
            }
        }
        else if (group == 1)
        {
            const float a = K ("A"), b = K ("B"), c = K ("C");
            const int mode = (int) K ("Mode");
            if (type == shFilter)
            {
                const float fc = a, kq = 2.0f - 1.96f * b;
                g.strokePath (plot (r, points, [&] (float x)
                {
                    const float w = std::pow (1000.0f, x - fc);     // frequenza / taglio
                    const float re = 1.0f - w * w, im = w * kq, den = re * re + im * im;
                    float mag;
                    switch (mode)
                    {
                        case 1:  mag = 1.0f / den; break;
                        case 2:  mag = w * w / std::sqrt (den); break;
                        case 3:  mag = w * kq / std::sqrt (den); break;
                        case 4:  mag = std::abs (re) / std::sqrt (den); break;
                        default: mag = 1.0f / std::sqrt (den); break;
                    }
                    return juce::jlimit (-1.0f, 1.0f, juce::Decibels::gainToDecibels (mag, -60.0f) / 30.0f + 0.25f);
                }), juce::PathStrokeType (stroke));
            }
            else if (type == shDrive || type == shFold)
            {
                g.strokePath (plot (r, points, [&] (float x)
                {
                    const float in = x * 2.0f - 1.0f;
                    if (type == shDrive)
                    {
                        const float pg = 1.0f + a * a * 40.0f, v = in * pg + c * 0.6f;
                        const float y = mode == 1 ? juce::jlimit (-1.0f, 1.0f, v) : mode == 2 ? (v > 0 ? std::tanh (v) : std::tanh (v * 0.55f) * 1.4f)
                                      : mode == 3 ? std::tanh (v - 0.35f * std::abs (v)) : mode == 4 ? std::sin (juce::jlimit (-4.0f, 4.0f, v) * 1.5708f) : std::tanh (v);
                        return y * 0.9f;
                    }
                    const float v = in * (1.0f + a * 10.0f) + b * 0.8f, t = v * 0.25f + 0.25f;
                    const float tri = 4.0f * std::abs (t - std::floor (t + 0.5f)) - 1.0f;
                    return (tri + (std::sin (v * 1.5708f) - tri) * c) * 0.9f;
                }), juce::PathStrokeType (stroke));
            }
            else if (type == shResonator)
            {
                const int teeth = mini ? 6 : 12;
                for (int i = 0; i < teeth; ++i)
                {
                    const float x = r.getX() + (i + 0.5f) / teeth * r.getWidth();
                    const float h = r.getHeight() * (0.25f + 0.7f * b) * std::exp (-(float) i * (0.05f + c * 0.45f));
                    g.fillRect (juce::Rectangle<float> (x - 1.0f, r.getBottom() - h, mode == 1 ? 3.0f : 2.0f, h));
                }
            }
            else if (type == shCrush)
            {
                const float levels = juce::jmax (1.5f, std::pow (2.0f, 5.0f - a * 4.0f)), steps = juce::jmax (4.0f, 64.0f - b * 58.0f);
                g.strokePath (plot (r, points, [&] (float x) { return std::round (std::sin (std::floor (x * steps) / steps * twoPi) * levels) / levels * 0.9f; }),
                              juce::PathStrokeType (stroke));
            }
            else if (type == shVowel)
            {
                static const float F1[] { 800.0f, 500.0f, 300.0f, 500.0f, 350.0f }, F2[] { 1200.0f, 1900.0f, 2300.0f, 900.0f, 700.0f };
                const float p = a * 4.0f; const int i0 = juce::jmin (3, (int) p); const float fr = p - (float) i0;
                const float shift = std::pow (2.0f, (c - 0.5f) * 2.0f);
                const float f1 = (F1[i0] + (F1[i0 + 1] - F1[i0]) * fr) * shift, f2 = (F2[i0] + (F2[i0 + 1] - F2[i0]) * fr) * shift;
                const float q = 3.0f + b * 12.0f;
                g.strokePath (plot (r, points, [&] (float x)
                {
                    const float f = 150.0f * std::pow (30.0f, x);
                    auto peak = [&] (float fc) { const float d = (f / fc - fc / f) * q; return 1.0f / std::sqrt (1.0f + d * d); };
                    return juce::jmax (peak (f1), peak (f2) * 0.8f) * 1.7f - 0.9f;
                }), juce::PathStrokeType (stroke));
            }
            else if (type == shDelay)
            {
                const int bars = mini ? 4 : 7;
                for (int i = 0; i < bars; ++i)
                {
                    const float h = r.getHeight() * std::pow (0.25f + a * 0.65f, (float) i);
                    g.setColour (colour.withAlpha (i == 0 ? 1.0f : 0.75f));
                    g.fillRect (juce::Rectangle<float> (r.getX() + (float) i / bars * r.getWidth() + 2.0f, r.getBottom() - h, r.getWidth() / bars * 0.35f, h));
                }
            }
            else if (type == shReverb)
            {
                juce::Path p;
                p.startNewSubPath (r.getX(), r.getBottom());
                p.lineTo (r.getX() + 3.0f, r.getY());
                for (int i = 1; i <= 40; ++i)
                {
                    const float x = (float) i / 40.0f;
                    p.lineTo (r.getX() + 3.0f + x * (r.getWidth() - 3.0f), r.getBottom() - r.getHeight() * std::exp (-x * (7.0f - a * 6.0f)));
                }
                p.lineTo (r.getRight(), r.getBottom());
                p.closeSubPath();
                g.setColour (colour.withAlpha (0.35f));
                g.fillPath (p);
                g.setColour (colour);
                g.strokePath (p, juce::PathStrokeType (1.0f));
            }
            else if (type == shChorus)
            {
                for (int v = 0; v < 3; ++v)
                {
                    g.setColour (colour.withAlpha (v == 0 ? 1.0f : 0.5f));
                    g.strokePath (plot (r, points, [&] (float x) { return std::sin ((x * 2.0f + v * 0.07f) * twoPi) * (0.8f - v * 0.1f); }), juce::PathStrokeType (stroke - 0.3f));
                }
            }
        }
        else
        {
            const float now = proc.shownMover[(size_t) slot].load();
            const float phase = proc.shownPhase[(size_t) slot].load();
            if (type == mvEnv)
            {
                auto q = [&] (const char* s) { return 0.04f + K (s); };
                g.strokePath (envPath (r, q ("A"), q ("B"), K ("C"), q ("D")), juce::PathStrokeType (stroke));
                g.setColour (colText.withAlpha (0.8f));
                g.fillRect (juce::Rectangle<float> (r.getRight() - 3.0f, r.getBottom() - now * r.getHeight(), 3.0f, now * r.getHeight()));
            }
            else if (type == mvLfo)
            {
                const int shape = (int) K ("Mode");
                juce::Random rnd (11);
                float hold = 0.5f; int lastTurn = -1;
                g.strokePath (plot (r, points, [&] (float x)
                {
                    const float ph = x * 2.0f;
                    const int turn = (int) (ph * 4.0f);
                    if (turn != lastTurn) { lastTurn = turn; hold = rnd.nextFloat() * 2.0f - 1.0f; }
                    const float p = ph - std::floor (ph);
                    switch (shape)
                    {
                        case 0:  return std::sin (p * twoPi) * 0.9f;
                        case 1:  return (p < 0.25f ? 4.0f * p : p < 0.75f ? 2.0f - 4.0f * p : 4.0f * p - 4.0f) * 0.9f;
                        case 2:  return (2.0f * p - 1.0f) * 0.9f;
                        case 3:  return (1.0f - 2.0f * p) * 0.9f;
                        case 4:  return p < 0.5f ? 0.9f : -0.9f;
                        default: return hold * 0.9f;
                    }
                }), juce::PathStrokeType (stroke));
                if (! mini)
                {
                    g.setColour (colText);
                    g.fillEllipse (juce::Rectangle<float> (6.0f, 6.0f).withCentre ({ r.getX() + phase * 0.5f * r.getWidth(), r.getCentreY() - now * r.getHeight() * 0.46f * 0.9f }));
                }
            }
            else if (type == mvSeq)
            {
                const int count = 2 + juce::jlimit (0, 6, juce::roundToInt (K ("C") * 6.0f));
                const float w = r.getWidth() / (float) numSteps;
                for (int i = 0; i < numSteps; ++i)
                {
                    const float v = raw (proc, moverId (slot, "S") + juce::String (i + 1));
                    auto bar = juce::Rectangle<float> (r.getX() + i * w + 1.0f, r.getCentreY() - juce::jmax (0.0f, v) * r.getHeight() * 0.5f,
                                                       w - 2.0f, std::abs (v) * r.getHeight() * 0.5f + 1.0f);
                    g.setColour (i >= count ? colLine : (int) phase == i ? colText : colour);
                    g.fillRect (bar);
                }
                g.setColour (colLine);
                g.fillRect (juce::Rectangle<float> (r.getX(), r.getCentreY(), r.getWidth(), 1.0f));
            }
            else
            {
                // valore attuale (casuale, nota, forza, rotella)
                g.setColour (colLine);
                g.fillRect (juce::Rectangle<float> (r.getX(), r.getCentreY() - 1.0f, r.getWidth(), 2.0f));
                const bool bip = type == mvRandom || type == mvNote;
                const float x0 = bip ? r.getCentreX() : r.getX(), x1 = bip ? r.getCentreX() + now * r.getWidth() * 0.5f : r.getX() + now * r.getWidth();
                g.setColour (colour);
                g.fillRect (juce::Rectangle<float> (juce::jmin (x0, x1), r.getCentreY() - 4.0f, std::abs (x1 - x0) + 2.0f, 8.0f));
            }
        }
    }

    /** Il disegno di un blocco; nella sequenza i passi si disegnano col dito. */
    class Display : public juce::Component
    {
    public:
        Display (MateriaAudioProcessor& p, int g, int s) : proc (p), group (g), slot (s) {}
        void paint (juce::Graphics& g) override     { paintDisplay (g, getLocalBounds().toFloat(), proc, group, slot, false); }
        void mouseDown (const juce::MouseEvent& e) override    { edit (e); }
        void mouseDrag (const juce::MouseEvent& e) override    { edit (e); }
    private:
        void edit (const juce::MouseEvent& e)
        {
            if (group != 2 || (int) raw (proc, moverId (slot, "Type")) != mvSeq) return;
            auto r = getLocalBounds().toFloat().reduced (6.0f, 5.0f);
            const int i = juce::jlimit (0, numSteps - 1, (int) ((e.position.x - r.getX()) / r.getWidth() * numSteps));
            float v = juce::jlimit (-1.0f, 1.0f, (r.getCentreY() - e.position.y) / (r.getHeight() * 0.5f));
            if (std::abs (v) < 0.06f) v = 0.0f;
            setParam (proc, moverId (slot, "S") + juce::String (i + 1), v);
            repaint();
        }
        MateriaAudioProcessor& proc;
        int group, slot;
    };

    //==========================================================================
    /** Una riga "muove questa manopola di tanto" di un movimento. */
    class TargetRow : public juce::Component
    {
    public:
        TargetRow (MateriaAudioProcessor& p, int moverSlot, int index) : proc (p), mover (moverSlot), k (index) {}

        void paint (juce::Graphics& g) override
        {
            auto r = getLocalBounds().toFloat().reduced (0.0f, 1.5f);
            const int dest = target();
            const auto name = destName (proc.apvts, dest);
            auto left = r.removeFromLeft (r.getWidth() * 0.56f);
            buttonArea = left;
            g.setColour (colDark);
            g.fillRoundedRectangle (left, 4.0f);
            g.setColour (colLine);
            g.drawRoundedRectangle (left, 4.0f, 1.0f);
            g.setFont (frui::sans (juce::jlimit (10.0f, 12.0f, r.getHeight() * 0.5f), false));
            g.setColour (name.isEmpty() ? colDim : colText);
            g.drawFittedText (name.isEmpty() ? juce::String ("+ muovi una manopola...") : juce::String (juce::CharPointer_UTF8 ("\xe2\x86\x92 ")) + name,
                              left.reduced (6.0f, 0.0f).toNearestInt(), juce::Justification::centredLeft, 1, 0.7f);
            r.removeFromLeft (6.0f);
            barArea = r;
            if (name.isEmpty()) return;
            // quanto: barra da -100% a +100%
            g.setColour (colDark);
            g.fillRoundedRectangle (r, 4.0f);
            const float amt = amount();
            const float x0 = r.getCentreX(), x1 = r.getCentreX() + amt * r.getWidth() * 0.5f;
            g.setColour (colMove.withAlpha (0.85f));
            g.fillRoundedRectangle (juce::Rectangle<float> (juce::jmin (x0, x1), r.getY() + 2.0f, std::abs (x1 - x0), r.getHeight() - 4.0f), 3.0f);
            g.setColour (colLine.brighter (0.3f));
            g.fillRect (juce::Rectangle<float> (r.getCentreX() - 0.5f, r.getY(), 1.0f, r.getHeight()));
            g.setColour (colText);
            g.drawFittedText ((amt > 0.004f ? "+" : "") + juce::String (juce::roundToInt (amt * 100.0f)) + "%", r.toNearestInt(), juce::Justification::centred, 1);
        }

        void mouseDown (const juce::MouseEvent& e) override
        {
            draggingBar = false;
            if (buttonArea.contains (e.position)) { showMenu(); return; }
            if (barArea.contains (e.position) && target() > 0) { draggingBar = true; setFromX (e.position.x); }
        }
        void mouseDrag (const juce::MouseEvent& e) override    { if (draggingBar) setFromX (e.position.x); }
        void mouseDoubleClick (const juce::MouseEvent& e) override
        {
            if (barArea.contains (e.position)) { setParam (proc, moverId (mover, "Q") + juce::String (k + 1), 0.0f); repaint(); }
        }

    private:
        int target() const      { return (int) raw (proc, moverId (mover, "T") + juce::String (k + 1)); }
        float amount() const    { return raw (proc, moverId (mover, "Q") + juce::String (k + 1)); }
        void setFromX (float x)
        {
            float v = juce::jlimit (-1.0f, 1.0f, (x - barArea.getCentreX()) / (barArea.getWidth() * 0.5f));
            if (std::abs (v) < 0.03f) v = 0.0f;
            setParam (proc, moverId (mover, "Q") + juce::String (k + 1), v);
            repaint();
        }
        void showMenu()
        {
            juce::PopupMenu m;
            m.addSectionHeader ("Cosa muove?");
            const int current = target();
            static const char* common[] { "DETUNE", "NOTA", "VOLUME", "PAN" };
            for (int s = 0; s < numSources; ++s)
            {
                const int type = (int) raw (proc, sourceId (s, "Type"));
                if (type == srcEmpty) continue;
                juce::PopupMenu sub;
                for (int i = 0; i < destsPerSource; ++i)
                {
                    const char* label = i < numKnobs ? sourceInfo (type).knobs[i] : common[i - numKnobs];
                    if (label == nullptr) continue;
                    if (type != srcOsc && type != srcWave && i == sdDetune) continue;
                    sub.addItem (sourceDest (s, i), label, true, current == sourceDest (s, i));
                }
                m.addSubMenu (juce::String (sourceInfo (type).name) + " " + juce::String (s + 1), sub);
            }
            for (int s = 0; s < numShapers; ++s)
            {
                const int type = (int) raw (proc, shaperId (s, "Type"));
                if (type == shEmpty) continue;
                juce::PopupMenu sub;
                for (int i = 0; i < destsPerShaper; ++i)
                {
                    const char* label = i < 3 ? shaperInfo (type).knobs[i] : "MIX";
                    if (label != nullptr) sub.addItem (shaperDest (s, i), label, true, current == shaperDest (s, i));
                }
                m.addSubMenu (juce::String (shaperInfo (type).name) + " " + juce::String (s + 1), sub);
            }
            if (current > 0) { m.addSeparator(); m.addItem (9999, "Scollega"); }
            juce::Component::SafePointer<TargetRow> safe (this);
            m.showMenuAsync (juce::PopupMenu::Options().withTargetComponent (this), [safe] (int r)
            {
                if (safe == nullptr || r == 0) return;
                const auto t = moverId (safe->mover, "T") + juce::String (safe->k + 1), q = moverId (safe->mover, "Q") + juce::String (safe->k + 1);
                if (r == 9999) { setParam (safe->proc, t, 0.0f); setParam (safe->proc, q, 0.0f); }
                else
                {
                    setParam (safe->proc, t, (float) r);
                    if (std::abs (raw (safe->proc, q)) < 0.01f) setParam (safe->proc, q, 0.5f);
                }
                if (auto* p = safe->getParentComponent()) p->repaint();
            });
        }
        MateriaAudioProcessor& proc;
        int mover, k;
        juce::Rectangle<float> buttonArea, barArea;
        bool draggingBar = false;
    };

    //==========================================================================
    /** Un blocco aperto: titolo, scelte, disegno, manopole (e per i movimenti: cosa muovono). */
    class BlockPanel : public juce::Component
    {
    public:
        BlockPanel (MateriaEditor& ed, int g, int s) : editor (ed), proc (ed.processor), group (g), slot (s)
        {
            type = group == 3 ? 1 : (int) raw (proc, suffixOf (group, slot, "Type"));
            auto& st = proc.apvts;
            static const char* letters[] { "A", "B", "C", "D" };
            colour = group == 3 ? colBrand : juce::Colour (infoOf (group, type).colour);
            display = std::make_unique<Display> (proc, group, slot);
            addAndMakeVisible (*display);

            auto dial = [&] (const juce::String& id, const juce::String& name, int knob, int dest)
            {
                if (auto* p = st.getParameter (id))
                {
                    std::function<juce::String (float)> text;
                    if (knob >= 0) text = [g = group, t = type, knob] (float v) { return knobText (g, t, knob, v); };
                    auto* d = dials.add (new Dial (*p, name, colour, std::move (text)));
                    dialDest.push_back (dest);
                    addAndMakeVisible (d);
                }
            };
            auto choice = [&] (const juce::String& id, juce::StringArray names, const juce::String& prefix = {})
            {
                if (auto* p = st.getParameter (id))
                    addAndMakeVisible (choices.add (new ChoiceButton (*p, std::move (names), prefix)));
            };

            if (group == 3)
            {
                title = "Volume";
                dial ("ampA", "ATTACCO", -1, 0); dial ("ampD", "DECAD.", -1, 0); dial ("ampS", "TENUTA", -1, 0); dial ("ampR", "RILASCIO", -1, 0);
                firstRow = 4;
                dial ("glide", "GLIDE", -1, 0); dial ("master", "VOLUME", -1, 0);
                choice ("voiceMode", { "Poly (accordi)", "Mono", "Legato" });
            }
            else
            {
                const auto& info = infoOf (group, type);
                title = juce::String (info.name) + " " + juce::String (slot + 1);
                if (group == 0 && type == srcWave)
                    choice (sourceId (slot, "Sel"), prisma::Bank::get().names());
                if (info.modeTitle != nullptr)
                {
                    juce::StringArray names;
                    for (auto* n : info.modes) names.add (n);
                    choice (suffixOf (group, slot, "Mode"), names, group == 0 && type == srcWave ? juce::String ("Warp: ") : juce::String());
                }
                for (int k = 0; k < (group == 1 ? 3 : numKnobs); ++k)
                    if (info.knobs[k] != nullptr)
                        dial (suffixOf (group, slot, letters[k]), info.knobs[k], k,
                              group == 0 ? sourceDest (slot, k) : group == 1 ? shaperDest (slot, k) : 0);
                if (group == 1) dial (shaperId (slot, "Mix"), isSpace (type) ? "QUANTO" : "MIX", -1, shaperDest (slot, 3));
                firstRow = dials.size();
                if (group == 0)
                {
                    choice (sourceId (slot, "Oct"), { "Ott -3", "Ott -2", "Ott -1", "Ott 0", "Ott +1", "Ott +2", "Ott +3" });
                    if (type == srcOsc || type == srcWave)
                    {
                        choice (sourceId (slot, "Uni"), { "1 voce", "2 voci", "3 voci", "5 voci", "7 voci" });
                        dial (sourceId (slot, "Det"), "DETUNE", -1, sourceDest (slot, sdDetune));
                    }
                    dial (sourceId (slot, "Semi"), "SEMIT.", -1, 0);
                    dial (sourceId (slot, "Fine"), "FINE", -1, 0);
                    dial (sourceId (slot, "Lvl"), "VOLUME", -1, sourceDest (slot, sdLevel));
                    dial (sourceId (slot, "Pan"), "PAN", -1, sourceDest (slot, sdPan));
                }
                if (group == 2)
                    for (int k = 0; k < numTargets; ++k)
                        addAndMakeVisible (targets.add (new TargetRow (proc, slot, k)));

                addAndMakeVisible (removeButton);
                removeButton.setButtonText (juce::String (juce::CharPointer_UTF8 ("\xe2\x9c\x95")));
                removeButton.setTooltip ("Togli questo blocco");
                removeButton.onClick = [this] { editor.removeBlock (group, slot); };
                if (group == 1 && ! isSpace (type))
                {
                    for (auto* b : { &upButton, &downButton }) addAndMakeVisible (*b);
                    upButton.setButtonText (juce::String (juce::CharPointer_UTF8 ("\xe2\x96\xb2")));
                    downButton.setButtonText (juce::String (juce::CharPointer_UTF8 ("\xe2\x96\xbc")));
                    upButton.setTooltip ("Sposta prima nella catena");
                    downButton.setTooltip ("Sposta dopo nella catena");
                    upButton.onClick = [this] { editor.moveBlock (slot, -1); };
                    downButton.onClick = [this] { editor.moveBlock (slot, 1); };
                }
            }
            refresh();
        }

        /** Altezza giusta per questa larghezza (colonne del PC). */
        int idealHeight (int width) const
        {
            const int perRow = juce::jmax (3, juce::jmin (5, (width - 16) / 56));
            const int rows1 = (firstRow + perRow - 1) / perRow, rows2 = (dials.size() - firstRow + perRow - 1) / perRow;
            return 30 + (choices.isEmpty() ? 0 : 28) + 50 + (rows1 + rows2) * 62 + targets.size() * 26 + (targets.isEmpty() ? 0 : 6) + 12;
        }

        void paint (juce::Graphics& g) override
        {
            auto r = getLocalBounds().toFloat().reduced (0.5f);
            g.setColour (colCard);
            g.fillRoundedRectangle (r, 6.0f);
            g.setColour (colLine);
            g.drawRoundedRectangle (r, 6.0f, 1.0f);
            g.setColour (colour);
            g.fillRoundedRectangle (r.withHeight (3.0f), 1.5f);
            g.fillEllipse (juce::Rectangle<float> (9.0f, 9.0f).withCentre ({ 14.0f, 17.0f }));
            g.setColour (colText);
            g.setFont (frui::sans (13.5f, true));
            g.drawFittedText (title, titleArea, juce::Justification::centredLeft, 1, 0.8f);
        }

        void resized() override
        {
            auto r = getLocalBounds().reduced (8, 0);
            r.removeFromTop (4);
            auto head = r.removeFromTop (26);
            if (removeButton.isVisible()) removeButton.setBounds (head.removeFromRight (28).reduced (1, 2));
            if (downButton.isVisible()) { downButton.setBounds (head.removeFromRight (26).reduced (1, 2)); upButton.setBounds (head.removeFromRight (26).reduced (1, 2)); }
            titleArea = head.withTrimmedLeft (16);
            if (! choices.isEmpty())
            {
                auto row = r.removeFromTop (28).reduced (0, 1);
                const int w = row.getWidth() / choices.size();
                for (auto* c : choices) c->setBounds (row.removeFromLeft (w).reduced (2, 0));
            }
            // quello che avanza va al disegno (pannello in basso sul telefono); nelle colonne e' alto 50
            const int perRow = juce::jmax (3, juce::jmin (5, (getWidth() - 16) / 56));
            const int rows1 = (firstRow + perRow - 1) / perRow, rows2 = (dials.size() - firstRow + perRow - 1) / perRow;
            const int targetsH = targets.size() * 26 + (targets.isEmpty() ? 0 : 6);
            const int rest = r.getHeight() - targetsH - 8;
            const int rowsTotal = juce::jmax (1, rows1 + rows2);
            int dialH = juce::jlimit (46, 76, (rest - 44) / rowsTotal);
            int dispH = juce::jlimit (30, 110, rest - dialH * (rows1 + rows2));
            display->setBounds (r.removeFromTop (dispH).reduced (0, 3));
            auto place = [&] (int from, int to)
            {
                for (int i = from; i < to; i += perRow)
                {
                    auto row = r.removeFromTop (dialH);
                    const int count = juce::jmin (perRow, to - i);
                    const int w = juce::jmin (row.getWidth() / juce::jmax (count, 4), 96);
                    row = row.withSizeKeepingCentre (w * count, row.getHeight());
                    for (int j = 0; j < count; ++j) dials[i + j]->setBounds (row.removeFromLeft (w).reduced (2, 2));
                }
            };
            place (0, firstRow);
            place (firstRow, dials.size());
            if (! targets.isEmpty())
            {
                r.removeFromTop (4);
                for (auto* t : targets) t->setBounds (r.removeFromTop (26));
            }
        }

        /** Chiamata dal timer: puntini "mossa da un movimento" e disegni che si muovono. */
        void refresh()
        {
            for (int i = 0; i < dials.size(); ++i)
            {
                const bool m = dialDest[(size_t) i] > 0 && isMoved (proc, dialDest[(size_t) i]);
                if (m != dials[i]->moved) { dials[i]->moved = m; dials[i]->repaint(); }
            }
            display->repaint();
        }
        int group, slot;

    private:
        MateriaEditor& editor;
        MateriaAudioProcessor& proc;
        int type = 0, firstRow = 0;
        juce::Colour colour;
        juce::String title;
        juce::Rectangle<int> titleArea;
        std::unique_ptr<Display> display;
        juce::OwnedArray<Dial> dials;
        std::vector<int> dialDest;
        juce::OwnedArray<ChoiceButton> choices;
        juce::OwnedArray<TargetRow> targets;
        FlatButton removeButton { "x", colDim }, upButton { "su", colDim }, downButton { "giu", colDim };
    };

    //==========================================================================
    /** Un blocco chiuso nella lista (telefono): nome, due parole su com'e' messo, disegnino. */
    class BlockRow : public juce::Component
    {
    public:
        BlockRow (MateriaEditor& ed, int g, int s) : group (g), slot (s), editor (ed), proc (ed.processor) {}

        void paint (juce::Graphics& g) override
        {
            const int type = group == 3 ? 1 : (int) raw (proc, suffixOf (group, slot, "Type"));
            const auto colour = group == 3 ? colBrand : juce::Colour (infoOf (group, type).colour);
            auto r = getLocalBounds().toFloat().reduced (0.5f, 2.0f);
            g.setColour (selected ? colCard.brighter (0.1f) : colCard);
            g.fillRoundedRectangle (r, 6.0f);
            g.setColour (selected ? colour : colLine);
            g.drawRoundedRectangle (r, 6.0f, selected ? 1.6f : 1.0f);
            g.setColour (colour);
            g.fillRoundedRectangle (r.withWidth (4.0f), 2.0f);
            auto in = r.reduced (12.0f, 0.0f);
            const float miniW = juce::jmin (84.0f, in.getWidth() * 0.26f);
            auto mini = in.removeFromRight (miniW).reduced (0.0f, 6.0f);
            paintDisplay (g, mini, proc, group, slot, true);
            in.removeFromRight (8.0f);
            const auto sub = subtitle (type);
            g.setColour (colText);
            g.setFont (frui::sans (13.5f, true));
            g.drawFittedText (group == 3 ? juce::String ("Volume") : juce::String (infoOf (group, type).name) + " " + juce::String (slot + 1),
                              (sub.isEmpty() ? in : in.withTrimmedBottom (in.getHeight() * 0.42f).withTrimmedTop (3.0f)).toNearestInt(),
                              juce::Justification::centredLeft, 1, 0.8f);
            g.setColour (colDim);
            g.setFont (frui::sans (10.5f, false));
            g.drawFittedText (sub, in.withTrimmedTop (in.getHeight() * 0.52f).withTrimmedBottom (3.0f).toNearestInt(), juce::Justification::centredLeft, 1, 0.7f);
        }
        void mouseUp (const juce::MouseEvent& e) override    { if (! e.mouseWasDraggedSinceMouseDown()) editor.selectBlock (group, slot); }
        int group, slot;
        bool selected = false;

    private:
        juce::String subtitle (int type) const
        {
            const juce::String dot = juce::String (juce::CharPointer_UTF8 (" \xc2\xb7 "));
            auto K = [&] (const char* s) { return raw (proc, suffixOf (group, slot, s)); };
            if (group == 3)
            {
                static const char* modes[] { "Poly", "Mono", "Legato" };
                return juce::String (modes[juce::jlimit (0, 2, (int) raw (proc, "voiceMode"))]) + dot + "inviluppo del volume";
            }
            const auto& info = infoOf (group, type);
            const int mode = juce::jlimit (0, juce::jmax (0, (int) info.modes.size() - 1), (int) K ("Mode"));
            if (group == 0)
            {
                static const int uni[] { 1, 2, 3, 5, 7 };
                const int u = uni[juce::jlimit (0, 4, (int) K ("Uni"))];
                const juce::String voices = u > 1 ? dot + juce::String (u) + " voci" : juce::String();
                if (type == srcWave) return prisma::Bank::get().names()[(int) K ("Sel")] + voices;
                if (type == srcOsc)
                {
                    static const char* shapes[] { "sinusoide", "triangolo", "sega", "quadra" };
                    return juce::String (shapes[juce::jlimit (0, 3, juce::roundToInt (K ("A") * 3.0f))]) + voices;
                }
                if (type == srcString) return juce::String (info.modes[(size_t) mode]) + (K ("D") > 0.05f ? dot + "archetto" : dot + "pizzicata");
                return K ("B") > 0.3f ? "intonato" : K ("A") < 0.35f ? "scuro" : K ("A") > 0.65f ? "chiaro" : "bianco";
            }
            if (group == 1)
            {
                juce::String s = info.modeTitle != nullptr ? juce::String (info.modes[(size_t) mode]) : juce::String();
                if (info.knobs[0] != nullptr) s << (s.isEmpty() ? juce::String() : dot) << knobText (1, type, 0, K ("A"));
                if (isSpace (type)) s << (s.isEmpty() ? juce::String() : dot) << "sul suono intero";
                return s;
            }
            juce::StringArray to;
            for (int k = 0; k < numTargets; ++k)
            {
                const auto n = destName (proc.apvts, (int) raw (proc, moverId (slot, "T") + juce::String (k + 1)));
                if (n.isNotEmpty()) to.add (n);
            }
            return to.isEmpty() ? juce::String ("non muove ancora niente") : juce::String (juce::CharPointer_UTF8 ("\xe2\x86\x92 ")) + to.joinIntoString (", ");
        }
        MateriaEditor& editor;
        MateriaAudioProcessor& proc;
    };

    /** I titoli delle sezioni, disegnati dentro i contenuti che scorrono. */
    struct Titles : juce::Component
    {
        explicit Titles (std::vector<std::pair<juce::String, juce::Rectangle<int>>>& l) : list (l) { setInterceptsMouseClicks (false, false); }
        std::vector<std::pair<juce::String, juce::Rectangle<int>>>& list;
        void paint (juce::Graphics& g) override
        {
            for (auto& [text, r] : list)
            {
                const bool hint = text.length() > 24;
                g.setColour (colDim);
                g.setFont (frui::sans (hint ? 12.0f : 11.0f, ! hint));
                if (hint) g.drawFittedText (text, r, juce::Justification::centredLeft, 2);
                else frui::drawSpaced (g, text, r.toFloat(), 1.4f, juce::Justification::centredLeft);
            }
        }
    };
}

//==============================================================================
MateriaEditor::MateriaEditor (MateriaAudioProcessor& p) : juce::AudioProcessorEditor (&p), processor (p)
{
    frui::accentRef() = colBrand;
    setLookAndFeel (&laf);
    static const char* addNames[] { "+ sorgente", "+ forma", "+ movimento" };
    for (int g = 0; g < 3; ++g)
    {
        auto* b = addButtons.add (new FlatButton (addNames[g], colBrand));
        b->onClick = [this, g, b] { addBlock (g, b); };
    }
    listView.setViewedComponent (&listContent, false);
    listView.setScrollBarsShown (true, false);
    listView.setScrollBarThickness (6);
    addChildComponent (listView);
    for (int c = 0; c < 3; ++c)
    {
        columnView[c].setViewedComponent (&columnContent[c], false);
        columnView[c].setScrollBarsShown (true, false);
        columnView[c].setScrollBarThickness (6);
        addChildComponent (columnView[c]);
    }
    for (int c = 0; c < 4; ++c)
    {
        auto* t = titleComps.add (new Titles (headers[c]));
        (c < 3 ? columnContent[c] : listContent).addAndMakeVisible (t);
        t->setBounds (0, 0, 3000, 6000);
    }
    // si apre sul primo blocco che c'e'
    for (int s = numSources - 1; s >= 0; --s) if (typeOf (0, s) != 0) selSlot = s;
    setSize (900, 420);
    startTimerHz (15);
}

MateriaEditor::~MateriaEditor()
{
    stopTimer();
    setLookAndFeel (nullptr);
}

int MateriaEditor::typeOf (int group, int slot) const
{
    return group == 3 ? 1 : (int) raw (processor, suffixOf (group, slot, "Type"));
}

juce::String MateriaEditor::signature() const
{
    juce::String s;
    for (int i = 0; i < numSources; ++i) s << typeOf (0, i);
    for (int i = 0; i < numShapers; ++i) s << typeOf (1, i);
    for (int i = 0; i < numMovers; ++i)  s << typeOf (2, i);
    return s;
}

void MateriaEditor::timerCallback()
{
    if (signature() != shownSignature) { rebuild(); return; }
    for (auto* p : panels) static_cast<BlockPanel*> (p)->refresh();
    if (detail != nullptr) static_cast<BlockPanel*> (detail.get())->refresh();
    for (auto* r : rows) r->repaint();
}

void MateriaEditor::selectBlock (int group, int slot)
{
    selGroup = group; selSlot = slot;
    if (wide) return;
    detail.reset();
    if (typeOf (group, slot) != 0)
    {
        detail = std::make_unique<BlockPanel> (*this, group, slot);
        addAndMakeVisible (*detail);
        detail->setBounds (detailArea);
    }
    for (auto* r : rows)
    {
        auto* row = static_cast<BlockRow*> (r);
        row->selected = row->group == group && row->slot == slot;
        row->repaint();
    }
    repaint();
}

void MateriaEditor::addBlock (int group, juce::Component* anchor)
{
    juce::PopupMenu m;
    const int count = group == 0 ? numSourceTypes : group == 1 ? numShaperTypes : numMoverTypes;
    m.addSectionHeader (group == 0 ? "Da dove nasce il suono" : group == 1 ? "Come gli dai forma" : "Cosa lo fa muovere");
    for (int t = 1; t < count; ++t)
    {
        const auto& info = infoOf (group, t);
        if (group == 1 && t == shChorus) m.addSeparator();
        m.addItem (t, juce::String (info.name) + juce::String (juce::CharPointer_UTF8 ("  \xe2\x80\x94  ")) + info.hint);
    }
    juce::Component::SafePointer<MateriaEditor> safe (this);
    m.showMenuAsync (juce::PopupMenu::Options().withTargetComponent (anchor), [safe, group] (int t)
    {
        if (safe == nullptr || t <= 0) return;
        auto& p = safe->processor;
        const int slot = group == 0 ? p.addSource (t) : group == 1 ? p.addShaper (t) : p.addMover (t);
        if (slot < 0)
        {
            juce::AlertWindow::showMessageBoxAsync (juce::MessageBoxIconType::InfoIcon, "Materia",
                                                    group == 0 ? "Ci sono gia' 4 sorgenti: togline una per aggiungerne un'altra."
                                                               : "Ci sono gia' 6 blocchi qui: togline uno per aggiungerne un altro.");
            return;
        }
        safe->selGroup = group; safe->selSlot = slot;
        safe->rebuild();
    });
}

void MateriaEditor::removeBlock (int group, int slot)
{
    // (il pannello che ha chiamato sta per essere distrutto: si finisce dopo)
    juce::Component::SafePointer<MateriaEditor> safe (this);
    juce::MessageManager::callAsync ([safe, group, slot]
    {
        if (safe == nullptr) return;
        auto& p = safe->processor;
        if (group == 0) p.removeSource (slot); else if (group == 1) p.removeShaper (slot); else if (group == 2) p.removeMover (slot);
        safe->selGroup = 3; safe->selSlot = 0;
        for (int s = numSources - 1; s >= 0; --s) if (safe->typeOf (0, s) != 0) { safe->selGroup = 0; safe->selSlot = s; }
        safe->rebuild();
    });
}

void MateriaEditor::rebuild()
{
    shownSignature = signature();
    built = true;
    detail.reset();
    rows.clear();
    panels.clear();
    for (auto& h : headers) h.clear();
    wide = getWidth() >= 700;
    listView.setVisible (! wide);
    for (auto& c : columnView) c.setVisible (wide);

    // i blocchi nell'ordine in cui si vedono: sorgenti; forma (prima quelli per nota, poi lo spazio); volume + movimenti
    std::vector<std::pair<int, int>> blocks[3];
    for (int s = 0; s < numSources; ++s) if (typeOf (0, s) != 0) blocks[0].push_back ({ 0, s });
    for (int pass = 0; pass < 2; ++pass)
        for (int s = 0; s < numShapers; ++s)
            if (typeOf (1, s) != 0 && isSpace (typeOf (1, s)) == (pass == 1)) blocks[1].push_back ({ 1, s });
    blocks[2].push_back ({ 3, 0 });
    for (int s = 0; s < numMovers; ++s) if (typeOf (2, s) != 0) blocks[2].push_back ({ 2, s });
    static const char* titles[] { "SORGENTI", "FORMA (in catena)", "MOVIMENTO" };

    if (wide)
    {
        auto area = getLocalBounds().withTrimmedTop (34).reduced (4, 2);
        const int colW = area.getWidth() / 3;
        for (int c = 0; c < 3; ++c)
        {
            auto col = area.removeFromLeft (colW).reduced (3, 0);
            columnView[c].setBounds (col);
            const int w = col.getWidth() - 8;
            int y = 0;
            headers[c].push_back ({ titles[c], { 2, y, w - 110, 26 } });
            columnContent[c].addAndMakeVisible (addButtons[c]);
            addButtons[c]->setBounds (w - 106, y + 2, 104, 22);
            y += 28;
            for (auto [g, s] : blocks[c])
            {
                auto* p = new BlockPanel (*this, g, s);
                panels.add (p);
                columnContent[c].addAndMakeVisible (p);
                const int h = p->idealHeight (w);
                p->setBounds (0, y, w, h);
                y += h + 6;
            }
            columnContent[c].setSize (w, juce::jmax (y, col.getHeight()));
        }
    }
    else
    {
        auto area = getLocalBounds().withTrimmedTop (34);
        const int detailH = juce::jlimit (210, 360, (int) (area.getHeight() * 0.5f));
        detailArea = area.removeFromBottom (detailH).reduced (6, 4);
        listView.setBounds (area.reduced (6, 0));
        const int w = area.getWidth() - 12 - 8;
        int y = 0;
        for (int c = 0; c < 3; ++c)
        {
            headers[3].push_back ({ titles[c], { 2, y, w - 124, 26 } });
            listContent.addAndMakeVisible (addButtons[c]);
            addButtons[c]->setBounds (w - 118, y + 2, 116, 22);
            y += 28;
            for (auto [g, s] : blocks[c])
            {
                auto* r = new BlockRow (*this, g, s);
                rows.add (r);
                listContent.addAndMakeVisible (r);
                r->setBounds (0, y, w, 48);
                y += 48;
            }
            if (c == 0 && blocks[0].empty())
            {
                headers[3].push_back ({ "Il banco e' vuoto: aggiungi una sorgente per cominciare.", { 2, y, w - 4, 30 } });
                y += 30;
            }
            y += 4;
        }
        listContent.setSize (w, y);
        if (typeOf (selGroup, selSlot) == 0) { selGroup = 3; selSlot = 0; }
        selectBlock (selGroup, selSlot);
    }
    repaint();
}

void MateriaEditor::paint (juce::Graphics& g)
{
    g.fillAll (colBg);
    g.setColour (colPanel);
    g.fillRect (getLocalBounds().removeFromTop (32));
    g.setColour (colLine);
    g.fillRect (0, 32, getWidth(), 1);
    g.setFont (frui::sans (16.0f, true));
    frui::drawNeon (g, "MATERIA", juce::Rectangle<float> (12.0f, 0.0f, 200.0f, 32.0f), 4.0f, juce::Justification::centredLeft, colBrand);
    g.setColour (colDim);
    g.setFont (frui::sans (11.0f, false));
    if (getWidth() > 520)
        g.drawFittedText ("parti da un suono e dagli forma", getLocalBounds().removeFromTop (32).withTrimmedRight (12), juce::Justification::centredRight, 1);
    if (! wide)
    {
        g.setColour (colPanel);
        g.fillRect (detailArea.expanded (6, 4));
        g.setColour (colLine);
        g.fillRect (0, detailArea.getY() - 4, getWidth(), 1);
    }
}

void MateriaEditor::resized()
{
    rebuild();
}

void MateriaEditor::moveBlock (int slot, int delta)
{
    juce::Component::SafePointer<MateriaEditor> safe (this);
    juce::MessageManager::callAsync ([safe, slot, delta]
    {
        if (safe == nullptr) return;
        safe->processor.moveShaper (slot, delta);
        safe->rebuild();
    });
}
