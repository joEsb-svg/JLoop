#pragma once

#include <JuceHeader.h>
#include <array>
#include <atomic>
#include <vector>
#include "../../Engine/SampleStore.h"

//==============================================================================
/**
    SLICER (stile Slicex): carichi un loop o una voce, lo taglia sui colpi (o in parti uguali)
    e ogni pezzo va su un tasto: il primo pezzo e' il C3, poi C#3, D3...

    - Dal channel rack: gli step suonano il primo pezzo; con la modalita' PITCH ogni step sceglie il suo
      pezzo (+1 = secondo pezzo, +2 = terzo...): cosi' si fanno i vocal chop fidget direttamente con gli step.
    - Dal piano roll: un tasto per pezzo.
    - "Taglia": un pezzo nuovo zittisce quello prima (come un chop vero); "Sovrapponi": suonano insieme.
*/
namespace slicer
{
    inline constexpr int maxSlices = 32;
    inline constexpr int firstNote = 48;   // C3 = primo pezzo (e' anche la nota degli step dei synth)
    inline constexpr int maxVoices = 8;

    juce::AudioProcessorValueTreeState::ParameterLayout createLayout();

    /** Trova i punti di taglio: sui colpi (sensibilita' 0..1) o in parti uguali. */
    std::vector<int> findSlices (const juce::AudioBuffer<float>& audio, double sampleRate, int mode, float sensitivity);

    juce::String noteName (int midi);

    class Presets
    {
    public:
        Presets (juce::AudioProcessorValueTreeState& s, std::function<void()> onSampleChanged) : apvts (s), changed (std::move (onSampleChanged)) {}
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
        std::function<void()> changed;
    };
}

//==============================================================================
class SlicerAudioProcessor : public juce::AudioProcessor,
                             private juce::AudioProcessorValueTreeState::Listener,
                             private juce::AsyncUpdater
{
public:
    SlicerAudioProcessor();
    ~SlicerAudioProcessor() override;

    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override {}
    void reset() override;
    bool isBusesLayoutSupported (const BusesLayout& layouts) const override
    {
        return layouts.getMainOutputChannelSet() == juce::AudioChannelSet::stereo();
    }
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;
    using AudioProcessor::processBlock;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override                 { return true; }
    const juce::String getName() const override     { return "Slicer"; }
    bool acceptsMidi() const override               { return true; }
    bool producesMidi() const override              { return false; }
    bool isMidiEffect() const override              { return false; }
    double getTailLengthSeconds() const override    { return 1.0; }
    int getNumPrograms() override                   { return 1; }
    int getCurrentProgram() override                { return 0; }
    void setCurrentProgram (int) override {}
    const juce::String getProgramName (int) override { return {}; }
    void changeProgramName (int, const juce::String&) override {}
    void getStateInformation (juce::MemoryBlock& destData) override;
    void setStateInformation (const void* data, int sizeInBytes) override;

    // --- Campione e pezzi (thread della UI)
    bool setSample (const juce::String& storeName);
    juce::String getSampleName() const              { return apvts.state.getProperty ("sample", "").toString(); }
    const SampleData* getSample() const             { return sample.load(); }
    int getNumSlices() const                        { return numSlices.load(); }
    int getSliceStart (int i) const                 { return sliceStarts[(size_t) juce::jlimit (0, slicer::maxSlices, i)].load(); }
    /** Suona un pezzo (tocco sulla forma d'onda). */
    void auditionSlice (int index);
    /** Ultimo pezzo suonato (per illuminarlo), -1 = nessuno. */
    int getLastSlice() const                        { return lastSlice.load(); }
    SampleStore& getSampleStore()                   { return store; }
    slicer::Presets& getPresets()                   { return presets; }
    void recomputeSlices();

    juce::AudioProcessorValueTreeState apvts;

private:
    void parameterChanged (const juce::String&, float) override     { triggerAsyncUpdate(); }
    void handleAsyncUpdate() override                                { recomputeSlices(); }

    struct Voice
    {
        bool active = false, releasing = false;
        int note = -1, slice = 0;
        double pos = 0.0, end = 0.0, inc = 1.0;
        float gain = 1.0f, env = 0.0f, attackStep = 1.0f, releaseCoef = 0.99f, fadeOut = 1.0f;
    };
    void startVoice (int note, float velocity);
    void stopNote (int note);

    SampleStore store;
    std::atomic<SampleData*> sample { nullptr };
    std::array<std::atomic<int>, slicer::maxSlices + 1> sliceStarts {};
    std::atomic<int> numSlices { 0 };
    std::atomic<int> lastSlice { -1 }, pendingAudition { -1 };
    std::array<Voice, slicer::maxVoices> voices;
    double hostRate = 48000.0;
    std::atomic<float> *sensP {}, *modeP {}, *attackP {}, *releaseP {}, *pitchP {}, *voiceP {}, *gainP {};
    slicer::Presets presets { apvts, [this] { recomputeSlices(); } };

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (SlicerAudioProcessor)
};
