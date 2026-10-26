#pragma once

#include <JuceHeader.h>
#include <array>
#include <atomic>
#include <vector>
#include "../Common/ForgeFx.h"

//==============================================================================
/**
    PSYFORGE: cassa e basso "rolling" per Psytrance e Hi-tech.
    - Una griglia di una battuta (16 caselle a 1/16, o 32 a 1/32 per l'Hi-tech): K = cassa, B = basso, . = pausa.
      Le figure pronte (Full-on K-B-B-B, Night, Goa, Hi-tech) riempiono la griglia; ogni casella si cambia toccandola.
    - Suona mentre c'e' una nota: ogni nota tiene acceso almeno il suo quarto. Nel channel rack bastano
      gli step sui quarti (1, 5, 9, 13); nel piano roll una nota lunga = rolling continuo, e l'altezza della nota
      decide la tonalita' (cassa accordata un'ottava sotto il basso).
    - Basso con inviluppo cortissimo (CODA) e filtro che schiocca (MORSO), onda saw / quadra / FM.
*/
namespace psy
{
    inline constexpr int maxCells = 32;

    juce::AudioProcessorValueTreeState::ParameterLayout createLayout();
    /** Figura pronta (16 o 32 caratteri). */
    juce::String figureGrid (int figure);
    inline int cellsFor (int figure)    { return figure == 3 ? 32 : 16; }

    struct BlockParams
    {
        int figure = 0, bassWave = 0, bassOct = 1, transpose = 0;
        bool kickOn = true;
        float kickClick = 0.5f, kickPunch = 0.5f, kickDecay = 0.35f, kickTune = 0.0f, kickLevel = 0.9f;
        float cutoff = 400.0f, reso = 0.3f, drive = 0.3f, bassLevel = 0.8f;
        float coda = 0.3f, morso = 0.6f, snap = 0.3f;
        float colore = 0.0f, sub = 0.0f, zap = 0.0f;
        int motion = 0;
        forge::FxParams fx;
    };

    class Presets;
}

class PsyForgeAudioProcessor;

namespace psy
{
    class Presets
    {
    public:
        Presets (juce::AudioProcessorValueTreeState& s, PsyForgeAudioProcessor& p) : apvts (s), proc (p) {}
        static int numFactory();
        static juce::String factoryName (int i);
        void loadFactory (int i);
        juce::String currentName() const    { return apvts.state.getProperty ("presetName", "Init").toString(); }
        juce::File userDir() const;
        juce::Array<juce::File> userPresets() const;
        bool save (const juce::String& name);
        bool load (const juce::File& file);
        void set (const juce::String& id, float value);

    private:
        juce::AudioProcessorValueTreeState& apvts;
        PsyForgeAudioProcessor& proc;
    };
}

//==============================================================================
class PsyForgeAudioProcessor : public juce::AudioProcessor
{
public:
    PsyForgeAudioProcessor();
    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override {}
    void reset() override;
    bool isBusesLayoutSupported (const BusesLayout& l) const override { return l.getMainOutputChannelSet() == juce::AudioChannelSet::stereo(); }
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;
    using AudioProcessor::processBlock;
    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override                 { return true; }
    const juce::String getName() const override     { return "PsyForge"; }
    bool acceptsMidi() const override               { return true; }
    bool producesMidi() const override              { return false; }
    bool isMidiEffect() const override              { return false; }
    double getTailLengthSeconds() const override    { return 2.0; }
    int getNumPrograms() override                   { return 1; }
    int getCurrentProgram() override                { return 0; }
    void setCurrentProgram (int) override {}
    const juce::String getProgramName (int) override { return {}; }
    void changeProgramName (int, const juce::String&) override {}
    void getStateInformation (juce::MemoryBlock&) override;
    void setStateInformation (const void*, int) override;

    psy::Presets& getPresets()                      { return presets; }
    juce::AudioProcessorValueTreeState apvts;

    // --- Griglia (0 pausa, 1 cassa, 2 basso)
    int getCell (int i) const                       { return cells[(size_t) juce::jlimit (0, psy::maxCells - 1, i)].load (std::memory_order_relaxed); }
    void setCell (int i, int v)                     { cells[(size_t) juce::jlimit (0, psy::maxCells - 1, i)].store (juce::jlimit (0, 2, v), std::memory_order_relaxed); }
    void setGrid (const juce::String& g);
    juce::String getGrid() const;
    void gridToState()                              { apvts.state.setProperty ("grid", getGrid(), nullptr); }
    void gridFromState();
    int numCells() const;
    /** Casella che suona adesso (-1 = fermo), per l'interfaccia. */
    std::atomic<int> playCell { -1 };

private:
    psy::BlockParams readParams();
    void render (float* L, float* R, int n, const psy::BlockParams& b, double bpm, double ppq);
    void hit (int what, int cellIdx, const psy::BlockParams& b);

    psy::Presets presets { apvts, *this };
    std::vector<std::atomic<float>*> p;
    std::array<std::atomic<int>, psy::maxCells> cells {};
    std::atomic<int> lastFigure { 0 };
    friend class psy::Presets;

    double sr = 48000.0, freeBeats = 0.0, latchEnd = -1.0;
    std::vector<int> held;
    int note = 48, lastCell = -1;
    bool noteOnPending = false;
    // cassa
    double kPh = 0.0, kT = 1.0e9;
    float kEnv = 0.0f;
    // basso
    double bPh = 0.0, bPh2 = 0.0, subPh = 0.0, bMod = 0.0, bT = 1.0e9;
    int bOffset = 0;
    float bEnv = 0.0f, fEnv = 0.0f, s1 = 0.0f, s2 = 0.0f, s1b = 0.0f, s2b = 0.0f;
    juce::Random rng;
    forge::Effects fx;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (PsyForgeAudioProcessor)
};
