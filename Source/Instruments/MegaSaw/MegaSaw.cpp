#include "../../Engine/Performance.h"
#include "MegaSaw.h"
#include "MegaSawEditor.h"

namespace ms
{
    // Ordine dei parametri (indice in MegaSawAudioProcessor::p)
    static const char* ids[] { "mode", "unison", "detune", "spread", "hoover", "scoop", "sub", "octave", "thick", "drive",
                               "cutoff", "reso", "fEnv", "fDecay", "aA", "aD", "aS", "aR", "glide", "playMode",
                               "chorus", "dlyMix", "dlyTime", "dlyFb", "revMix", "revSize", "master" };
    enum { iMode, iUnison, iDetune, iSpread, iHoover, iScoop, iSub, iOctave, iThick, iDrive, iCutoff, iReso, iFEnv, iFDecay,
           iA, iD, iS, iR, iGlide, iPlay, iChorus, iDlyMix, iDlyTime, iDlyFb, iRevMix, iRevSize, iMaster, numIds };

    static inline float mtof (float m) noexcept { return 440.0f * std::exp2 ((m - 69.0f) / 12.0f); }
    static inline float blep (float t, float dt) noexcept
    {
        if (t < dt)        { const float x = t / dt; return x + x - x * x - 1.0f; }
        if (t > 1.0f - dt) { const float x = (t - 1.0f) / dt; return x * x + x + x + 1.0f; }
        return 0.0f;
    }

    juce::AudioProcessorValueTreeState::ParameterLayout createLayout()
    {
        using namespace juce;
        std::vector<std::unique_ptr<RangedAudioParameter>> v;
        auto pct = [] (float x, int) { return String (roundToInt (x * 100.0f)) + "%"; };
        auto sec = [] (float x, int) { return x < 1.0f ? String (roundToInt (x * 1000.0f)) + " ms" : String (x, 2) + " s"; };
        auto flt = [&v] (const char* id, const char* name, NormalisableRange<float> r, float def, std::function<String (float, int)> t)
        { v.push_back (std::make_unique<AudioParameterFloat> (ParameterID { id, 1 }, name, r, def, AudioParameterFloatAttributes().withStringFromValueFunction (std::move (t)))); };
        auto skew = [] (float a, float b, float c) { NormalisableRange<float> r (a, b); r.setSkewForCentre (c); return r; };
        auto choice = [&v] (const char* id, const char* name, StringArray o, int def) { v.push_back (std::make_unique<AudioParameterChoice> (ParameterID { id, 1 }, name, o, def)); };

        choice ("mode", "Modo", { "Supersaw", "Hoover", "Reese" }, 0);
        flt ("unison", "Voci", { 1.0f, 9.0f, 1.0f }, 7.0f, [] (float x, int) { return String (roundToInt (x)); });
        flt ("detune", "Detune", { 0.0f, 1.0f }, 0.4f, pct);
        flt ("spread", "Apertura", { 0.0f, 1.0f }, 0.8f, pct);
        flt ("hoover", "Hoover", { 0.0f, 1.0f }, 0.0f, pct);
        flt ("scoop", "Pitch scoop", { 0.0f, 12.0f }, 0.0f, [] (float x, int) { return String (x, 1) + " st"; });
        flt ("sub", "Sub", { 0.0f, 1.0f }, 0.0f, pct);
        choice ("octave", "Ottava", { "-2", "-1", "0", "+1" }, 2);
        flt ("thick", "Spessore", { 0.0f, 1.0f }, 0.5f, pct);
        flt ("drive", "Drive", { 0.0f, 1.0f }, 0.0f, pct);
        flt ("cutoff", "Cutoff", skew (60.0f, 18000.0f, 2000.0f), 9000.0f, [] (float x, int) { return x >= 1000.0f ? String (x / 1000.0f, 1) + " kHz" : String (roundToInt (x)) + " Hz"; });
        flt ("reso", "Risonanza", { 0.0f, 1.0f }, 0.1f, pct);
        flt ("fEnv", "Inviluppo filtro", { 0.0f, 1.0f }, 0.2f, pct);
        flt ("fDecay", "Decay filtro", skew (0.01f, 3.0f, 0.3f), 0.3f, sec);
        flt ("aA", "Attacco", skew (0.001f, 3.0f, 0.2f), 0.004f, sec);
        flt ("aD", "Decay", skew (0.01f, 4.0f, 0.5f), 0.6f, sec);
        flt ("aS", "Sustain", { 0.0f, 1.0f }, 1.0f, pct);
        flt ("aR", "Rilascio", skew (0.005f, 4.0f, 0.4f), 0.2f, sec);
        flt ("glide", "Glide", skew (0.0f, 1.0f, 0.15f), 0.0f, sec);
        choice ("playMode", "Voce", { "Poly", "Mono", "Legato" }, 0);
        flt ("chorus", "Chorus", { 0.0f, 1.0f }, 0.0f, pct);
        flt ("dlyMix", "Delay", { 0.0f, 1.0f }, 0.0f, pct);
        choice ("dlyTime", "Tempo delay", { "1/16", "1/8", "3/16", "1/4", "3/8", "1/2" }, 2);
        flt ("dlyFb", "Feedback", { 0.0f, 0.9f }, 0.35f, pct);
        flt ("revMix", "Riverbero", { 0.0f, 1.0f }, 0.0f, pct);
        flt ("revSize", "Stanza", { 0.0f, 1.0f }, 0.6f, pct);
        flt ("master", "Volume", { -24.0f, 6.0f }, 0.0f, [] (float x, int) { return (x > 0.05f ? "+" : "") + String (x, 1) + " dB"; });
        return { v.begin(), v.end() };
    }

    void Env::set (double sr, float a, float d, float s, float r) noexcept
    {
        ai = (float) (1.0 / (juce::jmax (0.0005, (double) a) * sr));
        dc = (float) std::exp (-4.6 / (juce::jmax (0.005, (double) d) * sr));
        rc = (float) std::exp (-4.6 / (juce::jmax (0.005, (double) r) * sr));
        sl = s;
    }

    float Env::next() noexcept
    {
        switch (stage)
        {
            case attack:  value += ai; if (value >= 1.0f) { value = 1.0f; stage = decay; } break;
            case decay:   value = sl + (value - sl) * dc; if (std::abs (value - sl) < 1.0e-4f) stage = sustain; break;
            case sustain: value = sl; if (sl <= 0.0f) stage = idle; break;
            case release: value *= rc; if (value < 1.0e-4f) { value = 0.0f; stage = idle; } break;
            default: break;
        }
        return value;
    }

    //==========================================================================
    void Voice::start (int n, float v, float fromPitch, bool retrigger)
    {
        const bool was = isActive();
        note = n; vel = v; target = (float) n;
        pitch = fromPitch >= 0.0f ? fromPitch : target;
        if (! was)
        {
            // fasi casuali: l'unison suona "largo" fin dal primo istante
            juce::Random r ((juce::int64) n * 7919 + 13);
            for (auto& x : ph) x = r.nextDouble();
            for (auto& x : phPulse) x = r.nextDouble();
            s1[0] = s1[1] = s2[0] = s2[1] = 0.0f;
        }
        if (retrigger || ! was)
        {
            amp.stage = Env::attack;
            scoopEnv = 1.0f;
            fenv = 1.0f;
        }
    }

    void Voice::render (float* L, float* R, int n, const BlockParams& p, float glideCoef, int unisonLimit)
    {
        if (! isActive()) return;
        amp.set (sr, p.aA, p.aD, p.aS, p.aR);
        pitch = glideCoef > 0.0f ? target + (pitch - target) * glideCoef : target;
        scoopEnv *= (float) std::exp (-(double) n / (0.09 * sr));
        fenv *= (float) std::exp (-(double) n / (juce::jmax (0.01f, p.fDecay) * 0.4 * sr));
        reeseLfo += (float) n * 0.23f / (float) sr;
        if (reeseLfo > 1.0f) reeseLfo -= 1.0f;
        pwm += (float) n * 0.9f / (float) sr;
        if (pwm > 1.0f) pwm -= 1.0f;

        const int uni = juce::jlimit (1, juce::jmin (unisonLimit, maxUnison), p.unison);
        const float base = pitch + (float) (p.octave * 12) - p.scoop * scoopEnv;
        const float detune = p.detune * (0.35f + 0.65f * p.thick) * (p.mode == 2 ? 0.45f : 1.0f);
        float inc[maxUnison], panL[maxUnison], panR[maxUnison];
        for (int u = 0; u < uni; ++u)
        {
            // stonature distribuite (come i supersaw classici): le esterne piu' lontane
            const float pos = uni == 1 ? 0.0f : (float) u / (float) (uni - 1) * 2.0f - 1.0f;
            float cents = pos * std::abs (pos) * 55.0f * detune;
            if (p.mode == 2) cents *= 1.0f + 0.5f * std::sin (juce::MathConstants<float>::twoPi * (reeseLfo + 0.31f * (float) u));
            inc[u] = mtof (base + cents * 0.01f) / (float) sr;
            const float pan = pos * p.spread;
            panL[u] = std::sqrt (0.5f * (1.0f - pan));
            panR[u] = std::sqrt (0.5f * (1.0f + pan));
        }
        const float uniGain = 1.0f / std::sqrt ((float) uni) * 0.55f;
        const float hoover = p.mode == 1 ? juce::jmax (0.35f, p.hoover) : p.hoover;
        const float pw = 0.5f - 0.38f * (0.5f + 0.5f * std::sin (juce::MathConstants<float>::twoPi * pwm));
        const float fSub = mtof (base - 12.0f) / (float) sr, fOct = mtof (base - 12.0f) / (float) sr;
        const float cutoff = juce::jlimit (40.0f, (float) sr * 0.45f, p.cutoff * std::exp2 (p.fEnv * fenv * 5.0f));
        const float g = std::tan (juce::MathConstants<float>::pi * cutoff / (float) sr), k = 2.0f - 1.9f * p.reso;
        const float a1 = 1.0f / (1.0f + g * (g + k)), a2 = g * a1, a3 = g * a2;
        const float drive = 1.0f + p.drive * 6.0f, dNorm = 1.0f / std::tanh (drive * 0.6f);
        const float v = (0.5f + 0.5f * vel) * 0.6f;

        for (int i = 0; i < n; ++i)
        {
            float l = 0.0f, r = 0.0f;
            for (int u = 0; u < uni; ++u)
            {
                const float t = (float) ph[(size_t) u], dt = inc[u];
                float s = 2.0f * t - 1.0f - blep (t, dt);
                if (hoover > 0.0f)
                {
                    float t2 = t + 1.0f - pw; t2 -= std::floor (t2);
                    const float sq = (t < pw ? 1.0f : -1.0f) + blep (t, dt) - blep (t2, dt);
                    s = s * (1.0f - 0.6f * hoover) + sq * 0.7f * hoover;
                }
                l += s * panL[u];
                r += s * panR[u];
                ph[(size_t) u] += dt; if (ph[(size_t) u] >= 1.0) ph[(size_t) u] -= 1.0;
            }
            l *= uniGain; r *= uniGain;
            if (hoover > 0.0f)   // strato un'ottava sotto (il "corpo" dell'hoover)
            {
                const float t = (float) phOct;
                const float o = (2.0f * t - 1.0f - blep (t, fOct)) * 0.35f * hoover;
                l += o; r += o;
                phOct += fOct; if (phOct >= 1.0) phOct -= 1.0;
            }
            // filtro passa-basso (SVF) su sinistra e destra
            float x[2] { l, r };
            for (int c = 0; c < 2; ++c)
            {
                const float v3 = x[c] - s2[c], v1 = a1 * s1[c] + a2 * v3, v2 = s2[c] + a2 * s1[c] + a3 * v3;
                s1[c] = 2.0f * v1 - s1[c]; s2[c] = 2.0f * v2 - s2[c];
                x[c] = v2;
                if (p.drive > 0.001f) x[c] = std::tanh (x[c] * drive) * dNorm * 0.6f;
            }
            if (p.sub > 0.0f)
            {
                const float sb = std::sin ((float) phSub * juce::MathConstants<float>::twoPi) * p.sub * 0.5f;
                x[0] += sb; x[1] += sb;
                phSub += fSub; if (phSub >= 1.0) phSub -= 1.0;
            }
            const float e = amp.next() * v;
            L[i] += x[0] * e;
            R[i] += x[1] * e;
        }
    }

    //==========================================================================
    void Synth::prepare (double sampleRate, int)
    {
        sr = sampleRate;
        for (auto& v : voices) v.prepare (sr);
        fx.prepare (sr);
        killAll();
    }

    void Synth::killAll()
    {
        held.clear();
        for (auto& v : voices) v.prepare (sr);
        fx.reset();
    }

    Voice* Synth::findVoice()
    {
        const int limit = perf::lowPower() ? 5 : maxVoices;
        Voice* best = nullptr;
        for (int i = 0; i < limit; ++i) if (! voices[(size_t) i].isActive()) return &voices[(size_t) i];
        for (int i = 0; i < limit; ++i) if (voices[(size_t) i].isReleasing() && (best == nullptr || voices[(size_t) i].age < best->age)) best = &voices[(size_t) i];
        if (best != nullptr) return best;
        for (int i = 0; i < limit; ++i) if (best == nullptr || voices[(size_t) i].age < best->age) best = &voices[(size_t) i];
        return best;
    }

    void Synth::noteOn (int note, float velocity)
    {
        held.erase (std::remove (held.begin(), held.end(), note), held.end());
        held.push_back (note);
        const float from = currentGlide > 0.0001f && lastPitch >= 0.0f ? lastPitch : -1.0f;
        if (currentMode == 0)
        {
            auto* v = findVoice();
            v->age = ++ageCounter;
            v->start (note, velocity, from, true);
        }
        else
        {
            auto& v = voices[0];
            const bool sounding = v.isActive() && ! v.isReleasing();
            if (sounding && currentMode == 2) v.setTarget (note);
            else v.start (note, velocity, sounding ? v.getPitch() : from, true);
        }
        lastPitch = (float) note;
    }

    void Synth::noteOff (int note)
    {
        held.erase (std::remove (held.begin(), held.end(), note), held.end());
        if (currentMode == 0)
        {
            for (auto& v : voices) if (v.isActive() && ! v.isReleasing() && v.getNote() == note) v.stop();
            return;
        }
        auto& v = voices[0];
        if (v.getNote() != note || ! v.isActive()) return;
        if (! held.empty()) { v.setTarget (held.back()); lastPitch = (float) held.back(); }
        else v.stop();
    }

    void Synth::allNotesOff()
    {
        held.clear();
        for (auto& v : voices) v.stop();
    }

    void Synth::render (juce::AudioBuffer<float>& buffer, int start, int n, const BlockParams& p, double bpm)
    {
        if (n <= 0) return;
        if (p.playMode != currentMode)
        {
            for (int i = 1; i < maxVoices; ++i) voices[(size_t) i].stop();
            currentMode = p.playMode;
        }
        currentGlide = p.glide;
        auto* L = buffer.getWritePointer (0, start);
        auto* R = buffer.getNumChannels() > 1 ? buffer.getWritePointer (1, start) : L;
        const float glideCoef = p.glide > 0.0001f ? (float) std::exp (-1.0 / (p.glide * 0.3 * sr / controlRate)) : 0.0f;
        const int unisonLimit = perf::lowPower() ? 5 : maxUnison;
        for (int pos = 0; pos < n; pos += controlRate)
        {
            const int m = juce::jmin (controlRate, n - pos);
            for (auto& v : voices)
                if (v.isActive())
                    v.render (L + pos, R + pos, m, p, glideCoef, unisonLimit);
        }
        fx.process (L, R, n, p.fx, bpm);
        // limitatore morbido: accordi grossi a tutto volume non graffiano
        for (int i = 0; i < n; ++i)
        {
            auto soft = [] (float x) { const float a = std::abs (x); return a < 0.8f ? x : std::copysign (0.8f + 0.2f * std::tanh ((a - 0.8f) * 5.0f), x); };
            L[i] = soft (L[i]);
            if (R != L) R[i] = soft (R[i]);
        }
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
        // mode 0 supersaw 1 hoover 2 reese | octave 2 = 0 | playMode 0 poly 1 mono 2 legato
        static const std::vector<Factory> list
        {
            { "Init", {} },
            { "Supersaw Anthem", { { "unison", 9 }, { "detune", 0.45f }, { "spread", 1.0f }, { "thick", 0.7f }, { "cutoff", 11000 }, { "fEnv", 0.1f },
                                   { "aA", 0.01f }, { "aR", 0.35f }, { "chorus", 0.2f }, { "revMix", 0.3f }, { "revSize", 0.75f }, { "dlyMix", 0.15f }, { "master", -6.0f } } },
            { "Electro Supersaw Lead", { { "unison", 7 }, { "detune", 0.35f }, { "spread", 0.7f }, { "thick", 0.5f }, { "cutoff", 4500 }, { "reso", 0.25f }, { "fEnv", 0.35f },
                                         { "fDecay", 0.25f }, { "drive", 0.35f }, { "playMode", 2 }, { "glide", 0.05f }, { "dlyMix", 0.2f }, { "master", 2.2f } } },
            { "Supersaw Pluck", { { "unison", 7 }, { "detune", 0.3f }, { "spread", 0.9f }, { "cutoff", 900 }, { "reso", 0.2f }, { "fEnv", 0.75f }, { "fDecay", 0.18f },
                                  { "aD", 0.35f }, { "aS", 0.0f }, { "aR", 0.25f }, { "dlyMix", 0.3f }, { "revMix", 0.25f }, { "master", 6.0f } } },
            { "Stab Accordi Grosso", { { "unison", 7 }, { "detune", 0.5f }, { "spread", 1.0f }, { "thick", 0.8f }, { "cutoff", 3500 }, { "fEnv", 0.4f }, { "fDecay", 0.2f },
                                       { "aD", 0.3f }, { "aS", 0.3f }, { "aR", 0.2f }, { "drive", 0.25f }, { "revMix", 0.2f }, { "master", -0.9f } } },
            { "Hoover Rave", { { "mode", 1 }, { "unison", 5 }, { "detune", 0.55f }, { "spread", 0.8f }, { "hoover", 0.75f }, { "scoop", 5.0f }, { "cutoff", 6000 },
                               { "playMode", 2 }, { "glide", 0.12f }, { "chorus", 0.5f }, { "revMix", 0.2f }, { "master", -3.9f } } },
            { "Hoover Dutch", { { "mode", 1 }, { "unison", 7 }, { "detune", 0.6f }, { "spread", 1.0f }, { "hoover", 0.9f }, { "scoop", 7.0f }, { "cutoff", 4000 }, { "reso", 0.2f },
                                { "drive", 0.4f }, { "playMode", 2 }, { "glide", 0.08f }, { "chorus", 0.4f }, { "dlyMix", 0.15f }, { "master", -1.4f } } },
            { "Hoover Bass", { { "mode", 1 }, { "octave", 1 }, { "unison", 5 }, { "detune", 0.4f }, { "spread", 0.5f }, { "hoover", 0.7f }, { "scoop", 4.0f }, { "sub", 0.5f },
                               { "cutoff", 2500 }, { "drive", 0.45f }, { "playMode", 2 }, { "glide", 0.06f }, { "master", 2.4f } } },
            { "Reese Classico", { { "mode", 2 }, { "octave", 1 }, { "unison", 3 }, { "detune", 0.6f }, { "spread", 0.6f }, { "sub", 0.6f }, { "cutoff", 1200 }, { "reso", 0.15f },
                                  { "playMode", 2 }, { "glide", 0.05f }, { "master", 3.7f } } },
            { "Reese Distorto", { { "mode", 2 }, { "octave", 1 }, { "unison", 4 }, { "detune", 0.8f }, { "spread", 0.5f }, { "sub", 0.5f }, { "cutoff", 2200 }, { "reso", 0.3f },
                                  { "drive", 0.7f }, { "playMode", 2 }, { "glide", 0.05f }, { "master", 0.8f } } },
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
        return juce::File::getSpecialLocation (juce::File::userApplicationDataDirectory).getChildFile ("MegaSaw").getChildFile ("Presets");
    }
    juce::Array<juce::File> Presets::userPresets() const
    {
        auto files = userDir().findChildFiles (juce::File::findFiles, false, "*.mspreset");
        files.sort();
        return files;
    }
    bool Presets::save (const juce::String& name)
    {
        const auto clean = juce::File::createLegalFileName (name.trim());
        if (clean.isEmpty()) return false;
        userDir().createDirectory();
        apvts.state.setProperty ("presetName", clean, nullptr);
        if (auto xml = apvts.copyState().createXml()) return xml->writeTo (userDir().getChildFile (clean + ".mspreset"));
        return false;
    }
    bool Presets::load (const juce::File& file)
    {
        auto xml = juce::XmlDocument::parse (file);
        if (xml == nullptr || ! xml->hasTagName (apvts.state.getType())) return false;
        apvts.replaceState (juce::ValueTree::fromXml (*xml));
        apvts.state.setProperty ("presetName", file.getFileNameWithoutExtension(), nullptr);
        return true;
    }
}

//==============================================================================
MegaSawAudioProcessor::MegaSawAudioProcessor()
    : AudioProcessor (BusesProperties().withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
      apvts (*this, nullptr, "MegaSaw", ms::createLayout())
{
    for (auto* id : ms::ids) p.push_back (apvts.getRawParameterValue (id));
    apvts.state.setProperty ("presetName", "Init", nullptr);
}

ms::BlockParams MegaSawAudioProcessor::readParams()
{
    using namespace ms;
    static constexpr double delayBeats[] { 0.25, 0.5, 0.75, 1.0, 1.5, 2.0 };
    BlockParams b;
    b.mode = (int) p[iMode]->load(); b.unison = juce::roundToInt (p[iUnison]->load()); b.detune = p[iDetune]->load();
    b.spread = p[iSpread]->load(); b.hoover = p[iHoover]->load(); b.scoop = p[iScoop]->load(); b.sub = p[iSub]->load();
    b.octave = (int) p[iOctave]->load() - 2; b.thick = p[iThick]->load(); b.drive = p[iDrive]->load();
    b.cutoff = p[iCutoff]->load(); b.reso = p[iReso]->load(); b.fEnv = p[iFEnv]->load(); b.fDecay = p[iFDecay]->load();
    b.aA = p[iA]->load(); b.aD = p[iD]->load(); b.aS = p[iS]->load(); b.aR = p[iR]->load(); b.glide = p[iGlide]->load();
    b.playMode = (int) p[iPlay]->load();
    b.fx.chorus = p[iChorus]->load(); b.fx.delayMix = p[iDlyMix]->load(); b.fx.delayFeedback = p[iDlyFb]->load();
    b.fx.delayBeats = delayBeats[juce::jlimit (0, 5, (int) p[iDlyTime]->load())];
    b.fx.reverbMix = p[iRevMix]->load(); b.fx.reverbSize = p[iRevSize]->load();
    b.fx.gain = juce::Decibels::decibelsToGain (p[iMaster]->load());
    return b;
}

void MegaSawAudioProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midi)
{
    juce::ScopedNoDenormals nd;
    buffer.clear();
    double bpm = 120.0;
    if (auto* ph = getPlayHead()) if (auto pos = ph->getPosition()) if (auto b = pos->getBpm()) bpm = *b;
    const auto b = readParams();
    int done = 0;
    for (const auto meta : midi)
    {
        const int pos = juce::jlimit (0, buffer.getNumSamples(), meta.samplePosition);
        synth.render (buffer, done, pos - done, b, bpm);
        done = pos;
        const auto m = meta.getMessage();
        if (m.isNoteOn()) synth.noteOn (m.getNoteNumber(), m.getFloatVelocity());
        else if (m.isNoteOff()) synth.noteOff (m.getNoteNumber());
        else if (m.isAllNotesOff() || m.isAllSoundOff()) synth.allNotesOff();
    }
    synth.render (buffer, done, buffer.getNumSamples() - done, b, bpm);
}

juce::AudioProcessorEditor* MegaSawAudioProcessor::createEditor()   { return new MegaSawEditor (*this); }

void MegaSawAudioProcessor::getStateInformation (juce::MemoryBlock& d)
{
    if (auto xml = apvts.copyState().createXml()) copyXmlToBinary (*xml, d);
}

void MegaSawAudioProcessor::setStateInformation (const void* data, int size)
{
    if (auto xml = getXmlFromBinary (data, size))
        if (xml->hasTagName (apvts.state.getType()))
            apvts.replaceState (juce::ValueTree::fromXml (*xml));
}
