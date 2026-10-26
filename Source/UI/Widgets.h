#pragma once

#include <JuceHeader.h>
#include <functional>
#include "Theme.h"

//==============================================================================
/** Pulsante Play/Stop: triangolo quando fermo, quadrato (e LED verde) quando suona. */
/** Pulsante rotondo del trasporto (play, rec): disco in rilievo con un anello che si accende. */
inline void drawRoundTransport (juce::Graphics& g, juce::Rectangle<float> r, bool isHighlighted, bool isDown, juce::Colour ring, bool on)
{
    const float d = juce::jmin (r.getWidth(), r.getHeight()) - 4.0f;
    auto disc = juce::Rectangle<float> (d, d).withCentre (r.getCentre());
    if (on)
    {
        g.setGradientFill (juce::ColourGradient (ring.withAlpha (0.35f), disc.getCentre(), ring.withAlpha (0.0f),
                                                 disc.getCentre().translated (d * 0.62f, 0.0f), true));
        g.fillEllipse (disc.expanded (3.0f));
    }
    auto base = juce::Colour (0xff2a3138);
    if (isDown)             base = base.darker (0.3f);
    else if (isHighlighted) base = base.brighter (0.08f);
    g.setGradientFill (juce::ColourGradient (base.brighter (0.12f), disc.getCentreX(), disc.getY(), base.darker (0.35f), disc.getCentreX(), disc.getBottom(), false));
    g.fillEllipse (disc);
    g.setColour (Theme::border);
    g.drawEllipse (disc, 1.0f);
    g.setColour (on ? ring : juce::Colour (0xff3a434c));
    g.drawEllipse (disc.reduced (2.0f), on ? 2.0f : 1.2f);
}

class PlayButton : public juce::Button
{
public:
    PlayButton() : juce::Button ("Play") {}

    void paintButton (juce::Graphics& g, bool isHighlighted, bool isDown) override
    {
        auto r = getLocalBounds().toFloat();
        const bool playing = getToggleState();
        drawRoundTransport (g, r, isHighlighted, isDown, Theme::led, playing);

        const float s = juce::jmin (r.getHeight(), r.getWidth()) * 0.32f;
        auto icon = juce::Rectangle<float> (s, s).withCentre (r.getCentre());
        juce::Path p;

        if (playing)
            p.addRectangle (icon.reduced (s * 0.08f));
        else
            p.addTriangle (icon.getX() + s * 0.2f, icon.getY(), icon.getX() + s * 0.2f, icon.getBottom(),
                           icon.getRight() + s * 0.1f, icon.getCentreY());

        g.setColour (playing ? Theme::led : Theme::textBright);
        g.fillPath (p);
    }
};

//==============================================================================
/**
    Display LCD con frecce ai lati.
    - BPM: trascina su/giu' sul numero, oppure tocca le frecce
    - Pattern: tocca le frecce per scorrere, il centro per il menu
*/
class LcdSelector : public juce::Component
{
public:
    std::function<juce::String()> getText;
    std::function<void (int delta)> onStep;          // freccia sinistra/destra
    std::function<void (double delta)> onDrag;       // trascinamento verticale (unita' per pixel / 4)
    std::function<void()> onCentreTap;
    juce::String caption;

    void paint (juce::Graphics& g) override
    {
        auto r = getLocalBounds().toFloat();
        Theme::drawSunken (g, r);

        auto left = getLeftArea().toFloat(), right = getRightArea().toFloat();
        drawArrow (g, left, true);
        drawArrow (g, right, false);

        auto middle = r.withLeft (left.getRight()).withRight (right.getX());

        if (caption.isNotEmpty())
            Theme::drawCaption (g, caption, middle.removeFromTop (r.getHeight() * 0.32f).translated (0.0f, 2.0f));

        g.setColour (Theme::lcdText);
        g.setFont (Theme::font (juce::jmin (22.0f, r.getHeight() * (caption.isNotEmpty() ? 0.46f : 0.55f)), true));
        g.drawText (getText ? getText() : juce::String(), middle, juce::Justification::centred, false);
    }

    void mouseDown (const juce::MouseEvent& e) override
    {
        dragged = false;
        downY = e.position.y;
        accumulated = 0.0;
    }

    void mouseDrag (const juce::MouseEvent& e) override
    {
        if (! onDrag)
            return;

        const double dy = downY - e.position.y;
        if (! dragged && std::abs (dy) > 6.0)
            dragged = true;

        if (dragged)
        {
            onDrag ((dy - accumulated) / 4.0);
            accumulated = dy;
            repaint();
        }
    }

    void mouseUp (const juce::MouseEvent& e) override
    {
        if (dragged)
            return;

        const auto p = e.getPosition();
        if (getLeftArea().contains (p))         { if (onStep) onStep (-1); }
        else if (getRightArea().contains (p))   { if (onStep) onStep (1); }
        else if (onCentreTap)                   onCentreTap();
        repaint();
    }

private:
    juce::Rectangle<int> getLeftArea() const   { return getLocalBounds().removeFromLeft (juce::jmin (34, getWidth() / 4)); }
    juce::Rectangle<int> getRightArea() const  { return getLocalBounds().removeFromRight (juce::jmin (34, getWidth() / 4)); }

    static void drawArrow (juce::Graphics& g, juce::Rectangle<float> area, bool pointLeft)
    {
        const float s = juce::jmin (area.getWidth(), area.getHeight()) * 0.24f;
        auto c = area.getCentre();
        juce::Path p;
        if (pointLeft) p.addTriangle (c.x + s * 0.5f, c.y - s, c.x + s * 0.5f, c.y + s, c.x - s * 0.6f, c.y);
        else           p.addTriangle (c.x - s * 0.5f, c.y - s, c.x - s * 0.5f, c.y + s, c.x + s * 0.6f, c.y);
        g.setColour (Theme::textDim);
        g.fillPath (p);
    }

    bool dragged = false;
    float downY = 0.0f;
    double accumulated = 0.0;
};

//==============================================================================
/** Selettore a segmenti (es. "16 | 32", "STEP | VEL"). */
class SegmentedControl : public juce::Component
{
public:
    SegmentedControl (juce::StringArray optionNames) : options (std::move (optionNames)) {}

    std::function<void (int)> onChange;

    void setSelected (int index)
    {
        if (index != selected)
        {
            selected = index;
            repaint();
        }
    }

    int getSelected() const     { return selected; }
    int getNumOptions() const   { return options.size(); }

    void setOptions (const juce::StringArray& newOptions)
    {
        if (newOptions != options)
        {
            options = newOptions;
            selected = juce::jlimit (0, juce::jmax (0, options.size() - 1), selected);
            repaint();
        }
    }

    void paint (juce::Graphics& g) override
    {
        auto r = getLocalBounds().toFloat();
        Theme::drawSunken (g, r, Theme::panel.darker (0.3f));

        const float w = r.getWidth() / (float) options.size();
        for (int i = 0; i < options.size(); ++i)
        {
            auto seg = juce::Rectangle<float> (r.getX() + w * (float) i, r.getY(), w, r.getHeight());

            if (i == selected)
            {
                Theme::drawRaised (g, seg.reduced (2.0f), Theme::raisedHi, Theme::corner - 1.0f);
                Theme::drawActiveGlow (g, seg.reduced (2.0f), Theme::corner - 1.0f);
            }

            g.setColour (i == selected ? Theme::accent : Theme::textDim);
            g.setFont (Theme::font (juce::jmin (12.0f, r.getHeight() * 0.5f), true).withExtraKerningFactor (0.05f));
            g.drawText (options[i], seg, juce::Justification::centred, false);
        }
    }

    void mouseUp (const juce::MouseEvent& e) override
    {
        if (! getLocalBounds().contains (e.getPosition()))
            return;

        const int i = juce::jlimit (0, options.size() - 1, e.x * options.size() / juce::jmax (1, getWidth()));
        setSelected (i);
        if (onChange)
            onChange (i);
    }

private:
    juce::StringArray options;
    int selected = 0;
};

//==============================================================================
/** Meter verticale sottile per il livello d'uscita. */
class LevelMeter : public juce::Component
{
public:
    void setLevel (float newPeak)
    {
        const float newLevel = juce::jmax (newPeak, level * 0.85f);
        if (std::abs (newLevel - level) > 0.002f)
        {
            level = newLevel;
            repaint();
        }
    }

    void paint (juce::Graphics& g) override
    {
        auto r = getLocalBounds().toFloat();
        Theme::drawSunken (g, r, Theme::lcdBg, 2.0f);

        auto inner = r.reduced (2.0f);
        const float h = inner.getHeight() * juce::jlimit (0.0f, 1.0f, level);
        auto bar = inner.removeFromBottom (h);

        g.setGradientFill (juce::ColourGradient (juce::Colour (0xffe0483e), inner.getX(), r.getY(),
                                                 Theme::led.darker (0.2f), inner.getX(), r.getBottom(), false));
        g.fillRect (bar);
    }

private:
    float level = 0.0f;
};

//==============================================================================
/** Righello sopra gli step: numeri dei quarti, tacche, e la posizione di riproduzione. */
class StepRuler : public juce::Component
{
public:
    /** Dove sta lo step (assoluto), fornito dal channel rack; e quanti step ha il pattern. */
    std::function<juce::Rectangle<float> (int step)> getStepBounds;
    /** Trascinamento orizzontale sul righello: scorre il pattern (fase 0 inizio, 1 movimento, 2 fine). */
    std::function<void (float dx, int phase)> onScrollDrag;

    void setState (int newPlayingStep, float newFirstStep, int newLength)
    {
        if (newPlayingStep != playingStep || newFirstStep != firstStep || newLength != length)
        {
            playingStep = newPlayingStep;
            firstStep = newFirstStep;
            length = newLength;
            repaint();
        }
    }

    void paint (juce::Graphics& g) override
    {
        if (! getStepBounds)
            return;

        const float h = (float) getHeight();
        for (int step = juce::jmax (0, (int) firstStep - 1); step < length; ++step)
        {
            auto col = getStepBounds (step).withY (0.0f).withHeight (h);
            if (col.getX() > (float) getWidth())
                break;
            if (col.getRight() < 0.0f)
                continue;
            const bool beat = step % 4 == 0;

            if (step == playingStep)
            {
                g.setColour (Theme::accent);
                g.fillRect (col.removeFromBottom (3.0f));
            }
            else
            {
                g.setColour (beat ? Theme::textDim : Theme::raisedHi);
                g.fillRect (col.withTop (h - (beat ? 5.0f : 3.0f)).withSizeKeepingCentre (1.0f, beat ? 5.0f : 3.0f));
            }

            if (beat)
            {
                g.setColour (step == playingStep ? Theme::accent : Theme::textDim);
                g.setFont (Theme::font (11.0f, true));
                g.drawText (juce::String (step / 4 + 1), col.withHeight (h - 5.0f).withTrimmedLeft (2.0f),
                            juce::Justification::centredLeft, false);
            }
        }
    }

    void mouseDown (const juce::MouseEvent&) override                { if (onScrollDrag) onScrollDrag (0.0f, 0); }
    void mouseDrag (const juce::MouseEvent& e) override              { if (onScrollDrag) onScrollDrag ((float) e.getDistanceFromDragStartX(), 1); }
    void mouseUp (const juce::MouseEvent& e) override                { if (onScrollDrag) onScrollDrag ((float) e.getDistanceFromDragStartX(), 2); }

private:
    int playingStep = -1;
    float firstStep = 0.0f;
    int length = 16;
};

//==============================================================================
/**
    Numeri dei beat (quarti) in trasparenza sopra gli step, mentre si scorre il pattern:
    compaiono quando il pattern si muove e svaniscono poco dopo.
*/
class BeatWatermark : public juce::Component,
                      private juce::Timer
{
public:
    BeatWatermark()     { setInterceptsMouseClicks (false, false); }

    std::function<juce::Rectangle<float> (int step)> getStepBounds;   // in coordinate di questo componente
    int length = 16;
    float clipLeft = 0.0f;     // dove iniziano gli step (a sinistra ci sono i nomi dei canali)

    /** Il pattern si e' mosso: mostra i numeri (e li fa svanire dopo un po'). */
    void poke()
    {
        alpha = 1.0f;
        holdFrames = 18;
        if (! isTimerRunning()) startTimerHz (30);
        repaint();
    }

    void holdForTest()  { poke(); holdFrames = 1 << 30; }

    void paint (juce::Graphics& g) override
    {
        if (alpha <= 0.0f || ! getStepBounds)
            return;
        g.reduceClipRegion (getLocalBounds().withTrimmedLeft (juce::roundToInt (clipLeft)));   // solo sopra gli step
        const float size = juce::jlimit (26.0f, 64.0f, (float) getHeight() * 0.3f);
        g.setFont (Theme::font (size, true));
        for (int beat = 0; beat * 4 < length; ++beat)
        {
            const auto a = getStepBounds (beat * 4);
            const auto b = getStepBounds (juce::jmin (length, beat * 4 + 4) - 1);
            auto area = juce::Rectangle<float> (a.getX(), 2.0f, b.getRight() - a.getX(), size * 1.1f);
            if (area.getRight() < 0.0f || area.getX() > (float) getWidth())
                continue;
            g.setColour (juce::Colours::black.withAlpha (0.25f * alpha));
            g.drawText (juce::String (beat + 1), area.translated (0.0f, 2.0f), juce::Justification::centred, false);
            g.setColour (Theme::accent.withAlpha (0.5f * alpha));
            g.drawText (juce::String (beat + 1), area, juce::Justification::centred, false);
        }
    }

private:
    void timerCallback() override
    {
        if (holdFrames > 0) { --holdFrames; return; }
        alpha -= 0.08f;
        if (alpha <= 0.0f) { alpha = 0.0f; stopTimer(); }
        repaint();
    }

    float alpha = 0.0f;
    int holdFrames = 0;
};

//==============================================================================
/** Lunghezza del pattern in quarti (1..64): ◂ 4q ▸, tocca il numero per scriverla. */
class PatternLengthChip : public juce::Component
{
public:
    /** Il valore e' in step; si mostra e si sceglie in quarti (1 quarto = 4 step, da 1 a 64 quarti). */
    std::function<void (int)> onChange;

    static constexpr int stepsPerQuarter = 4, maxQuarters = 64;

    void setValue (int v)       { if (v != value) { value = v; repaint(); } }
    int getValue() const        { return value; }

    static juce::String describe (int steps)
    {
        const int q = steps / stepsPerQuarter;
        juce::String t = steps % stepsPerQuarter == 0 ? juce::String (q) + (q == 1 ? " quarto" : " quarti") : juce::String (steps) + " step";
        if (steps % 16 == 0) t << " = " << (steps / 16) << (steps == 16 ? " battuta" : " battute");
        return t;
    }

    void paint (juce::Graphics& g) override
    {
        auto r = getLocalBounds().toFloat();
        Theme::drawSunken (g, r, Theme::panel.darker (0.3f));
        const float arrowW = juce::jmin (22.0f, r.getWidth() * 0.24f);
        auto left = r.removeFromLeft (arrowW), right = r.removeFromRight (arrowW);
        g.setColour (Theme::textDim);
        g.setFont (Theme::font (juce::jmin (12.0f, r.getHeight() * 0.5f), true));
        g.drawText (juce::String (juce::CharPointer_UTF8 ("\xe2\x97\x82")), left, juce::Justification::centred, false);
        g.drawText (juce::String (juce::CharPointer_UTF8 ("\xe2\x96\xb8")), right, juce::Justification::centred, false);
        Theme::drawRaised (g, r.reduced (1.0f, 2.0f), Theme::raisedHi, Theme::corner - 1.0f);
        // numero dei quarti grande, "q" piccola (step dispari dei progetti vecchi: "7 st")
        const bool whole = value % stepsPerQuarter == 0;
        const juce::String num (whole ? value / stepsPerQuarter : value), unit (whole ? "q" : "st");
        const auto big = Theme::font (juce::jmin (14.0f, r.getHeight() * 0.56f), true), small = Theme::font (juce::jmin (10.0f, r.getHeight() * 0.4f), true);
        const float wNum = juce::GlyphArrangement::getStringWidth (big, num), wUnit = juce::GlyphArrangement::getStringWidth (small, unit);
        auto text = r.withSizeKeepingCentre (wNum + 2.0f + wUnit, r.getHeight());
        g.setColour (Theme::accent);
        g.setFont (big);
        g.drawText (num, text.removeFromLeft (wNum + 1.0f), juce::Justification::centredRight, false);
        g.setColour (Theme::textDim);
        g.setFont (small);
        g.drawText (unit, text.withTrimmedLeft (1.0f).withTrimmedTop (r.getHeight() * 0.12f), juce::Justification::centredLeft, false);
    }

    void mouseUp (const juce::MouseEvent& e) override
    {
        if (! getLocalBounds().contains (e.getPosition()))
            return;
        const float arrowW = juce::jmin (22.0f, (float) getWidth() * 0.24f);
        const int q = value / stepsPerQuarter;
        if ((float) e.x < arrowW)                           change ((value % stepsPerQuarter == 0 ? q - 1 : q) * stepsPerQuarter);
        else if ((float) e.x > (float) getWidth() - arrowW) change ((q + 1) * stepsPerQuarter);
        else                                                askValue();
    }

    /** Il "display": si scrive la lunghezza in quarti. */
    void askValue()
    {
        auto* window = new juce::AlertWindow ("Lunghezza del pattern",
                                              "Quanti quarti? (1 quarto = 4 step, 4 quarti = 1 battuta)\nDa 1 a " + juce::String (maxQuarters)
                                                  + ". Adesso: " + describe (value) + ".",
                                              juce::MessageBoxIconType::NoIcon, this);
        window->setLookAndFeel (&getLookAndFeel());
        window->addTextEditor ("q", value % stepsPerQuarter == 0 ? juce::String (value / stepsPerQuarter) : juce::String());
        if (auto* te = window->getTextEditor ("q"))
        {
            te->setInputRestrictions (2, "0123456789");
            te->setKeyboardType (juce::TextInputTarget::numericKeyboard);
            te->setFont (Theme::font (20.0f, true));
            te->setJustification (juce::Justification::centred);
            te->selectAll();
        }
        window->addButton ("OK", 1, juce::KeyPress (juce::KeyPress::returnKey));
        window->addButton ("Annulla", 0, juce::KeyPress (juce::KeyPress::escapeKey));
        juce::Component::SafePointer<PatternLengthChip> safe (this);
        window->enterModalState (true, juce::ModalCallbackFunction::create ([window, safe] (int result)
        {
            const int q = window->getTextEditorContents ("q").getIntValue();
            if (safe != nullptr && result == 1 && q > 0)
                safe->change (q * stepsPerQuarter);
        }), true);
        if (auto* te = window->getTextEditor ("q"))
            te->grabKeyboardFocus();
    }

private:
    void change (int v)
    {
        v = juce::jlimit (stepsPerQuarter, maxQuarters * stepsPerQuarter, v);
        if (v == value) return;
        value = v;
        repaint();
        if (onChange) onChange (v);
    }

    int value = 16;
};

//==============================================================================
/** Pulsante con un'icona disegnata (undo, redo, metronomo). */
class IconButton : public juce::Button,
                   private juce::Timer
{
public:
    enum class Icon { undo, redo, metronome, menu };

    explicit IconButton (Icon i) : juce::Button ("icon"), icon (i) {}

    /** Tenendo premuto (mezzo secondo): es. la cronologia delle modifiche. */
    std::function<void()> onLongPress;

    /** true se il tocco appena finito era una pressione lunga (il clic va ignorato). */
    bool takeLongPress()        { const bool was = longPressFired; longPressFired = false; return was; }

    void mouseDown (const juce::MouseEvent& e) override
    {
        longPressFired = false;
        if (onLongPress != nullptr)
            startTimer (550);
        juce::Button::mouseDown (e);
    }

    void mouseUp (const juce::MouseEvent& e) override
    {
        stopTimer();
        juce::Button::mouseUp (e);
    }

    void mouseDrag (const juce::MouseEvent& e) override
    {
        if (e.getDistanceFromDragStart() > 12)
            stopTimer();
        juce::Button::mouseDrag (e);
    }

    void paintButton (juce::Graphics& g, bool isHighlighted, bool isDown) override
    {
        auto r = getLocalBounds().toFloat();
        auto base = Theme::raised;
        if (isDown)             base = base.darker (0.25f);
        else if (isHighlighted) base = base.brighter (0.06f);
        Theme::drawRaised (g, r, base);

        const bool on = getToggleState();
        const auto colour = ! isEnabled() ? Theme::textDim.withAlpha (0.4f) : (on ? Theme::accent : Theme::textBright);
        const float s = juce::jmin (r.getWidth(), r.getHeight()) * 0.5f;
        auto box = juce::Rectangle<float> (s, s).withCentre (r.getCentre());
        g.setColour (colour);

        if (icon == Icon::menu)
        {
            for (int i = -1; i <= 1; ++i)
                g.fillRoundedRectangle (box.getX() + s * 0.1f, box.getCentreY() + (float) i * s * 0.3f - 1.1f, s * 0.8f, 2.2f, 1.1f);
        }
        else if (icon == Icon::metronome)
        {
            juce::Path p;
            p.addTriangle (box.getCentreX(), box.getY(), box.getX() + s * 0.12f, box.getBottom(), box.getRight() - s * 0.12f, box.getBottom());
            g.strokePath (p, juce::PathStrokeType (1.6f));
            g.drawLine (box.getCentreX(), box.getBottom() - s * 0.2f, box.getRight() - s * 0.05f, box.getY() + s * 0.15f, 1.8f);
        }
        else
        {
            // Freccia curva: arco + punta
            const bool undo = icon == Icon::undo;
            juce::Path arc;
            const float rad = s * 0.42f;
            const auto c = box.getCentre().translated (0.0f, s * 0.08f);
            arc.addCentredArc (c.x, c.y, rad, rad, 0.0f,
                               undo ? -juce::MathConstants<float>::halfPi * 1.4f : juce::MathConstants<float>::halfPi * 1.4f,
                               undo ? juce::MathConstants<float>::halfPi * 0.9f : -juce::MathConstants<float>::halfPi * 0.9f, true);
            g.strokePath (arc, juce::PathStrokeType (1.8f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));

            const auto start = arc.getPointAlongPath (0.0f);
            const float dir = undo ? -1.0f : 1.0f;
            juce::Path head;
            head.addTriangle (start.x + dir * s * 0.02f - dir * s * 0.24f, start.y,
                              start.x + dir * s * 0.16f, start.y - s * 0.17f,
                              start.x + dir * s * 0.16f, start.y + s * 0.17f);
            g.fillPath (head);
        }
    }

private:
    void timerCallback() override
    {
        stopTimer();
        longPressFired = true;
        if (onLongPress != nullptr)
            onLongPress();
    }

    Icon icon;
    bool longPressFired = false;
};

//==============================================================================
/** Interruttore acceso/spento in stile hardware: pastiglia con LED verde e scritta ON/OFF. */
class LedToggle : public juce::Button
{
public:
    LedToggle() : juce::Button ("toggle") {}

    void paintButton (juce::Graphics& g, bool isHighlighted, bool isDown) override
    {
        auto r = getLocalBounds().toFloat();
        const bool on = getToggleState();
        auto base = Theme::raised;
        if (isDown)             base = base.darker (0.25f);
        else if (isHighlighted) base = base.brighter (0.06f);
        Theme::drawRaised (g, r, base);

        const float d = juce::jmin (r.getHeight() * 0.38f, 12.0f);
        auto led = juce::Rectangle<float> (d, d).withCentre ({ r.getX() + r.getHeight() * 0.55f, r.getCentreY() });
        if (on)
        {
            g.setColour (Theme::led.withAlpha (0.25f));
            g.fillEllipse (led.expanded (d * 0.4f));
        }
        g.setColour (on ? Theme::led : Theme::bg);
        g.fillEllipse (led);

        g.setColour (on ? Theme::textBright : Theme::textDim);
        g.setFont (Theme::font (juce::jmin (14.0f, r.getHeight() * 0.42f), true));
        g.drawText (on ? "ON" : "OFF", r.withTrimmedLeft (led.getRight() - r.getX() + 4.0f), juce::Justification::centred, false);
    }
};

//==============================================================================
/** Quanto lavora il processore per l'audio (e se ci sono stati scatti). Toccalo per le opzioni. */
class CpuBadge : public juce::Component
{
public:
    std::function<void()> onClick;

    void setState (float newLoad, bool newWarning)
    {
        newLoad = juce::jlimit (0.0f, 1.0f, newLoad);
        if (std::abs (newLoad - load) > 0.01f || newWarning != warning)
        {
            load = newLoad;
            warning = newWarning;
            repaint();
        }
    }

    void setLowPower (bool on)  { if (on != lowPower) { lowPower = on; repaint(); } }

    void paint (juce::Graphics& g) override
    {
        auto r = getLocalBounds().toFloat().reduced (1.0f, 1.0f);
        Theme::drawSunken (g, r, Theme::lcdBg, Theme::corner);
        const auto colour = load < 0.6f ? Theme::led : load < 0.85f ? juce::Colour (0xffe8c547) : juce::Colour (0xffe0483e);

        auto inner = r.reduced (5.0f, 2.0f);
        auto bar = inner.removeFromBottom (3.0f);
        g.setColour (Theme::raised);
        g.fillRoundedRectangle (bar, 2.0f);
        g.setColour (colour);
        g.fillRoundedRectangle (bar.withWidth (juce::jmax (2.0f, bar.getWidth() * load)), 2.0f);

        g.setColour (warning ? juce::Colour (0xffe0483e) : Theme::text);
        g.setFont (Theme::font (juce::jmin (12.0f, inner.getHeight() * 0.7f), true));
        g.drawFittedText ((lowPower ? "ECO " : "CPU ") + juce::String (juce::roundToInt (load * 100.0f)) + "%" + (warning ? " !" : ""),
                          inner.toNearestInt(), juce::Justification::centred, 1);
    }

    void mouseUp (const juce::MouseEvent& e) override
    {
        if (! e.mouseWasDraggedSinceMouseDown() && onClick)
            onClick();
    }

private:
    float load = 0.0f;
    bool warning = false, lowPower = false;
};

//==============================================================================
/** Pulsante di registrazione: cerchio rosso; acceso = lampeggia mentre registra. */
class RecordButton : public juce::Button
{
public:
    RecordButton() : juce::Button ("Rec") {}

    void setBlink (bool on)     { if (on != blink) { blink = on; repaint(); } }

    void paintButton (juce::Graphics& g, bool isHighlighted, bool isDown) override
    {
        auto r = getLocalBounds().toFloat();
        const bool armed = getToggleState();
        const auto red = juce::Colour (0xffe0483e);
        drawRoundTransport (g, r, isHighlighted, isDown, red, armed);

        const float d = juce::jmin (r.getHeight(), r.getWidth()) * 0.3f;
        auto dot = juce::Rectangle<float> (d, d).withCentre (r.getCentre());
        if (armed && blink)
        {
            g.setColour (red.withAlpha (0.3f));
            g.fillEllipse (dot.expanded (d * 0.35f));
        }
        g.setColour (armed ? red : red.withMultipliedSaturation (0.5f).withMultipliedBrightness (0.7f));
        g.fillEllipse (dot);
    }

private:
    bool blink = false;
};
