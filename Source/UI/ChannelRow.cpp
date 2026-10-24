#include "ChannelRow.h"
#include "StepFillPanel.h"
#include "Theme.h"
#include "../Instruments/Instruments.h"

ChannelRow::RowClipboard ChannelRow::clipboard;

ChannelRow::ChannelRow (SequencerEngine& e, int channelIndex)
    : engine (e),
      channel (channelIndex)
{
    volumeKnob.setSliderStyle (juce::Slider::RotaryVerticalDrag);
    volumeKnob.setTextBoxStyle (juce::Slider::NoTextBox, false, 0, 0);
    volumeKnob.setRange (0.0, 1.0);
    volumeKnob.setValue (engine.getChannel (channel).volume.load(), juce::dontSendNotification);
    volumeKnob.setDoubleClickReturnValue (true, 0.8);
    volumeKnob.setMouseDragSensitivity (160);
    volumeKnob.setColour (juce::Slider::rotarySliderFillColourId, Theme::text.darker (0.25f));
    volumeKnob.onValueChange = [this] { engine.getChannel (channel).volume.store ((float) volumeKnob.getValue()); };
    addAndMakeVisible (volumeKnob);
}

//==============================================================================
float ChannelRow::getHeaderWidth() const
{
    if (compact)
        return juce::jlimit (110.0f, 150.0f, (float) getWidth() * 0.33f);

    return juce::jlimit (170.0f, 240.0f, (float) getWidth() * 0.25f);
}

void ChannelRow::setLayout (int stepsToShow, bool compactHeader)
{
    if (stepsToShow != visibleSteps || compactHeader != compact)
    {
        visibleSteps = stepsToShow;
        compact = compactHeader;
        volumeKnob.setVisible (! compact);   // in verticale il volume si regola dall'editor del canale
        resized();
        repaint();
    }
}

void ChannelRow::resized()
{
    auto r = getLocalBounds().toFloat();
    const float h = r.getHeight();
    auto left = r.removeFromLeft (getHeaderWidth());

    muteArea = left.removeFromLeft (juce::jmin (26.0f, h * 0.7f));
    if (! compact)
    {
        const float knob = juce::jmin (h, 46.0f);
        volumeKnob.setBounds (left.removeFromLeft (knob).withSizeKeepingCentre (knob, knob).reduced (1.0f).toNearestInt());
        left.removeFromLeft (4.0f);
    }
    nameArea = left.reduced (0.0f, 1.0f);
    menuArea = nameArea.withLeft (nameArea.getRight() - juce::jmin (30.0f, nameArea.getWidth() * 0.28f));
}

juce::Rectangle<float> ChannelRow::getPadsArea() const
{
    auto r = getLocalBounds().toFloat();
    r.removeFromLeft (getHeaderWidth() + 8.0f);
    return r.reduced (0.0f, 1.0f);
}

float ChannelRow::getStepPitch() const
{
    // 4 step = 4 pad + 3 spazi piccoli + 1 spazio tra i gruppi
    const auto area = getPadsArea();
    const float groupGap = 6.0f, padGap = 2.0f;
    const int numGroups = juce::jmax (1, visibleSteps / 4);
    const float padWidth = (area.getWidth() - groupGap * (float) (numGroups - 1) - padGap * (float) (visibleSteps - numGroups)) / (float) visibleSteps;
    return padWidth + (3.0f * padGap + groupGap) / 4.0f;
}

juce::Rectangle<float> ChannelRow::getStepBounds (int step) const
{
    auto area = getPadsArea();
    const float groupGap = 6.0f, padGap = 2.0f;
    const int numGroups = juce::jmax (1, visibleSteps / 4);
    const float padWidth = (area.getWidth() - groupGap * (float) (numGroups - 1) - padGap * (float) (visibleSteps - numGroups)) / (float) visibleSteps;
    auto absX = [&] (float s)
    {
        const float group = std::floor (s / 4.0f);
        return s * padWidth + group * groupGap + (s - group) * padGap;
    };
    // posizione assoluta meno lo scorrimento (lo scorrimento usa il passo medio: e' continuo)
    const float x = area.getX() + absX ((float) step) - firstStep * getStepPitch();
    return { x, area.getY(), padWidth, area.getHeight() };
}

bool ChannelRow::isStepVisible (int step) const
{
    const auto area = getPadsArea();
    const auto b = getStepBounds (step);
    return b.getRight() > area.getX() + 1.0f && b.getX() < area.getRight() - 1.0f;
}

int ChannelRow::stepAt (juce::Point<float> p) const
{
    if (! getPadsArea().expanded (2.0f, 4.0f).contains (p))
        return -1;
    const int guess = (int) std::floor (firstStep + (p.x - getPadsArea().getX()) / getStepPitch());
    for (int s = juce::jmax (0, guess - 1); s <= guess + 1 && s < SequencerEngine::maxSteps; ++s)
        if (getStepBounds (s).expanded (1.5f, 4.0f).contains (p))
            return s;
    return -1;
}

void ChannelRow::setView (float firstStepToShow, EditMode newMode)
{
    if (firstStepToShow != firstStep || newMode != mode)
    {
        const bool onlyScroll = newMode == mode;
        firstStep = firstStepToShow;
        mode = newMode;
        if (onlyScroll)
            repaint (getPadsArea().toNearestInt().expanded (3));
        else
            repaint();
    }
}

//==============================================================================
void ChannelRow::paint (juce::Graphics& g)
{
    auto& ch = engine.getChannel (channel);
    const bool muted = ch.muted.load();
    const int length = engine.getPatternLength();
    const bool showPlayhead = engine.getPlayingPattern() == engine.getCurrentPattern();

    // LED di mute (verde = canale attivo)
    {
        const float d = juce::jmin (muteArea.getWidth(), muteArea.getHeight()) * 0.42f;
        auto led = juce::Rectangle<float> (d, d).withCentre (muteArea.getCentre());

        if (! muted)
        {
            g.setColour (Theme::led.withAlpha (0.12f));
            g.fillEllipse (led.expanded (d * 0.6f));
            g.setColour (Theme::led.withAlpha (0.25f));
            g.fillEllipse (led.expanded (d * 0.3f));
        }

        g.setColour (muted ? Theme::raised : Theme::led);
        g.fillEllipse (led);
        g.setColour (Theme::border);
        g.drawEllipse (led, 1.0f);
    }

    // Pulsante del canale: nome + suono scelto; si illumina quando lo strumento suona
    {
        auto r = nameArea;
        Theme::drawRaised (g, r, Theme::raised.interpolatedWith (Theme::raisedHi, flash));

        const auto colour = getColour();
        const bool soloed = ch.solo.load();
        auto stripe = r.withWidth (3.0f).reduced (0.0f, 5.0f).translated (3.0f, 0.0f);
        if (! muted)
        {
            g.setColour (colour.withAlpha (0.25f + 0.35f * flash));   // alone del colore del canale
            g.fillRoundedRectangle (stripe.expanded (2.0f, 1.0f), 2.5f);
        }
        g.setColour (colour.withAlpha (muted ? 0.3f : 1.0f));
        g.fillRoundedRectangle (stripe, 1.5f);

        auto textArea = r.withTrimmedLeft (11.0f).withRight (menuArea.getX());
        const float nameH = juce::jlimit (12.0f, 15.0f, r.getHeight() * 0.36f);
        auto nameLine = textArea.removeFromTop (textArea.getHeight() * 0.55f);

        g.setColour (muted ? Theme::textDim : Theme::textBright);
        g.setFont (Theme::font (nameH, true));
        g.drawFittedText (ch.name, nameLine.toNearestInt(), juce::Justification::bottomLeft, 1);

        if (soloed)   // "S" del solo, in alto a destra nel pulsante
        {
            auto badge = juce::Rectangle<float> (15.0f, 13.0f).withPosition (menuArea.getX() - 18.0f, r.getY() + 4.0f);
            g.setColour (Theme::accent);
            g.fillRoundedRectangle (badge, 3.0f);
            g.setColour (Theme::bg);
            g.setFont (Theme::font (10.5f, true));
            g.drawText ("S", badge, juce::Justification::centred, false);
        }

        g.setColour (Theme::textDim);
        g.setFont (Theme::font (juce::jmax (10.5f, nameH * 0.8f)));
        g.drawFittedText (getSoundLabel (engine, channel), textArea.toNearestInt(),
                          juce::Justification::topLeft, 1);

        g.setColour (Theme::border.withAlpha (0.8f));
        g.drawVerticalLine ((int) menuArea.getX(), menuArea.getY() + 5.0f, menuArea.getBottom() - 5.0f);

        const float a = juce::jmin (menuArea.getWidth(), menuArea.getHeight()) * 0.16f;
        auto c = menuArea.getCentre();
        juce::Path tri;
        tri.addTriangle (c.x - a, c.y - a * 0.5f, c.x + a, c.y - a * 0.5f, c.x, c.y + a * 0.7f);
        g.setColour (Theme::textDim);
        g.fillPath (tri);
    }

    // Canale con note nel piano roll: al posto degli step, un'anteprima delle note
    if (engine.hasNotes (channel))
    {
        paintNotesPreview (g, muted);
        return;
    }

    // Step (solo quelli in vista: il rack scorre in orizzontale)
    g.reduceClipRegion (getPadsArea().expanded (3.0f, 3.0f).toNearestInt());
    const int fromStep = juce::jmax (0, (int) std::floor (firstStep) - 1);
    const int toStep = juce::jmin (SequencerEngine::maxSteps, (int) std::ceil (firstStep) + visibleSteps + 1);
    for (int step = fromStep; step < toStep; ++step)
    {
        auto pad = getStepBounds (step);

        if (step >= length)   // oltre la lunghezza del pattern
        {
            g.setColour (Theme::panel);
            g.fillRoundedRectangle (pad, 4.0f);
            continue;
        }

        const int vel  = engine.getStepVelocity (channel, step);
        const bool isCurrent = showPlayhead && step == current;
        const auto off = (step / 4) % 2 == 0 ? Theme::stepOffA : Theme::stepOffB;
        const auto chColour = getColour().withMultipliedSaturation (muted ? 0.3f : 1.0f).withMultipliedAlpha (muted ? 0.5f : 1.0f);

        if (mode == EditMode::pitch || mode == EditMode::pan)
        {
            // Altezza (barra dal centro in su/giu') o pan (barra dal centro a sinistra/destra)
            Theme::drawSunken (g, pad, off.darker (0.35f), 4.0f);
            if (vel > 0)
            {
                auto inner = pad.reduced (4.0f);
                const auto c1 = mode == EditMode::pitch ? chColour : chColour.interpolatedWith (Theme::cyan, 0.4f);
                juce::String label;
                if (mode == EditMode::pitch)
                {
                    const int semi = engine.getStepPitch (channel, step);
                    const float amount = (float) semi / 24.0f, mid = inner.getCentreY();
                    g.setColour (Theme::raisedHi);
                    g.fillRect (inner.getX(), mid - 0.5f, inner.getWidth(), 1.0f);
                    auto bar = amount >= 0.0f ? juce::Rectangle<float> (inner.getX(), mid - inner.getHeight() * 0.5f * amount, inner.getWidth(), inner.getHeight() * 0.5f * amount)
                                              : juce::Rectangle<float> (inner.getX(), mid, inner.getWidth(), inner.getHeight() * 0.5f * -amount);
                    g.setColour (c1.withAlpha (0.85f));
                    g.fillRoundedRectangle (bar.withHeight (juce::jmax (2.0f, bar.getHeight())), 2.0f);
                    label = semi == 0 ? juce::String ("0") : (semi > 0 ? "+" : "") + juce::String (semi);
                }
                else
                {
                    const int pan = engine.getStepPan (channel, step);
                    const float amount = (float) pan / 64.0f, mid = inner.getCentreX();
                    g.setColour (Theme::raisedHi);
                    g.fillRect (mid - 0.5f, inner.getY(), 1.0f, inner.getHeight());
                    auto bar = amount >= 0.0f ? juce::Rectangle<float> (mid, inner.getY(), inner.getWidth() * 0.5f * amount, inner.getHeight())
                                              : juce::Rectangle<float> (mid + inner.getWidth() * 0.5f * amount, inner.getY(), inner.getWidth() * 0.5f * -amount, inner.getHeight());
                    g.setColour (c1.withAlpha (0.85f));
                    g.fillRoundedRectangle (bar.withWidth (juce::jmax (2.0f, bar.getWidth())), 2.0f);
                    label = pan == 0 ? juce::String ("C") : (pan < 0 ? "L" : "R") + juce::String (std::abs (juce::roundToInt (pan * 100.0f / 64.0f)));
                }
                g.setColour (Theme::textBright);
                g.setFont (Theme::font (juce::jlimit (9.0f, 13.0f, pad.getWidth() * 0.3f), true));
                g.drawText (label, pad, juce::Justification::centred, false);
            }
            if (isCurrent)
                Theme::drawActiveGlow (g, pad, 4.0f, Theme::cyan);
            continue;
        }

        if (mode == EditMode::velocity || mode == EditMode::probability)
        {
            // Barra verticale: velocity o probabilita', col colore del canale
            Theme::drawSunken (g, pad, off.darker (0.35f), 4.0f);

            if (vel > 0)
            {
                const float amount = mode == EditMode::velocity ? (float) vel / 127.0f
                                                                : (float) engine.getStepProbability (channel, step) / 100.0f;
                auto bar = pad.reduced (4.0f);
                bar = bar.removeFromBottom (juce::jmax (2.0f, bar.getHeight() * amount));
                const auto c1 = mode == EditMode::velocity ? chColour : chColour.interpolatedWith (Theme::accent, 0.4f);
                g.setGradientFill (juce::ColourGradient (c1.brighter (0.5f), bar.getX(), bar.getY(), c1.darker (0.2f), bar.getX(), bar.getBottom(), false));
                g.fillRoundedRectangle (bar, 2.5f);
                g.setColour (juce::Colours::white.withAlpha (0.85f));
                g.fillRoundedRectangle (bar.removeFromTop (2.0f), 1.0f);
            }
            if (isCurrent)
                Theme::drawActiveGlow (g, pad, 4.0f, Theme::cyan);
            continue;
        }

        if (vel > 0)
        {
            // Step acceso: brilla del colore del canale (piu' forte se la velocity e' alta)
            const float level = 0.45f + 0.55f * (float) vel / 127.0f;
            g.setColour (chColour.withAlpha (0.22f * level));
            g.fillRoundedRectangle (pad.expanded (1.5f), 5.5f);
            juce::ColourGradient grad (chColour.brighter (1.2f).withMultipliedAlpha (level), pad.getCentreX(), pad.getY() + pad.getHeight() * 0.38f,
                                       chColour.withMultipliedAlpha (0.55f + 0.45f * level), pad.getRight(), pad.getBottom(), true);
            g.setGradientFill (grad);
            g.fillRoundedRectangle (pad, 4.0f);
            g.setColour (chColour.brighter (0.8f).withAlpha (0.8f * level));
            g.drawRoundedRectangle (pad.reduced (0.5f), 4.0f, 1.0f);
        }
        else
        {
            g.setGradientFill (juce::ColourGradient (off.brighter (0.06f), pad.getTopLeft(), off.darker (0.15f), pad.getBottomLeft(), false));
            g.fillRoundedRectangle (pad, 4.0f);
            g.setColour (Theme::cyan.withAlpha (0.08f));
            g.drawRoundedRectangle (pad.reduced (0.5f), 4.0f, 1.0f);
        }

        if (isCurrent)   // la colonna che suona: contorno azzurro luminoso
            Theme::drawActiveGlow (g, pad, 4.0f, Theme::cyan);

        if (vel > 0)
        {
            // Segni sullo step: tacche per il ratchet, puntino per la probabilita' < 100%
            const int rat = engine.getStepRatchet (channel, step);
            const bool showRatchet = rat > 1 || mode == EditMode::ratchet;

            if (showRatchet)
            {
                auto inner = pad.reduced (pad.getWidth() * 0.22f, pad.getHeight() * 0.3f);
                const float segH = inner.getHeight() / (float) SequencerEngine::maxRatchet;

                for (int i = 0; i < rat; ++i)
                {
                    g.setColour (Theme::bg.withAlpha (0.75f));
                    g.fillRect (inner.withY (inner.getBottom() - segH * (float) (i + 1)).withHeight (segH - 2.0f));
                }
            }

            if (engine.getStepProbability (channel, step) < 100)
            {
                g.setColour (Theme::accent);
                g.fillEllipse (pad.getRight() - 7.0f, pad.getY() + 3.0f, 4.0f, 4.0f);
            }
        }
    }
}

void ChannelRow::paintOverChildren (juce::Graphics& g)
{
    // Canale congelato: velo azzurro "di ghiaccio" sugli step
    if (engine.getChannel (channel).frozen.load() == nullptr)
        return;
    const bool valid = engine.isFrozenValid (channel);
    auto area = getPadsArea();
    g.setColour (juce::Colour (0xff9fdcff).withAlpha (valid ? 0.16f : 0.08f));
    g.fillRoundedRectangle (area, 4.0f);
    g.setColour (juce::Colour (0xff9fdcff).withAlpha (0.7f));
    g.drawRoundedRectangle (area.reduced (0.5f), 4.0f, 1.0f);
    g.setFont (Theme::font (juce::jlimit (10.0f, 14.0f, area.getHeight() * 0.3f), true));
    g.setColour (juce::Colours::white.withAlpha (0.9f));
    g.drawText (juce::String (juce::CharPointer_UTF8 ("\xe2\x9d\x84  ")) + (valid ? "CONGELATO" : "DA RICONGELARE (tempo o lunghezza cambiati)"),
                area.reduced (8.0f, 0.0f), juce::Justification::centredRight, false);
}

//==============================================================================
void ChannelRow::mouseDown (const juce::MouseEvent& e)
{
    lastColumn = -1;
    pressOnName = false;

    if (muteArea.expanded (2.0f).contains (e.position))
    {
        auto& m = engine.getChannel (channel).muted;
        m.store (! m.load());
        if (onTap) onTap();
        repaint();
        return;
    }

    if (menuArea.contains (e.position))
    {
        showRowMenu();
        return;
    }

    if (nameArea.contains (e.position))
    {
        // Tocco breve = ascolta; pressione lunga = menu del canale; trascinamento = scorre la lista
        pressOnName = true;
        nameDragging = false;
        longPressDone = false;
        startTimer (450);
        return;
    }

    // Canale congelato: per modificarlo bisogna prima scongelarlo
    if (engine.getChannel (channel).frozen.load() != nullptr && getPadsArea().contains (e.position))
    {
        if (onCommand) onCommand (channel, askUnfreeze);
        return;
    }

    if (engine.hasNotes (channel))
    {
        if (getPadsArea().contains (e.position) && onOpenPianoRoll)
            onOpenPianoRoll (channel);
        return;
    }

    const int step = stepAt (e.position);
    if (step < 0 || step >= engine.getPatternLength())
        return;

    lastColumn = step;
    pressPos = e.position;
    hScrolling = false;
    pressedStep = step;
    savedVelocity = engine.getStepVelocity (channel, step);
    savedProbability = engine.getStepProbability (channel, step);
    savedRatchet = engine.getStepRatchet (channel, step);

    switch (mode)
    {
        case EditMode::velocity:
        case EditMode::probability:
            applyVerticalValueAt (e.position);
            return;

        case EditMode::pitch:
        case EditMode::pan:
            valueDragStep = -1;
            if (! engine.isStepOn (channel, step))
                return;
            if (e.getNumberOfClicks() >= 2)   // doppio tocco: torna al valore naturale
            {
                if (mode == EditMode::pitch) engine.setStepPitch (channel, step, 0);
                else                         engine.setStepPan (channel, step, 0);
                repaint();
                return;
            }
            valueDragStep = step;
            valueDragStart = mode == EditMode::pitch ? engine.getStepPitch (channel, step) : engine.getStepPan (channel, step);
            if (mode == EditMode::pan)
                applyStepValue (e);
            repaint();
            return;

        case EditMode::ratchet:
            if (engine.isStepOn (channel, step))
                engine.setStepRatchet (channel, step, engine.getStepRatchet (channel, step) % SequencerEngine::maxRatchet + 1);
            break;

        case EditMode::step:
            // Tenendo premuto (senza muoversi) si apre il riempimento automatico: si ricorda com'era lo step
            pressOnStep = true;
            stepLongPress = false;
            startTimer (500);

            paintValue = ! engine.isStepOn (channel, step);
            if (paintValue)
            {
                engine.setStepVelocity (channel, step, SequencerEngine::defaultVelocity);
                engine.setStepProbability (channel, step, 100);
                engine.setStepRatchet (channel, step, 1);
            }
            else
            {
                engine.setStepVelocity (channel, step, 0);
            }
            break;
    }

    if (onTap) onTap();
    repaint();
}

void ChannelRow::mouseDrag (const juce::MouseEvent& e)
{
    if (pressOnName)
    {
        if (! nameDragging && ! longPressDone && std::abs (e.getDistanceFromDragStartY()) > 8)
        {
            nameDragging = true;
            stopTimer();
            if (onScrollDrag) onScrollDrag (0.0f, true);
        }

        if (nameDragging && onScrollDrag)
            onScrollDrag ((float) e.getDistanceFromDragStartY(), false);
        return;
    }

    if ((mode == EditMode::pitch || mode == EditMode::pan) && valueDragStep >= 0)
    {
        applyStepValue (e);
        return;
    }

    if (lastColumn < 0)
        return;

    // Pattern piu' lungo dello schermo: trascinando in orizzontale si scorre (il tocco iniziale viene annullato)
    if (scrollable && (mode == EditMode::step || mode == EditMode::ratchet) && ! stepLongPress)
    {
        const float dx = (float) e.getDistanceFromDragStartX(), dy = (float) e.getDistanceFromDragStartY();
        if (! hScrolling && std::abs (dx) > 10.0f && std::abs (dx) > std::abs (dy))
        {
            hScrolling = true;
            stopTimer();
            pressOnStep = false;
            engine.setStepVelocity (channel, pressedStep, savedVelocity);
            engine.setStepProbability (channel, pressedStep, savedProbability);
            engine.setStepRatchet (channel, pressedStep, savedRatchet);
            repaint();
            if (onScrollSteps) onScrollSteps (0.0f, 0);
        }
        if (hScrolling)
        {
            if (onScrollSteps) onScrollSteps (dx, 1);
            return;
        }
        return;   // finche' non si capisce se e' uno scorrimento, niente "pittura"
    }

    if (mode == EditMode::velocity || mode == EditMode::probability)
    {
        applyVerticalValueAt (e.position);
        return;
    }

    if (mode != EditMode::step)
        return;

    const int col = stepAt ({ e.position.x, getPadsArea().getCentreY() });
    if (stepLongPress)
        return;
    if (col >= 0 && col != lastColumn && col < engine.getPatternLength())
    {
        pressOnStep = false;   // e' un trascinamento: niente riempimento
        stopTimer();
        const int step = col;
        engine.setStepVelocity (channel, step, paintValue ? SequencerEngine::defaultVelocity : 0);
        if (paintValue)
        {
            engine.setStepProbability (channel, step, 100);
            engine.setStepRatchet (channel, step, 1);
        }
        lastColumn = col;
        if (onTap) onTap();
        repaint();
    }
}

void ChannelRow::mouseUp (const juce::MouseEvent& e)
{
    stopTimer();
    if (hScrolling)
    {
        hScrolling = false;
        lastColumn = -1;
        if (onScrollSteps) onScrollSteps ((float) e.getDistanceFromDragStartX(), 2);
        return;
    }
    if (stepLongPress)
    {
        stepLongPress = pressOnStep = false;
        showFillPanel (pressedStep);
        return;
    }
    pressOnStep = false;
    const bool wasName = pressOnName, dragged = nameDragging, longPress = longPressDone;
    pressOnName = nameDragging = longPressDone = false;

    if (! wasName || dragged)
        return;

    if (longPress)
    {
        showRowMenu();   // il menu si apre al rilascio: cosi' il dito non sceglie una voce per sbaglio
        return;
    }

    engine.previewChannel (channel);
    if (onTap) onTap();
}

void ChannelRow::timerCallback()
{
    stopTimer();
    if (pressOnStep)
    {
        // Pressione lunga su uno step: lo step torna com'era e al rilascio si apre il riempimento
        engine.setStepVelocity (channel, pressedStep, savedVelocity);
        engine.setStepProbability (channel, pressedStep, savedProbability);
        engine.setStepRatchet (channel, pressedStep, savedRatchet);
        stepLongPress = true;
        flash = 1.0f;
        repaint();
        if (onLongPress) onLongPress();
        return;
    }
    if (pressOnName && ! nameDragging)
    {
        longPressDone = true;
        flash = 1.0f;          // il pulsante si illumina: "rilascia per il menu"
        repaint();
        if (onLongPress) onLongPress();
    }
}

void ChannelRow::showFillPanel (int step)
{
    auto* top = overlayHost (*this);
    if (top == nullptr)
        return;

    auto panel = std::make_unique<StepFillPanel> (engine, channel, step);
    juce::Component::SafePointer<ChannelRow> safe (this);
    panel->onChanged = [safe] { if (safe != nullptr) safe->repaint(); };

    auto* overlay = new FillOverlay (std::move (panel));   // si cancella da sola quando si chiude
    top->addAndMakeVisible (overlay);
    overlay->setBounds (top->getLocalBounds());
    overlay->toFront (true);
}

void ChannelRow::applyStepValue (const juce::MouseEvent& e)
{
    const int step = valueDragStep;
    if (step < 0)
        return;
    if (mode == EditMode::pitch)
    {
        // Relativo: un semitono ogni 7 pixel (preciso anche su pad piccoli)
        const int semi = valueDragStart - juce::roundToInt ((float) e.getDistanceFromDragStartY() / 7.0f);
        if (semi != engine.getStepPitch (channel, step))
        {
            engine.setStepPitch (channel, step, semi);
            if (onTap) onTap();
        }
    }
    else
    {
        // Assoluto: posizione del dito dentro lo step (bordo sinistro = tutto a sinistra)
        const auto pad = getStepBounds (step).reduced (4.0f, 0.0f);
        const float rel = juce::jlimit (-1.0f, 1.0f, (e.position.x - pad.getCentreX()) / (pad.getWidth() * 0.5f));
        const int pan = juce::roundToInt (rel * 64.0f / 8.0f) * 8;   // a scatti (L100, L88 ... C ... R100)
        if (pan != engine.getStepPan (channel, step))
        {
            engine.setStepPan (channel, step, pan);
            if (onTap) onTap();
        }
    }
    repaint (getStepBounds (step).toNearestInt().expanded (3));
}

void ChannelRow::applyVerticalValueAt (juce::Point<float> p)
{
    // Il dito puo' scorrere sugli step: conta la colonna sotto il dito e la sua altezza nella riga.
    const int col = stepAt ({ p.x, getPadsArea().getCentreY() });
    if (col < 0 || col >= engine.getPatternLength())
        return;

    const int step = col;
    auto pad = getStepBounds (step).reduced (3.0f);
    const float norm = juce::jlimit (0.0f, 1.0f, 1.0f - (p.y - pad.getY()) / juce::jmax (1.0f, pad.getHeight()));

    if (mode == EditMode::velocity)
    {
        if (! engine.isStepOn (channel, step))
        {
            engine.setStepProbability (channel, step, 100);
            engine.setStepRatchet (channel, step, 1);
        }
        engine.setStepVelocity (channel, step, juce::jlimit (1, 127, juce::roundToInt (norm * 127.0f)));
    }
    else if (engine.isStepOn (channel, step))
    {
        // Probabilita' a scatti del 5%, minimo 5%
        engine.setStepProbability (channel, step, juce::jlimit (5, 100, juce::roundToInt (norm * 20.0f) * 5));
    }

    lastColumn = col;
    repaint();
}

//==============================================================================
void ChannelRow::refresh (int currentStep)
{
    auto& activity = engine.getChannel (channel).activity;
    const float a = activity.load();
    bool needsRepaint = false;

    if (a > 0.0f)
    {
        flash = juce::jmax (flash, a);
        activity.store (0.0f);
    }

    // Il lampeggio del nome tocca solo il pulsante del nome (non tutta la riga)
    bool nameRepaint = false;
    if (flash > 0.01f)
    {
        flash *= 0.8f;
        nameRepaint = true;
    }
    else if (flash > 0.0f)
    {
        flash = 0.0f;
        nameRepaint = true;
    }

    // Nome, colore, solo, muto cambiati altrove (menu, undo, caricamento)
    {
        auto& ch = engine.getChannel (channel);
        const auto look = (juce::uint64) ch.name.hashCode64() * 31u + ch.colour.load() * 7u
                        + (ch.solo.load() ? 3u : 0u) + (ch.muted.load() ? 5u : 0u) + (juce::uint64) ch.sampleName.hashCode64();
        if (look != shownLook)
        {
            shownLook = look;
            needsRepaint = true;
        }
    }

    // Cambiamenti fatti altrove (kit globale, cambio pattern, caricamento progetto, undo)
    if (const int kit = engine.getChannelKit (channel); kit != shownKit)
    {
        shownKit = kit;
        needsRepaint = true;
    }

    if (engine.isSynth (channel))
        if (const auto label = getSoundLabel (engine, channel); label != shownLabel)
        {
            shownLabel = label;
            needsRepaint = true;
        }

    if (const void* smp = engine.getChannel (channel).sample.load(); smp != shownSample)
    {
        shownSample = smp;
        needsRepaint = true;
    }

    if (const int pat = engine.getCurrentPattern() * 16 + engine.getPlayingPattern(); pat != shownPattern)
    {
        shownPattern = pat;
        needsRepaint = true;
    }

    if (std::abs (volumeKnob.getValue() - engine.getChannel (channel).volume.load()) > 0.0001
         && ! volumeKnob.isMouseButtonDown())
        volumeKnob.setValue (engine.getChannel (channel).volume.load(), juce::dontSendNotification);

    // La colonna che suona: si ridisegnano solo lo step di prima e quello nuovo
    const int previousStep = current;
    const bool stepMoved = currentStep != current;
    current = currentStep;

    // Note cambiate (dal piano roll, undo, caricamento)
    juce::uint64 hash = (juce::uint64) engine.getCurrentPattern() + 1;
    for (auto& n : engine.getNotes (channel))
        hash = hash * 1099511628211ull + (juce::uint64) SequencerEngine::packNote (n);
    if (hash != shownNotesHash)
    {
        shownNotesHash = hash;
        needsRepaint = true;
    }

    if (needsRepaint)
    {
        repaint();
        return;
    }
    if (nameRepaint)
        repaint (nameArea.toNearestInt().expanded (3));
    if (stepMoved && ! engine.hasNotes (channel))
        for (int s : { previousStep, currentStep })
            if (s >= 0 && isStepVisible (s))
                repaint (getStepBounds (s).toNearestInt().expanded (3).getIntersection (getPadsArea().toNearestInt().expanded (3)));
}

//==============================================================================
void ChannelRow::showRowMenu()
{
    auto& chan = engine.getChannel (channel);
    const bool synth = engine.isSynth (channel);
    const int numChannels = engine.getNumChannels();

    juce::PopupMenu fill, steps, colours;
    fill.addItem (201, "Ogni step");
    fill.addItem (202, "Ogni 2 step (ottavi)");
    fill.addItem (204, "Ogni 4 step (quarti)");
    fill.addItem (208, "Ogni 8 step");
    fill.addItem (210, "Controtempo (sugli ottavi pari)");

    steps.addItem (2, juce::String (juce::CharPointer_UTF8 ("Sposta a sinistra  \xe2\x97\x80")));
    steps.addItem (3, juce::String (juce::CharPointer_UTF8 ("Sposta a destra  \xe2\x96\xb6")));
    steps.addItem (4, "Inverti");
    steps.addSubMenu ("Riempi", fill);
    steps.addSeparator();
    steps.addItem (5, "Copia riga");
    steps.addItem (6, "Incolla riga", clipboard.valid);
    steps.addItem (7, engine.hasNotes (channel) ? "Svuota riga (step e note)" : "Svuota riga");

    for (int i = 0; i < numColours; ++i)
        colours.addColouredItem (colourBase + i, getPaletteName (i), getPaletteColour (i), true,
                                 getPaletteColour (i).getARGB() == chan.colour.load());

    juce::PopupMenu menu;
    menu.addSectionHeader (chan.name);
    menu.addItem (changeSound, synth ? "Preset..." : "Cambia suono...");
    const auto* info = Instruments::find (chan.instrumentKind);
    menu.addItem (editSound, synth ? "Apri " + juce::String (info != nullptr ? info->name : "il synth") + "..."
                                   : juce::String ("Modifica suono e mix..."));
    menu.addItem (openPianoRoll, "Piano roll...");
    if (synth)
        menu.addItem (chan.frozen.load() != nullptr ? (int) unfreeze : (int) freeze,
                      chan.frozen.load() != nullptr ? juce::String (juce::CharPointer_UTF8 ("Scongela  \xe2\x9d\x84"))
                                                    : juce::String (juce::CharPointer_UTF8 ("Congela traccia (meno CPU)  \xe2\x9d\x84")));
    menu.addSeparator();
    menu.addItem (15, "Muto", true, chan.muted.load());
    menu.addItem (solo, "Solo (ascolta solo questo)", true, chan.solo.load());
    menu.addSeparator();
    menu.addItem (clone, "Clona canale", numChannels < SequencerEngine::maxChannels);
    menu.addItem (rename, "Rinomina...");
    menu.addSubMenu ("Colore", colours);
    menu.addItem (moveUp, juce::String (juce::CharPointer_UTF8 ("Sposta su  \xe2\x96\xb2")), channel > 0);
    menu.addItem (moveDown, juce::String (juce::CharPointer_UTF8 ("Sposta gi\xc3\xb9  \xe2\x96\xbc")), channel < numChannels - 1);
    menu.addSeparator();
    menu.addSubMenu ("Step", steps);

    // Mixer: a quale traccia va il suono del canale
    juce::PopupMenu route;
    const int currentTrack = engine.getChannelTrack (channel);
    const int freeTrack = engine.findFreeInsert();
    for (int t = 0; t <= mixer::numInserts; ++t)
        if (t == 0 || t == currentTrack || engine.isTrackUsed (t))
            route.addColouredItem (mixerTrackBase + t, (t == 0 ? juce::String ("Master") : juce::String (t) + "  " + engine.getTrackName (t)),
                                   engine.getTrackColour (t), true, t == currentTrack);
    if (freeTrack > 0 && freeTrack != currentTrack)
        route.addItem (mixerTrackBase + freeTrack, "Traccia nuova (" + juce::String (freeTrack) + ")");
    route.addSeparator();
    route.addItem (openMixer, "Apri nel mixer...");
    menu.addSubMenu ("Mixer: " + (currentTrack == 0 ? juce::String ("Master") : "traccia " + juce::String (currentTrack)), route);
    menu.addSeparator();
    menu.addItem (remove, "Elimina canale", numChannels > 1);

    juce::Component::SafePointer<ChannelRow> safeThis (this);

    menu.showMenuAsync (juce::PopupMenu::Options().withTargetComponent (this)
                                                  .withTargetScreenArea (localAreaToGlobal (nameArea.toNearestInt())),
                        [safeThis] (int result)
                        {
                            if (safeThis == nullptr || result <= 0)
                                return;

                            auto& self = *safeThis;
                            auto& e = self.engine;
                            const int c = self.channel;

                            if (result == 2)        e.shiftRow (c, -1);
                            else if (result == 3)   e.shiftRow (c, 1);
                            else if (result == 4)   e.reverseRow (c);
                            else if (result == 5)
                            {
                                for (int s = 0; s < SequencerEngine::maxSteps; ++s)
                                {
                                    clipboard.velocity[s]    = (uint8_t) e.getStepVelocity (c, s);
                                    clipboard.probability[s] = (uint8_t) e.getStepProbability (c, s);
                                    clipboard.ratchet[s]     = (uint8_t) e.getStepRatchet (c, s);
                                    clipboard.semis[s]       = (int8_t) e.getStepPitch (c, s);
                                    clipboard.pan[s]         = (int8_t) e.getStepPan (c, s);
                                }
                                clipboard.valid = true;
                            }
                            else if (result == 6)
                            {
                                for (int s = 0; s < SequencerEngine::maxSteps; ++s)
                                {
                                    e.setStepVelocity (c, s, clipboard.velocity[s]);
                                    e.setStepProbability (c, s, clipboard.probability[s]);
                                    e.setStepRatchet (c, s, clipboard.ratchet[s]);
                                    e.setStepPitch (c, s, clipboard.semis[s]);
                                    e.setStepPan (c, s, clipboard.pan[s]);
                                }
                            }
                            else if (result == 7)   { e.clearRow (c); e.setNotes (c, {}); }
                            else if (result == 15)
                            {
                                auto& m = e.getChannel (c).muted;
                                m.store (! m.load());
                            }
                            else if (result == 210) e.fillRow (c, 4, 2);
                            else if (result > 200 && result < 300)  e.fillRow (c, result - 200);
                            else if (result >= mixerTrackBase && result <= mixerTrackBase + mixer::numInserts)
                                e.setChannelTrack (c, result - mixerTrackBase);
                            else if (result >= colourBase && result < colourBase + numColours)
                                e.getChannel (c).colour.store (getPaletteColour (result - colourBase).getARGB());
                            else if (self.onCommand)
                            {
                                self.onCommand (c, result);   // gestiti dalla schermata principale
                                return;                       // (la riga potrebbe essere gia' stata ricreata)
                            }

                            self.repaint();
                        });
}

juce::Colour ChannelRow::getPaletteColour (int i)
{
    static const juce::uint32 colours[numColours] =
    {
        0xffe8833a, 0xffd2555a, 0xffd8b14a, 0xff4fb3a9, 0xff5b8fd6, 0xff9a78d0,
        0xff2fc6d6, 0xff6fbf5a, 0xffe36fae, 0xffc49360, 0xff8aa0b8, 0xffe0e0e0
    };
    return juce::Colour (colours[juce::jlimit (0, numColours - 1, i)]);
}

juce::String ChannelRow::getPaletteName (int i)
{
    static const char* names[numColours] =
    {
        "Arancione", "Rosso", "Giallo", "Verde acqua", "Blu", "Viola",
        "Ciano", "Verde", "Rosa", "Legno", "Grigio", "Bianco"
    };
    return names[juce::jlimit (0, numColours - 1, i)];
}

juce::String ChannelRow::getSoundLabel (SequencerEngine& engine, int channel)
{
    if (engine.isSynth (channel))
    {
        return Instruments::presetName (engine.getInstrument (channel));
    }

    auto& ch = engine.getChannel (channel);
    if (ch.sample.load() != nullptr)
    {
        // Libreria: il nome del suono (o la categoria, se il canale si chiama gia' come il suono)
        if (auto* entry = SampleStore::findLibraryEntry (ch.sampleName); entry != nullptr && SampleStore::isLibrary (ch.sampleName))
        {
            const auto soundName = juce::String (juce::CharPointer_UTF8 (entry->name));
            return soundName == ch.name ? juce::String (juce::CharPointer_UTF8 (entry->category)) : soundName;
        }
        return juce::String (juce::CharPointer_UTF8 ("\xe2\x99\xaa ")) + SampleStore::displayName (ch.sampleName);
    }

    return SoundLibrary::getKitName (engine.getChannelKit (channel));
}

void ChannelRow::setPlayTick (int tick)
{
    if (tick != playTick)
    {
        playTick = tick;
        if (engine.hasNotes (channel))
            repaint (getPadsArea().toNearestInt().expanded (2));
    }
}

void ChannelRow::paintNotesPreview (juce::Graphics& g, bool muted)
{
    auto area = getPadsArea();
    Theme::drawSunken (g, area, Theme::bg, 3.0f);

    const auto notes = engine.getNotes (channel);
    const int lengthTicks = engine.getPatternLength() * SequencerEngine::ticksPerStep;
    const int firstTick = juce::roundToInt (firstStep * (float) SequencerEngine::ticksPerStep);
    const int visibleTicks = visibleSteps * SequencerEngine::ticksPerStep;

    // Linee dei quarti
    for (int t = 0; t < visibleTicks; t += 4 * SequencerEngine::ticksPerStep)
    {
        g.setColour (Theme::raised);
        g.drawVerticalLine ((int) (area.getX() + area.getWidth() * (float) t / (float) visibleTicks), area.getY() + 2.0f, area.getBottom() - 2.0f);
    }

    // Gamma di altezze da mostrare (almeno un'ottava)
    int lo = 127, hi = 0;
    for (auto& n : notes) { lo = juce::jmin (lo, n.pitch); hi = juce::jmax (hi, n.pitch); }
    if (hi - lo < 12) { const int mid = (lo + hi) / 2; lo = mid - 6; hi = mid + 6; }

    auto inner = area.reduced (2.0f, 4.0f);
    const float rowH = inner.getHeight() / (float) (hi - lo + 1);
    const auto noteColour = getColour().interpolatedWith (Theme::stepOn, 0.35f).withMultipliedAlpha (muted ? 0.4f : 1.0f);

    for (auto& n : notes)
    {
        if (n.start + n.length <= firstTick || n.start >= firstTick + visibleTicks)
            continue;

        const float x0 = inner.getX() + inner.getWidth() * (float) (n.start - firstTick) / (float) visibleTicks;
        const float x1 = inner.getX() + inner.getWidth() * (float) (n.start + n.length - firstTick) / (float) visibleTicks;
        const float y = inner.getBottom() - (float) (n.pitch - lo + 1) * rowH;
        g.setColour (noteColour);
        g.fillRoundedRectangle (juce::Rectangle<float> (x0, y, juce::jmax (2.0f, x1 - x0 - 1.0f), juce::jmax (2.0f, rowH - 1.0f))
                                    .getIntersection (inner), 1.0f);
    }

    // Oltre la fine del pattern
    if (firstTick + visibleTicks > lengthTicks)
    {
        const float x = inner.getX() + inner.getWidth() * (float) (lengthTicks - firstTick) / (float) visibleTicks;
        g.setColour (Theme::panel);
        g.fillRect (juce::Rectangle<float> (juce::jmax (x, area.getX()), area.getY(), area.getRight() - juce::jmax (x, area.getX()), area.getHeight()));
    }

    // Posizione di riproduzione
    if (playTick >= firstTick && playTick < firstTick + visibleTicks && engine.getPlayingPattern() == engine.getCurrentPattern())
    {
        g.setColour (Theme::accent);
        g.fillRect (inner.getX() + inner.getWidth() * (float) (playTick - firstTick) / (float) visibleTicks, area.getY(), 1.5f, area.getHeight());
    }

    g.setColour (Theme::textDim);
    g.setFont (Theme::font (10.0f, true));
    g.drawText ("PIANO ROLL", area.reduced (6.0f, 3.0f), juce::Justification::topRight, false);
}
