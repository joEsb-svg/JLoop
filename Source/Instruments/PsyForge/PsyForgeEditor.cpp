#include "PsyForgeEditor.h"

static const juce::Colour psyAccent { 0xfff59ac8 };
static const juce::Colour kickColour { 0xffffd27a };

//==============================================================================
/** La griglia della battuta: K (cassa) e B (basso). Tocca per cambiare, la casella che suona si illumina. */
class PsyForgeEditor::GridView : public juce::Component
{
public:
    explicit GridView (PsyForgeAudioProcessor& p) : proc (p) {}

    void paint (juce::Graphics& g) override
    {
        const int n = proc.numCells();
        auto r = getLocalBounds().toFloat();
        auto labels = r.removeFromBottom (14.0f);
        const float gap = n > 16 ? 2.0f : 3.0f;
        const float w = (r.getWidth() - gap * (float) (n - 1) - 6.0f) / (float) n;
        const int playing = proc.playCell.load();
        const int perBeat = n / 4;
        for (int i = 0; i < n; ++i)
        {
            const float x = r.getX() + (float) i * (w + gap) + (float) (i / perBeat) * 2.0f;
            auto c = juce::Rectangle<float> (x, r.getY(), w, r.getHeight());
            const int v = proc.getCell (i);
            const auto col = v == 1 ? kickColour : psyAccent;
            if (v != 0)
            {
                g.setColour (col.withAlpha (i == playing ? 0.55f : 0.22f));
                g.fillRoundedRectangle (c.expanded (2.0f), 5.0f);   // alone
                g.setColour (i == playing ? col.brighter (0.35f) : col);
                g.fillRoundedRectangle (c, 4.0f);
            }
            else
            {
                g.setColour (juce::Colour (0xff0c0f12));
                g.fillRoundedRectangle (c, 4.0f);
                g.setColour (i == playing ? psyAccent.withAlpha (0.6f) : juce::Colours::black);
                g.drawRoundedRectangle (c, 4.0f, 1.0f);
            }
            if (v != 0 && w > 10.0f)
            {
                g.setColour (juce::Colour (0xff1a1014));
                g.setFont (frui::sans (juce::jmin (13.0f, w * 0.6f)));
                g.drawText (v == 1 ? "K" : "B", c, juce::Justification::centred, false);
            }
        }
        g.setColour (frui::colours::textDim);
        g.setFont (frui::sans (9.0f, false));
        for (int b = 0; b < 4; ++b)
            g.drawText (juce::String (b + 1), juce::Rectangle<float> (r.getX() + (float) (b * perBeat) * (w + gap) + (float) b * 2.0f, labels.getY(), 20.0f, 14.0f),
                        juce::Justification::centredLeft, false);
    }

    void mouseDown (const juce::MouseEvent& e) override
    {
        const int n = proc.numCells();
        const int i = juce::jlimit (0, n - 1, (int) (e.position.x / (float) getWidth() * (float) n));
        proc.setCell (i, (proc.getCell (i) + 1) % 3);   // ciclo: pausa -> cassa -> basso -> pausa
        proc.gridToState();
        repaint();
    }

    void refresh()
    {
        juce::int64 sig = proc.playCell.load() + 7 + proc.numCells() * 100;
        for (int i = 0; i < psy::maxCells; ++i) sig = sig * 3 + proc.getCell (i);
        if (sig != last) { last = sig; repaint(); }
    }

private:
    PsyForgeAudioProcessor& proc;
    juce::int64 last = 0;
};

//==============================================================================
/** Disegno di una battuta: colpi di cassa (ambra) e di basso (rosa) con le loro code. */
class PsyForgeEditor::WaveView : public juce::Component
{
public:
    WaveView (PsyForgeAudioProcessor& p) : proc (p) { setInterceptsMouseClicks (false, false); }

    void paint (juce::Graphics& g) override
    {
        auto r = getLocalBounds().toFloat().reduced (0.5f);
        g.setColour (juce::Colour (0xff06090b));
        g.fillRoundedRectangle (r, 5.0f);
        g.setColour (juce::Colours::black);
        g.drawRoundedRectangle (r, 5.0f, 1.0f);
        auto a = r.reduced (6.0f, 6.0f);
        const int n = proc.numCells();
        const float coda = value ("coda"), kickDecay = value ("kickDecay"), morso = value ("morso");
        const bool kickOn = value ("kickOn") > 0.5f;
        const int steps = juce::jmax (100, (int) a.getWidth());
        juce::Path kick, bass;
        kick.startNewSubPath (a.getX(), a.getCentreY());
        bass.startNewSubPath (a.getX(), a.getCentreY());
        const float cellW = 1.0f / (float) n;
        for (int i = 0; i <= steps; ++i)
        {
            const float x = (float) i / (float) steps;
            // ultimo colpo prima di x
            int cell = juce::jmin (n - 1, (int) (x / cellW));
            int kc = -1, bc = -1;
            for (int c = cell; c >= 0 && (kc < 0 || bc < 0); --c)
            {
                if (proc.getCell (c) == 1 && kc < 0) kc = c;
                if (proc.getCell (c) == 2 && bc < 0) bc = c;
            }
            float ky = 0.0f, by = 0.0f;
            if (kc >= 0 && kickOn)
            {
                const float t = (x - (float) kc * cellW) * 2.0f;   // in "secondi" finti: una battuta ~ 2 s
                ky = std::sin (t * 110.0f * (1.0f + 3.0f * std::exp (-t * 40.0f))) * std::exp (-t / (0.05f + 0.5f * kickDecay));
            }
            if (bc >= 0 && (kc < 0 || bc > kc || ! kickOn))
            {
                const float t = (x - (float) bc * cellW) * 2.0f;
                by = std::sin (t * 190.0f) * std::exp (-t / (0.015f + 0.2f * coda)) * (0.5f + 0.5f * std::exp (-t * (10.0f + 30.0f * morso)));
            }
            const float px = a.getX() + x * a.getWidth();
            kick.lineTo (px, a.getCentreY() - ky * a.getHeight() * 0.45f);
            bass.lineTo (px, a.getCentreY() - by * a.getHeight() * 0.4f);
        }
        g.setColour (kickColour.withAlpha (0.85f));
        g.strokePath (kick, juce::PathStrokeType (1.3f));
        g.setColour (psyAccent.withAlpha (0.25f));
        g.strokePath (bass, juce::PathStrokeType (3.5f));
        g.setColour (psyAccent);
        g.strokePath (bass, juce::PathStrokeType (1.4f));
    }

    void refresh()
    {
        juce::int64 sig = proc.numCells();
        for (int i = 0; i < psy::maxCells; ++i) sig = sig * 3 + proc.getCell (i);
        for (auto* id : { "coda", "kickDecay", "morso", "kickOn" }) sig = sig * 131 + (juce::int64) (value (id) * 1000.0f);
        if (sig != last) { last = sig; repaint(); }
    }

private:
    float value (const char* id) const   { return proc.apvts.getRawParameterValue (id)->load(); }
    PsyForgeAudioProcessor& proc;
    juce::int64 last = 0;
};

//==============================================================================
PsyForgeEditor::PsyForgeEditor (PsyForgeAudioProcessor& p)
    : NeonEditor (p, p.apvts, "PsyForge", psyAccent, { "ROLLING", "CASSA", "BASSO", "FX" },
                  { "coda", "CODA", juce::String::fromUTF8 ("CORTO \xe2\x86\x94 LUNGO"), {} },
                  { "morso", "MORSO", "SCHIOCCO FILTRO", {} })
{
    auto& fig = section (0, "FIGURA (TOCCA UNA CASELLA: CASSA > BASSO > PAUSA)", 1.0f, 0, 0, 1.0f);
    segment (fig, "figure", { "FULL-ON", "NIGHT", "GOA", "HI-TECH" });
    grid = static_cast<GridView*> (&view (fig, std::make_unique<GridView> (p), 0.62f));

    auto& snd = section (0, "SUONO", 1.0f, 4, 1, 1.05f);
    wave = static_cast<WaveView*> (&view (snd, std::make_unique<WaveView> (p), 0.45f));
    snd.viewLeft = true;
    knob (snd, "transpose", "NOTA");
    knob (snd, "cutoff", "CUTOFF");
    knob (snd, "reso", "RESO");
    knob (snd, "drive", "DRIVE");

    auto& k = section (1, "CASSA (ACCORDATA UN'OTTAVA SOTTO IL BASSO)", 1.0f, 5);
    choice (k, "kickOn", "CASSA");
    knob (k, "kickClick", "CLICK");
    knob (k, "kickPunch", "PUNCH");
    knob (k, "kickDecay", "DURATA");
    knob (k, "kickTune", "ACCORDA");
    knob (k, "kickLevel", "VOLUME");

    auto& t = section (2, "TIMBRO", 1.0f, 3);
    choice (t, "bassWave");
    choice (t, "bassOct", "OTTAVA ");
    choice (t, "motion", "MOVIMENTO ");
    knob (t, "colore", "COLORE");
    knob (t, "sub", "SUB");
    knob (t, "zap", "ZAP");
    auto& b = section (2, "FILTRO E VOLUME", 1.0f, 3);
    knob (b, "cutoff", "CUTOFF");
    knob (b, "reso", "RESO");
    knob (b, "drive", "DRIVE");
    knob (b, "snap", "SCHIOCCO");
    knob (b, "transpose", "NOTA");
    knob (b, "bassLevel", "VOLUME");

    auto& d = section (3, "DELAY", 1.0f, 2);
    choice (d, "dlyTime", "DELAY ");
    knob (d, "dlyMix", "DELAY");
    knob (d, "dlyFb", "FEEDBACK");
    auto& r = section (3, "RIVERBERO E USCITA", 1.0f, 3);
    knob (r, "revMix", "RIVERBERO");
    knob (r, "revSize", "STANZA");
    knob (r, "master", "VOLUME");

    showTab (0);
}

void PsyForgeEditor::refreshViews()
{
    if (grid != nullptr) grid->refresh();
    if (wave != nullptr) wave->refresh();
}
