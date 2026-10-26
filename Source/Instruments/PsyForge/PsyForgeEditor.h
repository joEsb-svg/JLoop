#pragma once

#include <JuceHeader.h>
#include "PsyForge.h"
#include "../Common/NeonEditor.h"

//==============================================================================
/**
    Interfaccia di PSYFORGE: rosa tenue neon (cassa in ambra). Schede ROLLING / CASSA / BASSO / FX,
    knob grandi CODA e MORSO. ROLLING: la griglia della battuta (tocca una casella: cassa > basso > pausa)
    e il suono del basso.
*/
class PsyForgeEditor : public NeonEditor
{
public:
    explicit PsyForgeEditor (PsyForgeAudioProcessor&);

private:
    class GridView;
    class WaveView;
    void refreshViews() override;
    GridView* grid = nullptr;
    WaveView* wave = nullptr;
};
