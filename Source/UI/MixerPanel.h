#pragma once

#include <JuceHeader.h>
#include <functional>
#include "../Engine/SequencerEngine.h"
#include "Theme.h"
#include "Widgets.h"
#include "ChannelEditor.h"   // LabeledKnob

//==============================================================================
/** Conversione volume della traccia (guadagno) <-> dB del fader. */
namespace MixerFader
{
    inline constexpr double minDb = -60.0, maxDb = 4.0;
    inline double gainToDb (float g)     { return g <= 0.001f ? minDb : juce::jlimit (minDb, maxDb, (double) juce::Decibels::gainToDecibels (g)); }
    inline float dbToGain (double db)    { return db <= minDb + 0.01 ? 0.0f : juce::Decibels::decibelsToGain ((float) db); }
    inline juce::String text (double db) { return db <= minDb + 0.01 ? juce::String ("-inf") : (db > 0.05 ? "+" : "") + juce::String (db, 1) + " dB"; }
}

//==============================================================================
/** Una striscia del mixer: nome, effetti, panorama, fader con meter, muto e solo. */
class MixerStrip : public juce::Component
{
public:
    MixerStrip (SequencerEngine& e, int trackIndex) : engine (e), track (trackIndex)
    {
        fader.setSliderStyle (juce::Slider::LinearVertical);
        fader.setTextBoxStyle (juce::Slider::NoTextBox, false, 0, 0);
        fader.setRange (MixerFader::minDb, MixerFader::maxDb);
        fader.setSkewFactorFromMidPoint (-12.0);
        fader.setDoubleClickReturnValue (true, 0.0);
        fader.setColour (juce::Slider::trackColourId, Theme::raisedHi);
        fader.onValueChange = [this]
        {
            if (track == 0)
                engine.setMasterVolume (juce::jmin (1.5f, MixerFader::dbToGain (fader.getValue())));   // = la manopola Master
            else
                engine.getTrack (track).volume.store (MixerFader::dbToGain (fader.getValue()));
            repaint();
        };
        addAndMakeVisible (fader);

        pan.setSliderStyle (juce::Slider::RotaryVerticalDrag);
        pan.setTextBoxStyle (juce::Slider::NoTextBox, false, 0, 0);
        pan.setRange (-1.0, 1.0);
        pan.setDoubleClickReturnValue (true, 0.0);
        pan.setColour (juce::Slider::rotarySliderFillColourId, Theme::cyan);
        pan.getProperties().set ("bipolar", true);
        pan.setMouseDragSensitivity (160);
        pan.onValueChange = [this] { engine.getTrack (track).pan.store ((float) pan.getValue()); };
        addAndMakeVisible (pan);

        for (auto* b : { &muteButton, &soloButton })
        {
            b->setClickingTogglesState (true);
            addAndMakeVisible (b);
        }
        muteButton.setColour (juce::TextButton::buttonOnColourId, juce::Colour (0xffc94a4f));
        soloButton.setColour (juce::TextButton::buttonOnColourId, Theme::accent.darker (0.2f));
        muteButton.onClick = [this] { engine.getTrack (track).muted.store (muteButton.getToggleState()); };
        soloButton.onClick = [this] { engine.getTrack (track).solo.store (soloButton.getToggleState()); };
        muteButton.setVisible (track > 0);
        soloButton.setVisible (track > 0);
        pan.setVisible (track > 0);

        refresh();
    }

    std::function<void (int track)> onOpenEffects, onNameTap;
    int getTrack() const    { return track; }

    void setSelected (bool s)       { if (selected != s) { selected = s; repaint(); } }

    /** Rilegge le impostazioni (undo, caricamento) senza disturbare un dito che sta muovendo. */
    void refresh()
    {
        auto& tr = engine.getTrack (track);
        const float vol = track == 0 ? engine.getMasterVolume() : tr.volume.load();
        if (! fader.isMouseButtonDown())
            fader.setValue (MixerFader::gainToDb (vol), juce::dontSendNotification);
        if (! pan.isMouseButtonDown())
            pan.setValue (tr.pan.load(), juce::dontSendNotification);
        muteButton.setToggleState (tr.muted.load(), juce::dontSendNotification);
        soloButton.setToggleState (tr.solo.load(), juce::dontSendNotification);

        juce::String fx;
        int count = 0;
        for (auto& s : tr.slots)
            if (const int type = s.type.load(); type != mixer::none)
            {
                ++count;
                fx << (fx.isEmpty() ? "" : "\n") << mixer::info (type).name << (s.bypass.load() ? " (off)" : "");
            }
        const bool eq = tr.eqLow.load() != 0.0f || tr.eqMid.load() != 0.0f || tr.eqHigh.load() != 0.0f;
        const auto name = engine.getTrackName (track);
        const auto colour = engine.getTrackColour (track);
        if (fx != fxText || name != nameText || colour != nameColour || eq != eqOn)
        {
            fader.setColour (juce::Slider::thumbColourId, colour);
            fxText = fx; nameText = name; nameColour = colour; eqOn = eq; numFx = count;
            repaint();
        }
    }

    void setLevel (float peak)
    {
        const float l = juce::jmax (peak, level * 0.82f);
        if (std::abs (l - level) > 0.002f)
        {
            level = l;
            repaint (meterArea.toNearestInt().expanded (1));
        }
    }

    void paint (juce::Graphics& g) override
    {
        auto r = getLocalBounds().toFloat().reduced (2.0f, 0.0f);
        Theme::drawGlassPanel (g, r, 8.0f);
        if (selected || track == 0)
            Theme::drawActiveGlow (g, r, 8.0f, selected ? Theme::accent : Theme::accent.withAlpha (0.5f));

        // Nome con il colore della traccia (barretta luminosa)
        auto name = nameArea;      // copia: non consumare l'area del membro a ogni disegno
        auto stripe = name.removeFromTop (3.0f).withTrimmedLeft (4).withTrimmedRight (4);
        g.setColour (nameColour.withAlpha (0.3f));
        g.fillRoundedRectangle (stripe.expanded (1.0f, 2.0f), 2.5f);
        g.setColour (nameColour);
        g.fillRoundedRectangle (stripe, 1.5f);
        g.setColour (Theme::textDim);
        g.setFont (Theme::font (10.0f, true));
        g.drawText (track == 0 ? juce::String ("M") : juce::String (track), name.removeFromTop (12.0f), juce::Justification::centred, false);
        g.setColour (Theme::textBright);
        g.setFont (Theme::font (12.0f, true));
        g.drawFittedText (nameText, name.toNearestInt(), juce::Justification::centredTop, 2, 0.8f);

        // Effetti (tocca per aprirli)
        Theme::drawSunken (g, fxArea, Theme::lcdBg, 2.0f);
        g.setFont (Theme::font (10.0f));
        if (numFx == 0 && ! eqOn)
        {
            g.setColour (Theme::textDim);
            g.drawFittedText ("+ FX", fxArea.toNearestInt(), juce::Justification::centred, 1);
        }
        else
        {
            g.setColour (Theme::lcdText);
            auto lines = juce::StringArray::fromLines (fxText);
            if (eqOn) lines.insert (0, "EQ");
            auto area = fxArea.reduced (3.0f, 2.0f);
            const int maxLines = juce::jmax (1, (int) (area.getHeight() / 11.0f));
            for (int i = 0; i < juce::jmin (lines.size(), maxLines); ++i)
                g.drawText (i == maxLines - 1 && lines.size() > maxLines ? "+" + juce::String (lines.size() - i) + " ..." : lines[i],
                            area.removeFromTop (11.0f), juce::Justification::centredLeft, true);
        }

        // Valore del fader
        g.setColour (fader.isMouseButtonDown() ? Theme::accent : Theme::text);
        g.setFont (Theme::font (10.0f, true));
        g.drawText (MixerFader::text (fader.getValue()), valueArea, juce::Justification::centred, false);

        // Meter a LED: azzurro, poi arancione, rosso in cima
        Theme::drawSunken (g, meterArea, Theme::lcdBg, 2.0f);
        auto inner = meterArea.reduced (1.5f);
        const float amount = juce::jlimit (0.0f, 1.0f, std::sqrt (level));
        const int segments = juce::jlimit (8, 28, (int) (inner.getHeight() / 6.0f));
        const float segH = inner.getHeight() / (float) segments;
        for (int i = 0; i < segments; ++i)
        {
            const float f = (float) i / (float) segments;
            const auto colour = f < 0.62f ? Theme::cyan : f < 0.86f ? Theme::accent : juce::Colour (0xffff4d4d);
            const bool lit = f < amount;
            auto seg = juce::Rectangle<float> (inner.getX(), inner.getBottom() - segH * (float) (i + 1) + 0.75f, inner.getWidth(), segH - 1.5f);
            g.setColour (lit ? colour : colour.withAlpha (0.08f));
            g.fillRoundedRectangle (seg, 1.0f);
        }
    }

    void resized() override
    {
        auto r = getLocalBounds().reduced (5, 4);
        nameArea = r.removeFromTop (44).toFloat();
        r.removeFromTop (3);
        fxArea = r.removeFromTop (juce::jlimit (26, 70, getHeight() / 6)).toFloat();
        r.removeFromTop (3);

        auto buttons = r.removeFromBottom (track > 0 ? 26 : 0);
        if (track > 0)
        {
            muteButton.setBounds (buttons.removeFromLeft (buttons.getWidth() / 2).reduced (1, 0));
            soloButton.setBounds (buttons.reduced (1, 0));
            r.removeFromBottom (3);
        }

        if (track > 0)
            pan.setBounds (r.removeFromTop (34).withSizeKeepingCentre (34, 34));
        valueArea = r.removeFromTop (14).toFloat();

        auto meter = r.removeFromRight (8);
        meterArea = meter.toFloat().reduced (0.0f, 6.0f);
        fader.setBounds (r);
    }

    void mouseUp (const juce::MouseEvent& e) override
    {
        if (e.mouseWasDraggedSinceMouseDown())
            return;
        if (fxArea.contains (e.position))       { if (onOpenEffects) onOpenEffects (track); }
        else if (nameArea.contains (e.position)) { if (onNameTap) onNameTap (track); }
    }

private:
    SequencerEngine& engine;
    const int track;
    juce::Slider fader, pan;
    juce::TextButton muteButton { "M" }, soloButton { "S" };
    juce::Rectangle<float> nameArea, fxArea, meterArea, valueArea;
    juce::String fxText, nameText;
    juce::Colour nameColour;
    bool selected = false, eqOn = false;
    int numFx = 0;
    float level = 0.0f;
};

//==============================================================================
/**
    Griglia dell'effetto Glitch: 16 step (una battuta). Scegli un effetto dalla tavolozza
    (Ripeti 1/8, 1/16, 1/32, Tape stop, Reverse, Muto, Rallenta) e tocca/trascina sugli step per "dipingerlo";
    toccando uno step che ha gia' quell'effetto lo si riporta a normale.
*/
class GlitchGridView : public juce::Component,
                       private juce::Timer
{
public:
    GlitchGridView (SequencerEngine& e, int trackIndex, int slotIndex) : engine (e), track (trackIndex), slot (slotIndex)
    {
        startTimerHz (20);
    }

    std::function<void()> onChanged;

    void paint (juce::Graphics& g) override
    {
        // Tavolozza
        for (int m = 1; m < mixer::glitchGrid::numModes; ++m)
        {
            auto b = brushBounds (m);
            const bool on = m == brush;
            Theme::drawRaised (g, b, on ? modeColour (m).withAlpha (0.9f) : Theme::raised, 5.0f);
            g.setColour (on ? Theme::bg : modeColour (m));
            g.setFont (Theme::font (juce::jlimit (9.0f, 12.0f, b.getHeight() * 0.42f), true));
            g.drawFittedText (mixer::glitchGrid::name (m), b.toNearestInt().reduced (2, 0), juce::Justification::centred, 2, 0.7f);
        }
        auto pb = presetBounds();
        Theme::drawRaised (g, pb, Theme::raisedHi, 5.0f);
        g.setColour (Theme::accent);
        g.setFont (Theme::font (11.0f, true));
        g.drawFittedText ("Pronti " + juce::String (juce::CharPointer_UTF8 ("\xe2\x96\xbe")), pb.toNearestInt(), juce::Justification::centred, 1);

        // Griglia
        float params[mixer::maxParams];
        readParams (params);
        for (int s = 0; s < 16; ++s)
        {
            auto cell = stepBounds (s);
            const int mode = mixer::glitchGrid::get (params, s);
            const auto off = (s / 4) % 2 == 0 ? Theme::stepOffA : Theme::stepOffB;
            if (mode == 0)
                Theme::drawSunken (g, cell, off, 4.0f);
            else
            {
                const auto col = modeColour (mode);
                g.setColour (col.withAlpha (0.25f));
                g.fillRoundedRectangle (cell.expanded (1.5f), 5.0f);
                g.setGradientFill (juce::ColourGradient (col.brighter (0.4f), cell.getX(), cell.getY(), col.darker (0.3f), cell.getX(), cell.getBottom(), false));
                g.fillRoundedRectangle (cell, 4.0f);
                g.setColour (Theme::bg.withAlpha (0.9f));
                g.setFont (Theme::font (juce::jlimit (8.0f, 12.0f, cell.getWidth() * 0.28f), true));
                g.drawFittedText (mixer::glitchGrid::shortName (mode), cell.toNearestInt().reduced (1, 0), juce::Justification::centred, 2, 0.6f);
            }
            if (s == playing)
                Theme::drawActiveGlow (g, cell, 4.0f, Theme::cyan);
        }
    }

    void mouseDown (const juce::MouseEvent& e) override
    {
        for (int m = 1; m < mixer::glitchGrid::numModes; ++m)
            if (brushBounds (m).contains (e.position)) { brush = m; repaint(); return; }
        if (presetBounds().contains (e.position)) { showPresets(); return; }

        const int s = stepAt (e.position);
        if (s < 0) return;
        float params[mixer::maxParams];
        readParams (params);
        paintValue = mixer::glitchGrid::get (params, s) == brush ? 0 : brush;
        lastStep = -1;
        paintStep (s);
    }

    void mouseDrag (const juce::MouseEvent& e) override
    {
        if (const int s = stepAt (e.position); s >= 0 && s != lastStep)
            paintStep (s);
    }

    void mouseUp (const juce::MouseEvent&) override    { if (lastStep >= 0 && onChanged) onChanged(); lastStep = -1; }

private:
    juce::Colour modeColour (int m) const
    {
        static const juce::uint32 cols[] { 0xff808080, 0xff4fe3ff, 0xff3fb8ff, 0xff7d8bff, 0xffff6b5b, 0xffb07cff, 0xff707a84, 0xff7dffb0 };
        return juce::Colour (cols[juce::jlimit (0, 7, m)]);
    }

    juce::Rectangle<float> paletteArea() const  { return getLocalBounds().toFloat().removeFromTop (juce::jlimit (30.0f, 44.0f, (float) getHeight() * 0.32f)); }
    juce::Rectangle<float> brushBounds (int m) const
    {
        auto r = paletteArea().withTrimmedRight (86.0f);
        const float w = r.getWidth() / 7.0f;
        return { r.getX() + w * (float) (m - 1), r.getY(), w - 4.0f, r.getHeight() - 4.0f };
    }
    juce::Rectangle<float> presetBounds() const { auto r = paletteArea(); return r.removeFromRight (82.0f).withTrimmedBottom (4.0f); }
    juce::Rectangle<float> gridArea() const     { return getLocalBounds().toFloat().withTrimmedTop (paletteArea().getHeight() + 4.0f); }
    juce::Rectangle<float> stepBounds (int s) const
    {
        auto r = gridArea();
        const float group = 6.0f, gap = 2.0f;
        const float w = (r.getWidth() - group * 3.0f - gap * 12.0f) / 16.0f;
        const float x = r.getX() + (float) s * w + (float) (s / 4) * group + (float) (s - s / 4) * gap;
        return { x, r.getY(), w, juce::jmin (r.getHeight(), 90.0f) };
    }
    int stepAt (juce::Point<float> p) const
    {
        for (int s = 0; s < 16; ++s)
            if (stepBounds (s).expanded (1.5f, 6.0f).contains (p)) return s;
        return -1;
    }

    void readParams (float* params) const
    {
        auto& sl = engine.getTrack (track).slots[(size_t) slot];
        for (int p = 0; p < mixer::maxParams; ++p) params[p] = sl.params[(size_t) p].load();
    }

    void paintStep (int s)
    {
        auto& sl = engine.getTrack (track).slots[(size_t) slot];
        float params[mixer::maxParams];
        readParams (params);
        mixer::glitchGrid::set (params, s, paintValue);
        sl.params[1].store (params[1]);
        sl.params[2].store (params[2]);
        lastStep = s;
        repaint();
    }

    void setGrid (const char* pattern)   // 16 caratteri: . normale, 8/6/3 ripeti, s stop, r reverse, m muto, h rallenta
    {
        auto& sl = engine.getTrack (track).slots[(size_t) slot];
        float params[mixer::maxParams];
        readParams (params);
        for (int s = 0; s < 16; ++s)
        {
            const char ch = pattern[s];
            const int m = ch == '8' ? 1 : ch == '6' ? 2 : ch == '3' ? 3 : ch == 's' ? 4 : ch == 'r' ? 5 : ch == 'm' ? 6 : ch == 'h' ? 7 : 0;
            mixer::glitchGrid::set (params, s, m);
        }
        sl.params[1].store (params[1]);
        sl.params[2].store (params[2]);
        repaint();
        if (onChanged) onChanged();
    }

    void showPresets()
    {
        struct P { const char* name; const char* grid; };
        static const P presets[] {
            { "Rullata finale (1/32)",          "............3333" },
            { "Stutter crescente",              "........88663333" },
            { "Tape stop finale",               "............ssss" },
            { "Reverse a meta'",                "........rrrr...." },
            { "Gate trance (muto in levare)",   ".m.m.m.m.m.m.m.m" },
            { "Rallenta l'ultimo quarto",       "............hhhh" },
            { "Fill fidget",                    "....66..33..r.ss" },
            { "Svuota la griglia",              "................" },
        };
        juce::PopupMenu m;
        for (int i = 0; i < (int) std::size (presets); ++i)
            m.addItem (i + 1, presets[i].name);
        juce::Component::SafePointer<GlitchGridView> safe (this);
        m.showMenuAsync (juce::PopupMenu::Options().withTargetComponent (this), [safe] (int r)
        {
            if (safe != nullptr && r > 0)
                safe->setGrid (presets[r - 1].grid);
        });
    }

    void timerCallback() override
    {
        const int step = engine.isPlaying() ? engine.getPlayStepInBar() : -1;
        if (step != playing) { playing = step; repaint(); }
    }

    SequencerEngine& engine;
    const int track, slot;
    int brush = 3, paintValue = 3, lastStep = -1, playing = -1;
};

//==============================================================================
/**
    Griglia del Trance gate: 16 step accesi (il suono passa) o spenti (il suono si abbassa).
    Tocca/trascina per accendere o spegnere; "Pronti" ha i ritmi classici.
*/
class GateGridView : public juce::Component,
                     private juce::Timer
{
public:
    GateGridView (SequencerEngine& e, int trackIndex, int slotIndex) : engine (e), track (trackIndex), slot (slotIndex)
    {
        startTimerHz (25);
    }

    std::function<void()> onChanged;

    void paint (juce::Graphics& g) override
    {
        auto pb = presetBounds();
        Theme::drawRaised (g, pb, Theme::raisedHi, 5.0f);
        g.setColour (Theme::accent);
        g.setFont (Theme::font (11.0f, true));
        g.drawFittedText ("Pronti " + juce::String (juce::CharPointer_UTF8 ("\xe2\x96\xbe")), pb.toNearestInt(), juce::Justification::centred, 1);

        const float v = param();
        for (int s = 0; s < 16; ++s)
        {
            auto cell = stepBounds (s);
            const bool on = ((juce::uint32) juce::roundToInt (v) >> s) & 1u;
            if (on)
            {
                g.setGradientFill (juce::ColourGradient (Theme::cyan.brighter (0.3f), cell.getX(), cell.getY(), Theme::cyan.darker (0.5f), cell.getX(), cell.getBottom(), false));
                g.fillRoundedRectangle (cell, 4.0f);
            }
            else
                Theme::drawSunken (g, cell, (s / 4) % 2 == 0 ? Theme::stepOffA : Theme::stepOffB, 4.0f);
            if (s == playing)
                Theme::drawActiveGlow (g, cell, 4.0f, Theme::accent);
        }
    }

    void mouseDown (const juce::MouseEvent& e) override
    {
        if (presetBounds().contains (e.position)) { showPresets(); return; }
        const int s = stepAt (e.position);
        if (s < 0) return;
        paintOn = ! (((juce::uint32) juce::roundToInt (param()) >> s) & 1u);
        lastStep = -1;
        paintStep (s);
    }

    void mouseDrag (const juce::MouseEvent& e) override
    {
        if (const int s = stepAt (e.position); s >= 0 && s != lastStep)
            paintStep (s);
    }

    void mouseUp (const juce::MouseEvent&) override    { if (lastStep >= 0 && onChanged) onChanged(); lastStep = -1; }

private:
    float param() const      { return engine.getTrack (track).slots[(size_t) slot].params[0].load(); }

    juce::Rectangle<float> presetBounds() const { return getLocalBounds().toFloat().removeFromRight (82.0f).withSizeKeepingCentre (82.0f, juce::jmin (34.0f, (float) getHeight())); }
    juce::Rectangle<float> stepBounds (int s) const
    {
        auto r = getLocalBounds().toFloat().withTrimmedRight (90.0f);
        const float group = 6.0f, gap = 2.0f;
        const float w = (r.getWidth() - group * 3.0f - gap * 12.0f) / 16.0f;
        const float x = r.getX() + (float) s * w + (float) (s / 4) * group + (float) (s - s / 4) * gap;
        const float h = juce::jmin (r.getHeight(), 70.0f);
        return { x, r.getCentreY() - h * 0.5f, w, h };
    }
    int stepAt (juce::Point<float> p) const
    {
        for (int s = 0; s < 16; ++s)
            if (stepBounds (s).expanded (1.5f, 8.0f).contains (p)) return s;
        return -1;
    }

    void paintStep (int s)
    {
        auto& sl = engine.getTrack (track).slots[(size_t) slot];
        sl.params[0].store (mixer::gateGrid::set (sl.params[0].load(), s, paintOn));
        lastStep = s;
        repaint();
    }

    void setGrid (const char* pattern)   // 16 caratteri: x = passa, . = taglia
    {
        float v = 0.0f;
        for (int s = 0; s < 16; ++s)
            v = mixer::gateGrid::set (v, s, pattern[s] == 'x');
        engine.getTrack (track).slots[(size_t) slot].params[0].store (v);
        repaint();
        if (onChanged) onChanged();
    }

    void showPresets()
    {
        struct P { const char* name; const char* grid; };
        static const P presets[] {
            { "Trance classico",        "xx.xx.xx.xx.xx.x" },
            { "Sedicesimi alterni",     "x.x.x.x.x.x.x.x." },
            { "Levare",                 ".x.x.x.x.x.x.x.x" },
            { "Galoppo",                "x.xxx.xxx.xxx.xx" },
            { "Psy rolling",            ".xxx.xxx.xxx.xxx" },
            { "Spezzato",               "xx..x.xx..x.x.xx" },
            { "Sincopato",              "x..x..x..x..x.x." },
            { "Tutto acceso",           "xxxxxxxxxxxxxxxx" },
        };
        juce::PopupMenu m;
        for (int i = 0; i < (int) std::size (presets); ++i)
            m.addItem (i + 1, presets[i].name);
        juce::Component::SafePointer<GateGridView> safe (this);
        m.showMenuAsync (juce::PopupMenu::Options().withTargetComponent (this), [safe] (int r)
        {
            if (safe != nullptr && r > 0)
                safe->setGrid (presets[r - 1].grid);
        });
    }

    void timerCallback() override
    {
        int step = -1;
        if (const int abs = engine.isPlaying() ? engine.getPlayStep() : -1; abs >= 0)
        {
            const int rate = juce::jlimit (0, 2, juce::roundToInt (engine.getTrack (track).slots[(size_t) slot].params[1].load()));
            const int spb = mixer::gateGrid::stepsPerBeat[rate];          // step del gate per quarto
            step = (abs * spb / 4) % 16;                                    // abs e' in sedicesimi
        }
        if (step != playing) { playing = step; repaint(); }
    }

    SequencerEngine& engine;
    const int track, slot;
    bool paintOn = true;
    int lastStep = -1, playing = -1;
};

//==============================================================================
/** La curva dell'EQ parametrico (si aggiorna da sola mentre giri le manopole). */
class EqCurveView : public juce::Component,
                    private juce::Timer
{
public:
    EqCurveView (SequencerEngine& e, int trackIndex, int slotIndex) : engine (e), track (trackIndex), slot (slotIndex)
    {
        startTimerHz (20);
    }

    void paint (juce::Graphics& g) override
    {
        auto r = getLocalBounds().toFloat();
        Theme::drawSunken (g, r, Theme::lcdBg, 6.0f);
        r = r.reduced (4.0f);
        auto xOf = [r] (double f) { return r.getX() + (float) (std::log (f / 20.0) / std::log (1000.0)) * r.getWidth(); };
        auto yOf = [r] (float db) { return r.getCentreY() - juce::jlimit (-20.0f, 20.0f, db) / 20.0f * r.getHeight() * 0.5f; };

        g.setColour (Theme::cyan.withAlpha (0.12f));
        for (double f : { 50.0, 100.0, 200.0, 500.0, 1000.0, 2000.0, 5000.0, 10000.0 })
            g.drawVerticalLine (juce::roundToInt (xOf (f)), r.getY(), r.getBottom());
        g.drawHorizontalLine (juce::roundToInt (yOf (0.0f)), r.getX(), r.getRight());
        g.setColour (Theme::textDim);
        g.setFont (Theme::font (9.0f));
        for (auto [f, t] : { std::pair<double, const char*> { 100.0, "100" }, { 1000.0, "1k" }, { 10000.0, "10k" } })
            g.drawText (t, juce::Rectangle<float> (xOf (f) + 2.0f, r.getBottom() - 11.0f, 30.0f, 10.0f), juce::Justification::left);

        float p[mixer::maxParams];
        auto& sl = engine.getTrack (track).slots[(size_t) slot];
        for (int i = 0; i < mixer::maxParams; ++i) p[i] = sl.params[(size_t) i].load();
        juce::Path curve;
        for (int x = 0; x <= (int) r.getWidth(); x += 2)
        {
            const double f = 20.0 * std::pow (1000.0, x / (double) r.getWidth());
            const float y = yOf (mixer::ParamEq::responseDb (p, 48000.0, f));
            if (x == 0) curve.startNewSubPath (r.getX(), y); else curve.lineTo (r.getX() + (float) x, y);
        }
        juce::Path fill (curve);
        fill.lineTo (r.getRight(), yOf (0.0f));
        fill.lineTo (r.getX(), yOf (0.0f));
        fill.closeSubPath();
        g.setColour (Theme::accent.withAlpha (0.15f));
        g.fillPath (fill);
        g.setColour (Theme::accent);
        g.strokePath (curve, juce::PathStrokeType (2.0f));
    }

private:
    void timerCallback() override
    {
        auto& sl = engine.getTrack (track).slots[(size_t) slot];
        bool changed = false;
        for (int i = 0; i < mixer::maxParams; ++i)
            if (const float v = sl.params[(size_t) i].load(); v != shown[(size_t) i]) { shown[(size_t) i] = v; changed = true; }
        if (changed) repaint();
    }

    SequencerEngine& engine;
    const int track, slot;
    std::array<float, mixer::maxParams> shown {};
};

//==============================================================================
/** Effetti ed EQ di una traccia: 6 slot in fila, e le manopole dello slot scelto. */
class TrackEffectsView : public juce::Component
{
public:
    explicit TrackEffectsView (SequencerEngine& e) : engine (e)
    {
        eqKnobs.add (new LabeledKnob ("Bassi", -15.0, 15.0, 0.0, [] (double v) { return dbText (v); }));
        eqKnobs.add (new LabeledKnob ("Medi", -15.0, 15.0, 0.0, [] (double v) { return dbText (v); }));
        eqKnobs.add (new LabeledKnob ("Freq. medi", 100.0, 10000.0, 1000.0,
                                      [] (double v) { return v >= 1000.0 ? juce::String (v / 1000.0, 1) + " kHz" : juce::String (juce::roundToInt (v)) + " Hz"; }));
        eqKnobs.add (new LabeledKnob ("Alti", -15.0, 15.0, 0.0, [] (double v) { return dbText (v); }));
        eqKnobs[2]->slider.setSkewFactorFromMidPoint (1000.0);
        for (int i = 0; i < eqKnobs.size(); ++i)
        {
            eqKnobs[i]->onChange = [this, i] (double v)
            {
                auto& tr = engine.getTrack (track);
                (i == 0 ? tr.eqLow : i == 1 ? tr.eqMid : i == 2 ? tr.eqMidFreq : tr.eqHigh).store ((float) v);
            };
            addAndMakeVisible (eqKnobs[i]);
        }

        for (int s = 0; s < mixer::numSlots; ++s)
        {
            auto* row = slotRows.add (new SlotRow (*this, s));
            addAndMakeVisible (row);
        }
    }

    void setTrack (int t)
    {
        track = juce::jlimit (0, mixer::numTracks - 1, t);
        selectedSlot = 0;
        for (int s = 0; s < mixer::numSlots; ++s)
            if (engine.getTrack (track).slots[(size_t) s].type.load() != mixer::none)
            {
                selectedSlot = s;
                break;
            }
        refresh();
        rebuildParams();
    }

    int getTrack() const        { return track; }
    int getSelectedSlot() const { return selectedSlot; }

    void refresh()
    {
        auto& tr = engine.getTrack (track);
        const float values[4] { tr.eqLow.load(), tr.eqMid.load(), tr.eqMidFreq.load(), tr.eqHigh.load() };
        for (int i = 0; i < 4; ++i)
            if (! eqKnobs[i]->slider.isMouseButtonDown())
                eqKnobs[i]->setValue (values[i]);

        for (auto* row : slotRows)
            row->refresh();

        // Tipo cambiato altrove (undo): ricrea le manopole
        if (engine.getTrack (track).slots[(size_t) selectedSlot].type.load() != shownType)
            rebuildParams();

        for (int i = 0; i < paramKnobs.size(); ++i)
            if (auto* k = paramKnobs[i]; k != nullptr && ! k->slider.isMouseButtonDown())
            {
                const auto& def = mixer::info (shownType).params[(size_t) paramIndex[(size_t) i]];
                k->setValue (mixer::toNormalised (def, tr.slots[(size_t) selectedSlot].params[(size_t) paramIndex[(size_t) i]].load()));
            }
        for (auto* b : paramButtons)
            b->refresh();
        repaint();
    }

    void paint (juce::Graphics& g) override
    {
        Theme::drawCaption (g, "Equalizzatore", eqCaption);
        Theme::drawCaption (g, "Effetti (dall'alto in basso)", slotsCaption);
        const auto type = engine.getTrack (track).slots[(size_t) selectedSlot].type.load();
        Theme::drawCaption (g, type == mixer::none ? juce::String ("Slot " + juce::String (selectedSlot + 1) + ": vuoto")
                                                   : "Slot " + juce::String (selectedSlot + 1) + ": " + mixer::info (type).name, paramsCaption);
        if (type == mixer::none)
        {
            g.setColour (Theme::textDim);
            g.setFont (Theme::font (13.0f));
            g.drawFittedText ("Tocca il nome dello slot per scegliere un effetto.", paramsArea.toNearestInt(), juce::Justification::centred, 3);
        }
        else if (const juce::String detail (mixer::info (type).detail); detail.isNotEmpty())
        {
            g.setColour (Theme::textDim);
            g.setFont (Theme::font (11.0f));
            g.drawFittedText (detail, detailArea.toNearestInt(), juce::Justification::centredLeft, 1);
        }
    }

    void resized() override
    {
        auto r = getLocalBounds();
        const bool wide = r.getWidth() > r.getHeight() * 1.3f;

        juce::Rectangle<int> left, right;
        if (wide)
        {
            left = r.removeFromLeft (juce::jmin (300, r.getWidth() * 2 / 5));
            r.removeFromLeft (10);
            right = r;
        }
        else
        {
            left = r.removeFromTop (16 + mixer::numSlots * 44);
            r.removeFromTop (10);
            right = r;
        }

        // Sinistra: gli slot
        slotsCaption = left.removeFromTop (16).toFloat();
        const int rowH = juce::jlimit (30, 44, left.getHeight() / mixer::numSlots);
        for (auto* row : slotRows)
            row->setBounds (left.removeFromTop (rowH).reduced (0, 2));

        // Destra: EQ in alto, parametri sotto
        eqCaption = right.removeFromTop (16).toFloat();
        auto eq = right.removeFromTop (juce::jlimit (70, 110, right.getHeight() * 2 / 5));
        const int ew = eq.getWidth() / 4;
        for (auto* k : eqKnobs)
            k->setBounds (eq.removeFromLeft (ew).reduced (2, 0));
        right.removeFromTop (8);
        paramsCaption = right.removeFromTop (16).toFloat();
        detailArea = right.removeFromTop (16).toFloat();
        paramsArea = right;
        layoutParams();
    }

private:
    static juce::String dbText (double v)     { return (v > 0.05 ? "+" : "") + juce::String (v, 1) + " dB"; }

    //==========================================================================
    /** Una riga: numero, nome dell'effetto (tocca per sceglierlo), acceso/spento, su/giu'. */
    class SlotRow : public juce::Component
    {
    public:
        SlotRow (TrackEffectsView& v, int s) : view (v), slot (s)
        {
            nameButton.onClick = [this] { view.selectSlot (slot); view.showTypeMenu (slot, nameButton); };
            power.setClickingTogglesState (true);
            power.onClick = [this]
            {
                view.engine.getTrack (view.track).slots[(size_t) slot].bypass.store (! power.getToggleState());
                view.selectSlot (slot);
            };
            up.onClick = [this] { view.moveSlot (slot, slot - 1); };
            down.onClick = [this] { view.moveSlot (slot, slot + 1); };
            for (auto* c : std::initializer_list<juce::Component*> { &nameButton, &power, &up, &down })
                addAndMakeVisible (c);
        }

        void refresh()
        {
            const auto& s = view.engine.getTrack (view.track).slots[(size_t) slot];
            const int type = s.type.load();
            nameButton.setButtonText (juce::String (slot + 1) + "   " + (type == mixer::none ? juce::String ("(vuoto)") : juce::String (mixer::info (type).name)));
            repaint();
            nameButton.setColour (juce::TextButton::textColourOffId, type == mixer::none ? Theme::textDim : Theme::textBright);
            power.setToggleState (type != mixer::none && ! s.bypass.load(), juce::dontSendNotification);
            power.setEnabled (type != mixer::none);
            up.setEnabled (slot > 0 && type != mixer::none);
            down.setEnabled (slot < mixer::numSlots - 1 && type != mixer::none);
        }

        void paintOverChildren (juce::Graphics& g) override
        {
            if (view.selectedSlot == slot)
            {
                g.setColour (Theme::accent);
                g.drawRoundedRectangle (nameButton.getBounds().toFloat().reduced (0.5f), Theme::corner, 2.0f);
            }
        }

        void resized() override
        {
            auto r = getLocalBounds();
            down.setBounds (r.removeFromRight (30));
            r.removeFromRight (2);
            up.setBounds (r.removeFromRight (30));
            r.removeFromRight (4);
            power.setBounds (r.removeFromRight (66));
            r.removeFromRight (4);
            nameButton.setBounds (r);
        }

    private:
        TrackEffectsView& view;
        const int slot;
        juce::TextButton nameButton;
        LedToggle power;
        juce::TextButton up { juce::String (juce::CharPointer_UTF8 ("\xe2\x96\xb2")) }, down { juce::String (juce::CharPointer_UTF8 ("\xe2\x96\xbc")) };
    };

    /** Un parametro a scelta (tipo di filtro, tempo del delay, traccia sorgente): un pulsante con il menu. */
    class ChoiceParam : public juce::Component
    {
    public:
        ChoiceParam (TrackEffectsView& v, int p) : view (v), param (p)
        {
            button.onClick = [this] { showMenu(); };
            addAndMakeVisible (button);
            refresh();
        }

        void refresh()
        {
            const auto& def = currentDef();
            const float value = view.engine.getTrack (view.track).slots[(size_t) view.selectedSlot].params[(size_t) param].load();
            button.setButtonText (def.unit == mixer::Unit::track ? view.trackLabel (juce::roundToInt (value)) : mixer::formatValue (def, value));
        }

        void paint (juce::Graphics& g) override     { Theme::drawCaption (g, currentDef().name, getLocalBounds().toFloat().removeFromTop (14)); }
        void resized() override                     { button.setBounds (getLocalBounds().withTrimmedTop (16).withSizeKeepingCentre (getWidth() - 4, juce::jmin (34, getHeight() - 16))); }

    private:
        const mixer::ParamDef& currentDef() const
        {
            return mixer::info (view.engine.getTrack (view.track).slots[(size_t) view.selectedSlot].type.load()).params[(size_t) param];
        }

        void showMenu()
        {
            const auto& def = currentDef();
            const float value = view.engine.getTrack (view.track).slots[(size_t) view.selectedSlot].params[(size_t) param].load();
            juce::PopupMenu menu;
            for (int v = juce::roundToInt (def.minValue); v <= juce::roundToInt (def.maxValue); ++v)
            {
                if (def.unit == mixer::Unit::track)
                {
                    if (v == view.track || ! view.engine.isTrackUsed (v))
                        continue;
                    menu.addColouredItem (v + 1, view.trackLabel (v), view.engine.getTrackColour (v), true, v == juce::roundToInt (value));
                }
                else
                    menu.addItem (v + 1, mixer::formatValue (def, (float) v), true, v == juce::roundToInt (value));
            }
            if (menu.getNumItems() == 0)
                menu.addItem (-1, "(nessuna altra traccia in uso)", false);

            juce::Component::SafePointer<ChoiceParam> safe (this);
            const int slot = view.selectedSlot, track = view.track;
            menu.showMenuAsync (juce::PopupMenu::Options().withTargetComponent (&button), [safe, slot, track] (int result)
            {
                if (safe == nullptr || result <= 0)
                    return;
                safe->view.engine.getTrack (track).slots[(size_t) slot].params[(size_t) safe->param].store ((float) (result - 1));
                safe->refresh();
                if (safe->view.onChanged) safe->view.onChanged();
            });
        }

        TrackEffectsView& view;
        const int param;
        juce::TextButton button;

        friend class TrackEffectsView;
    };

    //==========================================================================
    juce::String trackLabel (int t) const   { return juce::String (t) + "  " + engine.getTrackName (t); }

    void selectSlot (int s)
    {
        if (s == selectedSlot)
            return;
        selectedSlot = s;
        refresh();
        rebuildParams();
    }

    void moveSlot (int from, int to)
    {
        if (to < 0 || to >= mixer::numSlots)
            return;
        engine.swapTrackEffects (track, from, to);
        selectedSlot = to;
        refresh();
        rebuildParams();
        if (onChanged) onChanged();
    }

    void showTypeMenu (int slot, juce::Component& target)
    {
        const int current = engine.getTrack (track).slots[(size_t) slot].type.load();
        juce::PopupMenu menu;
        menu.addSectionHeader ("Slot " + juce::String (slot + 1));
        using namespace mixer;
        struct Group { const char* name; std::initializer_list<int> types; };
        const Group groups[] {
            { "Dinamica",          { compressor, ott, transient, limiter, sidechain } },
            { "Tono ed EQ",        { filter, peq, exciter, vowel } },
            { "Spazio",            { reverb, shimmer, delay, chorus, stereo } },
            { "Movimento",         { gate, tremolo, phaser, flanger } },
            { "Sporco e lo-fi",    { saturation, bitcrusher, trash, vinyl } },
            { "Creativi",          { glitch, pitch, ringmod } },
        };
        for (auto& gr : groups)
        {
            menu.addSectionHeader (gr.name);
            for (int t : gr.types)
                menu.addItem (t + 1, mixer::info (t).name, true, t == current);
        }
        menu.addSeparator();
        menu.addItem (1, "Togli l'effetto", current != mixer::none);

        juce::Component::SafePointer<TrackEffectsView> safe (this);
        const int trackIndex = track;
        menu.showMenuAsync (juce::PopupMenu::Options().withTargetComponent (&target), [safe, slot, trackIndex] (int result)
        {
            if (safe == nullptr || result <= 0 || safe->track != trackIndex)
                return;
            safe->engine.setTrackEffect (trackIndex, slot, result - 1);
            // Sidechain: parte dalla traccia della cassa, se c'e'
            if (result - 1 == mixer::sidechain)
                if (const int kick = safe->findKickTrack(); kick > 0)
                    safe->engine.getTrack (trackIndex).slots[(size_t) slot].params[0].store ((float) kick);
            safe->selectedSlot = slot;
            safe->refresh();
            safe->rebuildParams();
            if (safe->onChanged) safe->onChanged();
        });
    }

    int findKickTrack() const
    {
        for (int c = 0; c < engine.getNumChannels(); ++c)
        {
            const auto name = engine.getChannel (c).name.toLowerCase();
            const int t = engine.getChannelTrack (c);
            if (t > 0 && t != track && (name.contains ("kick") || name.contains ("cassa")))
                return t;
        }
        return 0;
    }

    void rebuildParams()
    {
        paramKnobs.clear();
        paramButtons.clear();
        paramIndex.clear();
        auto& slot = engine.getTrack (track).slots[(size_t) selectedSlot];
        shownType = slot.type.load();
        const auto& inf = mixer::info (shownType);

        glitchView.reset();
        gateView.reset();
        eqView.reset();
        for (int p = 0; p < inf.numParams; ++p)
        {
            if (shownType == mixer::glitch && p > 0)
                break;   // la griglia sostituisce i parametri 2 e 3
            if (shownType == mixer::gate && p == 0)
                continue;   // il ritmo si disegna nella griglia
            const auto& def = inf.params[(size_t) p];
            if (def.unit == mixer::Unit::choice || def.unit == mixer::Unit::track)
            {
                auto* b = paramButtons.add (new ChoiceParam (*this, p));
                addAndMakeVisible (b);
                paramKnobs.add (nullptr);
            }
            else
            {
                auto* k = paramKnobs.add (new LabeledKnob (def.name, 0.0, 1.0, mixer::toNormalised (def, def.defaultValue),
                                                           [def] (double n) { return mixer::formatValue (def, mixer::fromNormalised (def, (float) n)); }));
                const int trackIndex = track, slotIndex = selectedSlot;
                k->onChange = [this, def, p, trackIndex, slotIndex] (double n)
                {
                    engine.getTrack (trackIndex).slots[(size_t) slotIndex].params[(size_t) p].store (mixer::fromNormalised (def, (float) n));
                };
                k->setValue (mixer::toNormalised (def, slot.params[(size_t) p].load()));
                k->setColour (Theme::accent);
                addAndMakeVisible (k);
            }
            paramIndex.push_back (p);
        }

        if (shownType == mixer::glitch)
        {
            glitchView = std::make_unique<GlitchGridView> (engine, track, selectedSlot);
            glitchView->onChanged = [this] { if (onChanged) onChanged(); };
            addAndMakeVisible (*glitchView);
        }
        if (shownType == mixer::gate)
        {
            gateView = std::make_unique<GateGridView> (engine, track, selectedSlot);
            gateView->onChanged = [this] { if (onChanged) onChanged(); };
            addAndMakeVisible (*gateView);
        }
        if (shownType == mixer::peq)
        {
            eqView = std::make_unique<EqCurveView> (engine, track, selectedSlot);
            addAndMakeVisible (*eqView);
        }

        for (auto* row : slotRows)
            row->refresh();
        layoutParams();
        repaint();
    }

    void layoutParams()
    {
        const int n = (int) paramIndex.size();
        if (n == 0 || paramsArea.isEmpty())
            return;
        if (glitchView != nullptr)
        {
            auto area = paramsArea;
            if (auto* k = paramKnobs[0])
                k->setBounds (area.removeFromLeft (juce::jmin (96, area.getWidth() / 5)).withSizeKeepingCentre (juce::jmin (96, area.getWidth() / 5), juce::jmin (120, area.getHeight())));
            glitchView->setBounds (area.reduced (4, 0));
            return;
        }
        auto area = paramsArea;
        if (gateView != nullptr)
        {
            gateView->setBounds (area.removeFromTop (juce::jlimit (40, 76, area.getHeight() * 2 / 5)));
            area.removeFromTop (6);
        }
        if (eqView != nullptr)
        {
            const bool room = area.getHeight() >= 300;
            eqView->setVisible (room);
            if (room)
            {
                eqView->setBounds (area.removeFromTop (juce::jlimit (60, 110, area.getHeight() / 3)).reduced (3, 0));
                area.removeFromTop (6);
            }
        }
        int cols = area.getWidth() >= area.getHeight() * 2 ? n : juce::jmin (n, 3);
        if (n > 6) cols = juce::jmin (n, juce::jmax (4, area.getWidth() / 80), 6);
        const int rows = (n + cols - 1) / cols;
        const int w = area.getWidth() / cols, h = juce::jmin (120, area.getHeight() / rows);
        int button = 0;
        for (int i = 0; i < n; ++i)
        {
            const juce::Rectangle<int> cell (area.getX() + (i % cols) * w, area.getY() + (i / cols) * h, w, h);
            if (auto* k = paramKnobs[i])
                k->setBounds (cell.reduced (3, 0));
            else if (auto* b = paramButtons[button++])
                b->setBounds (cell.reduced (3, 0));
        }
    }

public:
    std::function<void()> onChanged;

private:
    SequencerEngine& engine;
    int track = 1, selectedSlot = 0, shownType = -1;
    juce::OwnedArray<LabeledKnob> eqKnobs, paramKnobs;
    juce::OwnedArray<ChoiceParam> paramButtons;
    std::vector<int> paramIndex;
    juce::OwnedArray<SlotRow> slotRows;
    std::unique_ptr<GlitchGridView> glitchView;
    std::unique_ptr<GateGridView> gateView;
    std::unique_ptr<EqCurveView> eqView;
    juce::Rectangle<float> eqCaption, slotsCaption, paramsCaption, detailArea;
    juce::Rectangle<int> paramsArea;
};

//==============================================================================
/**
    Il mixer: le strisce delle tracce (scorrono col dito), e la pagina degli effetti di una traccia.
    Si apre sopra il channel rack, come la playlist.
*/
class MixerPanel : public juce::Component,
                   private juce::Timer
{
public:
    explicit MixerPanel (SequencerEngine& e) : engine (e), effects (e)
    {
        for (auto* b : { &closeButton, &allButton, &backButton })
            addAndMakeVisible (b);
        closeButton.onClick = [this] { if (onClose) onClose(); };
        allButton.setClickingTogglesState (true);
        allButton.onClick = [this] { rebuildStrips(); };
        backButton.onClick = [this] { showEffects (-1); };

        viewport.setViewedComponent (&stripHolder, false);
        viewport.setScrollOnDragMode (juce::Viewport::ScrollOnDragMode::nonHover);
        viewport.setScrollBarsShown (false, true);
        viewport.setScrollBarThickness (6);
        addAndMakeVisible (viewport);
        addChildComponent (effects);
        effects.onChanged = [this] { for (auto* s : strips) s->refresh(); };
    }

    std::function<void()> onClose;

    void open (int trackToShow = -1)
    {
        rebuildStrips();
        showEffects (-1);
        if (trackToShow >= 0)
            showEffects (trackToShow);
        startTimerHz (30);
    }

    void visibilityChanged() override
    {
        if (! isVisible())
            stopTimer();
    }

    /** Apre gli effetti di una traccia (-1 = torna alle strisce). */
    void showEffects (int track)
    {
        effectsTrack = track;
        effects.setVisible (track >= 0);
        viewport.setVisible (track < 0);
        backButton.setVisible (track >= 0);
        allButton.setVisible (track < 0);
        if (track >= 0)
        {
            effects.setTrack (track);
            selectStrip (track);
        }
        resized();
        repaint();
    }

    int getEffectsTrack() const     { return effectsTrack; }
    TrackEffectsView& getEffectsView()   { return effects; }

    void paint (juce::Graphics& g) override
    {
        auto r = getLocalBounds().toFloat();
        Theme::drawGlassPanel (g, r);

        auto title = titleArea;
        g.setColour (effectsTrack >= 0 ? engine.getTrackColour (effectsTrack) : Theme::accent);
        g.fillRect (title.removeFromLeft (4.0f).reduced (0.0f, 6.0f));
        title.removeFromLeft (8.0f);
        g.setColour (Theme::textBright);
        g.setFont (Theme::font (16.0f, true));
        const auto text = effectsTrack < 0 ? juce::String ("Mixer")
                        : effectsTrack == 0 ? juce::String ("Master: effetti")
                                            : "Traccia " + juce::String (effectsTrack) + ": " + engine.getTrackName (effectsTrack);
        g.drawFittedText (text, title.toNearestInt(), juce::Justification::centredLeft, 1);
    }

    /** PC, banco di lavoro: il mixer sta fisso sotto il channel rack (niente "Chiudi"). */
    void setEmbedded (bool e)        { embedded = e; resized(); }
    bool embedded = false;

    void resized() override
    {
        auto r = getLocalBounds().reduced (8, 6);
        auto bar = r.removeFromTop (32);
        const bool narrow = getWidth() < 520;
        closeButton.setVisible (! embedded);
        if (! embedded)
        {
            closeButton.setBounds (bar.removeFromRight (narrow ? 64 : 76).reduced (0, 2));
            bar.removeFromRight (6);
        }
        if (effectsTrack >= 0)
            backButton.setBounds (bar.removeFromRight (narrow ? 70 : 90).reduced (0, 2));
        else
            allButton.setBounds (bar.removeFromRight (narrow ? 70 : 110).reduced (0, 2));
        titleArea = bar.toFloat();
        r.removeFromTop (6);

        viewport.setBounds (r);
        effects.setBounds (r);
        layoutStrips();
    }

private:
    void rebuildStrips()
    {
        strips.clear();
        for (int t = 0; t < mixer::numTracks; ++t)
            if (allButton.getToggleState() || engine.isTrackUsed (t))
            {
                auto* s = strips.add (new MixerStrip (engine, t));
                s->onOpenEffects = [this] (int track) { showEffects (track); };
                s->onNameTap = [this] (int track) { showTrackMenu (track); };
                stripHolder.addAndMakeVisible (s);
            }
        allButton.setButtonText (allButton.getToggleState() ? "Solo usate" : "Tutte (" + juce::String (mixer::numTracks) + ")");
        shownSignature = signature();
        layoutStrips();
    }

    void layoutStrips()
    {
        const int h = juce::jmax (200, viewport.getHeight() - (viewport.isHorizontalScrollBarShown() ? 8 : 0));
        const int w = getWidth() < 520 ? 70 : 82;
        stripHolder.setSize (juce::jmax (viewport.getWidth(), w * strips.size()), h);
        for (int i = 0; i < strips.size(); ++i)
            strips[i]->setBounds (i * w, 0, w, h);
    }

    void selectStrip (int track)
    {
        for (auto* s : strips)
            s->setSelected (s->getTrack() == track);
    }

    /** Quali tracce sono in uso (cambia aggiungendo canali o caricando un progetto). */
    juce::uint64 signature() const
    {
        juce::uint64 h = allButton.getToggleState() ? 7 : 3;
        for (int t = 0; t < mixer::numTracks; ++t)
            h = h * 3 + (engine.isTrackUsed (t) ? 1u : 0u);
        return h;
    }

    void timerCallback() override
    {
        if (effectsTrack < 0 && signature() != shownSignature)
            rebuildStrips();

        for (auto* s : strips)
        {
            s->setLevel (engine.takeTrackPeak (s->getTrack()));
            if (++refreshCounter % 6 == 0)
                s->refresh();
        }
        if (effectsTrack >= 0 && refreshCounter % 6 == 0 && ! isMouseButtonDownAnywhere())
            effects.refresh();
    }

    void showTrackMenu (int track)
    {
        juce::PopupMenu colours;
        static const juce::uint32 palette[] = { 0xffe8833a, 0xffd2555a, 0xffd8b14a, 0xff4fb3a9, 0xff5b8fd6, 0xff9a78d0,
                                                0xff6fbf5a, 0xffe36fae, 0xff8aa0b8, 0xffc49360 };
        colours.addItem (100, "Automatico", true, engine.getTrack (track).colour == 0);
        for (int i = 0; i < (int) std::size (palette); ++i)
            colours.addColouredItem (101 + i, "Colore " + juce::String (i + 1), juce::Colour (palette[i]), true, engine.getTrack (track).colour == palette[i]);

        // Canali che suonano in questa traccia
        juce::String routed;
        for (int c = 0; c < engine.getNumChannels(); ++c)
            if (engine.getChannelTrack (c) == track)
                routed << (routed.isEmpty() ? "" : ", ") << engine.getChannel (c).name;

        juce::PopupMenu menu;
        menu.addSectionHeader (track == 0 ? juce::String ("Master") : "Traccia " + juce::String (track) + ": " + engine.getTrackName (track));
        if (routed.isNotEmpty())
            menu.addItem (-1, "Canali: " + routed, false);
        menu.addItem (1, "Effetti ed equalizzatore...");
        if (track > 0)
        {
            menu.addItem (2, "Rinomina...");
            menu.addSubMenu ("Colore", colours);
            menu.addSeparator();
            menu.addItem (3, "Azzera la traccia (volume, EQ, effetti)");
        }

        juce::Component::SafePointer<MixerPanel> safe (this);
        menu.showMenuAsync (juce::PopupMenu::Options(), [safe, track] (int result)
        {
            if (safe == nullptr || result == 0)
                return;
            auto& eng = safe->engine;
            if (result == 1)       safe->showEffects (track);
            else if (result == 2)
            {
                auto* w = new juce::AlertWindow ("Nome della traccia", {}, juce::MessageBoxIconType::NoIcon, safe.getComponent());
                w->addTextEditor ("text", eng.getTrack (track).name.isNotEmpty() ? eng.getTrack (track).name : eng.getTrackName (track));
                w->addButton ("OK", 1, juce::KeyPress (juce::KeyPress::returnKey));
                w->addButton ("Automatico", 2);
                w->addButton ("Annulla", 0, juce::KeyPress (juce::KeyPress::escapeKey));
                w->enterModalState (true, juce::ModalCallbackFunction::create ([safe, w, track] (int r)
                {
                    if (safe != nullptr && r > 0)
                    {
                        safe->engine.getTrack (track).name = r == 1 ? w->getTextEditorContents ("text").trim() : juce::String();
                        for (auto* s : safe->strips) s->refresh();
                    }
                }), true);
            }
            else if (result == 3)  { eng.resetTrack (track); safe->rebuildStrips(); }
            else if (result == 100) eng.getTrack (track).colour = 0;
            else if (result > 100)  eng.getTrack (track).colour = palette[result - 101];
            for (auto* s : safe->strips) s->refresh();
        });
    }

    SequencerEngine& engine;
    juce::Viewport viewport;
    juce::Component stripHolder;
    juce::OwnedArray<MixerStrip> strips;
    TrackEffectsView effects;
    juce::TextButton closeButton { "Chiudi" }, allButton, backButton { juce::String (juce::CharPointer_UTF8 ("\xe2\x97\x80 Mixer")) };
    juce::Rectangle<float> titleArea;
    int effectsTrack = -1, refreshCounter = 0;
    juce::uint64 shownSignature = 0;
};
