#pragma once

#include <JuceHeader.h>
#include "Prisma.h"
#include "../Common/NeonEditor.h"

//==============================================================================
/**
    Interfaccia di PRISMA: azzurro-ciano neon. Schede OSC A / OSC B / FILTRO / ENV LFO / MOD / FX,
    knob grandi MORPH (scorre tutte e due le tabelle) e CUTOFF.
    La tabella si vede "in 3D" (le 32 forme una dietro l'altra) con la forma che suona accesa;
    trascina su e giu' sul disegno per cambiare la posizione.
*/
class PrismaEditor : public NeonEditor
{
public:
    explicit PrismaEditor (PrismaAudioProcessor&);

private:
    class TableView;
    void refreshViews() override;
    TableView* tables[2] { nullptr, nullptr };
};
