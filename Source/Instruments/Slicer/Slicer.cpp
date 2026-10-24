#include "../../Engine/Performance.h"
#include "Slicer.h"
#include "SlicerEditor.h"

namespace slicer
{
    static const juce::StringArray modeNames  { "Sui colpi", "4 parti", "8 parti", "16 parti" };
    static const juce::StringArray voiceNames { "Taglia (un pezzo alla volta)", "Sovrapponi" };

    juce::String noteName (int midi)
    {
        static const char* names[] { "C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B" };
        return juce::String (names[((midi % 12) + 12) % 12]) + juce::String (midi / 12 - 1);
    }

    juce::AudioProcessorValueTreeState::ParameterLayout createLayout()
    {
        using namespace juce;
        std::vector<std::unique_ptr<RangedAudioParameter>> p;
        auto pct = AudioParameterFloatAttributes().withStringFromValueFunction ([] (float v, int) { return String (roundToInt (v * 100.0f)) + "%"; });
        auto ms  = AudioParameterFloatAttributes().withStringFromValueFunction ([] (float v, int) { return String (roundToInt (v)) + " ms"; });
        NormalisableRange<float> rel (5.0f, 2000.0f); rel.setSkewForCentre (150.0f);
        NormalisableRange<float> att (0.0f, 80.0f); att.setSkewForCentre (10.0f);
        p.push_back (std::make_unique<AudioParameterChoice> (ParameterID { "mode", 1 }, "Taglio", modeNames, 0));
        p.push_back (std::make_unique<AudioParameterFloat> (ParameterID { "sens", 1 }, "Sensibilita'", NormalisableRange<float> (0.0f, 1.0f), 0.55f, pct));
        p.push_back (std::make_unique<AudioParameterFloat> (ParameterID { "attack", 1 }, "Attacco", att, 1.0f, ms));
        p.push_back (std::make_unique<AudioParameterFloat> (ParameterID { "release", 1 }, "Rilascio", rel, 80.0f, ms));
        p.push_back (std::make_unique<AudioParameterFloat> (ParameterID { "pitch", 1 }, "Intonazione", NormalisableRange<float> (-24.0f, 24.0f, 1.0f), 0.0f,
                                                            AudioParameterFloatAttributes().withStringFromValueFunction ([] (float v, int) { return (v > 0.5f ? "+" : "") + String (roundToInt (v)) + " st"; })));
        p.push_back (std::make_unique<AudioParameterChoice> (ParameterID { "voice", 1 }, "Voce", voiceNames, 0));
        p.push_back (std::make_unique<AudioParameterFloat> (ParameterID { "gain", 1 }, "Volume", NormalisableRange<float> (-24.0f, 6.0f), 0.0f,
                                                            AudioParameterFloatAttributes().withStringFromValueFunction ([] (float v, int) { return (v > 0.05f ? "+" : "") + String (v, 1) + " dB"; })));
        return { p.begin(), p.end() };
    }

    std::vector<int> findSlices (const juce::AudioBuffer<float>& audio, double sampleRate, int mode, float sensitivity)
    {
        const int n = audio.getNumSamples();
        std::vector<int> starts { 0 };
        if (n < 64)
            return starts;

        if (mode > 0)   // parti uguali
        {
            const int parts = mode == 1 ? 4 : mode == 2 ? 8 : 16;
            for (int i = 1; i < parts; ++i)
                starts.push_back ((int) ((juce::int64) n * i / parts));
            return starts;
        }

        // Sui colpi: energia (con un passa-alto leggero) a finestre di 256 campioni; un colpo e' un salto netto
        const float* x = audio.getReadPointer (0);
        const int hop = 256;
        const int frames = n / hop;
        std::vector<float> energy ((size_t) frames, 0.0f);
        float prev = 0.0f, peak = 1.0e-9f;
        for (int f = 0; f < frames; ++f)
        {
            double e = 0.0;
            for (int i = f * hop; i < (f + 1) * hop; ++i)
            {
                const float hp = x[i] - 0.92f * prev;
                prev = x[i];
                e += (double) hp * hp;
            }
            energy[(size_t) f] = (float) (e / hop);
            peak = juce::jmax (peak, energy[(size_t) f]);
        }

        const float jump = juce::jmap (sensitivity, 2.6f, 0.45f);       // salto minimo (logaritmo dell'energia)
        const float floorLevel = peak * juce::jmap (sensitivity, 0.02f, 0.0008f);
        const int minGap = juce::jmax (1, (int) (0.07 * sampleRate / hop));
        struct Onset { int frame; float strength; };
        std::vector<Onset> onsets;
        int last = -minGap;
        for (int f = 2; f < frames; ++f)
        {
            const float before = (energy[(size_t) (f - 1)] + energy[(size_t) (f - 2)]) * 0.5f;
            const float s = std::log ((energy[(size_t) f] + 1.0e-9f) / (before + 1.0e-9f));
            if (s > jump && energy[(size_t) f] > floorLevel && f - last >= minGap)
            {
                onsets.push_back ({ f, s });
                last = f;
            }
        }
        // Troppi pezzi: si tengono i colpi piu' forti
        if ((int) onsets.size() > maxSlices - 1)
        {
            std::sort (onsets.begin(), onsets.end(), [] (const Onset& a, const Onset& b) { return a.strength > b.strength; });
            onsets.resize ((size_t) (maxSlices - 1));
            std::sort (onsets.begin(), onsets.end(), [] (const Onset& a, const Onset& b) { return a.frame < b.frame; });
        }
        for (auto& o : onsets)
        {
            // Punto esatto: il campione piu' silenzioso poco prima del colpo (il taglio non fa click)
            const int centre = o.frame * hop;
            int best = centre;
            float bestLevel = 1.0e9f;
            for (int i = juce::jmax (1, centre - hop); i < juce::jmin (n - 1, centre + hop / 2); ++i)
                if (const float l = std::abs (x[i]); l < bestLevel) { bestLevel = l; best = i; }
            if (best > (int) (0.02 * sampleRate))
                starts.push_back (best);
        }
        return starts;
    }

    //==========================================================================
    struct Factory { const char* name; const char* sample; int mode; float sens, release; int voice; };
    static const std::vector<Factory>& factory()
    {
        static const std::vector<Factory> list
        {
            { "Init", "x_loop_vox_phrase", 0, 0.55f, 60.0f, 0 },
            { "Loop voce (chop)", "x_loop_vox_phrase", 0, 0.55f, 60.0f, 0 },
            { "Loop batteria electro", "x_loop_break", 3, 0.5f, 120.0f, 0 },
            { "Vox Aah a pezzi", "x_vox_aah", 2, 0.5f, 40.0f, 0 },
            { "Riser a pezzi", "x_fx_riser", 3, 0.5f, 30.0f, 0 },
            { "Vocal chop Oh", "edm_vox", 1, 0.5f, 50.0f, 1 },
        };
        return list;
    }

    int Presets::numFactory()                    { return (int) factory().size(); }
    juce::String Presets::factoryName (int i)    { return factory()[(size_t) juce::jlimit (0, numFactory() - 1, i)].name; }

    void Presets::set (const juce::String& id, float value)
    {
        if (auto* prm = apvts.getParameter (id))
            prm->setValueNotifyingHost (prm->convertTo0to1 (value));
    }

    void Presets::loadFactory (int i)
    {
        const auto& f = factory()[(size_t) juce::jlimit (0, numFactory() - 1, i)];
        for (auto* prm : apvts.processor.getParameters())
            if (auto* r = dynamic_cast<juce::RangedAudioParameter*> (prm))
                r->setValueNotifyingHost (r->getDefaultValue());
        set ("mode", (float) f.mode);
        set ("sens", f.sens);
        set ("release", f.release);
        set ("voice", (float) f.voice);
        apvts.state.setProperty ("sample", juce::String (f.sample).isEmpty() ? juce::String() : SampleStore::libraryName (f.sample), nullptr);
        apvts.state.setProperty ("presetName", f.name, nullptr);
        if (changed) changed();
    }

    juce::File Presets::userDir() const
    {
        return juce::File::getSpecialLocation (juce::File::userApplicationDataDirectory).getChildFile ("Slicer").getChildFile ("Presets");
    }

    juce::Array<juce::File> Presets::userPresets() const
    {
        auto files = userDir().findChildFiles (juce::File::findFiles, false, "*.slicerpreset");
        files.sort();
        return files;
    }

    bool Presets::save (const juce::String& name)
    {
        const auto clean = juce::File::createLegalFileName (name.trim());
        if (clean.isEmpty()) return false;
        userDir().createDirectory();
        apvts.state.setProperty ("presetName", clean, nullptr);
        if (auto xml = apvts.copyState().createXml())
            return xml->writeTo (userDir().getChildFile (clean + ".slicerpreset"));
        return false;
    }

    bool Presets::load (const juce::File& file)
    {
        auto xml = juce::XmlDocument::parse (file);
        if (xml == nullptr || ! xml->hasTagName (apvts.state.getType()))
            return false;
        apvts.replaceState (juce::ValueTree::fromXml (*xml));
        apvts.state.setProperty ("presetName", file.getFileNameWithoutExtension(), nullptr);
        if (changed) changed();
        return true;
    }
}

//==============================================================================
SlicerAudioProcessor::SlicerAudioProcessor()
    : AudioProcessor (BusesProperties().withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
      apvts (*this, nullptr, "Slicer", slicer::createLayout())
{
    sensP = apvts.getRawParameterValue ("sens"); modeP = apvts.getRawParameterValue ("mode");
    attackP = apvts.getRawParameterValue ("attack"); releaseP = apvts.getRawParameterValue ("release");
    pitchP = apvts.getRawParameterValue ("pitch"); voiceP = apvts.getRawParameterValue ("voice");
    gainP = apvts.getRawParameterValue ("gain");
    apvts.addParameterListener ("sens", this);
    apvts.addParameterListener ("mode", this);
    apvts.state.setProperty ("presetName", "Init", nullptr);
    apvts.state.setProperty ("sample", SampleStore::libraryName ("x_loop_vox_phrase"), nullptr);   // si parte con un loop di voce pronto
    recomputeSlices();
}

SlicerAudioProcessor::~SlicerAudioProcessor()
{
    apvts.removeParameterListener ("sens", this);
    apvts.removeParameterListener ("mode", this);
    cancelPendingUpdate();
}

bool SlicerAudioProcessor::setSample (const juce::String& storeName)
{
    if (storeName.isNotEmpty() && store.load (storeName) == nullptr)
        return false;
    apvts.state.setProperty ("sample", storeName, nullptr);
    apvts.state.setProperty ("presetName", SampleStore::displayName (storeName), nullptr);
    recomputeSlices();
    return true;
}

void SlicerAudioProcessor::recomputeSlices()
{
    auto* s = store.load (getSampleName());
    std::vector<int> starts;
    if (s != nullptr)
        starts = slicer::findSlices (s->audio, s->sampleRate, (int) modeP->load(), sensP->load());

    // Prima si "spegne" il numero di pezzi, poi si riscrivono le posizioni: il thread audio non legge pezzi a meta'
    numSlices.store (0);
    sample.store (s);
    for (size_t i = 0; i < starts.size() && i < (size_t) slicer::maxSlices; ++i)
        sliceStarts[i].store (starts[i]);
    const int count = juce::jmin ((int) starts.size(), slicer::maxSlices);
    sliceStarts[(size_t) count].store (s != nullptr ? s->audio.getNumSamples() : 0);
    numSlices.store (count);
}

void SlicerAudioProcessor::auditionSlice (int index)
{
    pendingAudition.store (index);
    perf::wake();   // il motore calcola lo strumento anche senza note MIDI
}

void SlicerAudioProcessor::prepareToPlay (double sampleRate, int)
{
    hostRate = sampleRate;
    reset();
}

void SlicerAudioProcessor::reset()
{
    for (auto& v : voices) v = {};
}

void SlicerAudioProcessor::startVoice (int note, float velocity)
{
    const auto* s = sample.load();
    const int count = numSlices.load();
    const int index = note - slicer::firstNote;
    if (s == nullptr || index < 0 || index >= count)
        return;

    if ((int) voiceP->load() == 0)   // "Taglia": il pezzo nuovo zittisce gli altri (con una sfumatura di 3 ms)
        for (auto& v : voices)
            if (v.active) { v.releasing = true; v.releaseCoef = (float) std::exp (-1.0 / (0.003 * hostRate)); }

    Voice* voice = nullptr;
    for (auto& v : voices) if (! v.active) { voice = &v; break; }
    if (voice == nullptr)
    {
        voice = &voices[0];
        for (auto& v : voices) if (v.releasing) { voice = &v; break; }
    }

    voice->active = true;
    voice->releasing = false;
    voice->note = note;
    voice->slice = index;
    voice->pos = (double) sliceStarts[(size_t) index].load();
    voice->end = (double) sliceStarts[(size_t) index + 1].load();
    voice->inc = s->sampleRate / hostRate * std::exp2 (pitchP->load() / 12.0);
    voice->gain = (0.35f + 0.65f * velocity) * juce::Decibels::decibelsToGain (gainP->load());
    voice->env = attackP->load() < 0.5f ? 1.0f : 0.0f;
    voice->attackStep = (float) (1.0 / juce::jmax (1.0, attackP->load() * 0.001 * hostRate));
    voice->releaseCoef = (float) std::exp (-4.6 / (juce::jmax (0.005, releaseP->load() * 0.001) * hostRate));
    lastSlice.store (index);
}

void SlicerAudioProcessor::stopNote (int note)
{
    for (auto& v : voices)
        if (v.active && v.note == note && ! v.releasing)
            v.releasing = true;
}

void SlicerAudioProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midi)
{
    juce::ScopedNoDenormals noDenormals;
    buffer.clear();
    const auto* s = sample.load();

    if (const int a = pendingAudition.exchange (-1); a >= 0)
        midi.addEvent (juce::MidiMessage::noteOn (1, slicer::firstNote + a, 0.9f), 0), midi.addEvent (juce::MidiMessage::noteOff (1, slicer::firstNote + a), juce::jmax (0, buffer.getNumSamples() - 1));

    auto* L = buffer.getWritePointer (0);
    auto* R = buffer.getNumChannels() > 1 ? buffer.getWritePointer (1) : L;
    const int n = buffer.getNumSamples();
    int done = 0;

    auto render = [&] (int from, int to)
    {
        if (s == nullptr) return;
        const float* src = s->audio.getReadPointer (0);
        const int len = s->audio.getNumSamples();
        const double fadeLen = 0.004 * s->sampleRate;   // gli ultimi 4 ms del pezzo sfumano
        for (auto& v : voices)
        {
            if (! v.active) continue;
            for (int i = from; i < to; ++i)
            {
                if (v.pos >= v.end - 1.0 || v.pos >= len - 1)
                {
                    v.active = false;
                    break;
                }
                if (v.releasing)
                {
                    v.env *= v.releaseCoef;
                    if (v.env < 1.0e-4f) { v.active = false; break; }
                }
                else if (v.env < 1.0f)
                    v.env = juce::jmin (1.0f, v.env + v.attackStep);
                const int ip = (int) v.pos;
                const float f = (float) (v.pos - ip);
                const float x = src[ip] + f * (src[ip + 1] - src[ip]);
                const float edge = (float) juce::jmin (1.0, (v.end - v.pos) / fadeLen);
                const float y = x * v.env * v.gain * edge;
                L[i] += y;
                R[i] += y;
                v.pos += v.inc;
            }
        }
    };

    for (const auto meta : midi)
    {
        const int pos = juce::jlimit (0, n, meta.samplePosition);
        render (done, pos);
        done = pos;
        const auto msg = meta.getMessage();
        if (msg.isNoteOn())                                    startVoice (msg.getNoteNumber(), msg.getFloatVelocity());
        else if (msg.isNoteOff())                              stopNote (msg.getNoteNumber());
        else if (msg.isAllNotesOff() || msg.isAllSoundOff())   for (auto& v : voices) if (v.active) v.releasing = true;
    }
    render (done, n);
}

juce::AudioProcessorEditor* SlicerAudioProcessor::createEditor()    { return new SlicerEditor (*this); }

void SlicerAudioProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    if (auto xml = apvts.copyState().createXml())
        copyXmlToBinary (*xml, destData);
}

void SlicerAudioProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    if (auto xml = getXmlFromBinary (data, sizeInBytes))
        if (xml->hasTagName (apvts.state.getType()))
        {
            apvts.replaceState (juce::ValueTree::fromXml (*xml));
            recomputeSlices();
        }
}
