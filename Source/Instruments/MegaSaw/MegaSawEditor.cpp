#include "MegaSawEditor.h"

/** I dente di sega dell'unison disegnati uno sopra l'altro, ognuno con la sua stonatura. */
class MegaSawEditor::FanView : public juce::Component
{
public:
    explicit FanView (juce::AudioProcessorValueTreeState& s) : apvts (s) { setInterceptsMouseClicks (false, false); }

    void refresh()
    {
        const juce::String sig = juce::String (get ("mode")) + "|" + juce::String (get ("unison")) + "|" + juce::String (get ("detune"))
                               + "|" + juce::String (get ("thick")) + "|" + juce::String (get ("hoover"));
        if (sig != lastSig) { lastSig = sig; repaint(); }
    }

    void paint (juce::Graphics& g) override
    {
        auto r = getLocalBounds().toFloat().reduced (0.5f);
        g.setColour (juce::Colour (0xff0a0806));
        g.fillRoundedRectangle (r, 5.0f);
        g.setColour (juce::Colours::black);
        g.drawRoundedRectangle (r, 5.0f, 1.0f);
        auto area = r.reduced (6.0f, 6.0f);

        const int mode = (int) get ("mode");
        const int uni = juce::jlimit (1, 9, juce::roundToInt (get ("unison")));
        const float detune = get ("detune") * (0.35f + 0.65f * get ("thick")) * (mode == 2 ? 0.45f : 1.0f);
        const float hoover = mode == 1 ? juce::jmax (0.35f, get ("hoover")) : get ("hoover");
        const float cycles = juce::jlimit (3.0f, 8.0f, area.getWidth() / 70.0f);
        const auto accent = frui::accent();

        for (int u = 0; u < uni; ++u)
        {
            const float pos = uni == 1 ? 0.0f : (float) u / (float) (uni - 1) * 2.0f - 1.0f;
            const float ratio = 1.0f + pos * std::abs (pos) * 0.35f * detune;   // stonatura esagerata per vederla
            const float phase = 0.5f + 0.08f * pos;
            juce::Path p;
            const int steps = juce::jmax (40, (int) area.getWidth());
            for (int i = 0; i <= steps; ++i)
            {
                const float x = (float) i / (float) steps;
                float t = (x - 0.5f) * cycles * ratio + phase;   // si apre a ventaglio dal centro
                t -= std::floor (t);
                float s = 2.0f * t - 1.0f;
                if (hoover > 0.0f) s = s * (1.0f - 0.6f * hoover) + (t < 0.5f ? 0.7f : -0.7f) * hoover;
                const float y = area.getCentreY() - s * area.getHeight() * 0.44f;
                if (i == 0) p.startNewSubPath (area.getX() + x * area.getWidth(), y);
                else        p.lineTo (area.getX() + x * area.getWidth(), y);
            }
            const float a = uni == 1 ? 1.0f : 0.35f + 0.55f * (1.0f - std::abs (pos));
            g.setColour (accent.withAlpha (a * 0.25f));
            g.strokePath (p, juce::PathStrokeType (3.0f));
            g.setColour (accent.withAlpha (a));
            g.strokePath (p, juce::PathStrokeType (1.1f));
        }
    }

private:
    float get (const char* id) const   { return apvts.getRawParameterValue (id)->load(); }
    juce::AudioProcessorValueTreeState& apvts;
    juce::String lastSig;
};

MegaSawEditor::MegaSawEditor (MegaSawAudioProcessor& p)
    : NeonEditor (p, p.apvts, "MegaSaw", juce::Colour (0xffffb020), { "OSC", "FILTRO", "FX" },
                  { "thick", "SPESSORE", juce::String::fromUTF8 ("VOCI \xc3\x97 DETUNE"), {} },
                  { "drive", "DRIVE", {}, {} })
{
    auto& osc = section (0, "MODO E UNISON", 1.0f, 4);
    fan = static_cast<FanView*> (&view (osc, std::make_unique<FanView> (apvts), 0.3f));
    segment (osc, "mode", { "SUPERSAW", "HOOVER", "REESE" });
    knob (osc, "unison", "VOCI");
    knob (osc, "detune", "DETUNE");
    knob (osc, "spread", "APERTURA");
    knob (osc, "hoover", "HOOVER");
    knob (osc, "scoop", "PITCH");
    knob (osc, "sub", "SUB");
    knob (osc, "octave", "OTTAVA");
    knob (osc, "glide", "GLIDE");

    auto& flt = section (1, "FILTRO", 1.0f, 2);
    knob (flt, "cutoff", "CUTOFF");
    knob (flt, "reso", "RESO");
    knob (flt, "fEnv", "INVILUPPO");
    knob (flt, "fDecay", "DECAY");
    auto& amp = section (1, "AMPIEZZA E VOCE", 1.0f, 2);
    segment (amp, "playMode", { "POLY", "MONO", "LEGATO" });
    knob (amp, "aA", "ATTACCO");
    knob (amp, "aD", "DECAY");
    knob (amp, "aS", "SUSTAIN");
    knob (amp, "aR", "RILASCIO");

    auto& cd = section (2, "CHORUS E DELAY", 1.0f, 3);
    lcd (cd, "dlyTime", "DELAY ");
    knob (cd, "chorus", "CHORUS");
    knob (cd, "dlyMix", "DELAY");
    knob (cd, "dlyFb", "FEEDBACK");
    auto& rv = section (2, "RIVERBERO E USCITA", 1.0f, 3);
    knob (rv, "revMix", "RIVERBERO");
    knob (rv, "revSize", "STANZA");
    knob (rv, "master", "VOLUME");

    showTab (0);
}

void MegaSawEditor::refreshViews()
{
    if (fan != nullptr) fan->refresh();
}
