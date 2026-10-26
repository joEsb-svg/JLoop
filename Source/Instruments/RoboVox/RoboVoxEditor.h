#pragma once

#include <JuceHeader.h>
#include "RoboVox.h"
#include "../Common/NeonEditor.h"

//==============================================================================
/**
    Interfaccia di ROBOVOX: viola neon. Schede VOCE / PORTANTE / FX, knob grandi FORMANTE e MIX.
    VOCE: da dove viene la voce (libreria, file, microfono) e le bande che si accendono mentre parla.
*/
class RoboVoxEditor : public NeonEditor
{
public:
    explicit RoboVoxEditor (RoboVoxAudioProcessor&);
    ~RoboVoxEditor() override;
    void resized() override;

    void showLibraryMenu();
    void importFile();
    void openRecorder();
    void stepLibrary (int delta);
    void closeRecorder();

private:
    class SourceView;
    class BandsView;
    class Recorder;
    void refreshViews() override;

    RoboVoxAudioProcessor& proc;
    SourceView* source = nullptr;
    BandsView* bands = nullptr;
    std::unique_ptr<Recorder> recorder;
    std::unique_ptr<juce::FileChooser> chooser;
};
