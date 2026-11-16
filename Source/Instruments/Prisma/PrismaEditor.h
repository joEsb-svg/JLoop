#pragma once

#include <JuceHeader.h>
#include "Prisma.h"
#include "../Common/NeonEditor.h"

class PrismaWaveEditor;

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
    ~PrismaEditor() override;
    void resized() override;

    /** Apre "Le mie onde" per l'oscillatore 0 (A) o 1 (B). */
    void openWaveEditor (int osc);
    PrismaWaveEditor* getWaveEditor()   { return waveEditor.get(); }

private:
    class TableView;
    void refreshViews() override;
    static int modDestFor (const juce::String& paramId);
    float modAmountFor (int dest) const;
    float slotValue (int slot, const char* what) const;
    void setSlotValue (int slot, const char* what, float value);
    void showModMenu (frui::Knob&, int dest);
    void repaintKnobs();
    std::unique_ptr<PrismaWaveEditor> waveEditor;
    juce::int64 modSig = 0;
    TableView* tables[2] { nullptr, nullptr };
    PrismaAudioProcessor& prismaProc;
};
