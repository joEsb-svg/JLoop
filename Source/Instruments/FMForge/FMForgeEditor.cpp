#include "FMForgeEditor.h"

static const juce::Colour fmAccent { 0xff5fffc0 };
static const juce::Colour fmOrange { 0xffff9a3c };

//==============================================================================
/** Disegno dell'algoritmo: gli operatori come blocchi, chi modula chi, le uscite in basso. */
class FMForgeEditor::AlgorithmView : public juce::Component
{
public:
    explicit AlgorithmView (juce::AudioProcessorValueTreeState& s) : apvts (s) {}

    juce::uint64 signature() const
    {
        juce::uint64 h = (juce::uint64) value ("algo") + 1;
        h = h * 131 + (juce::uint64) (value ("feedback") * 100);
        for (int o = 0; o < fm::numOps; ++o)
            h = h * 1009 + (juce::uint64) value (fm::opId (o, "Ratio")) * 211 + (juce::uint64) (value (fm::opId (o, "Level")) * 100);
        return h;
    }

    juce::uint64 shown = 0;

    void paint (juce::Graphics& g) override
    {
        auto r = getLocalBounds().toFloat().reduced (2.0f);
        g.setColour (juce::Colour (0xff06090b));
        g.fillRoundedRectangle (r, 6.0f);
        g.setColour (juce::Colours::black);
        g.drawRoundedRectangle (r, 6.0f, 1.0f);

        const auto& alg = fm::algorithm ((int) value ("algo"));

        // Profondita': 0 = portante (esce), 1 = modula una portante, ...
        int depth[fm::numOps] {};
        for (int pass = 0; pass < 4; ++pass)
            for (int o = 0; o < fm::numOps; ++o)
                for (int t = 0; t < fm::numOps; ++t)
                    if ((alg.modulators[(size_t) t] >> o) & 1)
                        depth[o] = juce::jmax (depth[o], depth[t] + 1);
        int maxDepth = 0;
        for (int d : depth) maxDepth = juce::jmax (maxDepth, d);

        auto area = r.reduced (12.0f, 14.0f).withTrimmedBottom (14.0f).withTrimmedTop (10.0f);
        const float boxW = juce::jmin (56.0f, area.getWidth() / 5.0f), boxH = juce::jmin (38.0f, area.getHeight() / (float) (maxDepth + 2));
        const float rowStep = maxDepth > 0 ? (area.getHeight() - boxH) / (float) maxDepth : 0.0f;

        // Posizioni: portanti distribuite in basso, modulatori sopra i loro bersagli
        float x[fm::numOps] {};
        int numCarriers = 0;
        for (int o = 0; o < fm::numOps; ++o) if ((alg.carriers >> o) & 1) ++numCarriers;
        int k = 0;
        for (int o = 0; o < fm::numOps; ++o)
            if ((alg.carriers >> o) & 1)
                x[o] = area.getX() + area.getWidth() * ((float) k++ + 0.5f) / (float) numCarriers;
        for (int d = 1; d <= maxDepth; ++d)
        {
            std::vector<int> atDepth;
            for (int o = 0; o < fm::numOps; ++o)
                if (depth[o] == d && ! ((alg.carriers >> o) & 1))
                {
                    float sum = 0.0f; int n = 0;
                    for (int t = 0; t < fm::numOps; ++t)
                        if ((alg.modulators[(size_t) t] >> o) & 1) { sum += x[t]; ++n; }
                    x[o] = n > 0 ? sum / (float) n : area.getCentreX();
                    atDepth.push_back (o);
                }
            // Due operatori sullo stesso punto: si allargano
            for (size_t i = 0; i < atDepth.size(); ++i)
                for (size_t j = i + 1; j < atDepth.size(); ++j)
                    if (std::abs (x[atDepth[i]] - x[atDepth[j]]) < boxW * 1.2f)
                    {
                        x[atDepth[i]] -= boxW * 0.65f;
                        x[atDepth[j]] += boxW * 0.65f;
                    }
        }

        auto box = [&] (int o)
        {
            const float yTop = area.getBottom() - boxH - (float) depth[o] * rowStep;
            return juce::Rectangle<float> (x[o] - boxW * 0.5f, yTop, boxW, boxH);
        };

        // Collegamenti
        g.setColour (fmAccent.withAlpha (0.5f));
        for (int t = 0; t < fm::numOps; ++t)
            for (int m = 0; m < fm::numOps; ++m)
                if ((alg.modulators[(size_t) t] >> m) & 1)
                {
                    const auto from = box (m), to = box (t);
                    g.drawArrow ({ from.getCentreX(), from.getBottom(), to.getCentreX(), to.getY() }, 1.6f, 7.0f, 7.0f);
                }
        // Uscite
        const float outY = area.getBottom() + 12.0f;
        for (int o = 0; o < fm::numOps; ++o)
            if ((alg.carriers >> o) & 1)
            {
                g.setColour (fmAccent.withAlpha (0.6f));
                g.drawLine (x[o], box (o).getBottom(), x[o], outY - 2.0f, 1.6f);
            }
        g.setColour (fmAccent.withAlpha (0.6f));
        g.drawLine (area.getX(), outY, area.getRight(), outY, 1.6f);

        // Feedback dell'operatore 4
        if (value ("feedback") > 0.01f)
        {
            const auto b = box (3);
            juce::Path loop;
            loop.startNewSubPath (b.getRight(), b.getCentreY());
            loop.cubicTo (b.getRight() + 16.0f, b.getCentreY(), b.getRight() + 16.0f, b.getY() - 12.0f, b.getCentreX(), b.getY() - 10.0f);
            loop.lineTo (b.getCentreX(), b.getY());
            g.setColour (fmOrange);
            g.strokePath (loop, juce::PathStrokeType (1.5f));
        }

        // Blocchi
        for (int o = 0; o < fm::numOps; ++o)
        {
            const auto b = box (o);
            const bool carrier = (alg.carriers >> o) & 1;
            const auto c = carrier ? fmAccent : fmOrange;
            const float level = value (fm::opId (o, "Level"));
            g.setColour (juce::Colour (0xff081210));
            g.fillRoundedRectangle (b, 5.0f);
            g.setColour (c.withAlpha (0.12f + 0.25f * level));
            g.drawRoundedRectangle (b.expanded (1.5f), 6.0f, 4.0f);   // alone
            g.setColour (c.withAlpha (0.35f + 0.65f * level));
            g.drawRoundedRectangle (b, 5.0f, 1.8f);
            g.fillRect (b.withTrimmedTop (b.getHeight() - 3.0f).withWidth (b.getWidth() * level).reduced (3.0f, 0.0f));

            g.setColour (c);
            g.setFont (frui::sans (juce::jmin (15.0f, boxH * 0.42f)));
            g.drawText (juce::String (o + 1), b.withTrimmedBottom (b.getHeight() * 0.42f), juce::Justification::centred);
            g.setColour (frui::colours::textDim);
            g.setFont (frui::mono (juce::jmin (11.0f, boxH * 0.3f), false));
            g.drawText (juce::String (juce::CharPointer_UTF8 ("\xc3\x97")) + fm::ratioNames()[(int) value (fm::opId (o, "Ratio"))],
                        b.withTrimmedTop (b.getHeight() * 0.5f), juce::Justification::centred);
        }

        g.setColour (frui::colours::textDim);
        g.setFont (frui::sans (10.0f, false));
        g.drawText ("verde = suona   arancio = modula", r.reduced (8.0f, 3.0f), juce::Justification::topLeft);
    }

private:
    float value (const juce::String& id) const
    {
        if (auto* v = apvts.getRawParameterValue (id))
            return v->load();
        return 0.0f;
    }

    juce::AudioProcessorValueTreeState& apvts;
};

//==============================================================================
FMForgeEditor::FMForgeEditor (FMForgeAudioProcessor& p)
    : NeonEditor (p, p.apvts, "FMForge", fmAccent, { "ALGO", "OP 1", "OP 2", "OP 3", "OP 4", "FX" },
                  { "bright", "TIMBRO", "QUANTO FM", {} },
                  { "tone", "TONO", "BRILLANTEZZA", {} })
{
    // --- Algoritmo e voce
    auto& a = section (0, "ALGORITMO", 1.6f, 1);
    auto v0 = std::make_unique<AlgorithmView> (p.apvts);
    algoView = v0.get();
    view (a, std::move (v0), 0.58f);
    a.viewLeft = true;
    choice (a, "algo", "ALG ");
    knob (a, "feedback", "FEEDBACK");

    auto& v = section (0, "VOCE", 0.7f, 1);
    choice (v, "mode");
    knob (v, "glide", "GLIDE");

    // --- Operatori
    for (int o = 0; o < fm::numOps; ++o)
    {
        auto& op = section (1 + o, "OPERATORE " + juce::String (o + 1), 0.9f, 3);
        choice (op, fm::opId (o, "Ratio"), "RAPPORTO x");
        knob (op, fm::opId (o, "Fine"), "FINE");
        knob (op, fm::opId (o, "Level"), "LIVELLO");
        knob (op, fm::opId (o, "Vel"), "VELOCITY");

        auto& e = section (1 + o, "INVILUPPO OPERATORE " + juce::String (o + 1), 1.4f, 4);
        adsr (e, fm::opId (o, "A"), fm::opId (o, "D"), fm::opId (o, "S"), fm::opId (o, "R"));
        knob (e, fm::opId (o, "A"), "ATTACCO");
        knob (e, fm::opId (o, "D"), "DECAY");
        knob (e, fm::opId (o, "S"), "SUSTAIN");
        knob (e, fm::opId (o, "R"), "RILASCIO");
    }

    // --- LFO ed effetti
    auto& l = section (5, "LFO", 1.0f, 3);
    choice (l, "lfoWave", "FORMA: ");
    choice (l, "lfoSync", "A TEMPO");
    choice (l, "lfoDiv", "DIVISIONE: ");
    knob (l, "lfoRate", "VELOCITA'");
    knob (l, "lfoVib", "VIBRATO");
    knob (l, "lfoTrem", "TREMOLO");
    knob (l, "lfoBright", "TIMBRO");

    auto& fx = section (5, "EFFETTI", 1.2f, 4);
    choice (fx, "dlyTime", "DELAY ");
    knob (fx, "chorus", "CHORUS");
    knob (fx, "dlyMix", "DELAY");
    knob (fx, "dlyFb", "FEEDBACK");
    knob (fx, "revMix", "RIVERBERO");
    knob (fx, "revSize", "STANZA");
    knob (fx, "master", "VOLUME");

    showTab (0);
}

void FMForgeEditor::refreshViews()
{
    if (algoView != nullptr && algoView->isShowing())
        if (const auto sig = algoView->signature(); sig != algoView->shown)
        {
            algoView->shown = sig;
            algoView->repaint();
        }
}
