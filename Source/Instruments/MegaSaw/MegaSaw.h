#pragma once

#include <JuceHeader.h>
#include <array>
#include <atomic>
#include <vector>
#include "../Common/ForgeFx.h"

//==============================================================================
/**
    MEGASAW: supersaw, hoover e reese.
    - SUPERSAW: fino a 9 dente di sega stonati e aperti nello stereo (lead e accordi enormi)
    - HOOVER:   dente di sega + onda quadra con PWM + strato un'ottava sotto, con "scoop" di intonazione (rave, dutch)
    - REESE:    dente di sega stonati che "battono" lentamente (basso grosso)
    Filtro passa-basso, inviluppi, glide, sub, drive ed effetti (chorus, delay, riverbero).
*/
namespace ms
{
    inline constexpr int maxVoices = 8, maxUnison = 9, controlRate = 16;

    juce::AudioProcessorValueTreeState::ParameterLayout createLayout();

    struct BlockParams
    {
        int mode = 0, unison = 7, octave = 0, playMode = 0;
        float detune = 0.4f, spread = 0.8f, hoover = 0.0f, scoop = 0.0f, sub = 0.0f, thick = 0.5f, drive = 0.0f;
        float cutoff = 8000.0f, reso = 0.1f, fEnv = 0.2f, fDecay = 0.3f;
        float aA = 0.005f, aD = 0.5f, aS = 1.0f, aR = 0.2f, glide = 0.0f;
        forge::FxParams fx;
    };

    struct Env
    {
        enum Stage { idle, attack, decay, sustain, release };
        void set (double sr, float a, float d, float s, float r) noexcept;
        float next() noexcept;
        Stage stage = idle;
        float value = 0.0f, ai = 0.01f, dc = 0.99f, rc = 0.99f, sl = 1.0f;
    };

    class Voice
    {
    public:
        void prepare (double sampleRate)    { sr = sampleRate; amp = {}; }
        void start (int note, float vel, float fromPitch, bool retrigger);
        void setTarget (int n)              { note = n; target = (float) n; }
        void stop()                         { if (amp.stage != Env::idle) amp.stage = Env::release; }
        bool isActive() const               { return amp.stage != Env::idle; }
        bool isReleasing() const            { return amp.stage == Env::release; }
        int getNote() const                 { return note; }
        float getPitch() const              { return pitch; }
        uint32_t age = 0;
        void render (float* L, float* R, int n, const BlockParams& p, float glideCoef, int unisonLimit);

    private:
        double sr = 48000.0;
        int note = 60;
        float vel = 1.0f, pitch = 60.0f, target = 60.0f, scoopEnv = 0.0f, fenv = 0.0f;
        std::array<double, maxUnison> ph {}, phPulse {};
        double phSub = 0.0, phOct = 0.0;
        float pwm = 0.0f, reeseLfo = 0.0f;
        Env amp;
        float s1[2] {}, s2[2] {};   // filtro (SVF passa-basso), sinistra e destra
    };

    class Synth
    {
    public:
        void prepare (double sampleRate, int maxBlock);
        void noteOn (int note, float velocity);
        void noteOff (int note);
        void allNotesOff();
        void killAll();
        void render (juce::AudioBuffer<float>& buffer, int start, int n, const BlockParams& p, double bpm);

    private:
        Voice* findVoice();
        double sr = 48000.0;
        std::array<Voice, maxVoices> voices;
        uint32_t ageCounter = 0;
        std::vector<int> held;
        int currentMode = 0;
        float currentGlide = 0.0f, lastPitch = -1.0f;
        forge::Effects fx;
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
}

//==============================================================================
class MegaSawAudioProcessor : public juce::AudioProcessor
{
public:
    MegaSawAudioProcessor();
    void prepareToPlay (double sampleRate, int samplesPerBlock) override  { synth.prepare (sampleRate, samplesPerBlock); }
    void releaseResources() override {}
    void reset() override                           { synth.killAll(); }
    bool isBusesLayoutSupported (const BusesLayout& l) const override { return l.getMainOutputChannelSet() == juce::AudioChannelSet::stereo(); }
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;
    using AudioProcessor::processBlock;
    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override                 { return true; }
    const juce::String getName() const override     { return "MegaSaw"; }
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

    ms::Presets& getPresets()                       { return presets; }
    juce::AudioProcessorValueTreeState apvts;

private:
    ms::BlockParams readParams();
    ms::Synth synth;
    ms::Presets presets { apvts };
    std::vector<std::atomic<float>*> p;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (MegaSawAudioProcessor)
};
