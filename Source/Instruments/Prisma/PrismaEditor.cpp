#include "PrismaEditor.h"
#include "PrismaWaveEditor.h"

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
        g.drawText (prisma::Bank::get().names()[table].toUpperCase() + "   POS " + juce::String (juce::roundToInt (pos * 100.0f)) + "%",
                    area.removeFromTop (14.0f), juce::Justification::centredLeft, false);

        // 3D: le forme una dietro l'altra (spostate in alto a destra), la piu' vicina in basso a sinistra
        auto& bank = prisma::Bank::get();
        bank.ensure (table);
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
            const auto spec = warp >= prisma::firstSpectralWarp ? proc.spectralOf (osc) : nullptr;
            const float d = (float) shown / (float) (prisma::numFrames - 1);
            const float ox = area.getX() + depthX * d, oy = area.getY() + depthY * (1.0f - d);
            juce::Path p;
            for (int i = 0; i <= points * 2; ++i)
            {
                const double ph = (double) i / (points * 2);
                const double wp = warp == 7 ? ph : prisma::warpPhase (warp, warpAmt, ph);
                const float v = spec != nullptr ? spec->read (warpAmt, pos, ph, 2) : bank.read (table, pos, wp, 2);
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
        // il tasto per disegnare le proprie onde
        const auto chip = editRect().toFloat();
        g.setColour (juce::Colour (0xff0d2229));
        g.fillRoundedRectangle (chip, 5.0f);
        g.setColour (prismaAccent);
        g.drawRoundedRectangle (chip.reduced (0.5f), 5.0f, 1.2f);
        g.setFont (frui::sans (10.5f));
        g.drawText (juce::String (juce::CharPointer_UTF8 ("\xe2\x9c\x8e MIE ONDE")), chip, juce::Justification::centred, false);
    }

    std::function<void()> onEdit;

    juce::Rectangle<int> editRect() const   { return { getWidth() - 96, 5, 90, 24 }; }

    void mouseDown (const juce::MouseEvent& e) override
    {
        editing = editRect().expanded (6).contains (e.getPosition());
        if (editing) { if (onEdit) onEdit(); return; }
        startPos = param ("Pos");
    }
    void mouseDrag (const juce::MouseEvent& e) override
    {
        if (editing) return;
        if (auto* prm = proc.apvts.getParameter (prefix + "Pos"))
            prm->setValueNotifyingHost (juce::jlimit (0.0f, 1.0f, startPos - (float) e.getDistanceFromDragStartY() / (float) juce::jmax (60, getHeight())
                                                                           + (float) e.getDistanceFromDragStartX() / (float) juce::jmax (120, getWidth() * 2)));
    }

    void refresh()
    {
        const juce::int64 sig = (juce::int64) prisma::Bank::get().userVersion ((int) param ("Table")) * 977 + (juce::int64) (currentPos() * 1000.0f) * 131 + (juce::int64) param ("Table") * 7 + (juce::int64) param ("Warp") * 17
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
    bool editing = false;
    juce::int64 last = 0;
};

//==============================================================================
PrismaEditor::PrismaEditor (PrismaAudioProcessor& p)
    : NeonEditor (p, p.apvts, "Prisma", prismaAccent, { "OSC A", "OSC B", "FILTRO", "ENV LFO", "MOD", "FX" },
                  { "morph", "MORPH", "POSIZIONE TABELLE", {} },
                  { "cutoff", "CUTOFF", "FILTRO", {} }),
      prismaProc (p)
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
        tables[o]->onEdit = [this, o] { openWaveEditor (o); };
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

    // tieni premuta una manopola: "modula con..." (e un anello arancione mostra quanto si muove)
    for (auto& [id, k] : knobList)
        if (const int dest = modDestFor (id); dest > 0)
        {
            auto* knobPtr = k;
            k->onLongPress = [this, knobPtr, dest] { showModMenu (*knobPtr, dest); };
            k->modRing = [this, dest] { return modAmountFor (dest); };
        }

    showTab (0);
}

PrismaEditor::~PrismaEditor() = default;

int PrismaEditor::modDestFor (const juce::String& id)
{
    static const std::pair<const char*, int> map[] {
        { "aPos", 1 }, { "bPos", 2 }, { "aWarpAmt", 3 }, { "bWarpAmt", 4 }, { "cutoff", 5 }, { "res", 6 }, { "aSemi", 7 }, { "bSemi", 8 },
        { "aLevel", 9 }, { "bLevel", 10 }, { "aDetune", 11 }, { "bDetune", 11 }, { "aPan", 12 }, { "bPan", 12 }, { "subLevel", 13 },
        { "noise", 14 }, { "drive", 15 }, { "l1Rate", 16 }, { "l2Rate", 17 } };
    for (auto& m : map) if (id == m.first) return m.second;
    return 0;
}

float PrismaEditor::slotValue (int slot, const char* what) const
{
    if (auto* v = apvts.getRawParameterValue ("m" + juce::String (slot) + what)) return v->load();
    return 0.0f;
}

void PrismaEditor::setSlotValue (int slot, const char* what, float value)
{
    if (auto* p = apvts.getParameter ("m" + juce::String (slot) + what))
    {
        p->beginChangeGesture();
        p->setValueNotifyingHost (p->convertTo0to1 (value));
        p->endChangeGesture();
    }
}

float PrismaEditor::modAmountFor (int dest) const
{
    float sum = 0.0f;
    for (int s = 1; s <= prisma::numSlots; ++s)
        if ((int) slotValue (s, "Src") > 0 && (int) slotValue (s, "Dst") == dest) sum += slotValue (s, "Amt");
    return sum;
}

void PrismaEditor::showModMenu (frui::Knob& k, int dest)
{
    const auto sources = prisma::sourceNames();
    juce::PopupMenu menu;
    menu.addSectionHeader ("MODULA " + k.getTitle().toUpperCase());
    bool used[16] {};
    int freeSlot = 0;
    for (int s = 1; s <= prisma::numSlots; ++s)
    {
        const int src = (int) slotValue (s, "Src"), dst = (int) slotValue (s, "Dst");
        if ((src == 0 || dst == 0) && freeSlot == 0) freeSlot = s;
        if (src <= 0 || dst != dest) continue;
        used[juce::jlimit (0, 15, src)] = true;
        const int pct = juce::roundToInt (slotValue (s, "Amt") * 100.0f);
        juce::PopupMenu sub;
        const int amounts[] { 100, 50, 25, 10, -10, -25, -50, -100 };
        for (int a = 0; a < 8; ++a)
            sub.addItem (1000 + s * 20 + a, "Quanto " + juce::String (amounts[a] > 0 ? "+" : "") + juce::String (amounts[a]) + "%", true, pct == amounts[a]);
        sub.addSeparator();
        sub.addItem (1000 + s * 20 + 9, "Togli questo collegamento");
        menu.addSubMenu (sources[src] + "  (" + (pct > 0 ? "+" : "") + juce::String (pct) + "%)", sub);
    }
    menu.addSeparator();
    if (freeSlot == 0)
        menu.addItem (-1, "Matrice piena (8 su 8): libera un posto nella scheda MOD", false);
    else
        for (int src = 1; src < sources.size(); ++src)
            if (! used[src]) menu.addItem (100 + src, "Aggiungi " + sources[src]);
    menu.addSeparator();
    menu.addItem (50, "Apri la scheda MOD");

    juce::Component::SafePointer<PrismaEditor> safe (this);
    menu.showMenuAsync (juce::PopupMenu::Options().withTargetComponent (&k), [safe, dest, freeSlot] (int r)
    {
        if (safe == nullptr || r <= 0) return;
        if (r == 50) { safe->showTab (4); return; }
        if (r >= 100 && r < 200 && freeSlot > 0)
        {
            const int src = r - 100;
            safe->setSlotValue (freeSlot, "Src", (float) src);
            safe->setSlotValue (freeSlot, "Dst", (float) dest);
            safe->setSlotValue (freeSlot, "Amt", dest == 7 || dest == 8 ? 0.1f : 0.5f);   // il pitch: poco (vibrato)
        }
        else if (r >= 1000)
        {
            const int s = (r - 1000) / 20, a = (r - 1000) % 20;
            const int amounts[] { 100, 50, 25, 10, -10, -25, -50, -100 };
            if (a == 9) { safe->setSlotValue (s, "Src", 0.0f); safe->setSlotValue (s, "Dst", 0.0f); }
            else if (a < 8) safe->setSlotValue (s, "Amt", (float) amounts[a] / 100.0f);
        }
        safe->repaintKnobs();
    });
}

void PrismaEditor::repaintKnobs()
{
    for (auto& [id, k] : knobList) k->repaint();
}

void PrismaEditor::openWaveEditor (int osc)
{
    waveEditor = std::make_unique<PrismaWaveEditor> (prismaProc, osc);
    waveEditor->onClose = [safe = juce::Component::SafePointer<PrismaEditor> (this)]
    {
        juce::MessageManager::callAsync ([safe] { if (safe != nullptr) safe->waveEditor.reset(); });
    };
    addAndMakeVisible (*waveEditor);
    waveEditor->setBounds (getLocalBounds());
}

void PrismaEditor::resized()
{
    NeonEditor::resized();
    if (waveEditor != nullptr) waveEditor->setBounds (getLocalBounds());
}

void PrismaEditor::refreshViews()
{
    prismaProc.updateSpectral();   // tabella dell'utente cambiata: il warp spettrale si rifa'
    for (auto* t : tables)
        if (t != nullptr && t->isShowing()) t->refresh();
    // la matrice e' cambiata (anche dalla scheda MOD): gli anelli si ridisegnano
    juce::int64 sig = 0;
    for (int s = 1; s <= prisma::numSlots; ++s)
        sig = sig * 31 + (juce::int64) slotValue (s, "Src") * 7 + (juce::int64) slotValue (s, "Dst") * 131 + (juce::int64) (slotValue (s, "Amt") * 1000.0f);
    if (sig != modSig) { modSig = sig; repaintKnobs(); }
}
