#pragma once

#include <JuceHeader.h>
#include "AcidLine.h"
#include "../Common/NeonEditor.h"

//==============================================================================
/**
    Interfaccia di ACIDLINE: giallo acido neon. Schede SEQUENZA / SUONO / FX, knob grandi SQUELCH e ACIDO.
    SEQUENZA: le 16 note come barre da trascinare su e giu' (tocco = accesa/pausa), accent (pallini arancio),
    slide (lineette azzurre), CASUALE per una linea nuova; accanto il filtro disegnato.
*/
class AcidLineEditor : public NeonEditor
{
public:
    explicit AcidLineEditor (AcidLineAudioProcessor&);

private:
    class BarsView;
    class FilterView;
    void refreshViews() override;
    BarsView* bars = nullptr;
    FilterView* filter = nullptr;
};
