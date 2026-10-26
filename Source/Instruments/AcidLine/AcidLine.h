#pragma once

#include <JuceHeader.h>
#include <array>
#include <atomic>
#include <vector>
#include "../Common/ForgeFx.h"

//==============================================================================
/**
    ACIDLINE: linee acide stile TB-303 (acid techno, psytrance, goa, hi-tech).
    - Sequenza interna di 16 step: per ogni step nota (0..12 + ottava), acceso/pausa, ACCENT e SLIDE.
      Suona a tempo mentre c'e' una nota: ogni nota tiene accesa la sua battuta (nel channel rack basta lo step 1),
      e l'altezza della nota traspone la linea. In modalita' TASTIERA invece suona le note come un synth mono
      (legato = slide, velocity alta = accent).
    - Oscillatore saw o quadra, filtro a 4 poli che "squitta", inviluppo del filtro (ENV MOD, DECAY),
      accent che apre di piu' e alza il volume, slide che scivola senza ripartire.
    - Knob grandi SQUELCH (risonanza + inviluppo insieme) e ACIDO (distorsione).
*/
namespace acid
{
    inline constexpr int numSteps = 16;

    struct Step
    {
        int note = 0;      // 0..12 semitoni sopra la tonica
        int octave = 0;    // -1, 0, +1
        bool gate = true, accent = false, slide = false;
    };

    juce::AudioProcessorValueTreeState::ParameterLayout createLayout();
    juce::String encode (const std::array<Step, numSteps>&);
    std::array<Step, numSteps> decode (const juce::String&);
    /** Linea casuale nella scala minore (ripetibile dal numero). */
    std::array<Step, numSteps> randomLine (int seed);

    struct BlockParams
    {
        int wave = 0, mode = 0;
        float cutoff = 500.0f, reso = 0.7f, envMod = 0.6f, decay = 0.3f, accent = 0.6f, tune = 0.0f;
        float squelch = 0.5f, acid = 0.3f, slideTime = 0.06f, gateLen = 0.55f, level = 0.8f;
        forge::FxParams fx;
    };
}

class AcidLineAudioProcessor;

namespace acid
{
    class Presets
    {
    public:
        Presets (juce::AudioProcessorValueTreeState& s, AcidLineAudioProcessor& p) : apvts (s), proc (p) {}
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
        AcidLineAudioProcessor& proc;
    };
}

//==============================================================================
class AcidLineAudioProcessor : public juce::AudioProcessor
{
public:
    AcidLineAudioProcessor();
    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override {}
    void reset() override;
    bool isBusesLayoutSupported (const BusesLayout& l) const override { return l.getMainOutputChannelSet() == juce::AudioChannelSet::stereo(); }
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;
    using AudioProcessor::processBlock;
    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override                 { return true; }
    const juce::String getName() const override     { return "AcidLine"; }
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

    acid::Presets& getPresets()                     { return presets; }
    juce::AudioProcessorValueTreeState apvts;

    // --- La linea (thread della UI scrive, thread audio legge)
    acid::Step getStep (int i) const;
    void setStep (int i, const acid::Step&);
    std::array<acid::Step, acid::numSteps> getLine() const;
    void setLine (const std::array<acid::Step, acid::numSteps>&);
    void lineToState()                              { apvts.state.setProperty ("line", acid::encode (getLine()), nullptr); }
    void lineFromState();
    std::atomic<int> playStep { -1 };

private:
    acid::BlockParams readParams();
    void render (float* L, float* R, int n, const acid::BlockParams& b, double bpm, double ppq);
    void startNote (double pitch, bool accent, bool slide, const acid::BlockParams& b);

    acid::Presets presets { apvts, *this };
    std::vector<std::atomic<float>*> p;
    std::array<std::atomic<int>, acid::numSteps> packed {};   // nota | ottava | gate | accent | slide

    double sr = 48000.0, freeBeats = 0.0, latchEnd = -1.0;
    std::vector<int> held;
    int rootNote = 48, lastStep = -1;
    bool noteOnPending = false, gateOpen = false;
    double gateOff = 0.0;          // in battiti: quando si chiude la nota (sequenza)
    double pitch = 48.0, target = 48.0;
    double ph = 0.0;
    float amp = 0.0f, fEnv = 0.0f, accentNow = 0.0f;
    float z[4] {};
    forge::Effects fx;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (AcidLineAudioProcessor)
};
