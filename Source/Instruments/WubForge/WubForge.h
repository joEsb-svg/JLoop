#pragma once

#include <JuceHeader.h>
#include <array>
#include <atomic>
#include <vector>
#include "../Common/ForgeFx.h"

class WubForgeAudioProcessor;

//==============================================================================
/**
    WUBFORGE: basso wobble (dubstep / brostep 2010-2012).
    - 4 forme di LFO (A, B, C, D) disegnabili col dito, ognuna con la sua velocita' a tempo
    - una sequenza di 8 caselle (una per mezza battuta = 4 battute) sceglie quale forma suona
    - l'LFO muove cutoff, filtro a vocali (yoi, talking), volume e intonazione
    - oscillatore: dente di sega / quadra stonati + strato FM per il "growl", sub, glide (mono)
    Knob grandi: WOBBLE (profondita' di tutto) e GROWL (FM + drive).
*/
namespace wf
{
    inline constexpr int numShapes = 4, shapePoints = 32, seqSlots = 8, controlRate = 16;

    juce::AudioProcessorValueTreeState::ParameterLayout createLayout();
    juce::StringArray rateNames();
    double rateBeats (int index);

    /** Percorsi del filtro a vocali (indici a=0 e=1 i=2 o=3 u=4). */
    const std::vector<std::vector<int>>& vowelPaths();

    struct BlockParams
    {
        float shape = 0.0f, detune = 0.3f, fm = 0.0f, fmRatio = 1.0f, sub = 0.3f, glide = 0.05f;
        int octave = 0;
        float cutoff = 200.0f, reso = 0.3f, vowelQ = 0.5f;
        int vowelPath = 0;
        float depth = 0.8f, growl = 0.0f, amtCut = 0.8f, amtVowel = 0.0f, amtVol = 0.0f, amtPitch = 0.0f;
        int rate[numShapes] { 3, 3, 3, 3 }, seq[seqSlots] {};
        float drive = 0.2f, aA = 0.003f, aR = 0.08f;
        forge::FxParams fx;
    };

    /** Il synth (mono, con legato). */
    class Synth
    {
    public:
        void prepare (double sampleRate);
        void noteOn (int note, float velocity, bool hostPlaying);
        void noteOff (int note);
        void allNotesOff()              { held.clear(); gate = false; }
        void killAll();
        /** ppq < 0: trasporto fermo (l'LFO parte dalla nota). */
        void render (float* L, float* R, int n, const BlockParams& p, double bpm, double ppq, const WubForgeAudioProcessor& shapes);

        int slot = 0;
        float lfoPhase = 0.0f, lfoOut = 0.0f;

    private:
        struct Svf
        {
            float s1 = 0.0f, s2 = 0.0f;
            float lp (float x, float g, float k) noexcept;
            float bp (float x, float g, float k) noexcept;
        };
        double sr = 48000.0, freeBeats = 0.0;
        std::vector<int> held;
        bool gate = false;
        float vel = 1.0f, pitch = 36.0f, target = 36.0f, env = 0.0f, lfoSmooth = 0.0f, lastGain = 1.0f;
        double ph[3] {}, phSub = 0.0, phCar = 0.0, phMod = 0.0;
        Svf f1, f2, form1, form2;
        forge::Effects fx;
    };

    class Presets
    {
    public:
        Presets (juce::AudioProcessorValueTreeState& s, WubForgeAudioProcessor& p) : apvts (s), proc (p) {}
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
        WubForgeAudioProcessor& proc;
    };

    /** Forme pronte (per i preset e per il menu "forma" nell'editor). */
    juce::StringArray shapeKindNames();
    float shapeKind (int kind, float x);
}

//==============================================================================
class WubForgeAudioProcessor : public juce::AudioProcessor
{
public:
    WubForgeAudioProcessor();
    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override {}
    void reset() override                           { synth.killAll(); }
    bool isBusesLayoutSupported (const BusesLayout& l) const override { return l.getMainOutputChannelSet() == juce::AudioChannelSet::stereo(); }
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;
    using AudioProcessor::processBlock;
    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override                 { return true; }
    const juce::String getName() const override     { return "WubForge"; }
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

    wf::Presets& getPresets()                       { return presets; }
    juce::AudioProcessorValueTreeState apvts;

    // Forme dell'LFO (valori 0..1)
    float getShape (int s, int i) const             { return shapes[(size_t) (s * wf::shapePoints + i)].load (std::memory_order_relaxed); }
    void setShape (int s, int i, float v)           { shapes[(size_t) (s * wf::shapePoints + i)].store (juce::jlimit (0.0f, 1.0f, v), std::memory_order_relaxed); }
    float readShape (int s, float phase) const;
    void setShapeKind (int s, int kind);
    void shapesToState();
    void shapesFromState();

    // Per l'interfaccia: casella della sequenza che suona, fase e valore dell'LFO
    std::atomic<int> playSlot { -1 };
    std::atomic<float> playPhase { 0.0f }, playValue { 0.0f };

private:
    wf::BlockParams readParams();
    wf::Synth synth;
    wf::Presets presets { apvts, *this };
    std::vector<std::atomic<float>*> p;
    std::array<std::atomic<float>, wf::numShapes * wf::shapePoints> shapes;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (WubForgeAudioProcessor)
};
