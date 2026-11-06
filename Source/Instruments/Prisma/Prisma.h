#pragma once

#include <JuceHeader.h>
#include <array>
#include <atomic>
#include <vector>
#include "../Common/ForgeFx.h"

//==============================================================================
/**
    PRISMA: synth a wavetable (stile Serum) per lead, bassi, pad e suoni che si trasformano.
    - 2 oscillatori a wavetable: 10 tabelle da 32 forme ciascuna, POSIZIONE (si scorre tra le forme),
      WARP (sync, bend, specchio, quantizza, allunga, FM dall'altro), UNISONO fino a 7 voci con detune e larghezza.
    - Sub (sinusoide/triangolo/quadra) e rumore.
    - Filtro (LP 12/24, HP, BP, notch) con drive, inviluppo e keytrack.
    - 2 inviluppi, 2 LFO (liberi o a tempo), MATRICE di modulazione a 8 posti.
    - Poly (8 voci), mono o legato con glide. Effetti: chorus, delay, riverbero.
    Le tabelle sono "band-limited" (versioni con meno armonici per le note alte: niente fruscio).
*/
namespace prisma
{
    inline constexpr int frameSize = 1024;        // campioni per forma
    inline constexpr int numFrames = 32;          // forme per tabella
    inline constexpr int numLevels = 8;           // versioni con 512, 256, ... 4 armonici
    inline constexpr int numTables = 10;
    inline constexpr int maxVoices = 8;
    inline constexpr int maxUnison = 7;
    inline constexpr int numSlots = 8;            // matrice di modulazione

    juce::StringArray tableNames();

    /** Le tabelle (uguali per tutti i Prisma): costruite una volta sola. */
    class Bank
    {
    public:
        static const Bank& get();
        /** Campione (interpolato) della tabella t, posizione 0..1 (tra le forme), fase 0..1, livello di armonici. */
        inline float read (int table, float pos, double phase, int level) const noexcept
        {
            const float fp = juce::jlimit (0.0f, 1.0f, pos) * (float) (numFrames - 1);
            const int f0 = juce::jmin ((int) fp, numFrames - 2);
            const float ff = fp - (float) f0;
            const double x = phase * frameSize;
            const int i0 = (int) x & (frameSize - 1);
            const int i1 = (i0 + 1) & (frameSize - 1);
            const float fr = (float) (x - std::floor (x));
            const float* a = frame (table, f0, level);
            const float* b = frame (table, f0 + 1, level);
            const float va = a[i0] + (a[i1] - a[i0]) * fr;
            const float vb = b[i0] + (b[i1] - b[i0]) * fr;
            return va + (vb - va) * ff;
        }
        /** Livello di armonici giusto per una frequenza (niente armonici sopra i ~20 kHz). */
        static inline int levelFor (double freq, double sr) noexcept
        {
            const double maxHarm = (sr * 0.45) / juce::jmax (1.0, freq);
            int level = 0, harm = frameSize / 2;
            while (level < numLevels - 1 && harm > maxHarm) { harm /= 2; ++level; }
            return level;
        }
        const float* frame (int table, int f, int level) const noexcept
        {
            return data.data() + ((((size_t) juce::jlimit (0, numTables - 1, table) * numFrames + (size_t) f) * numLevels + (size_t) level) * frameSize);
        }

    private:
        Bank();
        std::vector<float> data;
    };

    juce::AudioProcessorValueTreeState::ParameterLayout createLayout();

    /** Sorgenti e destinazioni della matrice. */
    juce::StringArray sourceNames();
    juce::StringArray destNames();
    enum Source { srcNone = 0, srcEnv2, srcLfo1, srcLfo2, srcVelocity, srcNote, srcRandom, srcModWheel, numSources };
    enum Dest { dstNone = 0, dstPosA, dstPosB, dstWarpA, dstWarpB, dstCutoff, dstRes, dstPitchA, dstPitchB, dstLevelA, dstLevelB,
                dstDetune, dstPan, dstSub, dstNoise, dstDrive, dstLfo1Rate, dstLfo2Rate, numDests };

    /** Funzione di warp sulla fase (per l'audio e per il disegno). FM a parte (serve l'altro oscillatore). */
    double warpPhase (int mode, float amount, double phase) noexcept;

    struct Env
    {
        enum Stage { idle, attack, decay, sustain, release };
        Stage stage = idle;
        float value = 0.0f;
        void start()    { stage = attack; }
        void stop()     { if (stage != idle) stage = release; }
        float next (float a, float d, float s, float r, double sr) noexcept;
    };

    struct Voice
    {
        bool active = false;
        int note = 60;
        float velocity = 1.0f, random = 0.0f;
        double pitch = 60.0, targetPitch = 60.0;
        std::array<std::array<double, maxUnison>, 2> phase {};
        double subPhase = 0.0;
        float lastA = 0.0f, lastB = 0.0f;   // per il warp FM
        float noiseLp = 0.0f;
        Env amp, mod;
        // filtro (2 stadi per canale)
        std::array<std::array<float, 2>, 4> svf {};
        juce::uint32 age = 0;
    };

    struct LfoState
    {
        double phase = 0.0;
        float hold = 0.0f, value = 0.0f;
    };
}

class PrismaAudioProcessor;

namespace prisma
{
    class Presets
    {
    public:
        Presets (juce::AudioProcessorValueTreeState& s) : apvts (s) {}
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
class PrismaAudioProcessor : public juce::AudioProcessor
{
public:
    PrismaAudioProcessor();
    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override {}
    void reset() override;
    bool isBusesLayoutSupported (const BusesLayout& l) const override { return l.getMainOutputChannelSet() == juce::AudioChannelSet::stereo(); }
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;
    using AudioProcessor::processBlock;
    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override                 { return true; }
    const juce::String getName() const override     { return "Prisma"; }
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

    prisma::Presets& getPresets()                   { return presets; }
    juce::AudioProcessorValueTreeState apvts;

    /** Posizione attuale nelle tabelle (con la modulazione), per il disegno: 0..1, -1 = nessuna nota. */
    std::atomic<float> shownPos[2] { { -1.0f }, { -1.0f } };

private:
    struct Block;
    void readBlock (Block&);
    void noteOn (int note, float velocity, const Block&);
    void noteOff (int note, const Block&);
    void render (float* L, float* R, int n, const Block& b, double bpm, double ppq);

    prisma::Presets presets { apvts };
    std::vector<std::atomic<float>*> p;
    std::array<prisma::Voice, prisma::maxVoices> voices;
    std::vector<int> held;
    prisma::LfoState lfo[2];
    double sr = 48000.0;
    float pitchBend = 0.0f, modWheel = 0.0f;
    juce::uint32 ageCounter = 0;
    juce::Random rng;
    forge::Effects fx;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (PrismaAudioProcessor)
};
