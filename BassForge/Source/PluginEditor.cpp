#include "PluginEditor.h"

namespace ui = bf::ui;

//==============================================================================
BassForgeAudioProcessorEditor::MacroKnob::MacroKnob(juce::AudioProcessorValueTreeState& s, int index) : apvts(s), macro(index)
{
    slider.setSliderStyle(juce::Slider::RotaryHorizontalVerticalDrag);
    slider.setRotaryParameters(juce::MathConstants<float>::pi * 1.25f, juce::MathConstants<float>::pi * 2.75f, true);
    slider.setTextBoxStyle(juce::Slider::NoTextBox, false, 0, 0);
    addAndMakeVisible(slider);
    attachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(apvts, bf::ids::macro(index), slider);
    refresh();
}

void BassForgeAudioProcessorEditor::MacroKnob::refresh()
{
    // Dove va questa macro: le connessioni della matrice che partono da lei
    juce::StringArray dests;
    for (int i = 0; i < bf::kNumSlots; ++i)
    {
        const int src = (int) apvts.getRawParameterValue(bf::ids::slot(i, "src"))->load();
        const float amt = apvts.getRawParameterValue(bf::ids::slot(i, "amt"))->load();
        const int dst = (int) apvts.getRawParameterValue(bf::ids::slot(i, "dst"))->load();
        if (src == bf::SrcMacro1 + macro && std::abs(amt) > 0.001f && dst > bf::DstNone)
            dests.addIfNotAlreadyThere(juce::String(bf::modDestName(dst)).toUpperCase());
    }
    const auto text = dests.isEmpty() ? juce::String("LIBERA (VEDI MOD)")
                    : juce::String(juce::CharPointer_UTF8("\xe2\x86\x92 ")) + dests[0] + (dests.size() > 1 ? " +" + juce::String(dests.size() - 1) : juce::String());
    if (text != caption) { caption = text; repaint(); }
}

void BassForgeAudioProcessorEditor::MacroKnob::resized()
{
    auto r = getLocalBounds().reduced(6);
    captionArea = r.removeFromBottom(juce::jlimit(16, 22, r.getHeight() / 7)).withSizeKeepingCentre(juce::jmin(r.getWidth(), 150), juce::jlimit(16, 22, r.getHeight() / 7));
    r.removeFromBottom(3);
    nameArea = r.removeFromBottom(16);
    slider.setBounds(r);
}

void BassForgeAudioProcessorEditor::MacroKnob::paint(juce::Graphics& g)
{
    auto r = getLocalBounds().toFloat().reduced(1.0f);
    g.setGradientFill(juce::ColourGradient(juce::Colour(0xff1b1e22), 0.0f, r.getY(), juce::Colour(0xff14171a), 0.0f, r.getBottom(), false));
    g.fillRoundedRectangle(r, 9.0f);
    g.setColour(ui::col::edge);
    g.drawRoundedRectangle(r, 9.0f, 1.0f);
    g.setColour(ui::col::textDim);
    g.setFont(ui::spacedFont(11.0f));
    g.drawText("MACRO " + juce::String(macro + 1), nameArea, juce::Justification::centred, false);
    auto c = captionArea.toFloat();
    g.setColour(ui::col::lcdBg);
    g.fillRoundedRectangle(c, 4.0f);
    g.setColour(ui::col::edge);
    g.drawRoundedRectangle(c, 4.0f, 1.0f);
    g.setFont(ui::monoFont(juce::jlimit(9.0f, 12.0f, c.getHeight() * 0.6f)));
    ui::drawGlowText(g, caption, captionArea, juce::Justification::centred, ui::col::cyan.brighter(0.2f));
}

//==============================================================================

BassForgeAudioProcessorEditor::BassForgeAudioProcessorEditor(BassForgeAudioProcessor& p)
    : juce::AudioProcessorEditor(&p), proc(p),
      ring(p.apvts, p.getModMonitor()),
      scope(p.getScope()),
      masterBar(p.apvts, bf::ids::masterGain),
      presetBar(p.getPresets()),
      oscPage(p.apvts, ring, p.getWavetables(), p.getModMonitor()),
      filterPage(p.apvts, ring),
      modPage(p.apvts, ring, p.getPerformers()),
      fxPage(p.apvts, ring),
      macroKnobs { MacroKnob(p.apvts, 0), MacroKnob(p.apvts, 1) }
{
    setLookAndFeel(&laf);

    const char* names[4] = { "OSCILLATORS", "FILTERS", "MODULATORS", "FX" };
    for (int i = 0; i < 4; ++i)
    {
        tabs[i].setButtonText(names[i]);
        tabs[i].setClickingTogglesState(true);
        tabs[i].setRadioGroupId(1001);
        tabs[i].onClick = [this, i] { showPage(i); };
        addAndMakeVisible(tabs[i]);
    }
    addAndMakeVisible(scope);
    masterLabel.setText("MASTER", juce::dontSendNotification);
    masterLabel.setJustificationType(juce::Justification::centredRight);
    masterLabel.setColour(juce::Label::textColourId, ui::col::textDim);
    addAndMakeVisible(masterLabel);
    addAndMakeVisible(masterBar);
    limiterBtn.setClickingTogglesState(true);
    limiterBtn.setTooltip("Master limiter (brickwall -0.3 dBFS)");
    limiterAtt = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment>(p.apvts, bf::ids::masterLimiter, limiterBtn);
    addAndMakeVisible(limiterBtn);
    addAndMakeVisible(presetBar);
    for (auto& m : macroKnobs) addChildComponent(m);

    pages[0] = &oscPage; pages[1] = &filterPage; pages[2] = &modPage; pages[3] = &fxPage;
    for (auto* pg : pages) addChildComponent(*pg);
    tabs[0].setToggleState(true, juce::dontSendNotification);
    showPage(0);

    constrainer.setFixedAspectRatio((double) kBaseW / (double) kBaseH);
    constrainer.setSizeLimits(kBaseW * 3 / 4, kBaseH * 3 / 4, kBaseW * 3 / 2, kBaseH * 3 / 2);
    setConstrainer(&constrainer);
    setResizable(true, true);
    setSize(kBaseW, kBaseH);

    startTimerHz(30);
}

BassForgeAudioProcessorEditor::~BassForgeAudioProcessorEditor()
{
    stopTimer();
    setLookAndFeel(nullptr);
}

void BassForgeAudioProcessorEditor::timerCallback()
{
    ring.update();
    if (++refreshCounter >= 15)
    {
        refreshCounter = 0;
        for (auto& m : macroKnobs) if (m.isVisible()) m.refresh();
    }
}

bool BassForgeAudioProcessorEditor::macrosShown() const
{
    return compact && getWidth() >= 640 && getWidth() > getHeight();
}

void BassForgeAudioProcessorEditor::showPage(int index)
{
    current = juce::jlimit(0, 3, index);
    for (int i = 0; i < 4; ++i) pages[i]->setVisible(i == current);
}

void BassForgeAudioProcessorEditor::setCompactLayout(bool c)
{
    if (compact == c) return;
    compact = c;
    const char* full[4]  = { "OSCILLATORS", "FILTERS", "MODULATORS", "FX" };
    const char* brief[4] = { "OSC", "FILTER", "MOD", "FX" };
    for (int i = 0; i < 4; ++i) tabs[i].setButtonText(c ? brief[i] : full[i]);

    if (c)
    {
        setResizable(false, false);
        setConstrainer(nullptr);          // dimensione decisa dall'host (schermo del telefono)
    }
    else
    {
        setConstrainer(&constrainer);
        setResizable(true, true);
    }
    oscPage.setCompact(c); filterPage.setCompact(c); modPage.setCompact(c); fxPage.setCompact(c);
    resized();
    repaint();
}

void BassForgeAudioProcessorEditor::showPageSection(int page, int section, int view)
{
    page = juce::jlimit(0, 3, page);
    tabs[page].setToggleState(true, juce::dontSendNotification);
    showPage(page);
    if (page == 2 && view >= 0) modPage.selectView(view);
    static_cast<bf::ui::PageBase*>(pages[page])->showSection(section);
}

int BassForgeAudioProcessorEditor::compactHeaderHeight() const
{
    return getWidth() < 600 ? 76 : 40;    // verticale: due righe (logo + master, poi schede)
}

void BassForgeAudioProcessorEditor::paint(juce::Graphics& g)
{
    if (compact)
    {
        g.fillAll(ui::col::bg);
        auto head = getLocalBounds().removeFromTop(compactHeaderHeight());
        g.setGradientFill(juce::ColourGradient(juce::Colour(0xff17191c), 0.0f, 0.0f, juce::Colour(0xff101215), 0.0f, (float) head.getHeight(), false));
        g.fillRect(head);
        g.setColour(ui::col::edge);
        g.drawHorizontalLine(head.getBottom() - 1, 0.0f, (float) getWidth());
        auto logo = head.removeFromTop(38).withWidth(124).translated(10, 0);
        g.setFont(juce::Font(juce::FontOptions(16.0f, juce::Font::bold).withKerningFactor(0.18f)));
        ui::drawGlowText(g, "BASSFORGE", logo, juce::Justification::centredLeft, ui::col::cyan);
        return;
    }

    g.fillAll(ui::col::bg);
    auto head = getLocalBounds().removeFromTop(getHeight() / 11);
    g.setGradientFill(juce::ColourGradient(ui::col::panelHi, 0.0f, 0.0f, ui::col::bg, 0.0f, (float) head.getHeight(), false));
    g.fillRect(head);
    g.setColour(ui::col::edge);
    g.drawHorizontalLine(head.getBottom() - 1, 0.0f, (float) getWidth());

    const float s = (float) getHeight() / (float) kBaseH;
    g.setColour(ui::col::text);
    g.setFont(juce::Font(juce::FontOptions(26.0f * s, juce::Font::bold)));
    g.drawText("BASS", (int) (18 * s), 0, (int) (80 * s), head.getHeight(), juce::Justification::centredLeft);
    g.setColour(ui::col::cyan);
    g.drawText("FORGE", (int) (84 * s), 0, (int) (110 * s), head.getHeight(), juce::Justification::centredLeft);
    g.setColour(ui::col::orange);
    g.fillRoundedRectangle(18 * s, (float) head.getBottom() - 6.0f * s, 176.0f * s, 2.5f * s, 1.2f * s);
}

void BassForgeAudioProcessorEditor::resized()
{
    if (compact)
    {
        auto b = getLocalBounds();
        auto head = b.removeFromTop(compactHeaderHeight());
        const bool twoRows = head.getHeight() > 50;
        auto row1 = head.removeFromTop(38).reduced(8, 6);
        row1.removeFromLeft(124);                                      // logo
        masterBar.setBounds(row1.removeFromRight(twoRows ? 96 : 120).withSizeKeepingCentre(twoRows ? 96 : 120, 22));
        masterLabel.setVisible(! twoRows);
        if (! twoRows) masterLabel.setBounds(row1.removeFromRight(56));
        limiterBtn.setBounds(row1.removeFromRight(56).withSizeKeepingCentre(52, 24));
        row1.removeFromRight(6);
        auto tabRow = twoRows ? head.reduced(8, 3) : row1;
        if (! twoRows)
        {
            scope.setBounds(row1.removeFromRight(juce::jmin(150, juce::jmax(0, row1.getWidth() - 4 * 70))).reduced(0, 1));
            row1.removeFromRight(8);
            tabRow = row1;
        }
        else scope.setBounds(row1.reduced(0, 1));
        const int tw = juce::jmin(96, tabRow.getWidth() / 4);
        for (auto& t : tabs) t.setBounds(tabRow.removeFromLeft(tw).reduced(1, 0));

        if (presetBar.isVisible())
            presetBar.setBounds(b.removeFromTop(32).reduced(8, 2).withWidth(juce::jmin(500, b.getWidth() - 16)));
        const bool macros = macrosShown();
        for (auto& m : macroKnobs) m.setVisible(macros);
        if (macros)
        {
            // colonna a destra: le due macro grandi (come i knob grandi degli altri synth neon)
            auto side = b.removeFromRight(juce::jlimit(120, 180, b.getWidth() / 6)).reduced(4, 6);
            macroKnobs[0].setBounds(side.removeFromTop(side.getHeight() / 2).reduced(0, 2));
            macroKnobs[1].setBounds(side.reduced(0, 2));
        }
        for (auto* pg : pages) pg->setBounds(b);
        return;
    }
    masterLabel.setVisible(true);
    for (auto& m : macroKnobs) m.setVisible(false);

    auto b = getLocalBounds();
    auto head = b.removeFromTop(getHeight() / 11);
    const float s = (float) getHeight() / (float) kBaseH;

    auto h = head.reduced((int) (12 * s), (int) (6 * s));
    h.removeFromLeft((int) (200 * s));                         // logo
    masterBar.setBounds(h.removeFromRight((int) (150 * s)).withSizeKeepingCentre((int) (150 * s), (int) (26 * s)));
    masterLabel.setBounds(h.removeFromRight((int) (58 * s)));
    limiterBtn.setBounds(h.removeFromRight((int) (62 * s)).withSizeKeepingCentre((int) (56 * s), (int) (24 * s)));
    h.removeFromRight((int) (10 * s));
    scope.setBounds(h.removeFromRight((int) (190 * s)).reduced(0, (int) (2 * s)));
    h.removeFromRight((int) (14 * s));
    for (auto& t : tabs) t.setBounds(h.removeFromLeft((int) (140 * s)));

    auto sub = b.removeFromTop((int) (34 * s)).reduced((int) (14 * s), (int) (2 * s));
    presetBar.setBounds(sub.removeFromLeft((int) (500 * s)));
    for (auto* pg : pages) pg->setBounds(b);
}
