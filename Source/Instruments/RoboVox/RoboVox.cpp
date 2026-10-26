#include "../../Engine/Performance.h"
#include "RoboVox.h"
#include "RoboVoxEditor.h"

namespace rv
{
    static const char* ids[] { "bands", "carrier", "octave", "trig", "formant", "mix", "sibil", "gate", "attack", "release",
                               "detune", "noteRel", "noise", "chorus", "dlyMix", "dlyTime", "dlyFb", "revMix", "revSize", "master" };
    enum { iBands, iCarrier, iOctave, iTrig, iFormant, iMix, iSibil, iGate, iAttack, iRelease,
           iDetune, iNoteRel, iNoise, iChorus, iDlyMix, iDlyTime, iDlyFb, iRevMix, iRevSize, iMaster, numIds };

    static inline float mtof (float m) noexcept { return 440.0f * std::exp2 ((m - 69.0f) / 12.0f); }
    static inline float blep (float t, float dt) noexcept
    {
        if (t < dt)        { const float x = t / dt; return x + x - x * x - 1.0f; }
        if (t > 1.0f - dt) { const float x = (t - 1.0f) / dt; return x * x + x + x + 1.0f; }
        return 0.0f;
    }

    juce::StringArray libraryVoices()
    {
        return { "x_loop_vox_phrase", "x_loop_vox_robot", "x_loop_vox_count", "voice_solo", "choir_aah", "x_vox_aah", "x_vox_yeah", "x_vox_hey", "x_vox_chop", "edm_vox" };
    }

    juce::AudioProcessorValueTreeState::ParameterLayout createLayout()
    {
        using namespace juce;
        std::vector<std::unique_ptr<RangedAudioParameter>> v;
        auto pct = [] (float x, int) { return String (roundToInt (x * 100.0f)) + "%"; };
        auto ms = [] (float x, int) { return x < 1.0f ? String (roundToInt (x * 1000.0f)) + " ms" : String (x, 2) + " s"; };
        auto flt = [&v] (const char* id, const char* name, NormalisableRange<float> r, float def, std::function<String (float, int)> t)
        { v.push_back (std::make_unique<AudioParameterFloat> (ParameterID { id, 1 }, name, r, def, AudioParameterFloatAttributes().withStringFromValueFunction (std::move (t)))); };
        auto skew = [] (float a, float b, float c) { NormalisableRange<float> r (a, b); r.setSkewForCentre (c); return r; };
        auto choice = [&v] (const char* id, const char* name, StringArray o, int def) { v.push_back (std::make_unique<AudioParameterChoice> (ParameterID { id, 1 }, name, o, def)); };

        choice ("bands", "Bande", { "8", "16", "24" }, 1);
        choice ("carrier", "Portante", { "Supersaw", "Saw", "Quadra", "Robot" }, 0);
        choice ("octave", "Ottava", { "-2", "-1", "0", "+1" }, 2);
        choice ("trig", "Voce", { "Dalla nota", "A tempo" }, 0);
        flt ("formant", "Formante", { -1.0f, 1.0f }, 0.0f, [] (float x, int) { return std::abs (x) < 0.02f ? String ("0") : (x > 0 ? "+" : "") + String (roundToInt (x * 12.0f)) + " st"; });
        flt ("mix", "Mix", { 0.0f, 1.0f }, 1.0f, pct);
        flt ("sibil", "Sibilanti", { 0.0f, 1.0f }, 0.3f, pct);
        flt ("gate", "Gate", { 0.0f, 1.0f }, 0.1f, pct);
        flt ("attack", "Attacco", skew (0.001f, 0.1f, 0.01f), 0.004f, ms);
        flt ("release", "Rilascio", skew (0.01f, 0.6f, 0.08f), 0.06f, ms);
        flt ("detune", "Detune", { 0.0f, 1.0f }, 0.3f, pct);
        flt ("noteRel", "Rilascio note", skew (0.01f, 2.0f, 0.2f), 0.12f, ms);
        flt ("noise", "Rumore", { 0.0f, 1.0f }, 0.08f, pct);
        flt ("chorus", "Chorus", { 0.0f, 1.0f }, 0.0f, pct);
        flt ("dlyMix", "Delay", { 0.0f, 1.0f }, 0.0f, pct);
        choice ("dlyTime", "Tempo delay", { "1/16", "1/8", "3/16", "1/4", "3/8", "1/2" }, 2);
        flt ("dlyFb", "Feedback", { 0.0f, 0.9f }, 0.35f, pct);
        flt ("revMix", "Riverbero", { 0.0f, 1.0f }, 0.0f, pct);
        flt ("revSize", "Stanza", { 0.0f, 1.0f }, 0.5f, pct);
        flt ("master", "Volume", { -24.0f, 6.0f }, 0.0f, [] (float x, int) { return (x > 0.05f ? "+" : "") + String (x, 1) + " dB"; });
        return { v.begin(), v.end() };
    }

    //==========================================================================
    void Presets::set (const juce::String& id, float value)
    {
        if (auto* prm = apvts.getParameter (id))
            prm->setValueNotifyingHost (prm->convertTo0to1 (value));
    }

    struct Factory { const char* name; std::vector<std::pair<const char*, float>> values; };
    static const std::vector<Factory>& factory()
    {
        // bands 0=8 1=16 2=24 | carrier 0 supersaw 1 saw 2 quadra 3 robot | octave 2 = 0
        static const std::vector<Factory> list
        {
            { "Init", {} },
            { "Robot Francese", { { "carrier", 1 }, { "bands", 1 }, { "attack", 0.002f }, { "release", 0.04f }, { "sibil", 0.35f }, { "chorus", 0.35f }, { "master", -0.0f } } },
            { "Coro Robotico", { { "carrier", 0 }, { "bands", 2 }, { "detune", 0.55f }, { "release", 0.09f }, { "noteRel", 0.5f }, { "revMix", 0.35f }, { "revSize", 0.75f }, { "master", -5.6f } } },
            { "Basso Parlante", { { "carrier", 2 }, { "octave", 1 }, { "bands", 1 }, { "formant", -0.3f }, { "sibil", 0.15f }, { "noteRel", 0.06f }, { "master", -0.6f } } },
            { "Voce Elettro 80", { { "carrier", 3 }, { "bands", 0 }, { "formant", 0.2f }, { "dlyMix", 0.2f }, { "release", 0.05f }, { "master", -1.0f } } },
            { "Talkbox Funk", { { "carrier", 1 }, { "bands", 2 }, { "formant", 0.15f }, { "sibil", 0.45f }, { "attack", 0.002f }, { "release", 0.035f }, { "noise", 0.04f }, { "master", 0.4f } } },
            { "Alieno", { { "carrier", 3 }, { "bands", 1 }, { "formant", 0.75f }, { "chorus", 0.4f }, { "dlyMix", 0.15f }, { "master", 1.2f } } },
            { "Gigante", { { "carrier", 0 }, { "octave", 1 }, { "bands", 1 }, { "formant", -0.75f }, { "revMix", 0.2f }, { "master", -6.0f } } },
            { "Sussurro Robot", { { "carrier", 1 }, { "bands", 2 }, { "sibil", 0.8f }, { "noise", 0.35f }, { "mix", 0.85f }, { "revMix", 0.25f }, { "master", -4.0f } } },
        };
        return list;
    }

    int Presets::numFactory()                    { return (int) factory().size(); }
    juce::String Presets::factoryName (int i)    { return factory()[(size_t) juce::jlimit (0, numFactory() - 1, i)].name; }

    void Presets::loadFactory (int i)
    {
        const auto& f = factory()[(size_t) juce::jlimit (0, numFactory() - 1, i)];
        for (auto* prm : apvts.processor.getParameters())
            if (auto* r = dynamic_cast<juce::RangedAudioParameter*> (prm))
                r->setValueNotifyingHost (r->getDefaultValue());
        for (auto& [id, value] : f.values) set (id, value);
        apvts.state.setProperty ("presetName", f.name, nullptr);
    }

    juce::File Presets::userDir() const
    {
        return juce::File::getSpecialLocation (juce::File::userApplicationDataDirectory).getChildFile ("RoboVox").getChildFile ("Presets");
    }
    juce::Array<juce::File> Presets::userPresets() const
    {
        auto files = userDir().findChildFiles (juce::File::findFiles, false, "*.rvpreset");
        files.sort();
        return files;
    }
    bool Presets::save (const juce::String& name)
    {
        const auto clean = juce::File::createLegalFileName (name.trim());
        if (clean.isEmpty()) return false;
        userDir().createDirectory();
        apvts.state.setProperty ("presetName", clean, nullptr);
        if (auto xml = apvts.copyState().createXml()) return xml->writeTo (userDir().getChildFile (clean + ".rvpreset"));
        return false;
    }
    bool Presets::load (const juce::File& file)
    {
        auto xml = juce::XmlDocument::parse (file);
        if (xml == nullptr || ! xml->hasTagName (apvts.state.getType())) return false;
        const auto keepSample = apvts.state.getProperty ("sample");   // il preset non cambia la voce scelta
        apvts.replaceState (juce::ValueTree::fromXml (*xml));
        apvts.state.setProperty ("sample", keepSample, nullptr);
        apvts.state.setProperty ("presetName", file.getFileNameWithoutExtension(), nullptr);
        return true;
    }
}

//==============================================================================
void RoboVoxAudioProcessor::Coef::set (float freq, float q, double rate)
{
    const float g = std::tan (juce::MathConstants<float>::pi * juce::jlimit (20.0f, (float) rate * 0.45f, freq) / (float) rate);
    k = 1.0f / q;
    a1 = 1.0f / (1.0f + g * (g + k)); a2 = g * a1; a3 = g * a2;
}

RoboVoxAudioProcessor::RoboVoxAudioProcessor()
    : AudioProcessor (BusesProperties().withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
      apvts (*this, nullptr, "RoboVox", rv::createLayout())
{
    for (auto* id : rv::ids) p.push_back (apvts.getRawParameterValue (id));
    jassert ((int) p.size() == rv::numIds);
    apvts.state.setProperty ("presetName", "Init", nullptr);
    setSample (SampleStore::libraryName ("x_loop_vox_phrase"));
    apvts.state.setProperty ("presetName", "Init", nullptr);
}

bool RoboVoxAudioProcessor::setSample (const juce::String& storeName)
{
    auto* s = store.load (storeName);
    if (storeName.isNotEmpty() && s == nullptr)
        return false;
    sample.store (s);
    apvts.state.setProperty ("sample", storeName, nullptr);
    return true;
}

void RoboVoxAudioProcessor::prepareToPlay (double sampleRate, int)
{
    sr = sampleRate;
    fx.prepare (sr);
    lastFormant = -99.0f;
    reset();
}

void RoboVoxAudioProcessor::reset()
{
    for (auto& v : voices) v = {};
    modA = {}; modB = {}; carA = {}; carB = {};
    env = {};
    compEnv = activeEnv = gateEnv = levelEnv = prevMod = sibLp1 = sibLp2 = 0.0f;
    modPos = 0.0;
    fx.reset();
}

rv::BlockParams RoboVoxAudioProcessor::readParams()
{
    using namespace rv;
    static constexpr double delayBeats[] { 0.25, 0.5, 0.75, 1.0, 1.5, 2.0 };
    BlockParams b;
    b.bands = juce::jmin (perf::lowPower() ? 16 : 24, 8 * (1 + (int) p[iBands]->load()));
    b.carrier = (int) p[iCarrier]->load(); b.octave = (int) p[iOctave]->load() - 2; b.trig = (int) p[iTrig]->load();
    b.formant = p[iFormant]->load(); b.mix = p[iMix]->load(); b.sibil = p[iSibil]->load(); b.gate = p[iGate]->load();
    b.attack = p[iAttack]->load(); b.release = p[iRelease]->load();
    b.detune = p[iDetune]->load(); b.noteRel = p[iNoteRel]->load(); b.noise = p[iNoise]->load();
    b.fx.chorus = p[iChorus]->load(); b.fx.delayMix = p[iDlyMix]->load(); b.fx.delayFeedback = p[iDlyFb]->load();
    b.fx.delayBeats = delayBeats[juce::jlimit (0, 5, (int) p[iDlyTime]->load())];
    b.fx.reverbMix = p[iRevMix]->load(); b.fx.reverbSize = p[iRevSize]->load();
    b.fx.gain = juce::Decibels::decibelsToGain (p[iMaster]->load());
    return b;
}

void RoboVoxAudioProcessor::updateBands (const rv::BlockParams& b)
{
    if (b.bands == bandsUsed && std::abs (b.formant - lastFormant) < 1.0e-4f) return;
    const bool countChanged = b.bands != bandsUsed;
    bandsUsed = b.bands;
    lastFormant = b.formant;
    const float lo = 120.0f, hi = 7500.0f;
    const float ratio = std::pow (hi / lo, 1.0f / (float) (bandsUsed - 1));
    // Q in modo che le bande si tocchino (due filtri in fila: un po' piu' larghe)
    const float q = 1.2f / (std::sqrt (ratio) - 1.0f / std::sqrt (ratio)) * 0.7f;
    const float shift = std::exp2 (b.formant);
    for (int i = 0; i < bandsUsed; ++i)
    {
        const float f = lo * std::pow (ratio, (float) i);
        modC[(size_t) i].set (f, q, sr);
        carC[(size_t) i].set (f * shift, q, sr);
    }
    if (countChanged) { modA = {}; modB = {}; carA = {}; carB = {}; env = {}; }
    numBandsShown.store (bandsUsed);
}

void RoboVoxAudioProcessor::noteOn (int note, float vel)
{
    juce::ignoreUnused (vel);
    bool anyOn = false;
    for (auto& v : voices) anyOn = anyOn || v.on;
    if (! anyOn) anyHeldBefore = false;
    Voice* best = nullptr;
    for (auto& v : voices) if (v.note == note) best = &v;
    if (best == nullptr) for (auto& v : voices) if (! v.on && v.env < 1.0e-4f) { best = &v; break; }
    if (best == nullptr) for (auto& v : voices) if (! v.on && (best == nullptr || v.env < best->env)) best = &v;
    if (best == nullptr) for (auto& v : voices) if (best == nullptr || v.age < best->age) best = &v;
    if (best->env < 1.0e-4f)
        for (auto& x : best->ph) x = rng.nextDouble();
    best->note = note;
    best->on = true;
    best->age = ++ageCounter;
}

void RoboVoxAudioProcessor::noteOff (int note)
{
    for (auto& v : voices) if (v.note == note) v.on = false;
}

void RoboVoxAudioProcessor::render (float* L, float* R, int n, const rv::BlockParams& b, double bpm, double ppq)
{
    if (n <= 0) return;
    updateBands (b);
    const auto* s = sample.load();
    const int len = s != nullptr ? s->audio.getNumSamples() : 0;
    const float* src = len > 0 ? s->audio.getReadPointer (0) : nullptr;
    const double step = s != nullptr ? s->sampleRate / sr : 1.0;

    bool anyOn = false;
    for (auto& v : voices) anyOn = anyOn || v.on;
    // la voce riparte dall'inizio a ogni accordo nuovo (o segue il brano, "a tempo")
    if (anyOn && ! anyHeldBefore && activeEnv < 0.02f && b.trig == 0) modPos = 0.0;
    anyHeldBefore = anyOn;
    if (b.trig == 1 && ppq >= 0.0 && len > 0)
        modPos = std::fmod (ppq * 60.0 / bpm * s->sampleRate, (double) len);

    const float atk = (float) (1.0 - std::exp (-1.0 / (b.attack * sr))), rel = (float) (1.0 - std::exp (-1.0 / (b.release * sr)));
    const float noteAtk = (float) (1.0 / (0.004 * sr)), noteRelC = (float) std::exp (-4.6 / (b.noteRel * sr));
    const float sibC = (float) (1.0 - std::exp (-juce::MathConstants<double>::twoPi * 4000.0 / sr));
    const float thr = b.gate * b.gate * 0.08f;
    const float compAtk = (float) (1.0 - std::exp (-1.0 / (0.003 * sr))), compRel = (float) (1.0 - std::exp (-1.0 / (0.12 * sr)));
    const int nb = bandsUsed;
    const float outGain = 60.0f / std::sqrt ((float) nb);
    constexpr float tau = juce::MathConstants<float>::twoPi;

    // frequenze delle voci del synth
    float inc[rv::maxVoices][3];
    for (size_t vi = 0; vi < voices.size(); ++vi)
        for (int u = 0; u < 3; ++u)
            inc[vi][u] = rv::mtof ((float) voices[vi].note + (float) (b.octave * 12) + (b.carrier == 0 ? (float) (u - 1) * b.detune * 0.25f : 0.0f)) / (float) sr;

    std::array<float, rv::maxBands> peak {};
    for (int i = 0; i < n; ++i)
    {
        // --- voce (modulante)
        float mod = 0.0f;
        const bool running = anyOn || activeEnv > 1.0e-4f;
        if (src != nullptr && running)
        {
            const int i0 = (int) modPos;
            const float fr = (float) (modPos - i0);
            mod = src[i0] + fr * (src[(i0 + 1) % len] - src[i0]);
            modPos += step;
            if (modPos >= (double) len) modPos -= (double) len;
        }
        const float emph = mod - 0.5f * prevMod;   // un po' di acuti in piu': si capiscono meglio le parole
        prevMod = mod;

        // gate sul volume della voce
        const float lv = std::abs (mod);
        levelEnv += (lv - levelEnv) * (lv > levelEnv ? 0.01f : 0.0005f);
        const float gTarget = thr <= 0.0f ? 1.0f : juce::jlimit (0.0f, 1.0f, (levelEnv - thr) / thr);
        gateEnv += (gTarget - gateEnv) * 0.002f;

        // --- synth (portante)
        float car = 0.0f, active = 0.0f;
        for (size_t vi = 0; vi < voices.size(); ++vi)
        {
            auto& v = voices[vi];
            if (! v.on && v.env < 1.0e-5f) { v.env = 0.0f; continue; }
            v.env = v.on ? juce::jmin (1.0f, v.env + noteAtk) : v.env * noteRelC;
            active = juce::jmax (active, v.env);
            float o = 0.0f;
            const int count = b.carrier == 0 ? 3 : 1;
            for (int u = 0; u < count; ++u)
            {
                const float t = (float) v.ph[u], dt = inc[vi][u];
                if (b.carrier == 2)
                {
                    float t2 = t + 0.5f; t2 -= std::floor (t2);
                    o += (t < 0.5f ? 1.0f : -1.0f) + rv::blep (t, dt) - rv::blep (t2, dt);
                }
                else if (b.carrier == 3)
                {
                    float t2 = t + 0.88f; t2 -= std::floor (t2);
                    o += (t < 0.12f ? 1.0f : -0.14f) + rv::blep (t, dt) - rv::blep (t2, dt);
                }
                else
                    o += 2.0f * t - 1.0f - rv::blep (t, dt);
                v.ph[u] += dt; if (v.ph[u] >= 1.0) v.ph[u] -= 1.0;
            }
            car += o * v.env * (b.carrier == 0 ? 0.5f : 0.8f);
        }
        activeEnv = active;
        car += (rng.nextFloat() * 2.0f - 1.0f) * b.noise * 0.6f * active;

        // --- bande
        float voc = 0.0f;
        for (int k = 0; k < nb; ++k)
        {
            const auto& mc = modC[(size_t) k];
            const auto& cc = carC[(size_t) k];
            const float mb = modB[(size_t) k].bp (modA[(size_t) k].bp (emph, mc.a1, mc.a2, mc.a3, mc.k), mc.a1, mc.a2, mc.a3, mc.k);
            const float a = std::abs (mb);
            auto& e = env[(size_t) k];
            e += (a - e) * (a > e ? atk : rel);
            if (e > peak[(size_t) k]) peak[(size_t) k] = e;
            const float cb = carB[(size_t) k].bp (carA[(size_t) k].bp (car, cc.a1, cc.a2, cc.a3, cc.k), cc.a1, cc.a2, cc.a3, cc.k);
            voc += cb * e;
        }
        voc *= outGain * gateEnv;
        // compressore: le sillabe deboli e quelle forti escono simili (il vocoder "spinge")
        {
            const float av = std::abs (voc);
            compEnv += (av - compEnv) * (av > compEnv ? compAtk : compRel);
            if (compEnv > 0.08f) voc *= std::pow (compEnv / 0.08f, -0.75f);
            voc *= 2.5f;
        }

        // sibilanti: le alte frequenze della voce passano dirette mentre suona un accordo
        sibLp1 += (mod - sibLp1) * sibC;
        const float h1 = mod - sibLp1;
        sibLp2 += (h1 - sibLp2) * sibC;
        const float sib = (h1 - sibLp2) * b.sibil * 1.5f * active;

        const float out = voc * b.mix + mod * (1.0f - b.mix) * 0.6f * active + sib;
        L[i] += out;
        R[i] += out;
    }

    for (int k = 0; k < rv::maxBands; ++k)
    {
        const float target = k < nb ? juce::jmin (1.0f, std::sqrt (peak[(size_t) k] * 4.0f)) : 0.0f;
        const float old = bandLevel[(size_t) k].load (std::memory_order_relaxed);
        bandLevel[(size_t) k].store (target > old ? target : old * 0.85f + target * 0.15f, std::memory_order_relaxed);
    }

    fx.process (L, R, n, b.fx, bpm);
    for (int i = 0; i < n; ++i)
    {
        auto soft = [] (float v) { const float av = std::abs (v); return av < 0.8f ? v : std::copysign (0.8f + 0.2f * std::tanh ((av - 0.8f) * 5.0f), v); };
        L[i] = soft (L[i]);
        if (R != L) R[i] = soft (R[i]);
    }
}

void RoboVoxAudioProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midi)
{
    juce::ScopedNoDenormals nd;
    buffer.clear();
    double bpm = 120.0, ppq = -1.0;
    if (auto* ph = getPlayHead())
        if (auto pos = ph->getPosition())
        {
            if (auto bp = pos->getBpm()) bpm = *bp;
            if (pos->getIsPlaying()) if (auto q = pos->getPpqPosition()) ppq = *q;
        }
    const auto b = readParams();
    auto* L = buffer.getWritePointer (0);
    auto* R = buffer.getNumChannels() > 1 ? buffer.getWritePointer (1) : L;
    int done = 0;
    for (const auto meta : midi)
    {
        const int pos = juce::jlimit (0, buffer.getNumSamples(), meta.samplePosition);
        render (L + done, R + done, pos - done, b, bpm, ppq >= 0.0 ? ppq + bpm / 60.0 * done / sr : -1.0);
        done = pos;
        const auto m = meta.getMessage();
        if (m.isNoteOn()) noteOn (m.getNoteNumber(), m.getFloatVelocity());
        else if (m.isNoteOff()) noteOff (m.getNoteNumber());
        else if (m.isAllNotesOff() || m.isAllSoundOff()) for (auto& v : voices) v.on = false;
    }
    render (L + done, R + done, buffer.getNumSamples() - done, b, bpm, ppq >= 0.0 ? ppq + bpm / 60.0 * done / sr : -1.0);
}

juce::AudioProcessorEditor* RoboVoxAudioProcessor::createEditor()   { return new RoboVoxEditor (*this); }

void RoboVoxAudioProcessor::getStateInformation (juce::MemoryBlock& d)
{
    if (auto xml = apvts.copyState().createXml()) copyXmlToBinary (*xml, d);
}

void RoboVoxAudioProcessor::setStateInformation (const void* data, int size)
{
    if (auto xml = getXmlFromBinary (data, size))
        if (xml->hasTagName (apvts.state.getType()))
        {
            apvts.replaceState (juce::ValueTree::fromXml (*xml));
            const auto name = getSampleName();
            sample.store (store.load (name));
        }
}
