#include "AcidLineEditor.h"

static const juce::Colour acidAccent { 0xffd8ff4a };
static const juce::Colour accentOrange { 0xffff9a3c };
static const juce::Colour slideBlue { 0xff7fd7ff };

//==============================================================================
class AcidLineEditor::BarsView : public juce::Component
{
public:
    explicit BarsView (AcidLineAudioProcessor& p) : proc (p) {}

    void resized() override
    {
        auto r = getLocalBounds();
        header = r.removeFromTop (24);
        randomArea = header.removeFromRight (juce::jmin (120, header.getWidth() / 3)).reduced (0, 2);
        r.removeFromTop (4);
        accentRow = r.removeFromBottom (16);
        slideRow = r.removeFromTop (12);
        barArea = r.reduced (0, 2);
    }

    void paint (juce::Graphics& g) override
    {
        // pulsante CASUALE e legenda
        auto ra = randomArea.toFloat();
        g.setColour (frui::lcdBg());
        g.fillRoundedRectangle (ra, 4.0f);
        g.setColour (juce::Colours::black);
        g.drawRoundedRectangle (ra, 4.0f, 1.0f);
        g.setFont (frui::mono (12.0f));
        frui::drawNeon (g, juce::String (juce::CharPointer_UTF8 ("\xe2\x9a\x84 CASUALE")), ra, 0.5f, juce::Justification::centred, frui::lcdText());
        g.setFont (frui::sans (9.5f, false));
        g.setColour (accentOrange);
        g.drawText (juce::String (juce::CharPointer_UTF8 ("\xe2\x97\x8f ACCENT")), header.withWidth (80), juce::Justification::centredLeft, false);
        g.setColour (slideBlue);
        g.drawText (juce::String (juce::CharPointer_UTF8 ("\xe2\x96\xac SLIDE")), header.withTrimmedLeft (80).withWidth (80), juce::Justification::centredLeft, false);

        auto area = getLocalBounds().toFloat().withTop ((float) slideRow.getY() - 2.0f);
        g.setColour (juce::Colour (0xff06090b));
        g.fillRoundedRectangle (area, 6.0f);
        g.setColour (juce::Colours::black);
        g.drawRoundedRectangle (area, 6.0f, 1.0f);

        const int playing = proc.playStep.load();
        static const char* names[] { "C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B", "C" };
        for (int i = 0; i < acid::numSteps; ++i)
        {
            const auto st = proc.getStep (i);
            auto col = column (i);
            auto bar = col.withTop ((float) barArea.getY()).withBottom ((float) barArea.getBottom());
            const float v = (float) (st.note + 12 * st.octave + 12) / 36.0f;
            if (st.gate)
            {
                auto fill = bar.withTop (bar.getBottom() - juce::jmax (6.0f, v * bar.getHeight()));
                g.setColour (acidAccent.withAlpha (i == playing ? 0.5f : 0.2f));
                g.fillRoundedRectangle (fill.expanded (2.0f), 4.0f);
                g.setColour (i == playing ? acidAccent.brighter (0.4f) : acidAccent);
                g.fillRoundedRectangle (fill, 3.0f);
                if (fill.getHeight() > 16.0f && col.getWidth() > 14.0f)
                {
                    g.setColour (juce::Colour (0xff141a00));
                    g.setFont (frui::sans (juce::jmin (10.0f, col.getWidth() * 0.45f)));
                    g.drawText (names[st.note], fill.removeFromTop (14.0f), juce::Justification::centred, false);
                }
            }
            else
            {
                g.setColour (i == playing ? acidAccent.withAlpha (0.4f) : juce::Colour (0xff1a1f24));
                g.fillRoundedRectangle (bar.withTop (bar.getBottom() - 5.0f), 2.0f);
            }
            // slide: lineetta in alto, accent: pallino in basso
            auto sl = juce::Rectangle<float> (col.getX(), (float) slideRow.getY(), col.getWidth(), (float) slideRow.getHeight()).reduced (1.0f, 3.0f);
            g.setColour (st.slide ? slideBlue : juce::Colour (0xff14181c));
            g.fillRoundedRectangle (sl, 2.0f);
            const float d = juce::jmin (9.0f, col.getWidth() * 0.6f);
            auto dot = juce::Rectangle<float> (d, d).withCentre ({ col.getCentreX(), (float) accentRow.getCentreY() });
            g.setColour (st.accent ? accentOrange : juce::Colour (0xff1a1f24));
            g.fillEllipse (dot);
        }
    }

    void mouseDown (const juce::MouseEvent& e) override
    {
        moved = false;
        downStep = stepAt (e.position.x);
        const auto pos = e.getPosition();
        if (randomArea.contains (pos))
        {
            proc.setLine (acid::randomLine (juce::Random::getSystemRandom().nextInt ({ 1, 99999 })));
            proc.lineToState();
            repaint();
            downStep = -1;
            return;
        }
        if (downStep < 0) return;
        if (accentRow.expanded (0, 4).contains (pos)) { toggle (downStep, 1); downStep = -1; return; }
        if (slideRow.expanded (0, 4).contains (pos)) { toggle (downStep, 2); downStep = -1; return; }
        startY = e.position.y;
    }

    void mouseDrag (const juce::MouseEvent& e) override
    {
        if (downStep < 0) return;
        if (! moved && std::abs (e.position.y - startY) < 5.0f) return;
        moved = true;
        // trascinando si passa sopra a piu' step: ognuno prende l'altezza del dito
        const int s = stepAt (e.position.x);
        if (s < 0) return;
        const float v = juce::jlimit (0.0f, 1.0f, ((float) barArea.getBottom() - e.position.y) / (float) barArea.getHeight());
        const int semis = juce::jlimit (-12, 24, juce::roundToInt (v * 36.0f) - 12);
        auto st = proc.getStep (s);
        st.octave = juce::jlimit (-1, 1, (int) std::floor ((float) semis / 12.0f));
        st.note = juce::jlimit (0, 12, semis - 12 * st.octave);
        st.gate = true;
        proc.setStep (s, st);
        repaint();
    }

    void mouseUp (const juce::MouseEvent&) override
    {
        if (downStep >= 0 && ! moved) toggle (downStep, 0);   // tocco: accesa / pausa
        if (downStep >= 0) proc.lineToState();
        downStep = -1;
    }

    void refresh()
    {
        const juce::int64 sig = (juce::int64) acid::encode (proc.getLine()).hashCode64() * 31 + proc.playStep.load();
        if (sig != last) { last = sig; repaint(); }
    }

private:
    juce::Rectangle<float> column (int i) const
    {
        const float w = (float) barArea.getWidth() / (float) acid::numSteps;
        const float gapBeat = (i % 4 == 0 && i > 0) ? 3.0f : 0.0f;
        return juce::Rectangle<float> ((float) barArea.getX() + (float) i * w + 2.0f + gapBeat * 0.5f, (float) slideRow.getY(), w - 4.0f, (float) (accentRow.getBottom() - slideRow.getY()));
    }

    int stepAt (float x) const
    {
        if (barArea.getWidth() <= 0) return -1;
        const int s = (int) ((x - (float) barArea.getX()) / ((float) barArea.getWidth() / (float) acid::numSteps));
        return s >= 0 && s < acid::numSteps ? s : -1;
    }

    void toggle (int s, int what)
    {
        auto st = proc.getStep (s);
        if (what == 0) st.gate = ! st.gate;
        else if (what == 1) { st.accent = ! st.accent; st.gate = st.gate || st.accent; }
        else { st.slide = ! st.slide; st.gate = st.gate || st.slide; }
        proc.setStep (s, st);
        proc.lineToState();
        repaint();
    }

    AcidLineAudioProcessor& proc;
    juce::Rectangle<int> header, randomArea, slideRow, accentRow, barArea;
    int downStep = -1;
    bool moved = false;
    float startY = 0.0f;
    juce::int64 last = 0;
};

//==============================================================================
/** Come si apre e si richiude il filtro a ogni nota (piu' alto con l'accent). */
class AcidLineEditor::FilterView : public juce::Component
{
public:
    explicit FilterView (juce::AudioProcessorValueTreeState& s) : apvts (s) { setInterceptsMouseClicks (false, false); }

    void paint (juce::Graphics& g) override
    {
        auto r = getLocalBounds().toFloat().reduced (0.5f);
        g.setColour (juce::Colour (0xff06090b));
        g.fillRoundedRectangle (r, 5.0f);
        g.setColour (juce::Colours::black);
        g.drawRoundedRectangle (r, 5.0f, 1.0f);
        auto a = r.reduced (6.0f, 6.0f);
        const float cutoff = value ("cutoff"), env = juce::jlimit (0.0f, 1.0f, value ("envMod") + value ("squelch") * 0.4f);
        const float decay = value ("decay"), reso = juce::jlimit (0.0f, 0.97f, value ("reso") + value ("squelch") * 0.3f);
        const float base = std::log2 (cutoff / 60.0f) / std::log2 (100.0f);
        juce::Path p;
        const int notes = 6;
        for (int i = 0; i <= 240; ++i)
        {
            const float x = (float) i / 240.0f;
            const float local = std::fmod (x * (float) notes, 1.0f);
            const bool accent = ((int) (x * (float) notes)) % 3 == 0;
            const float dT = accent ? 0.18f : 0.06f + 1.2f * decay * decay;
            const float e = std::exp (-local * 0.35f / dT);
            float v = juce::jlimit (0.0f, 1.0f, base + env * (accent ? 1.4f : 1.0f) * e * 0.55f);
            v += reso * 0.12f * std::sin (local * 60.0f) * e;   // un po' di "squittio" disegnato
            const float px = a.getX() + x * a.getWidth(), py = a.getBottom() - juce::jlimit (0.0f, 1.0f, v) * a.getHeight();
            if (i == 0) p.startNewSubPath (px, py); else p.lineTo (px, py);
        }
        g.setColour (acidAccent.withAlpha (0.25f));
        g.strokePath (p, juce::PathStrokeType (4.0f));
        g.setColour (acidAccent);
        g.strokePath (p, juce::PathStrokeType (1.6f));
    }

    void refresh()
    {
        juce::int64 sig = 0;
        for (auto* id : { "cutoff", "envMod", "decay", "reso", "squelch" }) sig = sig * 1009 + (juce::int64) (value (id) * 1000.0f);
        if (sig != last) { last = sig; repaint(); }
    }

private:
    float value (const char* id) const   { return apvts.getRawParameterValue (id)->load(); }
    juce::AudioProcessorValueTreeState& apvts;
    juce::int64 last = 0;
};

//==============================================================================
AcidLineEditor::AcidLineEditor (AcidLineAudioProcessor& p)
    : NeonEditor (p, p.apvts, "AcidLine", acidAccent, { "SEQUENZA", "SUONO", "FX" },
                  { "squelch", "SQUELCH", "RESO + ENV", {} },
                  { "acid", "ACIDO", "DISTORSIONE", {} })
{
    auto& n = section (0, "NOTE (TRASCINA SU/GIU', TOCCA = PAUSA)", 1.4f);
    bars = static_cast<BarsView*> (&view (n, std::make_unique<BarsView> (p)));

    auto& f = section (0, "FILTRO", 1.0f, 3);
    filter = static_cast<FilterView*> (&view (f, std::make_unique<FilterView> (p.apvts), 0.36f));
    knob (f, "cutoff", "CUTOFF");
    knob (f, "reso", "RESO");
    knob (f, "envMod", "ENV MOD");
    knob (f, "decay", "DECAY");
    knob (f, "accent", "ACCENT");
    knob (f, "tune", "TUNE");

    auto& s = section (1, "SUONO E SEQUENZA", 1.0f, 3);
    choice (s, "wave");
    choice (s, "mode");
    knob (s, "slideTime", "SLIDE");
    knob (s, "gateLen", "DURATA NOTE");
    knob (s, "level", "LIVELLO");

    auto& d = section (2, "DELAY", 1.0f, 2);
    choice (d, "dlyTime", "DELAY ");
    knob (d, "dlyMix", "DELAY");
    knob (d, "dlyFb", "FEEDBACK");
    auto& r = section (2, "RIVERBERO E USCITA", 1.0f, 3);
    knob (r, "revMix", "RIVERBERO");
    knob (r, "revSize", "STANZA");
    knob (r, "master", "VOLUME");

    showTab (0);
}

void AcidLineEditor::refreshViews()
{
    if (bars != nullptr) bars->refresh();
    if (filter != nullptr) filter->refresh();
}
