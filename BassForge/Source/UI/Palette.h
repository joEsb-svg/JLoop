#pragma once
#include <juce_gui_basics/juce_gui_basics.h>

namespace bf::ui::col
{
    // Stile "neon" (come i synth nuovi di JLoop): nero-grafite, bordi neri, azzurro luminoso (valore) / arancio (modulazione)
    inline const juce::Colour bg        { 0xff0d1014 };
    inline const juce::Colour panel     { 0xff171a1e };
    inline const juce::Colour panelHi   { 0xff1f2328 };
    inline const juce::Colour edge      { 0xff050607 };
    inline const juce::Colour track     { 0xff262b31 };
    inline const juce::Colour text      { 0xffd2d8de };
    inline const juce::Colour textDim   { 0xff7a848e };
    inline const juce::Colour cyan      { 0xff3ad8ff };
    inline const juce::Colour cyanDim   { 0xff145a6b };
    inline const juce::Colour orange    { 0xffff8a1f };
    inline const juce::Colour orangeDim { 0xff7a4310 };
    inline const juce::Colour danger    { 0xffff4d6a };
    inline const juce::Colour lcdBg     { 0xff07090b };
}

namespace bf::ui
{
    /** Testo a lettere distanziate (titoli, nomi delle manopole). */
    inline juce::Font spacedFont (float size, bool bold = true)
    {
        return juce::Font (juce::FontOptions (size, bold ? juce::Font::bold : juce::Font::plain).withKerningFactor (0.14f));
    }
    inline juce::Font monoFont (float size)
    {
        return juce::Font (juce::FontOptions (juce::Font::getDefaultMonospacedFontName(), size, juce::Font::bold));
    }
    /** Testo luminoso: alone sfocato sotto il testo. */
    inline void drawGlowText (juce::Graphics& g, const juce::String& t, juce::Rectangle<int> r, juce::Justification j, juce::Colour c)
    {
        g.setColour (c.withAlpha (0.18f));
        for (auto d : { juce::Point<int> (-1, 0), { 1, 0 }, { 0, -1 }, { 0, 1 }, { -2, 0 }, { 2, 0 } })
            g.drawText (t, r.translated (d.x, d.y), j, false);
        g.setColour (c);
        g.drawText (t, r, j, false);
    }
}
