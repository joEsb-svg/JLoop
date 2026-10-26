#pragma once

#include <JuceHeader.h>
#include "PolyForge.h"
#include "../Common/NeonEditor.h"

//==============================================================================
/** Interfaccia di PolyForge (viola neon): schede OSC / FILTRO / ENV / LFO / FX, knob grandi CUTOFF e SPAZIO. */
class PolyForgeEditor : public NeonEditor
{
public:
    explicit PolyForgeEditor (PolyForgeAudioProcessor&);
};
