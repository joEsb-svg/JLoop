#pragma once

#include <JuceHeader.h>
#include "../Engine/SequencerEngine.h"
#include "AutomationView.h"
#include "Theme.h"
#include "Widgets.h"

//==============================================================================
/**
    Forme e onde per una automazione: scegli la forma (con l'anteprima), quanto spesso si ripete,
    da che battuta e per quante, e tra quali valori si muove (basso / alto).
*/
class AutoShapePanel : public juce::Component
{
public:
    explicit AutoShapePanel (SequencerEngine& e) : engine (e)
    {
        for (auto* b : { &cancelButton, &applyButton, &minusButton, &plusButton })
            addAndMakeVisible (b);
        cancelButton.onClick = [this] { setVisible (false); };
        applyButton.onClick = [this] { apply(); };
        minusButton.onClick = [this] { startBar = juce::jmax (0, startBar - 1); repaint(); };
        plusButton.onClick  = [this] { startBar = juce::jmin (SequencerEngine::maxBars - 1, startBar + 1); repaint(); };
        addAndMakeVisible (rateSwitch);
        addAndMakeVisible (lengthSwitch);
        rateSwitch.setSelected (1);
        lengthSwitch.setSelected (2);
        rateSwitch.onChange = [this] (int) { repaint(); };
        lengthSwitch.onChange = [this] (int) { repaint(); };
        for (auto* s : { &lowSlider, &highSlider })
        {
            s->setSliderStyle (juce::Slider::LinearHorizontal);
            s->setTextBoxStyle (juce::Slider::NoTextBox, false, 0, 0);
            s->setRange (0.0, 1.0);
            s->setColour (juce::Slider::trackColourId, Theme::accent);
            s->setColour (juce::Slider::thumbColourId, Theme::textBright);
            s->onValueChange = [this] { repaint(); };
            addAndMakeVisible (s);
        }
        lowSlider.setValue (0.0, juce::dontSendNotification);
        highSlider.setValue (1.0, juce::dontSendNotification);
    }

    std::function<void()> onApplied;

    void open (int laneIndex)
    {
        lane = laneIndex;
        startBar = engine.getSongStartBar();
        setVisible (true);
        toFront (true);
        repaint();
    }

    void selectForTest (int shape)   { shapeId = shape; rateSwitch.setVisible (automation::shapes::isWave (shape)); repaint(); }

    void paint (juce::Graphics& g) override
    {
        g.setColour (Theme::panel);
        g.fillRoundedRectangle (getLocalBounds().toFloat(), Theme::corner);
        const auto target = engine.getLane (lane).target();
        const auto colour = AutomationText::colour (engine, target);
        g.setColour (Theme::textBright);
        g.setFont (Theme::font (16.0f, true));
        g.drawFittedText ("Forme e onde: " + AutomationText::parameter (engine, target), titleArea, juce::Justification::centredLeft, 1);

        Theme::drawCaption (g, "Forma", shapesCaption, juce::Justification::centredLeft);
        for (int id = 0; id < automation::shapes::numShapes; ++id)
        {
            auto b = shapeBounds (id).toFloat();
            const bool sel = id == shapeId;
            Theme::drawRaised (g, b, sel ? Theme::raisedHi : Theme::raised, 6.0f);
            if (sel)
            {
                g.setColour (Theme::accent);
                g.drawRoundedRectangle (b.reduced (1.0f), 6.0f, 1.6f);
            }
            auto text = b.removeFromBottom (18.0f);
            g.setColour (sel ? Theme::textBright : Theme::text);
            g.setFont (Theme::font (11.5f, true));
            g.drawFittedText (automation::shapes::name (id), text.toNearestInt().reduced (4, 0), juce::Justification::centred, 1, 0.8f);
            drawShape (g, id, b.reduced (10.0f, 7.0f), sel ? colour : colour.withAlpha (0.6f), 0.0f, 1.0f, 2 * 192);
        }

        const bool wave = automation::shapes::isWave (shapeId);
        if (wave) Theme::drawCaption (g, "Si ripete ogni", rateCaption, juce::Justification::centredLeft);
        Theme::drawCaption (g, "Quante battute", lengthCaption, juce::Justification::centredLeft);
        Theme::drawCaption (g, "Tra", rangeCaption, juce::Justification::centredLeft);

        g.setColour (Theme::text);
        g.setFont (Theme::font (13.0f, true));
        g.drawFittedText ("Dalla battuta", startLabel.toNearestInt(), juce::Justification::centredLeft, 1);
        g.setColour (Theme::lcdText);
        g.setFont (Theme::font (22.0f, true));
        g.drawFittedText (juce::String (startBar + 1), startValue.toNearestInt(), juce::Justification::centred, 1);

        g.setColour (Theme::textDim);
        g.setFont (Theme::font (12.0f));
        g.drawFittedText ("Basso: " + AutomationText::value (engine, target, (float) lowSlider.getValue()), lowLabel.toNearestInt(), juce::Justification::centredLeft, 1);
        g.drawFittedText ("Alto: " + AutomationText::value (engine, target, (float) highSlider.getValue()), highLabel.toNearestInt(), juce::Justification::centredLeft, 1);

        // Anteprima del risultato
        Theme::drawSunken (g, previewArea, Theme::lcdBg, 6.0f);
        drawShape (g, shapeId, previewArea.reduced (8.0f, 8.0f), colour, (float) lowSlider.getValue(), (float) highSlider.getValue(), length());
        const auto pts = makePoints();
        g.setColour (Theme::textDim);
        g.setFont (Theme::font (11.0f));
        juce::String info = "Battute " + juce::String (startBar + 1) + " - " + juce::String (startBar + bars());
        if (! pts.empty() && pts.back().tick < (startBar + bars()) * 192 - 24 && wave)
            info << "  (accorciata: troppi punti a questa velocita')";
        g.drawText (info, previewArea.reduced (8.0f, 4.0f), juce::Justification::topLeft, true);
    }

    void resized() override
    {
        auto r = getLocalBounds().reduced (8, 6);
        auto top = r.removeFromTop (34);
        cancelButton.setBounds (top.removeFromRight (90).reduced (0, 2));
        titleArea = top;
        r.removeFromTop (6);
        const bool narrow = r.getWidth() < 640;

        juce::Rectangle<int> left, right;
        if (narrow) { left = r.removeFromTop (r.getHeight() * 45 / 100); r.removeFromTop (6); right = r; }
        else        { left = r.removeFromLeft (r.getWidth() * 55 / 100); r.removeFromLeft (12); right = r; }

        shapesCaption = left.removeFromTop (18).toFloat();
        shapesArea = left;

        rateCaption = right.removeFromTop (16).toFloat();
        rateSwitch.setBounds (right.removeFromTop (32));
        right.removeFromTop (6);
        auto row = right.removeFromTop (40);
        startLabel = row.removeFromLeft (100).toFloat();
        minusButton.setBounds (row.removeFromLeft (40).reduced (2));
        startValue = row.removeFromLeft (50).toFloat();
        plusButton.setBounds (row.removeFromLeft (40).reduced (2));
        right.removeFromTop (4);
        lengthCaption = right.removeFromTop (16).toFloat();
        lengthSwitch.setBounds (right.removeFromTop (32));
        right.removeFromTop (6);
        rangeCaption = right.removeFromTop (16).toFloat();
        auto lowRow = right.removeFromTop (28);
        lowLabel = lowRow.removeFromLeft (lowRow.getWidth() / 2).toFloat();
        lowSlider.setBounds (lowRow);
        auto highRow = right.removeFromTop (28);
        highLabel = highRow.removeFromLeft (highRow.getWidth() / 2).toFloat();
        highSlider.setBounds (highRow);
        right.removeFromTop (6);
        applyButton.setBounds (right.removeFromBottom (44).reduced (0, 2));
        right.removeFromBottom (6);
        previewArea = right.toFloat();
    }

    void mouseUp (const juce::MouseEvent& e) override
    {
        for (int id = 0; id < automation::shapes::numShapes; ++id)
            if (shapeBounds (id).contains (e.getPosition()))
            {
                shapeId = id;
                rateSwitch.setVisible (automation::shapes::isWave (id));
                repaint();
                return;
            }
    }

private:
    int bars() const      { static const int b[] { 1, 2, 4, 8, 16 }; return b[juce::jlimit (0, 4, lengthSwitch.getSelected())]; }
    int length() const    { return bars() * 192; }
    int cycle() const     { return automation::shapes::cycleChoices[juce::jlimit (0, 5, rateSwitch.getSelected())]; }

    std::vector<automation::Point> makePoints() const
    {
        return automation::shapes::make (shapeId, startBar * 192, length(), cycle(),
                                         (float) lowSlider.getValue(), (float) highSlider.getValue(), (juce::uint32) (startBar * 31 + 7));
    }

    juce::Rectangle<int> shapeBounds (int id) const
    {
        const int cols = shapesArea.getWidth() >= 520 ? 4 : 3;
        const int rows = (automation::shapes::numShapes + cols - 1) / cols;
        const int w = shapesArea.getWidth() / cols, h = juce::jmin (90, shapesArea.getHeight() / rows);
        return { shapesArea.getX() + (id % cols) * w, shapesArea.getY() + (id / cols) * h, w - 6, h - 6 };
    }

    /** Disegna la forma (anteprima): per le icone 2 battute a una velocita' fissa, per l'anteprima quella scelta. */
    void drawShape (juce::Graphics& g, int id, juce::Rectangle<float> area, juce::Colour c, float lo, float hi, int len) const
    {
        const int cyc = area.getWidth() < 200.0f ? 96 : cycle();
        const auto pts = automation::shapes::make (id, 0, len, cyc, lo, hi, 3);
        if (pts.empty()) return;
        auto x = [&] (double t) { return area.getX() + (float) (t / len) * area.getWidth(); };
        auto y = [&] (float v) { return area.getBottom() - v * area.getHeight(); };
        juce::Path p;
        p.startNewSubPath (x (pts[0].tick), y (pts[0].value));
        for (size_t i = 1; i < pts.size(); ++i)
        {
            const auto& q = pts[i - 1];
            for (int k = 1; k <= 8; ++k)
            {
                const float t = (float) k / 8.0f;
                p.lineTo (x (q.tick + (pts[i].tick - q.tick) * t), y (q.value + (pts[i].value - q.value) * automation::shape (t, q.curve)));
            }
        }
        p.lineTo (area.getRight(), y (pts.back().value));
        g.setColour (c);
        g.strokePath (p, juce::PathStrokeType (2.0f));
    }

    void apply()
    {
        const auto pts = makePoints();
        if (pts.empty()) return;
        AutomationView::replaceRange (engine, lane, startBar * 192, length(), pts);
        setVisible (false);
        if (onApplied) onApplied();
    }

    SequencerEngine& engine;
    int lane = 0, shapeId = automation::shapes::sine, startBar = 0;
    SegmentedControl rateSwitch { { "2 batt.", "1 batt.", "1/2", "1/4", "1/8", "1/16" } };
    SegmentedControl lengthSwitch { { "1", "2", "4", "8", "16" } };
    juce::Slider lowSlider, highSlider;
    juce::TextButton cancelButton { "Annulla" }, applyButton { "Disegna la forma" }, minusButton { "-" }, plusButton { "+" };
    juce::Rectangle<int> titleArea, shapesArea;
    juce::Rectangle<float> shapesCaption, rateCaption, lengthCaption, rangeCaption, startLabel, startValue, lowLabel, highLabel, previewArea;
};
