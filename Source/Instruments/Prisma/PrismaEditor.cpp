#include "PrismaEditor.h"

static const juce::Colour prismaAccent { 0xff44e6ff };

//==============================================================================
class PrismaEditor::TableView : public juce::Component
{
public:
    TableView (PrismaAudioProcessor& p, int oscIndex) : proc (p), osc (oscIndex), prefix (oscIndex == 0 ? "a" : "b") {}

    void paint (juce::Graphics& g) override
    {
        auto r = getLocalBounds().toFloat().reduced (0.5f);
        g.setColour (juce::Colour (0xff05080b));
        g.fillRoundedRectangle (r, 6.0f);
        g.setColour (juce::Colours::black);
        g.drawRoundedRectangle (r, 6.0f, 1.0f);
        const bool on = param ("On") > 0.5f;
        const int table = (int) param ("Table");
        const int warp = (int) param ("Warp");
        const float warpAmt = param ("WarpAmt");
        const float pos = currentPos();

        auto area = r.reduced (10.0f, 8.0f);
        g.setFont (frui::mono (11.0f, false));
        g.setColour (frui::colours::textDim);
        g.drawText (prisma::tableNames()[table].toUpperCase() + "   POS " + juce::String (juce::roundToInt (pos * 100.0f)) + "%",
                    area.removeFromTop (14.0f), juce::Justification::centredLeft, false);

        // 3D: le forme una dietro l'altra (spostate in alto a destra), la piu' vicina in basso a sinistra
        const auto& bank = prisma::Bank::get();
        const float depthX = area.getWidth() * 0.22f, depthY = area.getHeight() * 0.42f;
        const float w = area.getWidth() - depthX, h = area.getHeight() - depthY;
        const int shown = juce::roundToInt (pos * (prisma::numFrames - 1));
        constexpr int points = 96;
        for (int f = prisma::numFrames - 1; f >= 0; f -= 1)
        {
            if (f % 2 == 1 && f != shown) continue;
            const float d = (float) f / (float) (prisma::numFrames - 1);
            const float ox = area.getX() + depthX * d, oy = area.getY() + depthY * (1.0f - d);
            juce::Path p;
            const float* data = bank.frame (table, f, 3);
            for (int i = 0; i <= points; ++i)
            {
                const double ph = (double) i / points;
                const float v = data[(int) (ph * (prisma::frameSize - 1))];
                const float x = ox + (float) ph * w, y = oy + h * 0.5f - v * h * 0.45f;
                if (i == 0) p.startNewSubPath (x, y); else p.lineTo (x, y);
            }
            if (f == shown) continue;
            g.setColour ((on ? prismaAccent : frui::colours::textDim).withAlpha (0.10f + 0.12f * (1.0f - std::abs (d - pos))));
            g.strokePath (p, juce::PathStrokeType (1.0f));
        }
        // la forma che suona, con il warp
        {
            const float d = (float) shown / (float) (prisma::numFrames - 1);
            const float ox = area.getX() + depthX * d, oy = area.getY() + depthY * (1.0f - d);
            juce::Path p;
            for (int i = 0; i <= points * 2; ++i)
            {
                const double ph = (double) i / (points * 2);
                const double wp = warp == 7 ? ph : prisma::warpPhase (warp, warpAmt, ph);
                const float v = bank.read (table, pos, wp, 2);
                const float x = ox + (float) ph * w, y = oy + h * 0.5f - v * h * 0.45f;
                if (i == 0) p.startNewSubPath (x, y); else p.lineTo (x, y);
            }
            const auto c = on ? prismaAccent : frui::colours::textDim;
            g.setColour (c.withAlpha (0.25f));
            g.strokePath (p, juce::PathStrokeType (5.0f));
            g.setColour (c);
            g.strokePath (p, juce::PathStrokeType (1.8f));
        }
        if (! on)
        {
            g.setColour (frui::colours::textDim);
            g.setFont (frui::sans (12.0f));
            g.drawText ("SPENTO", r, juce::Justification::centred, false);
        }
    }

    void mouseDown (const juce::MouseEvent&) override   { startPos = param ("Pos"); }
    void mouseDrag (const juce::MouseEvent& e) override
    {
        if (auto* prm = proc.apvts.getParameter (prefix + "Pos"))
            prm->setValueNotifyingHost (juce::jlimit (0.0f, 1.0f, startPos - (float) e.getDistanceFromDragStartY() / (float) juce::jmax (60, getHeight())
                                                                           + (float) e.getDistanceFromDragStartX() / (float) juce::jmax (120, getWidth() * 2)));
    }

    void refresh()
    {
        const juce::int64 sig = (juce::int64) (currentPos() * 1000.0f) * 131 + (juce::int64) param ("Table") * 7 + (juce::int64) param ("Warp") * 17
                              + (juce::int64) (param ("WarpAmt") * 500.0f) * 3 + (juce::int64) param ("On");
        if (sig != last) { last = sig; repaint(); }
    }

private:
    float param (const char* id) const
    {
        if (auto* v = proc.apvts.getRawParameterValue (prefix + id)) return v->load();
        return 0.0f;
    }
    float currentPos() const
    {
        const float live = proc.shownPos[osc].load();
        if (live >= 0.0f) return live;
        return juce::jlimit (0.0f, 1.0f, param ("Pos") + proc.apvts.getRawParameterValue ("morph")->load());
    }

    PrismaAudioProcessor& proc;
    const int osc;
    const juce::String prefix;
    float startPos = 0.0f;
    juce::int64 last = 0;
};

//==============================================================================
PrismaEditor::PrismaEditor (PrismaAudioProcessor& p)
    : NeonEditor (p, p.apvts, "Prisma", prismaAccent, { "OSC A", "OSC B", "FILTRO", "ENV LFO", "MOD", "FX" },
                  { "morph", "MORPH", "POSIZIONE TABELLE", {} },
                  { "cutoff", "CUTOFF", "FILTRO", {} })
{
    for (int o = 0; o < 2; ++o)
    {
        const juce::String P = o == 0 ? "a" : "b", name = o == 0 ? "OSC A" : "OSC B";
        auto& t = section (o, name, 1.5f, 0, 0, 1.6f);
        t.viewLeft = true;
        choice (t, P + "On");
        choice (t, P + "Table", "TABELLA ");
        choice (t, P + "Warp", "WARP ");
        tables[o] = static_cast<TableView*> (&view (t, std::make_unique<TableView> (p, o), 0.6f));
        auto& k = section (o, "POSIZIONE E WARP", 1.0f, 2, 0, 1.6f);
        knob (k, P + "Pos", "POSIZIONE");
        knob (k, P + "WarpAmt", "WARP");
        knob (k, P + "Level", "VOLUME");
        knob (k, P + "Pan", "PAN");
        auto& u = section (o, "UNISONO E INTONAZIONE", 1.0f, 4, 1, 1.0f);
        choice (u, P + "Uni", "UNISONO ");
        choice (u, P + "Oct");
        knob (u, P + "Detune", "DETUNE");
        knob (u, P + "Blend", "LARGHEZZA");
        knob (u, P + "Semi", "SEMITONI");
        knob (u, P + "Fine", "FINE");
    }

    auto& f = section (2, "FILTRO", 1.3f, 3);
    choice (f, "fltType", "TIPO ");
    knob (f, "res", "RISONANZA");
    knob (f, "drive", "DRIVE");
    knob (f, "fltEnv", "INVILUPPO 2");
    knob (f, "keytrack", "KEYTRACK");
    auto& s = section (2, "SUB E RUMORE", 1.0f, 3);
    choice (s, "subOn");
    choice (s, "subShape", "SUB ");
    choice (s, "subOct", "OTTAVA SUB ");
    knob (s, "subLevel", "SUB");
    knob (s, "noise", "RUMORE");
    knob (s, "noiseColor", "COLORE");
    auto& v = section (2, "VOCI", 0.7f, 1);
    choice (v, "voiceMode");
    knob (v, "glide", "GLIDE");

    auto& e1 = section (3, "INVILUPPO 1 (VOLUME)", 1.0f, 4, 0);
    adsr (e1, "envA", "envD", "envS", "envR");
    knob (e1, "envA", "A"); knob (e1, "envD", "D"); knob (e1, "envS", "S"); knob (e1, "envR", "R");
    auto& e2 = section (3, "INVILUPPO 2 (MODULAZIONE)", 1.0f, 4, 0);
    adsr (e2, "env2A", "env2D", "env2S", "env2R");
    knob (e2, "env2A", "A"); knob (e2, "env2D", "D"); knob (e2, "env2S", "S"); knob (e2, "env2R", "R");
    for (int l = 1; l <= 2; ++l)
    {
        const juce::String P = "l" + juce::String (l);
        auto& lf = section (3, "LFO " + juce::String (l), 1.0f, 1, 1);
        choice (lf, P + "Shape", "FORMA ");
        choice (lf, P + "Sync", "TEMPO ");
        choice (lf, P + "Retrig");
        knob (lf, P + "Rate", "VELOCITA' (LIBERO)");
    }

    for (int m = 1; m <= prisma::numSlots; ++m)
    {
        const juce::String P = "m" + juce::String (m);
        auto& ms = section (4, "MOD " + juce::String (m), 1.0f, 1, (m - 1) / 4);
        choice (ms, P + "Src", "DA ");
        choice (ms, P + "Dst", "A ");
        knob (ms, P + "Amt", "QUANTO");
    }

    auto& d = section (5, "CHORUS E DELAY", 1.0f, 3);
    choice (d, "dlyTime", "DELAY ");
    knob (d, "chorus", "CHORUS");
    knob (d, "dlyMix", "DELAY");
    knob (d, "dlyFb", "FEEDBACK");
    auto& r = section (5, "RIVERBERO E USCITA", 1.0f, 3);
    knob (r, "revMix", "RIVERBERO");
    knob (r, "revSize", "STANZA");
    knob (r, "master", "VOLUME");

    showTab (0);
}

void PrismaEditor::refreshViews()
{
    for (auto* t : tables)
        if (t != nullptr && t->isShowing()) t->refresh();
}
