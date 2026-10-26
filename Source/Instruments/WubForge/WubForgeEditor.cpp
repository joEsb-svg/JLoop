#include "WubForgeEditor.h"

//==============================================================================
/** Le 4 forme dell'LFO, con la velocita' di ognuna sopra. Si disegnano trascinando il dito. */
class WubForgeEditor::ShapesView : public juce::Component
{
public:
    explicit ShapesView (WubForgeAudioProcessor& p) : proc (p)
    {
        const char* letters[] { "A", "B", "C", "D" };
        for (int s = 0; s < wf::numShapes; ++s)
        {
            rates[s] = std::make_unique<frui::Lcd> (proc.apvts, juce::String ("rate") + letters[s], juce::String (letters[s]) + " ");
            addAndMakeVisible (*rates[s]);
        }
    }

    void resized() override
    {
        auto r = getLocalBounds();
        const int w = r.getWidth() / wf::numShapes;
        const int lcdH = juce::jlimit (18, 26, r.getHeight() / 5);
        for (int s = 0; s < wf::numShapes; ++s)
        {
            auto cell = juce::Rectangle<int> (r.getX() + s * w, r.getY(), w, r.getHeight()).reduced (5, 0);
            rates[s]->setBounds (cell.removeFromTop (lcdH).withSizeKeepingCentre (juce::jmin (cell.getWidth(), 120), lcdH));
            cell.removeFromTop (4);
            graphs[s] = cell;
        }
    }

    void paint (juce::Graphics& g) override
    {
        const auto accent = frui::accent();
        const int playing = playingShape();
        for (int s = 0; s < wf::numShapes; ++s)
        {
            auto r = graphs[s].toFloat();
            g.setColour (juce::Colour (0xff050805));
            g.fillRoundedRectangle (r, 5.0f);
            g.setColour (s == playing ? accent.withAlpha (0.7f) : juce::Colours::black);
            g.drawRoundedRectangle (r.reduced (0.5f), 5.0f, s == playing ? 1.5f : 1.0f);
            auto area = r.reduced (4.0f, 6.0f);
            juce::Path p;   // i 32 punti, senza il ritorno all'inizio
            const float dx = area.getWidth() / (float) wf::shapePoints;
            for (int i = 0; i < wf::shapePoints; ++i)
            {
                const float y = area.getBottom() - proc.getShape (s, i) * area.getHeight();
                if (i == 0) p.startNewSubPath (area.getX(), y);
                p.lineTo (area.getX() + ((float) i + 0.5f) * dx, y);
                if (i == wf::shapePoints - 1) p.lineTo (area.getRight(), y);
            }
            g.setColour (accent.withAlpha (0.22f));
            g.strokePath (p, juce::PathStrokeType (4.0f));
            g.setColour (accent);
            g.strokePath (p, juce::PathStrokeType (1.6f));
            if (s == playing)
            {
                const float ph = proc.playPhase.load();
                const float x = area.getX() + ph * area.getWidth(), y = area.getBottom() - proc.readShape (s, ph) * area.getHeight();
                g.setColour (accent.withAlpha (0.25f));
                g.fillEllipse (x - 6.0f, y - 6.0f, 12.0f, 12.0f);
                g.setColour (juce::Colours::white);
                g.fillEllipse (x - 2.5f, y - 2.5f, 5.0f, 5.0f);
            }
        }
    }

    void mouseDown (const juce::MouseEvent& e) override
    {
        editing = -1;
        for (int s = 0; s < wf::numShapes; ++s)
            if (graphs[s].contains (e.getPosition())) editing = s;
        lastIndex = -1;
        if (editing >= 0) draw (e.position);
    }
    void mouseDrag (const juce::MouseEvent& e) override    { if (editing >= 0) draw (e.position); }
    void mouseUp (const juce::MouseEvent&) override         { if (editing >= 0) proc.shapesToState(); }

    void mouseDoubleClick (const juce::MouseEvent& e) override
    {
        for (int s = 0; s < wf::numShapes; ++s)
            if (graphs[s].contains (e.getPosition()))
            {
                juce::PopupMenu m;
                const auto names = wf::shapeKindNames();
                for (int k = 0; k < names.size(); ++k) m.addItem (k + 1, names[k]);
                juce::Component::SafePointer<ShapesView> safe (this);
                m.showMenuAsync (juce::PopupMenu::Options().withTargetComponent (this).withTargetScreenArea (localAreaToGlobal (graphs[s])),
                                 [safe, s] (int r)
                                 {
                                     if (safe == nullptr || r <= 0) return;
                                     safe->proc.setShapeKind (s, r - 1);
                                     safe->proc.shapesToState();
                                     safe->repaint();
                                 });
                return;
            }
    }

    void refresh()
    {
        const int slot = proc.playSlot.load();
        const float ph = proc.playPhase.load();
        if (slot != lastSlot || std::abs (ph - lastPhase) > 0.001f) { lastSlot = slot; lastPhase = ph; repaint(); }
    }

private:
    int playingShape() const
    {
        const int slot = proc.playSlot.load();
        if (slot < 0) return -1;
        return juce::roundToInt (proc.apvts.getRawParameterValue ("seq" + juce::String (slot))->load());
    }

    void draw (juce::Point<float> pos)
    {
        auto area = graphs[editing].toFloat().reduced (4.0f, 6.0f);
        const int idx = juce::jlimit (0, wf::shapePoints - 1, (int) ((pos.x - area.getX()) / area.getWidth() * (float) wf::shapePoints));
        const float v = juce::jlimit (0.0f, 1.0f, (area.getBottom() - pos.y) / area.getHeight());
        if (lastIndex < 0 || lastIndex == idx) proc.setShape (editing, idx, v);
        else
        {
            // riempie i punti saltati da un trascinamento veloce
            const int step = idx > lastIndex ? 1 : -1;
            for (int i = lastIndex + step; ; i += step)
            {
                const float t = (float) (i - lastIndex) / (float) (idx - lastIndex);
                proc.setShape (editing, i, lastValue + t * (v - lastValue));
                if (i == idx) break;
            }
        }
        lastIndex = idx;
        lastValue = v;
        repaint();
    }

    WubForgeAudioProcessor& proc;
    std::unique_ptr<frui::Lcd> rates[wf::numShapes];
    juce::Rectangle<int> graphs[wf::numShapes];
    int editing = -1, lastIndex = -1, lastSlot = -2;
    float lastValue = 0.0f, lastPhase = -1.0f;
};

//==============================================================================
/** Le 8 caselle della sequenza (mezza battuta ciascuna): tocca per passare A > B > C > D. */
class WubForgeEditor::SequenceView : public juce::Component
{
public:
    explicit SequenceView (WubForgeAudioProcessor& p) : proc (p) {}

    void paint (juce::Graphics& g) override
    {
        const auto accent = frui::accent();
        const int playing = proc.playSlot.load();
        const float w = (float) getWidth() / (float) wf::seqSlots;
        for (int s = 0; s < wf::seqSlots; ++s)
        {
            auto r = juce::Rectangle<float> ((float) s * w, 0.0f, w, (float) getHeight()).reduced (3.0f, 1.0f);
            const bool on = s == playing;
            g.setColour (on ? accent : juce::Colour (0xff15181b));
            g.fillRoundedRectangle (r, 4.0f);
            g.setColour (juce::Colours::black);
            g.drawRoundedRectangle (r, 4.0f, 1.0f);
            g.setFont (frui::sans (juce::jlimit (11.0f, 17.0f, r.getHeight() * 0.5f)));
            g.setColour (on ? juce::Colours::black : frui::colours::text);
            g.drawText (juce::String::charToString ((juce::juce_wchar) ('A' + value (s))), r, juce::Justification::centred, false);
            if (s % 2 == 0)   // numero della battuta
            {
                g.setFont (frui::sans (9.0f, false));
                g.setColour (on ? juce::Colours::black.withAlpha (0.6f) : frui::colours::textDim);
                g.drawText (juce::String (s / 2 + 1), r.reduced (4.0f, 2.0f), juce::Justification::topLeft, false);
            }
        }
    }

    void mouseDown (const juce::MouseEvent& e) override
    {
        const int s = juce::jlimit (0, wf::seqSlots - 1, (int) (e.position.x / ((float) getWidth() / (float) wf::seqSlots)));
        if (auto* prm = proc.apvts.getParameter ("seq" + juce::String (s)))
            prm->setValueNotifyingHost (prm->convertTo0to1 ((float) ((value (s) + 1) % wf::numShapes)));
        repaint();
    }

    void refresh()
    {
        int sig = proc.playSlot.load() + 100;
        for (int s = 0; s < wf::seqSlots; ++s) sig = sig * 4 + value (s);
        if (sig != lastSig) { lastSig = sig; repaint(); }
    }

private:
    int value (int s) const   { return juce::roundToInt (proc.apvts.getRawParameterValue ("seq" + juce::String (s))->load()); }
    WubForgeAudioProcessor& proc;
    int lastSig = 0;
};

//==============================================================================
WubForgeEditor::WubForgeEditor (WubForgeAudioProcessor& p)
    : NeonEditor (p, p.apvts, "WubForge", juce::Colour (0xff8cff3a), { "WOBBLE", "OSC", "FX" },
                  { "wobble", "WOBBLE", juce::String::fromUTF8 ("PROFONDITA'"), {} },
                  { "growl", "GROWL", "FM + DRIVE", {} })
{
    auto& forms = section (0, "FORME DELL'LFO (DISEGNALE COL DITO)", 1.0f, 0, 0, 1.0f);
    shapes = static_cast<ShapesView*> (&view (forms, std::make_unique<ShapesView> (p)));
    auto& seq = section (0, "SEQUENZA: UNA FORMA PER OGNI MEZZA BATTUTA", 1.0f, 4, 1, 1.15f);
    sequence = static_cast<SequenceView*> (&view (seq, std::make_unique<SequenceView> (p), 0.32f));
    const juce::String arrow = juce::String::fromUTF8 ("\xe2\x86\x92 ");
    knob (seq, "amtCut", arrow + "CUTOFF");
    knob (seq, "amtVowel", arrow + "VOCALE");
    knob (seq, "amtVol", arrow + "VOLUME");
    knob (seq, "amtPitch", arrow + "PITCH");

    auto& osc = section (1, "OSCILLATORE", 1.2f, 4);
    lcd (osc, "fmRatio", "FM x");
    knob (osc, "shape", "SAW/QUADRA");
    knob (osc, "detune", "DETUNE");
    knob (osc, "fm", "FM");
    knob (osc, "sub", "SUB");
    knob (osc, "octave", "OTTAVA");
    knob (osc, "glide", "GLIDE");
    knob (osc, "aA", "ATTACCO");
    knob (osc, "aR", "RILASCIO");
    auto& flt = section (1, "FILTRO E VOCALI", 1.0f, 2);
    lcd (flt, "vowels", "VOCALI: ");
    knob (flt, "cutoff", "CUTOFF");
    knob (flt, "reso", "RESO");
    knob (flt, "vowelQ", "NITIDEZZA");

    auto& dist = section (2, "DISTORSIONE", 0.55f, 1);
    knob (dist, "drive", "DRIVE");
    auto& cd = section (2, "CHORUS E DELAY", 1.0f, 2);
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

void WubForgeEditor::refreshViews()
{
    if (shapes != nullptr) shapes->refresh();
    if (sequence != nullptr) sequence->refresh();
}
