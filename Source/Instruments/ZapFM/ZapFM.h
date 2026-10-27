#pragma once

#include <JuceHeader.h>
#include <array>
#include <atomic>
#include <vector>
#include "../Common/ForgeFx.h"

//==============================================================================
/**
    ZAPFM: laser, zap, colpi metallici e glitch per l'Hi-tech (e la psy).
    - Sequenza interna di 16 colpi: per ogni colpo acceso/pausa, ALTEZZA (da dove cade il laser) e GLITCH.
      Suona a tempo mentre c'e' una nota (ogni nota tiene accesa la sua battuta, l'altezza della nota decide la tonalita').
      In modalita' TASTIERA ogni nota e' un colpo (velocity = altezza).
    - Voce FM a 2 operatori con feedback: RAPPORTO, INDICE (metallo), quanto il metallo si spegne, larghezza stereo.
    - Knob grandi ZAP (quanto in alto parte il laser) e GLITCH (quanti colpi impazziscono a caso):
      ripetizioni veloci, laser al contrario, salto di rapporto, suono "rotto".
*/
namespace zap
{
    inline constexpr int numSteps = 16;
    inline constexpr int maxHeight = 15;

    struct Step
    {
        bool gate = true, glitch = false;
        int height = 10;   // 0..15
    };

    juce::AudioProcessorValueTreeState::ParameterLayout createLayout();
    juce::String encode (const std::array<Step, numSteps>&);
    std::array<Step, numSteps> decode (const juce::String&);
    /** Sequenza casuale (ripetibile dal numero). */
    std::array<Step, numSteps> randomLine (int seed);
    /** Rapporti FM a scelta. */
    inline constexpr double ratios[] { 0.5, 1.0, 1.5, 2.0, 2.5, 3.0, 3.5, 4.0, 5.0, 7.0, 11.0 };

    struct BlockParams
    {
        int mode = 0, rate = 1, octave = 1, ratio = 3;
        float index = 0.5f, metal = 0.5f, feedback = 0.2f, spread = 0.3f;
        float zap = 0.6f, zapTime = 0.3f, decay = 0.3f, glitch = 0.2f, crush = 0.0f, tune = 0.0f, level = 0.8f;
        forge::FxParams fx;
    };
}

class ZapFMAudioProcessor;

namespace zap
{
    class Presets
    {
    public:
        Presets (juce::AudioProcessorValueTreeState& s, ZapFMAudioProcessor& p) : apvts (s), proc (p) {}
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
        ZapFMAudioProcessor& proc;
    };
}

//==============================================================================
class ZapFMAudioProcessor : public juce::AudioProcessor
{
public:
    ZapFMAudioProcessor();
    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override {}
    void reset() override;
    bool isBusesLayoutSupported (const BusesLayout& l) const override { return l.getMainOutputChannelSet() == juce::AudioChannelSet::stereo(); }
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;
    using AudioProcessor::processBlock;
    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override                 { return true; }
    const juce::String getName() const override     { return "ZapFM"; }
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

    zap::Presets& getPresets()                      { return presets; }
    juce::AudioProcessorValueTreeState apvts;

    zap::Step getStep (int i) const;
    void setStep (int i, const zap::Step&);
    std::array<zap::Step, zap::numSteps> getLine() const;
    void setLine (const std::array<zap::Step, zap::numSteps>&);
    void lineToState()                              { apvts.state.setProperty ("line", zap::encode (getLine()), nullptr); }
    void lineFromState();
    std::atomic<int> playStep { -1 };
    /** Ultimo glitch suonato (per l'interfaccia): passo * 8 + tipo, -1 nessuno. */
    std::atomic<int> lastGlitch { -1 };

private:
    zap::BlockParams readParams();
    void render (float* L, float* R, int n, const zap::BlockParams& b, double bpm, double ppq);
    void trigger (double heightNorm, int glitchType, const zap::BlockParams& b);

    zap::Presets presets { apvts, *this };
    std::vector<std::atomic<float>*> p;
    std::array<std::atomic<int>, zap::numSteps> packed {};

    double sr = 48000.0, freeBeats = 0.0, latchEnd = -1.0;
    std::vector<int> held;
    int rootNote = 60, lastStep = -1;
    bool noteOnPending = false;
    double keyNote = 60.0;
    // voce
    double t = 1.0e9, zapSemis = 0.0, ratioNow = 2.0;
    double carL = 0.0, carR = 0.0, modPh = 0.0, fbPrev = 0.0;
    int zapDir = 1, crushHold = 1, crushCount = 0;
    float heldL = 0.0f, heldR = 0.0f;
    double lpL = 0.0, lpR = 0.0;
    // ripetizioni (glitch)
    int ratchetLeft = 0;
    double ratchetNext = 0.0, ratchetGap = 0.0, ratchetHeight = 0.0;
    juce::Random rng;
    forge::Effects fx;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (ZapFMAudioProcessor)
};
