#pragma once

#include <JuceHeader.h>
#include "MegaSaw.h"
#include "../Common/NeonEditor.h"

//==============================================================================
/**
    Interfaccia di MEGASAW: arancione neon. Schede OSC / FILTRO / FX, knob grandi SPESSORE e DRIVE.
    In OSC il "ventaglio" mostra i dente di sega sovrapposti (piu' voci e piu' detune = piu' linee aperte).
*/
class MegaSawEditor : public NeonEditor
{
public:
    explicit MegaSawEditor (MegaSawAudioProcessor&);

private:
    class FanView;
    void refreshViews() override;
    FanView* fan = nullptr;
};
