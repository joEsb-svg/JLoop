#pragma once

#include <JuceHeader.h>
#include "Materia.h"
#include "../FrenchRoulette/FrenchRouletteEditor.h"   // frui::LookAndFeel (menu) e i font

//==============================================================================
/**
    Interfaccia di MATERIA.
    - Schermo stretto (telefono in verticale): la "catena". Tutti i blocchi in una lista (SORGENTI, FORMA, MOVIMENTO);
      si tocca un blocco e le sue manopole si aprono nel pannello in basso.
    - Schermo largo (PC, tablet, telefono in orizzontale): tre colonne coi blocchi gia' aperti.
    I blocchi si aggiungono con "+ aggiungi" e si tolgono con la X.
*/
class MateriaEditor : public juce::AudioProcessorEditor,
                      private juce::Timer
{
public:
    explicit MateriaEditor (MateriaAudioProcessor&);
    ~MateriaEditor() override;
    void paint (juce::Graphics&) override;
    void resized() override;

    /** Gruppi: 0 sorgenti, 1 forma, 2 movimento, 3 volume (blocco fisso). */
    void selectBlock (int group, int slot);
    void addBlock (int group, juce::Component* anchor);
    void removeBlock (int group, int slot);
    void moveBlock (int slot, int delta);
    void rebuild();
    MateriaAudioProcessor& processor;

private:
    void timerCallback() override;
    juce::String signature() const;
    int typeOf (int group, int slot) const;

    frui::LookAndFeel laf;
    juce::Viewport listView, columnView[3];
    juce::Component listContent, columnContent[3];
    std::vector<std::pair<juce::String, juce::Rectangle<int>>> headers[4];   // titoli delle sezioni (3 colonne + lista)
    juce::OwnedArray<juce::Component> titleComps, rows, panels;
    juce::OwnedArray<juce::Button> addButtons;
    std::unique_ptr<juce::Component> detail;
    juce::String shownSignature;
    bool wide = false, built = false;
    int selGroup = 0, selSlot = 0;
    juce::Rectangle<int> logoArea, detailArea;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (MateriaEditor)
};
