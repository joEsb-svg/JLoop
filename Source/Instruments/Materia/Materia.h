#pragma once

#include <JuceHeader.h>
#include <array>
#include <atomic>
#include <vector>
#include "../Common/ForgeFx.h"
#include "../Prisma/Prisma.h"   // le wavetable (prisma::Bank) e l'inviluppo

//==============================================================================
/**
    MATERIA: il "banco vuoto". Si parte da niente e si aggiungono i blocchi che si vogliono:
    - SORGENTI (fino a 4): oscillatore, wavetable, rumore, corda. Ognuna con ottava, intonazione, unisono, volume, pan.
    - FORMA (fino a 6, in catena): filtro, distorsione, piega-onda, risonatore, schiaccia-bit, vocale (per ogni nota),
      poi chorus, ritardo, riverbero (sul suono intero, sempre in fondo).
    - MOVIMENTO (fino a 6): inviluppo, LFO, sequenza a 8 passi, casuale, nota, forza, rotella. Ognuno muove fino a 3
      manopole qualsiasi delle sorgenti e della forma.
    I blocchi sono "posti" fissi con un TIPO (0 = vuoto) e manopole generiche A..D il cui significato dipende dal tipo:
    cosi' tutto resta un parametro normale (preset, automazioni, salvataggio).
*/
namespace materia
{
    inline constexpr int numSources = 4, numShapers = 6, numMovers = 6;
    inline constexpr int numKnobs = 4;          // A..D
    inline constexpr int numSteps = 8;          // sequenza
    inline constexpr int numTargets = 3;        // destinazioni per movimento
    inline constexpr int maxVoices = 8, maxUnison = 7;
    inline constexpr int lineSize = 4096;       // linee di ritardo di corde e risonatori (fino a ~12 Hz a 48 kHz)

    enum SourceType { srcEmpty = 0, srcOsc, srcWave, srcNoise, srcString, numSourceTypes };
    enum ShaperType { shEmpty = 0, shFilter, shDrive, shFold, shResonator, shCrush, shVowel, shChorus, shDelay, shReverb, numShaperTypes };
    enum MoverType  { mvEmpty = 0, mvEnv, mvLfo, mvSeq, mvRandom, mvNote, mvVelocity, mvWheel, numMoverTypes };
    inline bool isSpace (int shaperType) noexcept     { return shaperType >= shChorus; }   // sul suono intero, non per nota

    /** Com'e' fatto un tipo di blocco: nome, colore, cosa fanno le manopole A..D (nullptr = non c'e'), i modi. */
    struct ModuleInfo
    {
        const char* name;
        const char* hint;
        juce::uint32 colour;
        const char* knobs[numKnobs];
        float defaults[numKnobs];
        const char* modeTitle;               // nullptr = niente scelta di modo
        std::vector<const char*> modes;
        int defaultMode;
    };
    const ModuleInfo& sourceInfo (int type);
    const ModuleInfo& shaperInfo (int type);
    const ModuleInfo& moverInfo (int type);

    /** Destinazioni dei movimenti: 0 = niente, poi 8 per sorgente (A B C D, detune, nota, volume, pan), poi 4 per forma (A B C, mix). */
    inline constexpr int destsPerSource = 8, destsPerShaper = 4;
    inline constexpr int numDests = 1 + numSources * destsPerSource + numShapers * destsPerShaper;
    enum SourceDest { sdA = 0, sdB, sdC, sdD, sdDetune, sdPitch, sdLevel, sdPan };
    inline int sourceDest (int slot, int k) noexcept     { return 1 + slot * destsPerSource + k; }
    inline int shaperDest (int slot, int k) noexcept     { return 1 + numSources * destsPerSource + slot * destsPerShaper + k; }
    /** Nome leggibile della destinazione, coi tipi attuali dei blocchi ("Filtro 1: TAGLIO"). Vuoto se il blocco non c'e'. */
    juce::String destName (juce::AudioProcessorValueTreeState&, int dest);

    /** Testo del valore di una manopola (dipende dal tipo: Hz, passi, tempo...). */
    juce::String knobText (int group, int type, int knob, float value);   // group: 0 sorgenti, 1 forma, 2 movimento

    juce::AudioProcessorValueTreeState::ParameterLayout createLayout();

    juce::String sourceId (int slot, const char* suffix);   // "s1Type"
    juce::String shaperId (int slot, const char* suffix);   // "f1A"
    juce::String moverId (int slot, const char* suffix);    // "m1T1"

    struct Voice
    {
        bool active = false;
        int note = 60;
        float velocity = 1.0f;
        double pitch = 60.0, targetPitch = 60.0;
        juce::uint32 age = 0;
        prisma::Env amp;
        std::array<prisma::Env, numMovers> env;
        std::array<float, numMovers> random {};

        struct Src
        {
            std::array<double, maxUnison> phase {};
            float last = 0.0f;                       // ultima uscita (per l'FM della sorgente dopo)
            float lp = 0.0f, lp2 = 0.0f, hold = 0.0f, bp1 = 0.0f, bp2 = 0.0f;   // rumore
            int holdCount = 0;
            int write = 0; float damp = 0.0f, ap = 0.0f, apIn = 0.0f; int excite = 0; float exLp = 0.0f; bool primed = false;   // corda
        };
        std::array<Src, numSources> src;

        struct Shp { float s[2][6] {}; int write = 0; int count = 0; };
        std::array<Shp, numShapers> shp;
    };

    class Presets
    {
    public:
        Presets (juce::AudioProcessorValueTreeState& s) : apvts (s) {}
        static int numFactory();
        static juce::String factoryName (int i);
        void loadFactory (int i);
        juce::String currentName() const    { return apvts.state.getProperty ("presetName", "Vuoto").toString(); }
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
class MateriaAudioProcessor : public juce::AudioProcessor,
                              private juce::AudioProcessorValueTreeState::Listener
{
public:
    MateriaAudioProcessor();
    ~MateriaAudioProcessor() override;
    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override {}
    void reset() override;
    bool isBusesLayoutSupported (const BusesLayout& l) const override { return l.getMainOutputChannelSet() == juce::AudioChannelSet::stereo(); }
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;
    using AudioProcessor::processBlock;
    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override                 { return true; }
    const juce::String getName() const override     { return "Materia"; }
    bool acceptsMidi() const override               { return true; }
    bool producesMidi() const override              { return false; }
    bool isMidiEffect() const override              { return false; }
    double getTailLengthSeconds() const override    { return 4.0; }
    int getNumPrograms() override                   { return 1; }
    int getCurrentProgram() override                { return 0; }
    void setCurrentProgram (int) override {}
    const juce::String getProgramName (int) override { return {}; }
    void changeProgramName (int, const juce::String&) override {}
    void getStateInformation (juce::MemoryBlock&) override;
    void setStateInformation (const void*, int) override;

    materia::Presets& getPresets()                  { return presets; }
    juce::AudioProcessorValueTreeState apvts;

    /** Per l'interfaccia: aggiunge un blocco nel primo posto libero (coi valori di partenza del tipo). -1 = tutto pieno. */
    int addSource (int type);
    int addShaper (int type);
    int addMover (int type);
    void removeSource (int slot);
    void removeShaper (int slot);
    void removeMover (int slot);
    /** Sposta un blocco della forma su o giu' nella catena. */
    void moveShaper (int slot, int delta);
    /** Collega un movimento a una manopola (primo posto libero tra i 3). false = gia' 3 collegamenti. */
    bool connect (int mover, int dest, float amount);

    /** Per i test: stesse fasi e stesso rumore a ogni esecuzione. */
    void seedRandom (juce::int64 seed)              { rng.setSeed (seed); }

    /** Valore attuale di ogni movimento (-1..1) per i disegni; quello della nota piu' recente. */
    std::array<std::atomic<float>, materia::numMovers> shownMover {};
    std::array<std::atomic<float>, materia::numMovers> shownPhase {};

private:
    void parameterChanged (const juce::String&, float) override;
    void ensureTables();
    void resetSlot (const juce::String& prefix, const materia::ModuleInfo&, int type, bool isMover);
    struct Block;
    void readBlock (Block&);
    void noteOn (int note, float velocity, const Block&);
    void noteOff (int note, const Block&);
    void render (float* L, float* R, int n, const Block& b, double bpm, double ppq);

    materia::Presets presets { apvts };
    struct Ids;
    std::unique_ptr<Ids> ids;
    std::array<materia::Voice, materia::maxVoices> voices;
    std::vector<int> held;
    std::vector<float> stringLines, combLines;
    struct MoverState { double phase = 0.0; float hold = 0.0f, value = 0.0f, smooth = 0.0f; int step = 0, dir = 1; };
    std::array<MoverState, materia::numMovers> movers;
    std::array<forge::Effects, materia::numShapers> space;
    double sr = 48000.0;
    float pitchBend = 0.0f, modWheel = 0.0f;
    juce::uint32 ageCounter = 0;
    juce::Random rng;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (MateriaAudioProcessor)
};
