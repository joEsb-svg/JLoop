#pragma once

#include <JuceHeader.h>
#include "FMForge.h"
#include "../Common/NeonEditor.h"

//==============================================================================
/**
    Interfaccia di FMForge (verde acqua neon): ALGO (disegno dell'algoritmo, feedback, voce),
    OP 1..4 (rapporto, livello, velocity, inviluppo), LFO/FX; knob grandi TIMBRO e TONO.
*/
class FMForgeEditor : public NeonEditor
{
public:
    explicit FMForgeEditor (FMForgeAudioProcessor&);

private:
    class AlgorithmView;
    void refreshViews() override;
    AlgorithmView* algoView = nullptr;
};
