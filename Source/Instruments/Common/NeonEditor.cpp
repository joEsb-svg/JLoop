#include "NeonEditor.h"

namespace
{
    /** Didascalia fissa stile LCD sotto un knob grande. */
    class NeonCaption : public juce::Component
    {
    public:
        explicit NeonCaption (juce::String t) : text (std::move (t)) { setInterceptsMouseClicks (false, false); }
        void paint (juce::Graphics& g) override
        {
            auto r = getLocalBounds().toFloat().reduced (0.5f);
            g.setColour (frui::lcdBg());
            g.fillRoundedRectangle (r, 4.0f);
            g.setColour (juce::Colours::black);
            g.drawRoundedRectangle (r, 4.0f, 1.0f);
            g.setFont (frui::mono (juce::jlimit (9.0f, 13.0f, r.getHeight() * 0.55f)));
            frui::drawNeon (g, text, r, 0.5f, juce::Justification::centred, frui::lcdText());
        }
    private:
        juce::String text;
    };
}

NeonEditor::NeonEditor (juce::AudioProcessor& p, juce::AudioProcessorValueTreeState& state, const juce::String& brandName, juce::Colour accentColour,
                        const juce::StringArray& tabNames, BigKnob first, BigKnob second)
    : juce::AudioProcessorEditor (&p), apvts (state), accent (accentColour), brand (brandName)
{
    frui::accentRef() = accentColour;
    setLookAndFeel (&laf);
    sections.resize ((size_t) tabNames.size());
    for (int i = 0; i < tabNames.size(); ++i)
    {
        auto* t = tabs.add (new juce::TextButton (tabNames[i]));
        t->setClickingTogglesState (true);
        t->setRadioGroupId (7511);
        t->onClick = [this, i] { showTab (i); };
        addAndMakeVisible (t);
    }

    const BigKnob big[2] { first, second };
    for (int k = 0; k < 2; ++k)
    {
        bigPanel[k].showTitle = false;
        addAndMakeVisible (bigPanel[k]);
        bigKnob[k] = std::make_unique<frui::Knob> (apvts, big[k].id, big[k].title, true);
        bigPanel[k].addAndMakeVisible (*bigKnob[k]);
        if (big[k].lcdParam.isNotEmpty())
            bigCaption[k] = std::make_unique<frui::Lcd> (apvts, big[k].lcdParam, big[k].caption);
        else if (big[k].caption.isNotEmpty())
            bigCaption[k] = std::make_unique<NeonCaption> (big[k].caption);
        if (bigCaption[k] != nullptr)
            bigPanel[k].addAndMakeVisible (*bigCaption[k]);
    }
    setSize (900, 400);
    startTimerHz (15);
}

NeonEditor::~NeonEditor()
{
    stopTimer();
    setLookAndFeel (nullptr);
}

NeonEditor::Section& NeonEditor::section (int tabIndex, const juce::String& title, float weight, int columns, int row, float rowWeight)
{
    auto s = std::make_unique<Section>();
    s->row = row;
    s->rowWeight = rowWeight;
    s->panel = std::make_unique<frui::Panel> (title);
    s->weight = weight;
    s->columns = columns;
    addChildComponent (*s->panel);
    sections[(size_t) tabIndex].push_back (std::move (s));
    return *sections[(size_t) tabIndex].back();
}

void NeonEditor::knob (Section& s, const juce::String& id, const juce::String& title)
{
    auto k = std::make_unique<frui::Knob> (apvts, id, title);
    s.panel->addAndMakeVisible (*k);
    s.items.push_back (k.get());
    owned.push_back (std::move (k));
}

void NeonEditor::segment (Section& s, const juce::String& id, juce::StringArray labels)
{
    auto k = std::make_unique<frui::Segment> (apvts, id, std::move (labels), accent);
    s.panel->addAndMakeVisible (*k);
    s.items.push_back (k.get());
    owned.push_back (std::move (k));
}

void NeonEditor::lcd (Section& s, const juce::String& id, const juce::String& prefix)
{
    auto k = std::make_unique<frui::Lcd> (apvts, id, prefix);
    s.panel->addAndMakeVisible (*k);
    s.items.push_back (k.get());
    owned.push_back (std::move (k));
}

juce::Component& NeonEditor::view (Section& s, std::unique_ptr<juce::Component> v, float share)
{
    auto& ref = *v;
    s.view = v.get();
    s.viewShare = share;
    s.panel->addAndMakeVisible (ref);
    owned.push_back (std::move (v));
    return ref;
}

void NeonEditor::choice (Section& s, const juce::String& id, const juce::String& prefix)
{
    if (auto* c = dynamic_cast<juce::AudioParameterChoice*> (apvts.getParameter (id)))
    {
        int chars = 0;
        for (auto& x : c->choices) chars += x.length();
        if (c->choices.size() <= 4 && chars <= 22 && prefix.isEmpty())
        {
            juce::StringArray up;
            for (auto& x : c->choices) up.add (x.toUpperCase());
            segment (s, id, up);
            return;
        }
        lcd (s, id, prefix);
        return;
    }
    if (dynamic_cast<juce::AudioParameterBool*> (apvts.getParameter (id)) != nullptr)
    {
        segment (s, id, { prefix.isNotEmpty() ? prefix + ": NO" : "NO", prefix.isNotEmpty() ? prefix + ": SI'" : "SI'" });
        return;
    }
    lcd (s, id, prefix);
}

void NeonEditor::adsr (Section& s, const juce::String& a, const juce::String& d, const juce::String& su, const juce::String& r, float share)
{
    auto v = std::make_unique<AdsrView> (apvts, juce::StringArray { a, d, su, r });
    adsrViews.push_back (v.get());
    view (s, std::move (v), share);
}

NeonEditor::AdsrView::AdsrView (juce::AudioProcessorValueTreeState& s, juce::StringArray i) : apvts (s), ids (std::move (i))
{
    setInterceptsMouseClicks (false, false);
}

float NeonEditor::AdsrView::value (int i) const
{
    if (auto* v = apvts.getRawParameterValue (ids[i])) return v->load();
    return 0.0f;
}

void NeonEditor::AdsrView::refresh()
{
    juce::uint64 h = 0;
    for (int i = 0; i < 4; ++i) h = h * 1000003u + (juce::uint64) (value (i) * 100000.0f);
    if (h != shown) { shown = h; repaint(); }
}

void NeonEditor::AdsrView::paint (juce::Graphics& g)
{
    const auto accent = frui::accent();
    auto r = getLocalBounds().toFloat().reduced (0.5f);
    g.setColour (juce::Colour (0xff07090b));
    g.fillRoundedRectangle (r, 5.0f);
    g.setColour (juce::Colours::black);
    g.drawRoundedRectangle (r, 5.0f, 1.0f);

    const float A = value (0), D = value (1), S = value (2), R = value (3);
    const float a = std::sqrt (A), d = std::sqrt (D), rel = std::sqrt (R), hold = 0.35f;
    const float total = juce::jmax (0.001f, a + d + hold + rel);
    auto in = r.reduced (8.0f, 6.0f).withTrimmedTop (12.0f);
    auto x = [&] (float t) { return in.getX() + in.getWidth() * t / total; };
    auto y = [&] (float v) { return in.getBottom() - in.getHeight() * v; };
    juce::Path p;
    p.startNewSubPath (x (0), y (0));
    p.lineTo (x (a), y (1));
    p.quadraticTo (x (a + d * 0.3f), y (S + (1 - S) * 0.15f), x (a + d), y (S));
    p.lineTo (x (a + d + hold), y (S));
    p.quadraticTo (x (a + d + hold + rel * 0.3f), y (S * 0.15f), x (total), y (0));
    juce::Path fill (p);
    fill.lineTo (x (0), y (0));
    fill.closeSubPath();
    g.setColour (accent.withAlpha (0.12f));
    g.fillPath (fill);
    g.setColour (accent.withAlpha (0.25f));
    g.strokePath (p, juce::PathStrokeType (4.0f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
    g.setColour (accent);
    g.strokePath (p, juce::PathStrokeType (1.6f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));

    auto fmt = [] (float s) { return s < 1.0f ? juce::String (juce::roundToInt (s * 1000)) + "ms" : juce::String (s, 2) + "s"; };
    g.setFont (frui::mono (10.0f, false));
    g.setColour (frui::colours::textDim);
    g.drawText ("A " + fmt (A) + "  D " + fmt (D) + "  S " + juce::String (juce::roundToInt (S * 100)) + "%  R " + fmt (R),
                r.reduced (8.0f, 2.0f).removeFromTop (14.0f), juce::Justification::centredRight, false);
}

void NeonEditor::showTab (int index)
{
    tab = juce::jlimit (0, tabs.size() - 1, index);
    tabs[tab]->setToggleState (true, juce::dontSendNotification);
    for (size_t t = 0; t < sections.size(); ++t)
        for (auto& s : sections[t])
            s->panel->setVisible ((int) t == tab);
    resized();
    repaint();
}

void NeonEditor::paint (juce::Graphics& g)
{
    namespace c = frui::colours;
    frui::accentRef() = accent;
    g.fillAll (c::bg);
    const int headH = tabs.isEmpty() ? 40 : tabs[0]->getBottom() + 4;
    g.setGradientFill (juce::ColourGradient (juce::Colour (0xff17191c), 0.0f, 0.0f, juce::Colour (0xff101215), 0.0f, (float) headH, false));
    g.fillRect (0, 0, getWidth(), headH);
    g.setColour (juce::Colours::black);
    g.drawHorizontalLine (headH - 1, 0.0f, (float) getWidth());

    g.setFont (frui::sans (juce::jlimit (12.0f, 16.0f, (float) logoArea.getHeight() * 0.5f)));
    frui::drawNeon (g, brand.toUpperCase(), logoArea.toFloat(), getWidth() < 640 ? 0.5f : 3.0f, juce::Justification::left, accent);

    for (int i = 0; i < tabs.size(); ++i)
    {
        auto b = tabs[i]->getBounds().toFloat();
        const bool narrow = b.getWidth() < 80.0f;
        g.setFont (frui::sans (narrow ? 10.0f : juce::jlimit (10.0f, 13.0f, b.getHeight() * 0.42f)));
        if (i == tab)
        {
            frui::drawNeon (g, tabs[i]->getButtonText(), b, narrow ? 0.3f : 1.6f, juce::Justification::centred, accent);
            g.setColour (accent);
            g.fillRect (b.getX() + 6.0f, b.getBottom() + 1.0f, b.getWidth() - 12.0f, 2.0f);
        }
        else
        {
            g.setColour (c::textDim);
            frui::drawSpaced (g, tabs[i]->getButtonText(), b, narrow ? 0.3f : 1.6f, juce::Justification::centred);
        }
    }
}

void NeonEditor::layoutSection (Section& s)
{
    auto c = s.panel->content();
    bool anyKnob = false;
    for (auto* item : s.items) anyKnob = anyKnob || dynamic_cast<frui::Knob*> (item) != nullptr;
    if (s.view != nullptr && ! s.items.empty() && ! anyKnob)
    {
        // solo selettori: in fila sopra, il disegno prende tutto il resto (a tutta larghezza)
        const int barH = juce::jlimit (22, 30, c.getHeight() / 5);
        for (auto* item : s.items)
        {
            item->setBounds (c.removeFromTop (barH).withSizeKeepingCentre (juce::jmin (c.getWidth(), 360), barH));
            c.removeFromTop (5);
        }
        s.view->setBounds (c.reduced (0, 2));
        return;
    }
    if (s.view != nullptr)
    {
        if (s.items.empty())
        {
            s.view->setBounds (c);
            return;
        }
        if (s.viewLeft && c.getWidth() > 300)
        {
            s.view->setBounds (c.removeFromLeft ((int) ((float) c.getWidth() * s.viewShare)).reduced (0, 2));
            c.removeFromLeft (8);
        }
        else if (c.getHeight() < 150 && c.getWidth() > 420)
        {
            // pannello basso (telefono con la tastiera aperta): disegno a sinistra, controlli a destra
            const int vw = (int) ((float) c.getWidth() * 0.55f);
            s.view->setBounds (c.removeFromLeft (vw).withSizeKeepingCentre (vw, juce::jmin (c.getHeight(), 64)).reduced (0, 2));
            c.removeFromLeft (8);
        }
        else
        {
            s.view->setBounds (c.removeFromTop ((int) ((float) c.getHeight() * s.viewShare)).reduced (0, 2));
            c.removeFromTop (4);
        }
    }
    std::vector<juce::Component*> knobs;
    const int barH = juce::jlimit (22, 30, c.getHeight() / 6);
    for (auto* item : s.items)
    {
        if (dynamic_cast<frui::Knob*> (item) != nullptr) { knobs.push_back (item); continue; }
        item->setBounds (c.removeFromTop (barH).withSizeKeepingCentre (juce::jmin (c.getWidth(), 260), barH));
        c.removeFromTop (5);
    }
    const int n = (int) knobs.size();
    if (n == 0) return;
    int cols = s.columns;
    if (cols <= 0)
    {
        double bestScore = 1e9;
        for (int k = 1; k <= n; ++k)
        {
            const int rows = (n + k - 1) / k;
            const double cell = ((double) c.getWidth() / k) / ((double) c.getHeight() / rows);
            const double score = std::abs (std::log (cell / 0.95)) + 0.4 * (double) (k * rows - n) / (double) k;
            if (score < bestScore) { bestScore = score; cols = k; }
        }
    }
    const int rows = (n + cols - 1) / cols;
    const int cw = c.getWidth() / cols, ch = c.getHeight() / rows;
    for (int i = 0; i < n; ++i)
    {
        const int row = i / cols, col = i % cols;
        const int inRow = juce::jmin (cols, n - row * cols);
        const int offset = (c.getWidth() - inRow * cw) / 2;
        knobs[(size_t) i]->setBounds (juce::Rectangle<int> (c.getX() + offset + col * cw, c.getY() + row * ch, cw, ch).reduced (2));
    }
}

void NeonEditor::resized()
{
    auto b = getLocalBounds();
    const int headH = getHeight() < 330 ? 34 : 40;
    auto head = b.removeFromTop (headH);
    logoArea = head.removeFromLeft (getWidth() < 640 ? 108 : 190).withTrimmedLeft (10);
    const int tw = juce::jmin (100, head.getWidth() / juce::jmax (1, tabs.size()));
    for (auto* t : tabs)
        t->setBounds (head.removeFromLeft (tw).reduced (2, 6));

    auto area = b.reduced (6);
    const bool landscape = area.getWidth() > area.getHeight() * 1.25f;
    juce::Rectangle<int> side = landscape ? area.removeFromRight (juce::jlimit (118, 200, area.getWidth() / 5))
                                          : area.removeFromTop (juce::jlimit (110, 150, area.getHeight() / 4));
    auto first = landscape ? side.removeFromTop (side.getHeight() / 2) : side.removeFromLeft (side.getWidth() / 2);
    bigPanel[0].setBounds (first.reduced (3));
    bigPanel[1].setBounds (side.reduced (3));
    for (int k = 0; k < 2; ++k)
    {
        auto c = bigPanel[k].getLocalBounds().reduced (6);
        if (bigCaption[k] != nullptr)
        {
            const int capH = juce::jlimit (16, 24, c.getHeight() / 6);
            bigCaption[k]->setBounds (c.removeFromBottom (capH).withSizeKeepingCentre (juce::jmin (c.getWidth() - 8, 140), capH));
            c.removeFromBottom (3);
        }
        bigKnob[k]->setBounds (c);
    }

    std::vector<Section*> list;
    for (auto& s : sections[(size_t) tab]) list.push_back (s.get());
    if (list.empty()) return;

    // righe (una sola, salvo pannelli con row > 0)
    int numRows = 0;
    for (auto* s : list) numRows = juce::jmax (numRows, s->row + 1);
    std::vector<float> rowW ((size_t) numRows, 0.0f);
    for (auto* s : list) rowW[(size_t) s->row] = juce::jmax (rowW[(size_t) s->row], s->rowWeight);
    float rowTotal = 0.0f;
    for (auto w : rowW) rowTotal += w;
    int y = area.getY();
    for (int r = 0; r < numRows; ++r)
    {
        const int h = r + 1 == numRows ? area.getBottom() - y : juce::roundToInt ((float) area.getHeight() * rowW[(size_t) r] / juce::jmax (0.001f, rowTotal));
        const juce::Rectangle<int> rowArea (area.getX(), y, area.getWidth(), h);
        y += h;
        std::vector<Section*> inRow;
        for (auto* s : list) if (s->row == r) inRow.push_back (s);
        if (inRow.empty()) continue;
        float total = 0.0f;
        for (auto* s : inRow) total += s->weight;
        if (numRows > 1 || rowArea.getWidth() > 520 || inRow.size() <= 2)
        {
            int x = rowArea.getX();
            for (size_t i = 0; i < inRow.size(); ++i)
            {
                const int w = i + 1 == inRow.size() ? rowArea.getRight() - x : juce::roundToInt ((float) rowArea.getWidth() * inRow[i]->weight / total);
                inRow[i]->panel->setBounds (juce::Rectangle<int> (x, rowArea.getY(), w, rowArea.getHeight()).reduced (3));
                x += w;
            }
        }
        else
        {
            const int rows = ((int) inRow.size() + 1) / 2;
            const int hh = rowArea.getHeight() / rows, w = rowArea.getWidth() / 2;
            for (size_t i = 0; i < inRow.size(); ++i)
                inRow[i]->panel->setBounds (juce::Rectangle<int> (rowArea.getX() + (int) (i % 2) * w, rowArea.getY() + (int) (i / 2) * hh, w, hh).reduced (3));
        }
    }
    for (auto* s : list)
        layoutSection (*s);
}
