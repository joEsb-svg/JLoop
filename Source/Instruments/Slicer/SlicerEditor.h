#pragma once

#include <JuceHeader.h>
#include "Slicer.h"
#include "../Common/ForgeEditorBase.h"

//==============================================================================
/**
    Interfaccia dello Slicer: la forma d'onda con i tagli (tocca un pezzo per ascoltarlo),
    il suono da tagliare (libreria o file importato) e i controlli del taglio.
*/
class SlicerEditor : public ForgeEditorBase
{
public:
    explicit SlicerEditor (SlicerAudioProcessor&);
    ~SlicerEditor() override;

    void showSampleMenu();

private:
    class WaveView;
    void refreshViews() override;
    void importSample();

    SlicerAudioProcessor& proc;
    WaveView* wave = nullptr;
    juce::TextButton* sampleButton = nullptr;
    std::unique_ptr<juce::FileChooser> chooser;
};
