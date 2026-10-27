#include "ZapFMEditor.h"

static const juce::Colour zapAccent { 0xffa47cff };
static const juce::Colour glitchPink { 0xffff5ad0 };

//==============================================================================
class ZapFMEditor::HitsView : public juce::Component
{
public:
    explicit HitsView (ZapFMAudioProcessor& p) : proc (p) {}

    void resized() override
    {
        auto r = getLocalBounds();
        header = r.removeFromTop (24);
        randomArea = header.removeFromRight (juce::jmin (120, header.getWidth() / 3)).reduced (0, 2);
        r.removeFromTop (4);
        glitchRow = r.removeFromBottom (18);
        barArea = r.reduced (0, 4);
    }

    void paint (juce::Graphics& g) override
    {
        auto ra = randomArea.toFloat();
        g.setColour (frui::lcdBg());
        g.fillRoundedRectangle (ra, 4.0f);
        g.setColour (juce::Colours::black);
        g.drawRoundedRectangle (ra, 4.0f, 1.0f);
        g.setFont (frui::mono (12.0f));
        frui::drawNeon (g, juce::String (juce::CharPointer_UTF8 ("\xe2\x9a\x84 CASUALE")), ra, 0.5f, juce::Justification::centred, frui::lcdText());
        g.setFont (frui::sans (9.5f, false));
        g.setColour (glitchPink);
        g.drawText (juce::String (juce::CharPointer_UTF8 ("\xe2\x9c\xa6 GLITCH SU QUEL COLPO")), header.withWidth (180), juce::Justification::centredLeft, false);

        auto area = getLocalBounds().toFloat().withTop ((float) barArea.getY() - 4.0f);
        g.setColour (juce::Colour (0xff06090b));
        g.fillRoundedRectangle (area, 6.0f);
        g.setColour (juce::Colours::black);
        g.drawRoundedRectangle (area, 6.0f, 1.0f);

        const int playing = proc.playStep.load();
        const float zapTime = proc.apvts.getRawParameterValue ("zapTime")->load();
        const float k = 3.0f + 14.0f * (1.0f - zapTime);   // piu' veloce = curva piu' stretta
        for (int i = 0; i < zap::numSteps; ++i)
        {
            const auto st = proc.getStep (i);
            auto col = column (i);
            auto bar = col.withTop ((float) barArea.getY()).withBottom ((float) barArea.getBottom());
            g.setColour (i == playing ? zapAccent.withAlpha (0.12f) : juce::Colour (0xff0d1116));
            g.fillRect (bar);
            if (st.gate)
            {
                const float h = juce::jmax (0.06f, (float) st.height / (float) zap::maxHeight) * bar.getHeight();
                juce::Path curve;
                const float top = bar.getBottom() - h;
                curve.startNewSubPath (bar.getX(), top);
                for (int s = 1; s <= 24; ++s)
                {
                    const float x = (float) s / 24.0f;
                    curve.lineTo (bar.getX() + x * bar.getWidth(), bar.getBottom() - 2.0f - (h - 2.0f) * std::exp (-x * k * 0.5f));
                }
                juce::Path fill (curve);
                fill.lineTo (bar.getRight(), bar.getBottom());
                fill.lineTo (bar.getX(), bar.getBottom());
                fill.closeSubPath();
                const auto c = i == playing ? zapAccent.brighter (0.5f) : zapAccent;
                g.setColour (c.withAlpha (0.2f));
                g.fillPath (fill);
                g.setColour (c.withAlpha (0.3f));
                g.strokePath (curve, juce::PathStrokeType (4.0f));
                g.setColour (c);
                g.strokePath (curve, juce::PathStrokeType (1.6f));
            }
            else
            {
                g.setColour (juce::Colour (0xff1a1f24));
                g.fillRoundedRectangle (bar.withTop (bar.getBottom() - 4.0f), 2.0f);
            }
            g.setFont (frui::sans (13.0f));
            g.setColour (st.glitch ? glitchPink : juce::Colour (0xff2a3038));
            g.drawText (juce::String (juce::CharPointer_UTF8 ("\xe2\x9c\xa6")), col.withY ((float) glitchRow.getY()).withHeight ((float) glitchRow.getHeight()),
                        juce::Justification::centred, false);
        }
    }

    void mouseDown (const juce::MouseEvent& e) override
    {
        moved = false;
        downStep = stepAt (e.position.x);
        const auto pos = e.getPosition();
        if (randomArea.contains (pos))
        {
            proc.setLine (zap::randomLine (juce::Random::getSystemRandom().nextInt ({ 1, 99999 })));
            proc.lineToState();
            repaint();
            downStep = -1;
            return;
        }
        if (downStep < 0) return;
        if (glitchRow.expanded (0, 4).contains (pos))
        {
            auto st = proc.getStep (downStep);
            st.glitch = ! st.glitch;
            st.gate = st.gate || st.glitch;
            proc.setStep (downStep, st);
            proc.lineToState();
            repaint();
            downStep = -1;
            return;
        }
        startY = e.position.y;
    }

    void mouseDrag (const juce::MouseEvent& e) override
    {
        if (downStep < 0) return;
        if (! moved && std::abs (e.position.y - startY) < 5.0f) return;
        moved = true;
        const int s = stepAt (e.position.x);
        if (s < 0) return;
        const float v = juce::jlimit (0.0f, 1.0f, ((float) barArea.getBottom() - e.position.y) / (float) barArea.getHeight());
        auto st = proc.getStep (s);
        st.height = juce::jlimit (0, zap::maxHeight, juce::roundToInt (v * (float) zap::maxHeight));
        st.gate = true;
        proc.setStep (s, st);
        repaint();
    }

    void mouseUp (const juce::MouseEvent&) override
    {
        if (downStep >= 0 && ! moved)
        {
            auto st = proc.getStep (downStep);
            st.gate = ! st.gate;
            proc.setStep (downStep, st);
            repaint();
        }
        if (downStep >= 0) proc.lineToState();
        downStep = -1;
    }

    void refresh()
    {
        const juce::int64 sig = (juce::int64) zap::encode (proc.getLine()).hashCode64() * 31 + proc.playStep.load() * 7
                              + (juce::int64) (proc.apvts.getRawParameterValue ("zapTime")->load() * 100.0f);
        if (sig != last) { last = sig; repaint(); }
    }

private:
    juce::Rectangle<float> column (int i) const
    {
        const float w = (float) barArea.getWidth() / (float) zap::numSteps;
        return juce::Rectangle<float> ((float) barArea.getX() + (float) i * w + 1.5f, (float) barArea.getY(), w - 3.0f, (float) (glitchRow.getBottom() - barArea.getY()));
    }

    int stepAt (float x) const
    {
        if (barArea.getWidth() <= 0) return -1;
        const int s = (int) ((x - (float) barArea.getX()) / ((float) barArea.getWidth() / (float) zap::numSteps));
        return s >= 0 && s < zap::numSteps ? s : -1;
    }

    ZapFMAudioProcessor& proc;
    juce::Rectangle<int> header, randomArea, glitchRow, barArea;
    int downStep = -1;
    bool moved = false;
    float startY = 0.0f;
    juce::int64 last = 0;
};

//==============================================================================
ZapFMEditor::ZapFMEditor (ZapFMAudioProcessor& p)
    : NeonEditor (p, p.apvts, "ZapFM", zapAccent, { "COLPI", "SUONO FM", "FX" },
                  { "zap", "ZAP", "CADUTA LASER", {} },
                  { "glitch", "GLITCH", "CAOS CASUALE", {} })
{
    auto& c = section (0, "COLPI (ALTEZZA = DA DOVE CADE IL LASER, TOCCA = PAUSA)", 1.0f);
    hits = static_cast<HitsView*> (&view (c, std::make_unique<HitsView> (p)));

    auto& f = section (1, "FM", 1.0f, 4);
    choice (f, "ratio", "RAPPORTO ");
    choice (f, "octave");
    knob (f, "index", "INDICE");
    knob (f, "metal", "METALLO");
    knob (f, "feedback", "FEEDBACK");
    knob (f, "tune", "TUNE");
    auto& h = section (1, "COLPO", 1.0f, 4);
    choice (h, "rate");
    choice (h, "mode");
    knob (h, "zapTime", "CADUTA");
    knob (h, "decay", "DURATA");
    knob (h, "crush", "CRUSH");
    knob (h, "spread", "STEREO");

    auto& d = section (2, "DELAY", 1.0f, 2);
    choice (d, "dlyTime", "DELAY ");
    knob (d, "dlyMix", "DELAY");
    knob (d, "dlyFb", "FEEDBACK");
    auto& r = section (2, "RIVERBERO E USCITA", 1.3f, 4);
    knob (r, "revMix", "RIVERBERO");
    knob (r, "revSize", "STANZA");
    knob (r, "level", "LIVELLO");
    knob (r, "master", "VOLUME");

    showTab (0);
}

void ZapFMEditor::refreshViews()
{
    if (hits != nullptr) hits->refresh();
}
