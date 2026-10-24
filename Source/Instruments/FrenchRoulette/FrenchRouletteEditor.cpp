#include "FrenchRouletteEditor.h"

namespace frui
{
    namespace c = colours;
    static void drawNeon (juce::Graphics&, const juce::String&, juce::Rectangle<float>, float, juce::Justification, juce::Colour);
    juce::Font mono (float size, bool bold);

    /** Didascalia fissa stile LCD (es. "TUBE <> BITS" sotto CRUNCH). */
    class Caption : public juce::Component
    {
    public:
        explicit Caption (juce::String t) : text (std::move (t)) { setInterceptsMouseClicks (false, false); }
        void paint (juce::Graphics& g) override
        {
            auto r = getLocalBounds().toFloat().reduced (0.5f);
            g.setColour (c::lcdBg);
            g.fillRoundedRectangle (r, 4.0f);
            g.setColour (juce::Colours::black);
            g.drawRoundedRectangle (r, 4.0f, 1.0f);
            g.setFont (mono (juce::jlimit (9.0f, 13.0f, r.getHeight() * 0.55f), true));
            drawNeon (g, text, r, 0.5f, juce::Justification::centred, juce::Colour (0xffff5050));
        }
    private:
        juce::String text;
    };

    juce::Font mono (float size, bool bold)
    {
        return juce::Font (juce::FontOptions (juce::Font::getDefaultMonospacedFontName(), size, bold ? juce::Font::bold : juce::Font::plain));
    }

    juce::Font sans (float size, bool bold)
    {
        return juce::Font (juce::FontOptions (size, bold ? juce::Font::bold : juce::Font::plain));
    }

    /** Testo con lettere distanziate (stile pannello hardware). */
    static void drawSpaced (juce::Graphics& g, const juce::String& text, juce::Rectangle<float> area, float spacing, juce::Justification just)
    {
        juce::GlyphArrangement ga;
        ga.addLineOfText (g.getCurrentFont(), text, 0.0f, 0.0f);
        float width = 0.0f;
        for (int i = 0; i < ga.getNumGlyphs(); ++i) width += ga.getGlyph (i).getRight() - ga.getGlyph (i).getLeft() + spacing;
        width -= spacing;
        float x = just.testFlags (juce::Justification::horizontallyCentred) ? area.getCentreX() - width * 0.5f
                : just.testFlags (juce::Justification::right) ? area.getRight() - width : area.getX();
        const float y = area.getCentreY() + g.getCurrentFont().getAscent() * 0.5f - g.getCurrentFont().getDescent() * 0.35f;
        for (int i = 0; i < text.length(); ++i)
        {
            const auto ch = juce::String::charToString (text[i]);
            g.drawSingleLineText (ch, juce::roundToInt (x), juce::roundToInt (y));
            x += juce::GlyphArrangement::getStringWidth (g.getCurrentFont(), ch) + spacing;
        }
    }

    /** Testo rosso "al neon": un alone sotto e il testo sopra. */
    static void drawNeon (juce::Graphics& g, const juce::String& text, juce::Rectangle<float> area, float spacing, juce::Justification just, juce::Colour colour)
    {
        g.setColour (colour.withAlpha (0.18f));
        for (auto d : { juce::Point<float> (-1.2f, 0.0f), { 1.2f, 0.0f }, { 0.0f, -1.2f }, { 0.0f, 1.2f } })
            drawSpaced (g, text, area.translated (d.x, d.y), spacing, just);
        g.setColour (colour);
        drawSpaced (g, text, area, spacing, just);
    }

    //==========================================================================
    LookAndFeel::LookAndFeel()
    {
        setColour (juce::PopupMenu::backgroundColourId, c::panelTop);
        setColour (juce::PopupMenu::textColourId, c::text);
        setColour (juce::PopupMenu::highlightedBackgroundColourId, c::red.withAlpha (0.35f));
        setColour (juce::PopupMenu::highlightedTextColourId, juce::Colours::white);
    }

    void LookAndFeel::drawRotarySlider (juce::Graphics& g, int x, int y, int w, int h, float pos, float a0, float a1, juce::Slider& s)
    {
        const float size = (float) juce::jmin (w, h);
        const auto centre = juce::Rectangle<float> ((float) x, (float) y, (float) w, (float) h).getCentre();
        const float r = size * 0.5f - 2.0f;
        if (r < 6.0f) return;
        const float thick = juce::jmax (2.5f, r * 0.12f);
        const float arcR = r - thick * 0.5f;
        const float ang = a0 + pos * (a1 - a0);

        // corpo
        const float br = r - thick - 2.5f;
        g.setColour (juce::Colours::black.withAlpha (0.5f));
        g.fillEllipse (centre.x - br, centre.y - br + 2.0f, br * 2.0f, br * 2.0f);
        g.setGradientFill (juce::ColourGradient (juce::Colour (0xff30363d), centre.x, centre.y - br, juce::Colour (0xff121518), centre.x, centre.y + br, false));
        g.fillEllipse (centre.x - br, centre.y - br, br * 2.0f, br * 2.0f);

        juce::Path track;
        track.addCentredArc (centre.x, centre.y, arcR, arcR, 0.0f, a0, a1, true);
        g.setColour (c::track);
        g.strokePath (track, juce::PathStrokeType (thick, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));

        const bool bipolar = s.getMinimum() < 0.0 && s.getMaximum() > 0.0;
        const float from = a0 + (bipolar ? (float) s.valueToProportionOfLength (0.0) : 0.0f) * (a1 - a0);
        juce::Path val;
        val.addCentredArc (centre.x, centre.y, arcR, arcR, 0.0f, juce::jmin (from, ang), juce::jmax (from, ang), true);
        g.setColour (c::red.withAlpha (0.22f));
        g.strokePath (val, juce::PathStrokeType (thick + 5.0f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
        g.setColour (c::red);
        g.strokePath (val, juce::PathStrokeType (thick, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));

        const float i0 = br * 0.35f, i1 = br - 2.0f;
        g.setColour (c::text);
        g.drawLine ({ centre.x + std::sin (ang) * i0, centre.y - std::cos (ang) * i0, centre.x + std::sin (ang) * i1, centre.y - std::cos (ang) * i1 },
                    juce::jmax (1.5f, r * 0.06f));
    }

    juce::Slider::SliderLayout LookAndFeel::getSliderLayout (juce::Slider& s)
    {
        juce::Slider::SliderLayout l;
        l.sliderBounds = s.getLocalBounds();
        return l;
    }

    juce::Label* LookAndFeel::createSliderTextBox (juce::Slider& s)
    {
        auto* l = LookAndFeel_V4::createSliderTextBox (s);
        l->setVisible (false);
        return l;
    }

    void LookAndFeel::drawPopupMenuBackground (juce::Graphics& g, int w, int h)
    {
        g.fillAll (c::panelTop);
        g.setColour (c::red.withAlpha (0.5f));
        g.drawRect (0, 0, w, h);
    }

    //==========================================================================
    void Panel::paint (juce::Graphics& g)
    {
        auto r = getLocalBounds().toFloat().reduced (1.0f);
        g.setGradientFill (juce::ColourGradient (c::panelTop, 0.0f, r.getY(), c::panelBot, 0.0f, r.getBottom(), false));
        g.fillRoundedRectangle (r, 9.0f);
        g.setColour (c::edge);
        g.drawRoundedRectangle (r, 9.0f, 1.2f);
        g.setColour (juce::Colours::white.withAlpha (0.04f));
        g.drawHorizontalLine ((int) r.getY() + 1, r.getX() + 8.0f, r.getRight() - 8.0f);

        if (showTitle && title.isNotEmpty())
        {
            g.setColour (c::red);
            g.fillRect (r.getX() + 10.0f, r.getY() + 9.0f, 3.0f, 11.0f);
            g.setColour (c::textDim);
            g.setFont (sans (11.0f));
            drawSpaced (g, title.toUpperCase(), { r.getX() + 19.0f, r.getY() + 6.0f, r.getWidth() - 25.0f, 17.0f }, 1.6f, juce::Justification::left);
        }
    }

    juce::Rectangle<int> Panel::content() const
    {
        auto r = getLocalBounds().reduced (7);
        if (showTitle && title.isNotEmpty())
            r.removeFromTop (20);
        return r;
    }

    //==========================================================================
    Knob::Knob (juce::AudioProcessorValueTreeState& apvts, const juce::String& id, const juce::String& t, bool isBig)
        : title (t), big (isBig)
    {
        slider.setSliderStyle (juce::Slider::RotaryHorizontalVerticalDrag);
        slider.setRotaryParameters (juce::MathConstants<float>::pi * 1.25f, juce::MathConstants<float>::pi * 2.75f, true);
        slider.setTextBoxStyle (juce::Slider::NoTextBox, false, 0, 0);
        slider.setMouseDragSensitivity (big ? 260 : 200);
        slider.onValueChange = [this] { repaint(); };
        slider.onDragStart = slider.onDragEnd = [this] { repaint(); };
        addAndMakeVisible (slider);
        attachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (apvts, id, slider);
        if (auto* p = apvts.getParameter (id))
            slider.setDoubleClickReturnValue (true, (double) p->convertFrom0to1 (p->getDefaultValue()));
        slider.textFromValueFunction = nullptr;
    }

    void Knob::resized()
    {
        // Manopola + nome in un blocco centrato; misura massima per non avere manopole giganti
        const int labelH = big ? 18 : 15;
        auto b = getLocalBounds();
        const int s = juce::jmin (b.getWidth(), b.getHeight() - labelH, big ? 150 : 82);
        auto block = b.withSizeKeepingCentre (b.getWidth(), s + labelH + 2);
        slider.setBounds (block.removeFromTop (s).withSizeKeepingCentre (s, s));
        labelArea = block.withTrimmedTop (2);
    }

    void Knob::paint (juce::Graphics& g)
    {
        auto label = labelArea.toFloat();
        const bool dragging = slider.isMouseButtonDown();
        if (dragging)
        {
            g.setColour (c::text);
            g.setFont (sans (big ? 13.0f : 11.0f));
            g.drawText (slider.getTextFromValue (slider.getValue()), label, juce::Justification::centred, false);
        }
        else
        {
            g.setColour (c::textDim);
            g.setFont (sans (big ? 12.0f : 9.5f));
            const float spacing = big ? 2.0f : (label.getWidth() < 60.0f ? 0.2f : 1.2f);
            if (label.getWidth() < 52.0f) g.setFont (sans (8.5f));
            drawSpaced (g, title.toUpperCase(), label, spacing, juce::Justification::centred);
        }
    }

    //==========================================================================
    static juce::RangedAudioParameter& paramRef (juce::AudioProcessorValueTreeState& apvts, const juce::String& id)
    {
        auto* p = apvts.getParameter (id);
        jassert (p != nullptr);
        return *p;
    }

    static int numChoices (juce::RangedAudioParameter& p)
    {
        if (auto* ch = dynamic_cast<juce::AudioParameterChoice*> (&p)) return ch->choices.size();
        if (dynamic_cast<juce::AudioParameterBool*> (&p) != nullptr) return 2;
        return juce::jmax (2, (int) std::round (p.getNormalisableRange().getRange().getLength()) + 1);
    }

    Segment::Segment (juce::AudioProcessorValueTreeState& apvts, const juce::String& id, juce::StringArray l, juce::Colour on)
        : param (paramRef (apvts, id)), labels (std::move (l)), onColour (on),
          attachment (param, [this] (float v) { value = juce::roundToInt (v - param.getNormalisableRange().start); repaint(); })
    {
        if (labels.isEmpty())
            if (auto* ch = dynamic_cast<juce::AudioParameterChoice*> (&param))
                labels = ch->choices;
        attachment.sendInitialUpdate();
    }

    void Segment::paint (juce::Graphics& g)
    {
        auto r = getLocalBounds().toFloat().reduced (0.5f);
        g.setColour (juce::Colour (0xff0b0d10));
        g.fillRoundedRectangle (r, 5.0f);
        g.setColour (c::edge);
        g.drawRoundedRectangle (r, 5.0f, 1.0f);
        const int n = juce::jmax (1, labels.size());
        const float w = (r.getWidth() - 4.0f) / (float) n;
        g.setFont (sans (juce::jlimit (8.5f, 12.5f, juce::jmin (r.getHeight() * 0.45f, w * 0.26f))));
        for (int i = 0; i < n; ++i)
        {
            auto cell = juce::Rectangle<float> (r.getX() + 2.0f + w * (float) i, r.getY() + 2.0f, w, r.getHeight() - 4.0f).reduced (1.0f, 0.0f);
            const bool on = i == value;
            if (on)
            {
                g.setColour (onColour.withAlpha (0.25f));
                g.fillRoundedRectangle (cell.expanded (1.5f), 4.0f);
                g.setColour (onColour);
                g.fillRoundedRectangle (cell, 3.5f);
            }
            g.setColour (on ? juce::Colour (0xff0b0d10) : c::textDim);
            g.drawFittedText (labels[i], cell.toNearestInt(), juce::Justification::centred, 1, 0.7f);
        }
    }

    void Segment::mouseDown (const juce::MouseEvent& e)
    {
        const int n = juce::jmax (1, labels.size());
        const int i = juce::jlimit (0, n - 1, (int) (e.position.x / (float) getWidth() * (float) n));
        attachment.setValueAsCompleteGesture (param.getNormalisableRange().start + (float) i);
    }

    //==========================================================================
    Lcd::Lcd (juce::AudioProcessorValueTreeState& apvts, const juce::String& id, juce::String p)
        : param (paramRef (apvts, id)), prefix (std::move (p)),
          attachment (param, [this] (float v) { value = juce::roundToInt (v); repaint(); })
    {
        attachment.sendInitialUpdate();
    }

    void Lcd::paint (juce::Graphics& g)
    {
        auto r = getLocalBounds().toFloat().reduced (0.5f);
        g.setColour (c::lcdBg);
        g.fillRoundedRectangle (r, 4.0f);
        g.setColour (juce::Colours::black);
        g.drawRoundedRectangle (r, 4.0f, 1.0f);
        const float fs = juce::jlimit (10.0f, 15.0f, r.getHeight() * 0.55f);
        g.setFont (mono (fs));
        juce::String text = param.getCurrentValueAsText().toUpperCase();
        if (auto* ch = dynamic_cast<juce::AudioParameterChoice*> (&param))
            text = ch->choices[value].toUpperCase();
        auto inner = r.reduced (4.0f, 0.0f);
        const float arrowW = juce::jmin (16.0f, inner.getWidth() * 0.15f);
        auto left = inner.removeFromLeft (arrowW), right = inner.removeFromRight (arrowW);
        g.setColour (c::red.withAlpha (0.7f));
        g.drawText (juce::CharPointer_UTF8 ("\xe2\x97\x82"), left, juce::Justification::centred, false);
        g.drawText (juce::CharPointer_UTF8 ("\xe2\x96\xb8"), right, juce::Justification::centred, false);
        g.setFont (mono (fs));
        drawNeon (g, prefix + text, inner, 0.5f, juce::Justification::centred, juce::Colour (0xffff5050));
    }

    void Lcd::mouseDown (const juce::MouseEvent& e)
    {
        const int n = numChoices (param);
        const int dir = e.position.x < (float) getWidth() * 0.4f ? -1 : 1;
        attachment.setValueAsCompleteGesture ((float) ((value + dir + n) % n));
    }

    //==========================================================================
    MatrixView::MatrixView (FrenchRouletteAudioProcessor& p) : proc (p) {}

    float MatrixView::get (char laneId, int step) const
    {
        if (auto* v = proc.apvts.getRawParameterValue (fr::stepId (laneId, step)))
            return v->load();
        return 0.0f;
    }

    void MatrixView::set (char laneId, int step, float value)
    {
        if (auto* prm = proc.apvts.getParameter (fr::stepId (laneId, step)))
        {
            if (std::find (gesture.begin(), gesture.end(), prm) == gesture.end())
            {
                prm->beginChangeGesture();
                gesture.push_back (prm);
            }
            prm->setValueNotifyingHost (prm->convertTo0to1 (value));
        }
        repaint();
    }

    juce::uint64 MatrixView::signature() const
    {
        juce::uint64 h = (juce::uint64) lane * 7919u + (chopFlash ? 13u : 0u);
        for (char l : { 'g', 'c', 'x', 'p', 'l' })
            for (int s = 0; s < fr::numSteps; ++s)
                h = h * 1000003u + (juce::uint64) juce::roundToInt (get (l, s) * 1000.0f + 50000.0f);
        h = h * 31u + (juce::uint64) juce::roundToInt (proc.apvts.getRawParameterValue ("matLen")->load());
        h = h * 31u + (proc.apvts.getRawParameterValue ("matOn")->load() > 0.5f ? 1u : 0u);
        return h;
    }

    void MatrixView::refresh()
    {
        const int step = proc.getCurrentStep();
        const auto sig = signature();
        if (sig != shown || step != shownStep)
        {
            shown = sig;
            shownStep = step;
            repaint();
        }
    }

    void MatrixView::resized()
    {
        auto r = getLocalBounds().toFloat().reduced (8.0f, 6.0f);
        auto top = r.removeFromTop (juce::jlimit (20.0f, 28.0f, r.getHeight() * 0.13f));
        chopButton = top.removeFromRight (juce::jmin (150.0f, top.getWidth() * 0.32f));
        laneTabs = top.withTrimmedRight (8.0f);
        const float tw = juce::jmin (84.0f, laneTabs.getWidth() / 4.0f);
        for (int i = 0; i < 4; ++i)
            laneButton[(size_t) i] = juce::Rectangle<float> (laneTabs.getX() + tw * (float) i, laneTabs.getY(), tw - 4.0f, laneTabs.getHeight());
        r.removeFromTop (6.0f);
        crushRow = r.removeFromBottom (4.0f);
        r.removeFromBottom (3.0f);
        labelRow = r.removeFromBottom (juce::jlimit (12.0f, 17.0f, r.getHeight() * 0.1f));
        r.removeFromBottom (3.0f);
        gateRow = r.removeFromTop (juce::jlimit (6.0f, 10.0f, r.getHeight() * 0.06f));
        r.removeFromTop (3.0f);
        barArea = r;
    }

    int MatrixView::stepAt (float x) const
    {
        return juce::jlimit (0, fr::numSteps - 1, (int) ((x - barArea.getX()) / barArea.getWidth() * (float) fr::numSteps));
    }

    void MatrixView::paint (juce::Graphics& g)
    {
        auto screen = getLocalBounds().toFloat().reduced (0.5f);
        g.setColour (c::screen);
        g.fillRoundedRectangle (screen, 7.0f);
        g.setColour (c::green.withAlpha (0.06f));
        g.drawRoundedRectangle (screen.reduced (2.0f), 6.0f, 3.0f);
        g.setColour (juce::Colours::black);
        g.drawRoundedRectangle (screen, 7.0f, 1.2f);

        const juce::String laneNames[] { "GATE", "CUTOFF", "CRUSH", "PITCH" };
        g.setFont (sans (juce::jlimit (9.0f, 12.0f, laneTabs.getHeight() * 0.45f)));
        for (int i = 0; i < 4; ++i)
        {
            auto b = laneButton[(size_t) i];
            const bool on = i == lane;
            g.setColour (on ? c::green : c::cell);
            g.fillRoundedRectangle (b, 4.0f);
            g.setColour (on ? juce::Colour (0xff04140a) : c::textDim);
            drawSpaced (g, laneNames[i], b, 1.2f, juce::Justification::centred);
        }

        // Random Chop
        g.setColour (chopFlash ? c::green.withAlpha (0.4f) : c::green.withAlpha (0.08f));
        g.fillRoundedRectangle (chopButton, 5.0f);
        g.setColour (c::green);
        g.drawRoundedRectangle (chopButton.reduced (0.5f), 5.0f, 1.3f);
        drawSpaced (g, juce::String (juce::CharPointer_UTF8 ("\xe2\x9a\x84 ")) + "RANDOM CHOP", chopButton, 1.0f, juce::Justification::centred);

        const int len = juce::roundToInt (proc.apvts.getRawParameterValue ("matLen")->load());
        const bool active = proc.apvts.getRawParameterValue ("matOn")->load() > 0.5f;
        const int playing = proc.getCurrentStep();
        const float colW = barArea.getWidth() / (float) fr::numSteps;
        const float gap = juce::jmax (2.0f, colW * 0.08f);

        for (int s = 0; s < fr::numSteps; ++s)
        {
            const float x = barArea.getX() + colW * (float) s + gap * 0.5f, w = colW - gap;
            const bool inLen = s < len;
            const float dim = inLen && active ? 1.0f : 0.35f;
            const bool gate = get ('g', s) > 0.5f;

            // LED del gate
            auto led = juce::Rectangle<float> (x, gateRow.getY(), w, gateRow.getHeight());
            g.setColour (gate ? c::green.withAlpha (dim) : c::cell);
            g.fillRoundedRectangle (led, 2.0f);
            if (gate && dim > 0.5f)
            {
                g.setColour (c::green.withAlpha (0.25f));
                g.drawRoundedRectangle (led.expanded (1.0f), 2.5f, 1.5f);
            }

            // Colonna
            auto col = juce::Rectangle<float> (x, barArea.getY(), w, barArea.getHeight());
            g.setColour (s % 4 == 0 ? juce::Colour (0xff0b1b11) : c::cell);
            g.fillRoundedRectangle (col, 3.0f);
            if (s == playing && active)
            {
                g.setColour (c::green.withAlpha (0.16f));
                g.fillRoundedRectangle (col, 3.0f);
            }

            auto drawBar = [&] (juce::Rectangle<float> bar, juce::Colour colour)
            {
                if (bar.getHeight() < 1.0f) return;
                g.setGradientFill (juce::ColourGradient (colour.withAlpha (0.35f * dim), 0.0f, bar.getBottom(), colour.withAlpha (dim), 0.0f, bar.getY(), false));
                g.fillRoundedRectangle (bar, 2.0f);
            };

            if (lane == 0)
            {
                if (gate) drawBar (col.reduced (0.0f, 1.0f), c::green);
            }
            else if (lane == 1)
                drawBar (col.withTop (col.getBottom() - col.getHeight() * get ('c', s)), c::green);
            else if (lane == 2)
                drawBar (col.withTop (col.getBottom() - col.getHeight() * get ('x', s)), c::red);
            else
            {
                const float v = get ('p', s) / 24.0f, mid = col.getCentreY();
                g.setColour (c::green.withAlpha (0.25f * dim));
                g.fillRect (col.getX(), mid - 0.5f, col.getWidth(), 1.0f);
                auto bar = v >= 0.0f ? juce::Rectangle<float> (col.getX(), mid - col.getHeight() * 0.5f * v, w, col.getHeight() * 0.5f * v)
                                     : juce::Rectangle<float> (col.getX(), mid, w, col.getHeight() * 0.5f * -v);
                g.setColour (c::green.withAlpha (dim));
                g.fillRoundedRectangle (bar, 2.0f);
            }

            // Etichetta: semitoni e glide
            auto lab = juce::Rectangle<float> (x, labelRow.getY(), w, labelRow.getHeight());
            g.setColour (c::cell);
            g.fillRoundedRectangle (lab, 2.0f);
            const int pitch = juce::roundToInt (get ('p', s));
            const bool glide = get ('l', s) > 0.5f;
            juce::String t = pitch == 0 ? juce::String (juce::CharPointer_UTF8 ("\xc2\xb7")) : (pitch > 0 ? "+" : "") + juce::String (pitch);
            if (glide) t << juce::String (juce::CharPointer_UTF8 ("\xe2\x86\x97"));
            g.setColour ((glide ? c::green : c::text.withAlpha (0.75f)).withMultipliedAlpha (dim));
            g.setFont (sans (juce::jlimit (8.0f, 11.0f, w * 0.32f)));
            g.drawFittedText (t, lab.toNearestInt(), juce::Justification::centred, 1, 0.6f);

            // Crush
            const float cr = get ('x', s);
            g.setColour (c::red.withAlpha ((0.15f + 0.85f * cr) * dim));
            g.fillRoundedRectangle (juce::Rectangle<float> (x, crushRow.getY(), w, crushRow.getHeight()), 1.5f);
        }

        if (! active)
        {
            g.setColour (c::green.withAlpha (0.8f));
            g.setFont (sans (12.0f));
            drawSpaced (g, "MATRIX SPENTA: TOCCA ON", barArea.withHeight (20.0f).withCentre (barArea.getCentre()), 2.0f, juce::Justification::centred);
        }
    }

    void MatrixView::edit (juce::Point<float> p, bool first)
    {
        if (first)
        {
            dragMode = 0;
            if (gateRow.expanded (0.0f, 3.0f).contains (p) || (lane == 0 && barArea.contains (p)))
            {
                dragMode = 2;
                gateDragValue = get ('g', stepAt (p.x)) > 0.5f ? 0.0f : 1.0f;
            }
            else if (labelRow.expanded (0.0f, 3.0f).contains (p))
                dragMode = 3;
            else if (barArea.contains (p))
                dragMode = 1;
            lastEditStep = -1;
        }

        const int s = stepAt (p.x);
        if (dragMode == 2)
        {
            if (s != lastEditStep) set ('g', s, gateDragValue);
        }
        else if (dragMode == 3)
        {
            if (s != lastEditStep) set ('l', s, get ('l', s) > 0.5f ? 0.0f : 1.0f);
        }
        else if (dragMode == 1)
        {
            const float rel = juce::jlimit (0.0f, 1.0f, (barArea.getBottom() - p.y) / barArea.getHeight());
            if (lane == 1)      set ('c', s, rel);
            else if (lane == 2) set ('x', s, rel);
            else if (lane == 3) set ('p', s, (float) juce::roundToInt ((rel * 2.0f - 1.0f) * 24.0f));
        }
        lastEditStep = s;
    }

    void MatrixView::mouseDown (const juce::MouseEvent& e)
    {
        for (int i = 0; i < 4; ++i)
            if (laneButton[(size_t) i].contains (e.position)) { lane = i; repaint(); return; }

        if (chopButton.contains (e.position))
        {
            proc.getPresets().randomChop ((uint32_t) juce::Time::getMillisecondCounter());
            chopFlash = true;
            repaint();
            juce::Timer::callAfterDelay (180, [safe = juce::Component::SafePointer<MatrixView> (this)] { if (safe != nullptr) { safe->chopFlash = false; safe->repaint(); } });
            return;
        }

        if (e.getNumberOfClicks() >= 2 && barArea.contains (e.position) && lane > 0)
        {
            const int s = stepAt (e.position.x);   // doppio tocco: valore di partenza
            set (lane == 1 ? 'c' : lane == 2 ? 'x' : 'p', s, lane == 1 ? 1.0f : 0.0f);
            dragMode = 0;
            return;
        }
        edit (e.position, true);
    }

    void MatrixView::mouseDrag (const juce::MouseEvent& e)
    {
        if (dragMode == 3) return;    // il glide si cambia solo toccando
        if (dragMode != 0) edit (e.position, false);
    }

    void MatrixView::mouseUp (const juce::MouseEvent&)
    {
        for (auto* prm : gesture) prm->endChangeGesture();
        gesture.clear();
        dragMode = 0;
    }
}

//==============================================================================
FrenchRouletteEditor::FrenchRouletteEditor (FrenchRouletteAudioProcessor& p)
    : juce::AudioProcessorEditor (&p), proc (p)
{
    setLookAndFeel (&laf);
    auto& apvts = proc.apvts;

    const juce::StringArray tabNames { "SUONO", "FILTRO", "LFO / FX", "MATRIX", "CRUNCH" };
    sections.resize ((size_t) tabNames.size());
    for (int i = 0; i < tabNames.size(); ++i)
    {
        auto* t = tabs.add (new juce::TextButton (tabNames[i]));
        t->setClickingTogglesState (true);
        t->setRadioGroupId (7411);
        t->onClick = [this, i] { showTab (i); };
        addAndMakeVisible (t);
    }

    // --- SUONO
    {
        auto& o1 = section (0, "Osc 1 - Analog");
        segment (o1, "o1Wave", { "SAW", "SQUARE", "PULSE" });
        knob (o1, "o1Pw", "PW"); knob (o1, "o1Pwm", "PWM"); knob (o1, "o1Drift", "Drift");
        knob (o1, "o1Oct", "Ottava"); knob (o1, "o1Level", "Livello");
        auto& o2 = section (0, "Osc 2 - Digital");
        lcd (o2, "o2Table");
        knob (o2, "o2Wave", "Wave"); knob (o2, "o2Fm", "FM"); knob (o2, "o2Semi", "Semi");
        knob (o2, "o2Fine", "Fine"); knob (o2, "o2Level", "Livello");
        auto& sub = section (0, "Sub", 0.55f, 1);
        segment (sub, "subOct", { "-1", "-2" });
        knob (sub, "subLevel", "Livello"); knob (sub, "subDrive", "Drive");
        auto& voce = section (0, "Voce", 0.55f, 1);
        segment (voce, "mode", { "MONO", "LEG", "POLY" });
        knob (voce, "glide", "Glide");
    }
    // --- FILTRO
    {
        auto& f = section (1, "Filtro ladder");
        segment (f, "fType", { "LP24", "LP12", "BP", "HP" });
        knob (f, "cutoff", "Cutoff"); knob (f, "reso", "Reso"); knob (f, "fEnv", "Env"); knob (f, "fKey", "Key");
        auto& fe = section (1, "Inviluppo filtro");
        knob (fe, "fA", "A"); knob (fe, "fD", "D"); knob (fe, "fS", "S"); knob (fe, "fR", "R");
        auto& a = section (1, "Ampiezza");
        knob (a, "aA", "A"); knob (a, "aD", "D"); knob (a, "aS", "S"); knob (a, "aR", "R");
    }
    // --- LFO / FX
    {
        auto& l = section (2, "LFO a tempo");
        lcd (l, "lfoShape"); lcd (l, "lfoDiv", "SYNC ");
        knob (l, "lfoCut", "> Cutoff"); knob (l, "lfoWave", "> Wave"); knob (l, "lfoPitch", "> Pitch");
        auto& fx = section (2, "Effetti", 1.4f);
        lcd (fx, "dlyTime", "DELAY ");
        knob (fx, "dlyMix", "Delay"); knob (fx, "dlyFb", "Feedback"); knob (fx, "chorus", "Chorus");
        knob (fx, "revMix", "Riverbero"); knob (fx, "revSize", "Stanza");
        auto& v = section (2, "Uscita", 0.5f, 1);
        knob (v, "master", "Volume");
    }
    // --- CRUNCH
    {
        auto& t = section (4, "Valvola", 0.6f, 1);
        knob (t, "tube", "Drive");
        auto& b = section (4, "Bitcrusher", 0.9f);
        knob (b, "bits", "Bit"); knob (b, "rate", "Rate");
        auto& cl = section (4, "Clip", 0.75f, 1);
        segment (cl, "clipMode", { "SOFT", "HARD" });
        knob (cl, "clipDrive", "Drive");
        auto& pu = section (4, "Pump", 1.1f);
        lcd (pu, "pumpDiv", "SYNC ");
        knob (pu, "pumpShape", "Forma"); knob (pu, "pumpFilter", "> Filtro");
    }

    // --- MATRIX
    addChildComponent (matrixPanel);
    matrix = std::make_unique<frui::MatrixView> (proc);
    matrixPanel.addAndMakeVisible (*matrix);
    auto addControl = [this] (std::unique_ptr<juce::Component> comp)
    {
        matrixPanel.addAndMakeVisible (*comp);
        matrixControls.push_back (comp.get());
        owned.push_back (std::move (comp));
    };
    addControl (std::make_unique<frui::Segment> (apvts, "matOn", juce::StringArray { "OFF", "ON" }, frui::colours::green));
    addControl (std::make_unique<frui::Segment> (apvts, "matRate", juce::StringArray { "1/8", "1/16", "1/32" }));
    addControl (std::make_unique<frui::Lcd> (apvts, "retrig", "RETRIG "));
    addControl (std::make_unique<frui::Knob> (apvts, "swing", "Swing"));
    addControl (std::make_unique<frui::Knob> (apvts, "gateLen", "Gate"));
    addControl (std::make_unique<frui::Knob> (apvts, "matGlide", "Glide"));
    addControl (std::make_unique<frui::Knob> (apvts, "matLen", "Step"));
    addControl (std::make_unique<frui::Knob> (apvts, "matCut", "Cut"));
    addControl (std::make_unique<frui::Knob> (apvts, "matCrush", "Crush"));

    // --- Colonna destra
    crunchPanel.showTitle = pumpPanel.showTitle = false;
    addAndMakeVisible (crunchPanel);
    addAndMakeVisible (pumpPanel);
    crunchKnob = std::make_unique<frui::Knob> (apvts, "crunch", "Crunch", true);
    pumpKnob = std::make_unique<frui::Knob> (apvts, "pump", "Pump", true);
    pumpLcd = std::make_unique<frui::Lcd> (apvts, "pumpDiv", "SYNC ");
    crunchPanel.addAndMakeVisible (*crunchKnob);
    crunchCap = std::make_unique<frui::Caption> (juce::String (juce::CharPointer_UTF8 ("TUBE \xe2\x97\x82\xe2\x96\xb8 BITS")));
    crunchPanel.addAndMakeVisible (*crunchCap);
    pumpPanel.addAndMakeVisible (*pumpKnob);
    pumpPanel.addAndMakeVisible (*pumpLcd);

    setSize (900, 400);
    showTab (3);
    startTimerHz (30);
}

FrenchRouletteEditor::~FrenchRouletteEditor()
{
    stopTimer();
    setLookAndFeel (nullptr);
}

FrenchRouletteEditor::Section& FrenchRouletteEditor::section (int tabIndex, const juce::String& title, float weight, int columns)
{
    auto s = std::make_unique<Section>();
    s->panel = std::make_unique<frui::Panel> (title);
    s->weight = weight;
    s->columns = columns;
    addChildComponent (*s->panel);
    sections[(size_t) tabIndex].push_back (std::move (s));
    return *sections[(size_t) tabIndex].back();
}

frui::Knob& FrenchRouletteEditor::knob (Section& s, const juce::String& id, const juce::String& title)
{
    auto k = std::make_unique<frui::Knob> (proc.apvts, id, title);
    auto& ref = *k;
    s.panel->addAndMakeVisible (ref);
    s.items.push_back (&ref);
    owned.push_back (std::move (k));
    return ref;
}

frui::Segment& FrenchRouletteEditor::segment (Section& s, const juce::String& id, juce::StringArray labels)
{
    auto k = std::make_unique<frui::Segment> (proc.apvts, id, std::move (labels));
    auto& ref = *k;
    s.panel->addAndMakeVisible (ref);
    s.items.push_back (&ref);
    owned.push_back (std::move (k));
    return ref;
}

frui::Lcd& FrenchRouletteEditor::lcd (Section& s, const juce::String& id, const juce::String& prefix)
{
    auto k = std::make_unique<frui::Lcd> (proc.apvts, id, prefix);
    auto& ref = *k;
    s.panel->addAndMakeVisible (ref);
    s.items.push_back (&ref);
    owned.push_back (std::move (k));
    return ref;
}

void FrenchRouletteEditor::showTab (int index)
{
    tab = juce::jlimit (0, tabs.size() - 1, index);
    tabs[tab]->setToggleState (true, juce::dontSendNotification);
    for (size_t t = 0; t < sections.size(); ++t)
        for (auto& s : sections[t])
            s->panel->setVisible ((int) t == tab);
    matrixPanel.setVisible (tab == 3);
    resized();
    repaint();
}

void FrenchRouletteEditor::timerCallback()
{
    if (matrixPanel.isVisible())
        matrix->refresh();

    // LED del logo: lampeggia sugli step che suonano
    const int step = proc.getCurrentStep();
    if (step != lastStep)
    {
        lastStep = step;
        const bool on = step >= 0 && proc.apvts.getRawParameterValue (fr::stepId ('g', step))->load() > 0.5f;
        if (on != ledOn) { ledOn = on; repaint (ledArea.expanded (4)); }
    }
}

void FrenchRouletteEditor::paint (juce::Graphics& g)
{
    namespace c = frui::colours;
    g.fillAll (c::bg);
    const int headH = tabs[0]->getBottom() + 4;
    g.setGradientFill (juce::ColourGradient (juce::Colour (0xff17191c), 0.0f, 0.0f, juce::Colour (0xff101215), 0.0f, (float) headH, false));
    g.fillRect (0, 0, getWidth(), headH);
    g.setColour (juce::Colours::black);
    g.drawHorizontalLine (headH - 1, 0.0f, (float) getWidth());

    g.setFont (frui::mono (juce::jlimit (12.0f, 17.0f, (float) logoArea.getHeight() * 0.55f)));
    frui::drawNeon (g, getWidth() < 640 ? "F.ROULETTE" : "FRENCH ROULETTE", logoArea.toFloat(), getWidth() < 640 ? 0.5f : 3.0f,
                    juce::Justification::left, c::red);

    // Schede: testo rosso e sottolineatura su quella attiva
    for (int i = 0; i < tabs.size(); ++i)
    {
        auto b = tabs[i]->getBounds().toFloat();
        const bool on = i == tab;
        const bool narrowTabs = b.getWidth() < 80.0f;
        const float tabSpacing = narrowTabs ? 0.3f : 1.6f;
        g.setFont (frui::sans (narrowTabs ? 10.0f : juce::jlimit (10.0f, 13.0f, b.getHeight() * 0.42f)));
        if (on)
        {
            frui::drawNeon (g, tabs[i]->getButtonText(), b, tabSpacing, juce::Justification::centred, c::red);
            g.setColour (c::red);
            g.fillRect (b.getX() + 6.0f, b.getBottom() + 1.0f, b.getWidth() - 12.0f, 2.0f);
        }
        else
        {
            g.setColour (c::textDim);
            frui::drawSpaced (g, tabs[i]->getButtonText(), b, tabSpacing, juce::Justification::centred);
        }
    }

    // LED
    auto led = ledArea.toFloat();
    g.setColour (ledOn ? c::red.withAlpha (0.35f) : juce::Colours::transparentBlack);
    g.fillEllipse (led.expanded (3.0f));
    g.setColour (ledOn ? c::red : c::redDim);
    g.fillEllipse (led);

}

void FrenchRouletteEditor::layoutSection (Section& s)
{
    auto c = s.panel->content();
    std::vector<juce::Component*> knobs;
    const int barH = juce::jlimit (22, 30, c.getHeight() / 7);
    for (auto* item : s.items)
    {
        if (dynamic_cast<frui::Knob*> (item) != nullptr) { knobs.push_back (item); continue; }
        item->setBounds (c.removeFromTop (barH).withSizeKeepingCentre (juce::jmin (c.getWidth(), 230), barH));
        c.removeFromTop (5);
    }
    const int n = (int) knobs.size();
    if (n == 0) return;
    int cols = s.columns;
    if (cols <= 0)
    {
        double bestScore = 1e9;
        for (int k = 1; k <= n; ++k)
        {
            const int rows = (n + k - 1) / k;
            const double cell = ((double) c.getWidth() / k) / ((double) c.getHeight() / rows);
            const double score = std::abs (std::log (cell / 0.95)) + 0.4 * (double) (k * rows - n) / (double) k;
            if (score < bestScore) { bestScore = score; cols = k; }
        }
    }
    const int rows = (n + cols - 1) / cols;
    const int cw = c.getWidth() / cols, ch = c.getHeight() / rows;
    for (int i = 0; i < n; ++i)
    {
        const int row = i / cols, col = i % cols;
        const int inRow = juce::jmin (cols, n - row * cols);
        const int offset = (c.getWidth() - inRow * cw) / 2;      // ultima riga centrata
        knobs[(size_t) i]->setBounds (juce::Rectangle<int> (c.getX() + offset + col * cw, c.getY() + row * ch, cw, ch).reduced (2));
    }
}

void FrenchRouletteEditor::resized()
{
    auto b = getLocalBounds();
    const int headH = getHeight() < 330 ? 34 : 40;
    auto head = b.removeFromTop (headH);
    ledArea = head.removeFromRight (26).withSizeKeepingCentre (8, 8);
    logoArea = head.removeFromLeft (getWidth() < 640 ? 108 : 200).withTrimmedLeft (10);
    const int tw = juce::jmin (100, head.getWidth() / juce::jmax (1, tabs.size()));
    for (auto* t : tabs)
        t->setBounds (head.removeFromLeft (tw).reduced (2, 6));

    auto area = b.reduced (6);
    const bool landscape = area.getWidth() > area.getHeight() * 1.25f;

    // Colonna (o fascia) dei knob grandi
    juce::Rectangle<int> side;
    if (landscape)
        side = area.removeFromRight (juce::jlimit (118, 200, area.getWidth() / 5));
    else
        side = area.removeFromTop (juce::jlimit (110, 150, area.getHeight() / 4));

    auto first = landscape ? side.removeFromTop (side.getHeight() / 2) : side.removeFromLeft (side.getWidth() / 2);
    crunchPanel.setBounds (first.reduced (3));
    pumpPanel.setBounds (side.reduced (3));
    for (auto [panel, knobPtr, extraIsCaption] : { std::tuple<frui::Panel*, frui::Knob*, bool> { &crunchPanel, crunchKnob.get(), true },
                                                   std::tuple<frui::Panel*, frui::Knob*, bool> { &pumpPanel, pumpKnob.get(), false } })
    {
        auto c = panel->getLocalBounds().reduced (6);
        const int capH = juce::jlimit (16, 24, c.getHeight() / 6);
        auto cap = c.removeFromBottom (capH).withSizeKeepingCentre (juce::jmin (c.getWidth() - 8, 132), capH);
        c.removeFromBottom (3);
        knobPtr->setBounds (c);
        if (extraIsCaption)
            crunchCap->setBounds (cap);
        else
            pumpLcd->setBounds (cap);
    }

    // Contenuto della scheda
    if (tab == 3)
    {
        matrixPanel.setBounds (area.reduced (3));
        auto c = matrixPanel.getLocalBounds().reduced (7);
        const int ctrlH = juce::jlimit (40, 62, c.getHeight() / 4);
        const bool twoRows = c.getWidth() < 560;
        auto ctrl = c.removeFromBottom (twoRows ? ctrlH * 2 : ctrlH);
        c.removeFromBottom (5);
        matrix->setBounds (c);

        // controlli: ON/OFF, divisione, retrig, poi le manopole
        const int segH = juce::jmin (28, ctrlH / 2 - 2);
        auto left = twoRows ? ctrl.removeFromTop (ctrlH) : ctrl.removeFromLeft (juce::jmin (ctrl.getWidth() / 2, 250));
        const int half = left.getWidth() / 2;
        auto row1 = twoRows ? left.removeFromLeft (half).withSizeKeepingCentre (half, segH)
                            : left.removeFromTop (ctrlH / 2).withSizeKeepingCentre (left.getWidth(), segH);
        auto row2 = left.withSizeKeepingCentre (left.getWidth(), segH);
        matrixControls[0]->setBounds (row1.removeFromLeft (80).reduced (2, 0));
        matrixControls[1]->setBounds (row1.reduced (2, 0));
        matrixControls[2]->setBounds (row2.reduced (2, 0));
        const int knobs = (int) matrixControls.size() - 3;
        const int kw = ctrl.getWidth() / knobs;
        for (int i = 0; i < knobs; ++i)
            matrixControls[(size_t) (3 + i)]->setBounds (ctrl.removeFromLeft (kw));
        return;
    }

    std::vector<Section*> list;
    for (auto& s : sections[(size_t) tab]) list.push_back (s.get());
    float total = 0.0f;
    for (auto* s : list) total += s->weight;
    const bool wide = area.getWidth() > 520 || list.size() <= 2;
    if (wide)
    {
        int x = area.getX();
        for (size_t i = 0; i < list.size(); ++i)
        {
            const int w = i + 1 == list.size() ? area.getRight() - x : juce::roundToInt ((float) area.getWidth() * list[i]->weight / total);
            list[i]->panel->setBounds (juce::Rectangle<int> (x, area.getY(), w, area.getHeight()).reduced (3));
            x += w;
        }
    }
    else   // stretto (verticale): due colonne
    {
        const int rows = ((int) list.size() + 1) / 2;
        const int h = area.getHeight() / rows, w = area.getWidth() / 2;
        for (size_t i = 0; i < list.size(); ++i)
            list[i]->panel->setBounds (juce::Rectangle<int> (area.getX() + (int) (i % 2) * w, area.getY() + (int) (i / 2) * h, w, h).reduced (3));
    }
    for (auto* s : list)
        layoutSection (*s);
}
