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
        float viewShare = 0.4f, weight = 1.0f, rowWeight = 1.0f;
        int columns = 0, row = 0;
        bool viewLeft = false;   // disegno a sinistra dei controlli (invece che sopra)
    };

    /** Un pannello nella scheda 'tab'. I pannelli della stessa riga stanno affiancati, le righe una sotto l'altra
        (altezza in proporzione a rowWeight). */
    Section& section (int tab, const juce::String& title, float weight = 1.0f, int columns = 0, int row = 0, float rowWeight = 1.0f);
    void knob (Section&, const juce::String& id, const juce::String& title);
    void segment (Section&, const juce::String& id, juce::StringArray labels = {});
    void lcd (Section&, const juce::String& id, const juce::String& prefix = {});
    juce::Component& view (Section&, std::unique_ptr<juce::Component>, float share = 0.4f);
    /** Scelta: segmenti se le voci sono poche e corte, altrimenti display ◂ ▸. */
    void choice (Section&, const juce::String& id, const juce::String& prefix = {});
    /** Disegno di un inviluppo ADSR (letto dai parametri), come vista del pannello. */
    void adsr (Section&, const juce::String& a, const juce::String& d, const juce::String& s, const juce::String& r, float share = 0.4f);

    /** Disegno ADSR a neon. */
    class AdsrView : public juce::Component
    {
    public:
        AdsrView (juce::AudioProcessorValueTreeState&, juce::StringArray ids);
        void paint (juce::Graphics&) override;
        void refresh();
    private:
        float value (int i) const;
        juce::AudioProcessorValueTreeState& apvts;
        juce::StringArray ids;
        juce::uint64 shown = 0;
    };

    /** 15 volte al secondo: le viste che dipendono dai parametri si ridisegnano. */
    virtual void refreshViews() {}

    juce::AudioProcessorValueTreeState& apvts;
    juce::Colour accent;

private:
    void timerCallback() override       { for (auto* a : adsrViews) if (a->isShowing()) a->refresh(); refreshViews(); }
    std::vector<AdsrView*> adsrViews;
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
