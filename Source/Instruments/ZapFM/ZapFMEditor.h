#pragma once

#include <JuceHeader.h>
#include "ZapFM.h"
#include "../Common/NeonEditor.h"

//==============================================================================
/**
    Interfaccia di ZAPFM: viola neon. Schede COLPI / SUONO FM / FX, knob grandi ZAP e GLITCH.
    COLPI: 16 barre, l'altezza e' da dove cade il laser (si vede la curva), tocco = acceso/pausa,
    riga di stelline per il glitch su un colpo preciso, CASUALE per una sequenza nuova.
*/
class ZapFMEditor : public NeonEditor
{
public:
    explicit ZapFMEditor (ZapFMAudioProcessor&);

private:
    class HitsView;
    void refreshViews() override;
    HitsView* hits = nullptr;
};
