#pragma once

#include <JuceHeader.h>
#include "FrenchRoulette.h"

//==============================================================================
/**
    Interfaccia di FRENCH ROULETTE: rosso neon da rack anni '80, schermo della matrice verde fosforo.

    - in alto il logo e le schede SUONO / FILTRO / LFO-FX / MATRIX / CRUNCH
    - a destra, sempre visibili, i due knob grandi CRUNCH e PUMP
    - MATRIX: 16 step con le corsie GATE, CUTOFF, CRUSH, PITCH (+ glide), e RANDOM CHOP
*/
namespace frui
{
    namespace colours
    {
        const juce::Colour bg       { 0xff0d1014 };
        const juce::Colour panelTop { 0xff1b1e22 };
        const juce::Colour panelBot { 0xff14171a };
        const juce::Colour edge     { 0xff050607 };
        const juce::Colour red      { 0xffff3030 };
        const juce::Colour redDim   { 0xff5a1414 };
        const juce::Colour lcdBg    { 0xff1a0505 };
        const juce::Colour green    { 0xff39ff7a };
        const juce::Colour screen   { 0xff040806 };
        const juce::Colour cell     { 0xff07120b };
        const juce::Colour text     { 0xffd2d8de };
        const juce::Colour textDim  { 0xff7a848e };
        const juce::Colour track    { 0xff262b31 };
    }

    juce::Font mono (float size, bool bold = true);
    juce::Font sans (float size, bool bold = true);

    /** Look & feel: manopole con arco rosso luminoso. */
    class LookAndFeel : public juce::LookAndFeel_V4
    {
    public:
        LookAndFeel();
        void drawRotarySlider (juce::Graphics&, int x, int y, int w, int h, float pos, float a0, float a1, juce::Slider&) override;
        juce::Slider::SliderLayout getSliderLayout (juce::Slider&) override;
        juce::Label* createSliderTextBox (juce::Slider&) override;
        void drawPopupMenuBackground (juce::Graphics&, int w, int h) override;
        juce::Font getPopupMenuFont() override   { return sans (15.0f, false); }
        // Le schede (unici TextButton) sono disegnate dall'editor
        void drawButtonBackground (juce::Graphics&, juce::Button&, const juce::Colour&, bool, bool) override {}
        void drawButtonText (juce::Graphics&, juce::TextButton&, bool, bool) override {}
    };

    /** Pannello con titolo (barretta rossa). */
    class Panel : public juce::Component
    {
    public:
        explicit Panel (juce::String t) : title (std::move (t)) { setInterceptsMouseClicks (false, true); }
        void paint (juce::Graphics&) override;
        juce::Rectangle<int> content() const;
        bool showTitle = true;

    private:
        juce::String title;
    };

    /** Manopola con nome e valore, collegata a un parametro. */
    class Knob : public juce::Component
    {
    public:
        Knob (juce::AudioProcessorValueTreeState&, const juce::String& id, const juce::String& title, bool big = false);
        void resized() override;
        void paint (juce::Graphics&) override;
        juce::Slider slider;

    private:
        juce::String title;
        juce::Rectangle<int> labelArea;
        bool big;
        std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> attachment;
    };

    /** Selettore a segmenti (SAW | SQUARE | PULSE) per un parametro a scelta o booleano. */
    class Segment : public juce::Component
    {
    public:
        Segment (juce::AudioProcessorValueTreeState&, const juce::String& id, juce::StringArray labels = {}, juce::Colour on = colours::red);
        void paint (juce::Graphics&) override;
        void mouseDown (const juce::MouseEvent&) override;

    private:
        juce::RangedAudioParameter& param;
        juce::StringArray labels;
        juce::Colour onColour;
        int value = 0;
        juce::ParameterAttachment attachment;
    };

    /** Displayino rosso stile LCD con ◂ ▸: scorre le scelte di un parametro (es. la tabella di OSC 2). */
    class Lcd : public juce::Component
    {
    public:
        Lcd (juce::AudioProcessorValueTreeState&, const juce::String& id, juce::String prefix = {});
        void paint (juce::Graphics&) override;
        void mouseDown (const juce::MouseEvent&) override;

    private:
        juce::RangedAudioParameter& param;
        juce::String prefix;
        int value = 0;
        juce::ParameterAttachment attachment;
    };

    /** Lo schermo della matrice: 16 step, una corsia alla volta. */
    class MatrixView : public juce::Component
    {
    public:
        MatrixView (FrenchRouletteAudioProcessor&);
        void paint (juce::Graphics&) override;
        void resized() override;
        void mouseDown (const juce::MouseEvent&) override;
        void mouseDrag (const juce::MouseEvent&) override;
        void mouseUp (const juce::MouseEvent&) override;

        /** Ridisegna se qualcosa e' cambiato (passo che suona, valori). */
        void refresh();
        int lane = 1;   // 0 gate, 1 cutoff, 2 crush, 3 pitch

    private:
        float get (char laneId, int step) const;
        void set (char laneId, int step, float value);
        void edit (juce::Point<float> p, bool first);
        int stepAt (float x) const;
        juce::uint64 signature() const;

        FrenchRouletteAudioProcessor& proc;
        juce::Rectangle<float> laneTabs, chopButton, gateRow, barArea, labelRow, crushRow;
        std::array<juce::Rectangle<float>, 4> laneButton;
        int dragMode = 0;               // 0 niente, 1 barre, 2 gate, 3 glide
        float gateDragValue = 1.0f;
        int lastEditStep = -1, shownStep = -2;
        juce::uint64 shown = 0;
        bool chopFlash = false;
        std::vector<juce::RangedAudioParameter*> gesture;
    };
}

//==============================================================================
class FrenchRouletteEditor : public juce::AudioProcessorEditor,
                             private juce::Timer
{
public:
    explicit FrenchRouletteEditor (FrenchRouletteAudioProcessor&);
    ~FrenchRouletteEditor() override;

    void paint (juce::Graphics&) override;
    void resized() override;
    void showTab (int index);

private:
    void timerCallback() override;

    struct Section
    {
        std::unique_ptr<frui::Panel> panel;
        std::vector<juce::Component*> items;
        int columns = 0;     // 0 = automatico
        float weight = 1.0f;
    };

    Section& section (int tabIndex, const juce::String& title, float weight = 1.0f, int columns = 0);
    frui::Knob& knob (Section&, const juce::String& id, const juce::String& title);
    frui::Segment& segment (Section&, const juce::String& id, juce::StringArray labels = {});
    frui::Lcd& lcd (Section&, const juce::String& id, const juce::String& prefix = {});
    void layoutSection (Section&);

    FrenchRouletteAudioProcessor& proc;
    frui::LookAndFeel laf;
    juce::OwnedArray<juce::TextButton> tabs;
    std::vector<std::vector<std::unique_ptr<Section>>> sections;
    std::vector<std::unique_ptr<juce::Component>> owned;

    // Colonna destra (sempre visibile)
    frui::Panel crunchPanel { "" }, pumpPanel { "" };
    std::unique_ptr<frui::Knob> crunchKnob, pumpKnob;
    std::unique_ptr<frui::Lcd> pumpLcd;
    std::unique_ptr<juce::Component> crunchCap;

    // Matrice
    frui::Panel matrixPanel { "" };
    std::unique_ptr<frui::MatrixView> matrix;
    std::vector<juce::Component*> matrixControls;

    int tab = 3;
    juce::Rectangle<int> logoArea, ledArea;
    bool ledOn = false;
    int lastStep = -1;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (FrenchRouletteEditor)
};
