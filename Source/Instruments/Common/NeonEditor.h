#pragma once

#include <JuceHeader.h>
#include "../FrenchRoulette/FrenchRouletteEditor.h"   // i controlli "neon" (frui::Knob, Segment, Lcd, Panel)

//==============================================================================
/**
    Base delle interfacce "neon" (MegaSaw, WubForge, RoboVox), nello stile di French Roulette:
    logo luminoso e schede in alto, due knob grandi sempre visibili a destra, pannelli con manopole.
    Ogni pannello puo' avere un disegno (view) in alto e righe di selettori sopra le manopole.
*/
class NeonEditor : public juce::AudioProcessorEditor,
                   private juce::Timer
{
public:
    struct BigKnob { juce::String id, title, caption, lcdParam; };

    NeonEditor (juce::AudioProcessor&, juce::AudioProcessorValueTreeState&, const juce::String& brand, juce::Colour accent,
                const juce::StringArray& tabNames, BigKnob first, BigKnob second);
    ~NeonEditor() override;

    void paint (juce::Graphics&) override;
    void resized() override;
    void showTab (int index);

protected:
    struct Section
    {
        std::unique_ptr<frui::Panel> panel;
        std::vector<juce::Component*> items;
        juce::Component* view = nullptr;
        float viewShare = 0.4f, weight = 1.0f;
        int columns = 0;
    };

    Section& section (int tab, const juce::String& title, float weight = 1.0f, int columns = 0);
    void knob (Section&, const juce::String& id, const juce::String& title);
    void segment (Section&, const juce::String& id, juce::StringArray labels = {});
    void lcd (Section&, const juce::String& id, const juce::String& prefix = {});
    juce::Component& view (Section&, std::unique_ptr<juce::Component>, float share = 0.4f);

    /** 15 volte al secondo: le viste che dipendono dai parametri si ridisegnano. */
    virtual void refreshViews() {}

    juce::AudioProcessorValueTreeState& apvts;
    juce::Colour accent;

private:
    void timerCallback() override       { refreshViews(); }
    void layoutSection (Section&);

    frui::LookAndFeel laf;
    juce::String brand;
    juce::OwnedArray<juce::TextButton> tabs;
    std::vector<std::vector<std::unique_ptr<Section>>> sections;
    std::vector<std::unique_ptr<juce::Component>> owned;
    frui::Panel bigPanel[2] { frui::Panel (""), frui::Panel ("") };
    std::unique_ptr<frui::Knob> bigKnob[2];
    std::unique_ptr<juce::Component> bigCaption[2];
    int tab = 0;
    juce::Rectangle<int> logoArea;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (NeonEditor)
};
