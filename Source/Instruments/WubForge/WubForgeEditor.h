#pragma once

#include <JuceHeader.h>
#include "WubForge.h"
#include "../Common/NeonEditor.h"

//==============================================================================
/**
    Interfaccia di WUBFORGE: verde lime neon. Schede WOBBLE / OSC / FX, knob grandi WOBBLE e GROWL.
    WOBBLE: le 4 forme dell'LFO (si disegnano col dito; doppio tocco = forme pronte) e la sequenza
    di 8 caselle, una per mezza battuta (tocca per cambiare A > B > C > D).
*/
class WubForgeEditor : public NeonEditor
{
public:
    explicit WubForgeEditor (WubForgeAudioProcessor&);

private:
    class ShapesView;
    class SequenceView;
    void refreshViews() override;
    ShapesView* shapes = nullptr;
    SequenceView* sequence = nullptr;
};
