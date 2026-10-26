#pragma once

#include <JuceHeader.h>
#include <array>
#include <atomic>
#include <vector>
#include "../../Engine/SampleStore.h"
#include "../Common/ForgeFx.h"

//==============================================================================
/**
    ROBOVOX: vocoder. Una voce (dalla libreria, da un file o registrata col microfono) "parla"
    attraverso un synth: tieni un accordo e la voce lo fa cantare da robot.
    - la voce e' divisa in 8, 16 o 24 bande; il volume di ogni banda apre la stessa banda del synth
    - FORMANTE sposta le bande del synth (voce grossa <-> voce da cartone), MIX = voce vera <-> robot
    - SIBILANTI: le "s" e le "t" della voce passano dirette (si capiscono le parole)
    La voce parte dall'inizio a ogni accordo nuovo ("dalla nota") o segue il tempo del brano ("a tempo").
*/
namespace rv
{
    inline constexpr int maxBands = 24, maxVoices = 8;

    juce::AudioProcessorValueTreeState::ParameterLayout createLayout();

    struct BlockParams
    {
        int bands = 16, carrier = 0, octave = 0, trig = 0;
        float formant = 0.0f, mix = 1.0f, sibil = 0.3f, gate = 0.1f, attack = 0.004f, release = 0.06f;
        float detune = 0.3f, noteRel = 0.15f, noise = 0.05f;
        forge::FxParams fx;
    };

    class Presets
    {
    public:
        explicit Presets (juce::AudioProcessorValueTreeState& s) : apvts (s) {}
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
    };

    /** Voci della libreria adatte al vocoder (id). */
    juce::StringArray libraryVoices();
}

//==============================================================================
class RoboVoxAudioProcessor : public juce::AudioProcessor
{
public:
    RoboVoxAudioProcessor();
    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override {}
    void reset() override;
    bool isBusesLayoutSupported (const BusesLayout& l) const override { return l.getMainOutputChannelSet() == juce::AudioChannelSet::stereo(); }
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;
    using AudioProcessor::processBlock;
    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override                 { return true; }
    const juce::String getName() const override     { return "RoboVox"; }
    bool acceptsMidi() const override               { return true; }
    bool producesMidi() const override              { return false; }
    bool isMidiEffect() const override              { return false; }
    double getTailLengthSeconds() const override    { return 3.0; }
    int getNumPrograms() override                   { return 1; }
    int getCurrentProgram() override                { return 0; }
    void setCurrentProgram (int) override {}
    const juce::String getProgramName (int) override { return {}; }
    void changeProgramName (int, const juce::String&) override {}
    void getStateInformation (juce::MemoryBlock&) override;
    void setStateInformation (const void*, int) override;

    rv::Presets& getPresets()                       { return presets; }
    juce::AudioProcessorValueTreeState apvts;

    // --- Voce che parla (thread della UI)
    bool setSample (const juce::String& storeName);
    juce::String getSampleName() const              { return apvts.state.getProperty ("sample", "").toString(); }
    SampleStore& getSampleStore()                   { return store; }

    // Per l'interfaccia
    float getBandLevel (int b) const                { return bandLevel[(size_t) juce::jlimit (0, rv::maxBands - 1, b)].load (std::memory_order_relaxed); }
    int getNumBands() const                         { return numBandsShown.load(); }

private:
    struct Voice
    {
        int note = -1;
        bool on = false;
        float env = 0.0f, pitch = 60.0f;
        double ph[3] {};
        uint32_t age = 0;
    };
    struct Svf
    {
        float s1 = 0.0f, s2 = 0.0f;
        inline float bp (float x, float a1, float a2, float a3, float k) noexcept
        {
            const float v3 = x - s2, v1 = a1 * s1 + a2 * v3, v2 = s2 + a2 * s1 + a3 * v3;
            s1 = 2.0f * v1 - s1; s2 = 2.0f * v2 - s2;
            return v1 * k;
        }
    };
    struct Coef { float a1 = 0, a2 = 0, a3 = 0, k = 1; void set (float freq, float q, double sr); };

    rv::BlockParams readParams();
    void noteOn (int note, float vel);
    void noteOff (int note);
    void render (float* L, float* R, int n, const rv::BlockParams& b, double bpm, double ppq);
    void updateBands (const rv::BlockParams& b);

    SampleStore store;
    std::atomic<SampleData*> sample { nullptr };
    rv::Presets presets { apvts };
    std::vector<std::atomic<float>*> p;

    double sr = 48000.0, modPos = 0.0;
    std::array<Voice, rv::maxVoices> voices;
    uint32_t ageCounter = 0;
    bool anyHeldBefore = false;
    float compEnv = 0.0f, activeEnv = 0.0f, gateEnv = 0.0f, levelEnv = 0.0f, prevMod = 0.0f;
    std::array<Svf, rv::maxBands> modA, modB, carA, carB;
    std::array<Coef, rv::maxBands> modC, carC;
    std::array<float, rv::maxBands> env {};
    float sibLp1 = 0.0f, sibLp2 = 0.0f;
    int bandsUsed = 0;
    float lastFormant = -99.0f;
    forge::Effects fx;
    juce::Random rng;

    std::array<std::atomic<float>, rv::maxBands> bandLevel {};
    std::atomic<int> numBandsShown { 16 };

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (RoboVoxAudioProcessor)
};
