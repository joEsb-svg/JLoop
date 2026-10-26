#include "WubForge.h"
#include "WubForgeEditor.h"

namespace wf
{
    // Ordine dei parametri (indice in WubForgeAudioProcessor::p)
    static const char* ids[] { "shape", "detune", "fm", "fmRatio", "sub", "octave", "glide", "cutoff", "reso", "vowelQ", "vowels",
                               "wobble", "growl", "amtCut", "amtVowel", "amtVol", "amtPitch",
                               "rateA", "rateB", "rateC", "rateD", "seq0", "seq1", "seq2", "seq3", "seq4", "seq5", "seq6", "seq7",
                               "drive", "aA", "aR", "chorus", "dlyMix", "dlyTime", "dlyFb", "revMix", "revSize", "master" };
    enum { iShape, iDetune, iFm, iFmRatio, iSub, iOctave, iGlide, iCutoff, iReso, iVowelQ, iVowels,
           iWobble, iGrowl, iAmtCut, iAmtVowel, iAmtVol, iAmtPitch, iRate0, iSeq0 = iRate0 + numShapes,
           iDrive = iSeq0 + seqSlots, iA, iR, iChorus, iDlyMix, iDlyTime, iDlyFb, iRevMix, iRevSize, iMaster, numIds };

    static inline float mtof (float m) noexcept { return 440.0f * std::exp2 ((m - 69.0f) / 12.0f); }
    static inline float blep (float t, float dt) noexcept
    {
        if (t < dt)        { const float x = t / dt; return x + x - x * x - 1.0f; }
        if (t > 1.0f - dt) { const float x = (t - 1.0f) / dt; return x * x + x + x + 1.0f; }
        return 0.0f;
    }

    juce::StringArray rateNames()     { return { "1/1", "1/2", "1/4", "1/8", "1/8T", "1/16", "1/16T", "1/32" }; }
    double rateBeats (int i)
    {
        static constexpr double beats[] { 4.0, 2.0, 1.0, 0.5, 1.0 / 3.0, 0.25, 1.0 / 6.0, 0.125 };
        return beats[juce::jlimit (0, 7, i)];
    }

    static juce::StringArray vowelPathNames()  { return { "O > A", "O > A > I", "U > O > A", "A > E > I", "U > I", "O > E", "I > A > U" }; }
    const std::vector<std::vector<int>>& vowelPaths()
    {
        // a=0 e=1 i=2 o=3 u=4
        static const std::vector<std::vector<int>> paths { { 3, 0 }, { 3, 0, 2 }, { 4, 3, 0 }, { 0, 1, 2 }, { 4, 2 }, { 3, 1 }, { 2, 0, 4 } };
        return paths;
    }

    juce::StringArray shapeKindNames()
    {
        return { "Sinusoide", "Rampa giu'", "Rampa su", "Quadra", "Triangolo", "Yoi", "Colpo", "Scalini", "Doppia", "Gate 3/4" };
    }

    float shapeKind (int kind, float x)
    {
        constexpr float tau = juce::MathConstants<float>::twoPi;
        switch (kind)
        {
            case 1:  return 1.0f - x;
            case 2:  return x;
            case 3:  return x < 0.5f ? 1.0f : 0.0f;
            case 4:  return 1.0f - std::abs (2.0f * x - 1.0f);
            case 5:  return x < 0.85f ? std::sqrt (x / 0.85f) : (1.0f - x) / 0.15f;
            case 6:  return std::exp (-5.0f * x);
            case 7:  return std::floor (x * 4.0f) / 3.0f;
            case 8:  return 0.5f - 0.5f * std::cos (2.0f * tau * x);
            case 9:  return x < 0.75f ? 1.0f : 0.0f;
            default: return 0.5f - 0.5f * std::cos (tau * x);
        }
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
        auto choice = [&v] (const String& id, const String& name, StringArray o, int def) { v.push_back (std::make_unique<AudioParameterChoice> (ParameterID { id, 1 }, name, o, def)); };

        flt ("shape", "Forma onda", { 0.0f, 1.0f }, 0.0f, [] (float x, int) { return x < 0.05f ? String ("Saw") : x > 0.95f ? String ("Quadra") : String (roundToInt (x * 100.0f)) + "% quadra"; });
        flt ("detune", "Detune", { 0.0f, 1.0f }, 0.3f, pct);
        flt ("fm", "FM", { 0.0f, 1.0f }, 0.0f, pct);
        choice ("fmRatio", "Rapporto FM", { "1/2", "1", "3/2", "2", "3" }, 1);
        flt ("sub", "Sub", { 0.0f, 1.0f }, 0.4f, pct);
        choice ("octave", "Ottava", { "-2", "-1", "0", "+1" }, 2);
        flt ("glide", "Glide", skew (0.0f, 1.0f, 0.15f), 0.04f, sec);
        flt ("cutoff", "Cutoff", skew (30.0f, 12000.0f, 800.0f), 180.0f, [] (float x, int) { return x >= 1000.0f ? String (x / 1000.0f, 1) + " kHz" : String (roundToInt (x)) + " Hz"; });
        flt ("reso", "Risonanza", { 0.0f, 1.0f }, 0.3f, pct);
        flt ("vowelQ", "Nitidezza vocali", { 0.0f, 1.0f }, 0.5f, pct);
        choice ("vowels", "Vocali", vowelPathNames(), 1);
        flt ("wobble", "Wobble", { 0.0f, 1.0f }, 0.8f, pct);
        flt ("growl", "Growl", { 0.0f, 1.0f }, 0.0f, pct);
        flt ("amtCut", "LFO > cutoff", { 0.0f, 1.0f }, 0.8f, pct);
        flt ("amtVowel", "LFO > vocale", { 0.0f, 1.0f }, 0.0f, pct);
        flt ("amtVol", "LFO > volume", { 0.0f, 1.0f }, 0.0f, pct);
        flt ("amtPitch", "LFO > pitch", { -12.0f, 12.0f }, 0.0f, [] (float x, int) { return (x > 0.05f ? "+" : "") + String (x, 1) + " st"; });
        const char* letters[] { "A", "B", "C", "D" };
        for (int s = 0; s < numShapes; ++s) choice (String ("rate") + letters[s], String ("Tempo ") + letters[s], rateNames(), 3);
        for (int s = 0; s < seqSlots; ++s) choice ("seq" + String (s), "Sequenza " + String (s + 1), { "A", "B", "C", "D" }, 0);
        flt ("drive", "Drive", { 0.0f, 1.0f }, 0.25f, pct);
        flt ("aA", "Attacco", skew (0.001f, 1.0f, 0.05f), 0.003f, sec);
        flt ("aR", "Rilascio", skew (0.005f, 2.0f, 0.2f), 0.08f, sec);
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
    float Synth::Svf::lp (float x, float g, float k) noexcept
    {
        const float a1 = 1.0f / (1.0f + g * (g + k)), a2 = g * a1, a3 = g * a2;
        const float v3 = x - s2, v1 = a1 * s1 + a2 * v3, v2 = s2 + a2 * s1 + a3 * v3;
        s1 = 2.0f * v1 - s1; s2 = 2.0f * v2 - s2;
        return v2;
    }

    float Synth::Svf::bp (float x, float g, float k) noexcept
    {
        const float a1 = 1.0f / (1.0f + g * (g + k)), a2 = g * a1, a3 = g * a2;
        const float v3 = x - s2, v1 = a1 * s1 + a2 * v3, v2 = s2 + a2 * s1 + a3 * v3;
        s1 = 2.0f * v1 - s1; s2 = 2.0f * v2 - s2;
        return v1 * k;
    }

    void Synth::prepare (double sampleRate)
    {
        sr = sampleRate;
        fx.prepare (sr);
        killAll();
    }

    void Synth::killAll()
    {
        held.clear();
        gate = false;
        env = 0.0f;
        f1 = f2 = form1 = form2 = {};
        fx.reset();
    }

    void Synth::noteOn (int note, float velocity, bool hostPlaying)
    {
        held.erase (std::remove (held.begin(), held.end(), note), held.end());
        held.push_back (note);
        target = (float) note;
        if (gate) return;   // legato: scivola verso la nota nuova
        gate = true;
        pitch = target;
        vel = 0.6f + 0.4f * velocity;
        if (! hostPlaying) freeBeats = 0.0;   // a trasporto fermo l'LFO riparte dalla nota
        if (env < 0.001f)
        {
            juce::Random r ((juce::int64) note * 131 + 7);
            for (auto& x : ph) x = r.nextDouble();
            phCar = phMod = phSub = 0.0;
        }
    }

    void Synth::noteOff (int note)
    {
        const bool wasCurrent = ! held.empty() && held.back() == note;
        held.erase (std::remove (held.begin(), held.end(), note), held.end());
        if (held.empty()) gate = false;
        else if (wasCurrent) target = (float) held.back();
    }

    void Synth::render (float* L, float* R, int n, const BlockParams& p, double bpm, double ppq, const WubForgeAudioProcessor& proc)
    {
        if (n <= 0) return;
        constexpr float tau = juce::MathConstants<float>::twoPi;
        static constexpr float F1[5] { 800.0f, 450.0f, 300.0f, 450.0f, 330.0f };   // a e i o u
        static constexpr float F2[5] { 1150.0f, 1900.0f, 2300.0f, 800.0f, 700.0f };
        static constexpr float ratios[5] { 0.5f, 1.0f, 1.5f, 2.0f, 3.0f };

        const double bps = bpm / 60.0;
        const float depth = p.depth;
        const float fmEff = juce::jlimit (0.0f, 1.0f, p.fm + p.growl * 0.7f);
        const float driveEff = juce::jlimit (0.0f, 1.0f, p.drive + p.growl * 0.5f);
        const float drive = 1.0f + driveEff * 8.0f, dNorm = 0.75f / std::tanh (drive * 0.5f);
        const float glideCoef = p.glide > 0.0001f ? (float) std::exp (-1.0 / (p.glide * 0.3 * sr / controlRate)) : 0.0f;
        const float ai = (float) (1.0 / (juce::jmax (0.0005f, p.aA) * sr));
        const float rc = (float) std::exp (-4.6 / (juce::jmax (0.005f, p.aR) * sr));
        const float smooth = (float) (1.0 - std::exp (-(double) controlRate / (0.0015 * sr)));
        const auto& path = vowelPaths()[(size_t) juce::jlimit (0, (int) vowelPaths().size() - 1, p.vowelPath)];
        const float k1 = 2.0f - 1.85f * p.reso, k2 = 1.5f, kq = 1.0f / (3.0f + p.vowelQ * 10.0f);
        const float ratio = ratios[juce::jlimit (0, 4, (int) p.fmRatio)];

        for (int pos = 0; pos < n; pos += controlRate)
        {
            const int m = juce::jmin (controlRate, n - pos);
            // --- LFO: casella della sequenza (mezza battuta), forma e fase a tempo
            double beat;
            if (ppq >= 0.0) beat = ppq + bps * (double) pos / sr;
            else { beat = freeBeats; freeBeats += bps * (double) m / sr; }
            slot = ((int) std::floor (beat / 2.0) % seqSlots + seqSlots) % seqSlots;
            const int shape = juce::jlimit (0, numShapes - 1, p.seq[slot]);
            const double r = rateBeats (p.rate[shape]);
            const float phase = (float) (beat / r - std::floor (beat / r));
            lfoSmooth += (proc.readShape (shape, phase) - lfoSmooth) * smooth;
            const float lfo = lfoSmooth;
            lfoPhase = phase;
            lfoOut = lfo;

            if (! gate && env < 1.0e-5f) { env = 0.0f; continue; }

            pitch = glideCoef > 0.0f ? target + (pitch - target) * glideCoef : target;
            const float semis = pitch + (float) (p.octave * 12) + p.amtPitch * depth * lfo;
            const float f = mtof (semis);
            float inc[3];
            for (int u = 0; u < 3; ++u)
                inc[u] = mtof (semis + (float) (u - 1) * p.detune * 0.3f) / (float) sr;
            const float fInc = f / (float) sr, subInc = fInc;   // sub: sinusoide sulla fondamentale (le note wobble sono gia' basse)
            const float cutoff = juce::jlimit (30.0f, (float) sr * 0.45f, p.cutoff * std::exp2 (lfo * p.amtCut * depth * 7.0f));
            const float g = std::tan (juce::MathConstants<float>::pi * cutoff / (float) sr);
            // filtro a vocali: posizione lungo il percorso
            const float vx = juce::jlimit (0.0f, 1.0f, lfo * depth) * (float) (path.size() - 1);
            const int v0 = juce::jmin ((int) path.size() - 2, (int) vx);
            const float vf = vx - (float) v0;
            const int a = path[(size_t) v0], b = path[(size_t) v0 + 1];
            const float fr1 = F1[a] + vf * (F1[b] - F1[a]), fr2 = F2[a] + vf * (F2[b] - F2[a]);
            const float g1 = std::tan (juce::MathConstants<float>::pi * fr1 / (float) sr), g2 = std::tan (juce::MathConstants<float>::pi * fr2 / (float) sr);
            const float gain = 1.0f - p.amtVol * depth * (1.0f - lfo);
            const float fmIndex = fmEff * (1.5f + 4.0f * lfo * p.amtCut * depth);

            for (int i = 0; i < m; ++i)
            {
                env = gate ? juce::jmin (1.0f, env + ai) : env * rc;
                float osc = 0.0f;
                for (int u = 0; u < 3; ++u)
                {
                    const float t = (float) ph[u], dt = inc[u];
                    const float saw = 2.0f * t - 1.0f - blep (t, dt);
                    float s = saw;
                    if (p.shape > 0.0f)
                    {
                        float t2 = t + 0.5f; t2 -= std::floor (t2);
                        const float sq = (t < 0.5f ? 1.0f : -1.0f) + blep (t, dt) - blep (t2, dt);
                        s += (sq - saw) * p.shape;
                    }
                    osc += s;
                    ph[u] += dt; if (ph[u] >= 1.0) ph[u] -= 1.0;
                }
                osc *= 0.42f;
                float x = osc;
                if (fmEff > 0.0f)
                {
                    const float fmo = std::sin (tau * (float) phCar + fmIndex * std::sin (tau * (float) phMod));
                    x = osc * (1.0f - 0.6f * fmEff) + fmo * fmEff * 0.8f;
                    phCar += fInc; if (phCar >= 1.0) phCar -= 1.0;
                    phMod += fInc * ratio; if (phMod >= 1.0) phMod -= 1.0;
                }
                x = f2.lp (f1.lp (x, g, k1), g, k2);
                if (p.amtVowel > 0.0f)
                {
                    const float y = form1.bp (x, g1, kq) * 1.6f + form2.bp (x, g2, kq) * 1.1f;
                    x += p.amtVowel * (y * 1.8f - x);
                }
                x = std::tanh (x * drive) * dNorm;
                if (p.sub > 0.0f)
                {
                    x += std::sin (tau * (float) phSub) * p.sub * 0.6f;
                    phSub += subInc; if (phSub >= 1.0) phSub -= 1.0;
                }
                const float gNow = lastGain + (gain - lastGain) * (float) (i + 1) / (float) m;
                const float out = x * gNow * env * vel * 0.6f;
                L[pos + i] += out;
                R[pos + i] += out;
            }
            lastGain = gain;
        }

        fx.process (L, R, n, p.fx, bpm);
        for (int i = 0; i < n; ++i)   // limitatore morbido
        {
            auto soft = [] (float v) { const float av = std::abs (v); return av < 0.8f ? v : std::copysign (0.8f + 0.2f * std::tanh ((av - 0.8f) * 5.0f), v); };
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

    struct Factory
    {
        const char* name;
        std::array<int, numShapes> kinds;      // forme pronte per A B C D
        const char* seq;                        // 8 lettere
        std::array<int, numShapes> rates;       // indici di rateNames()
        std::vector<std::pair<const char*, float>> values;
    };

    static const std::vector<Factory>& factory()
    {
        // kinds: 0 sin 1 giu' 2 su 3 quadra 4 tri 5 yoi 6 colpo 7 scalini 8 doppia 9 gate | rates: 0 1/1 1 1/2 2 1/4 3 1/8 4 1/8T 5 1/16 6 1/16T 7 1/32
        // vowels: 0 O>A 1 O>A>I 2 U>O>A 3 A>E>I 4 U>I 5 O>E 6 I>A>U
        static const std::vector<Factory> list
        {
            { "Init", { 0, 1, 2, 3 }, "AAAAAAAA", { 3, 3, 3, 3 }, {} },
            { "Wobble Classico", { 0, 4, 1, 3 }, "AAAAAAAB", { 3, 3, 4, 5 },
              { { "cutoff", 110 }, { "reso", 0.4f }, { "amtCut", 0.85f }, { "sub", 0.5f }, { "wobble", 0.9f }, { "drive", 0.35f }, { "master", -3.2f } } },
            { "Talking Wub", { 0, 1, 2, 3 }, "AABACCDA", { 2, 3, 4, 5 },
              { { "amtVowel", 0.8f }, { "vowels", 1 }, { "amtCut", 0.5f }, { "cutoff", 260 }, { "fm", 0.2f }, { "drive", 0.4f }, { "sub", 0.4f }, { "master", 1.8f } } },
            { "Yoi Bass", { 5, 5, 0, 6 }, "ABABABCD", { 2, 3, 3, 5 },
              { { "amtVowel", 0.9f }, { "vowels", 4 }, { "amtCut", 0.45f }, { "cutoff", 220 }, { "growl", 0.3f }, { "vowelQ", 0.7f }, { "glide", 0.08f }, { "master", 2.4f } } },
            { "Brostep Growl", { 3, 7, 6, 8 }, "ABACDABC", { 5, 3, 4, 6 },
              { { "fm", 0.6f }, { "growl", 0.5f }, { "amtCut", 0.7f }, { "amtVowel", 0.4f }, { "vowels", 5 }, { "drive", 0.6f }, { "detune", 0.4f },
                { "shape", 0.3f }, { "fmRatio", 3 }, { "cutoff", 200 }, { "master", -3.9f } } },
            { "Wobble Terzinato", { 0, 4, 2, 3 }, "AAABAABB", { 4, 6, 3, 5 },
              { { "cutoff", 140 }, { "reso", 0.35f }, { "amtCut", 0.85f }, { "sub", 0.5f }, { "drive", 0.3f }, { "master", -3.2f } } },
            { "Dubstep Lento", { 0, 1, 4, 3 }, "AAAABBAA", { 2, 1, 3, 3 },
              { { "cutoff", 90 }, { "reso", 0.45f }, { "amtCut", 0.9f }, { "sub", 0.7f }, { "wobble", 1.0f }, { "drive", 0.25f }, { "revMix", 0.1f }, { "master", -9.3f } } },
            { "Robot Stutter", { 3, 9, 7, 3 }, "ABACABDC", { 5, 7, 6, 3 },
              { { "amtVol", 0.9f }, { "amtCut", 0.5f }, { "fm", 0.3f }, { "fmRatio", 3 }, { "cutoff", 300 }, { "drive", 0.45f }, { "shape", 0.5f }, { "master", 0.5f } } },
            { "Laser Wub", { 1, 2, 6, 0 }, "AABBAACD", { 3, 5, 3, 2 },
              { { "amtPitch", 7.0f }, { "amtCut", 0.7f }, { "cutoff", 180 }, { "fm", 0.25f }, { "drive", 0.4f }, { "dlyMix", 0.15f }, { "master", -3.7f } } },
            { "Reese Wobble", { 0, 4, 1, 2 }, "AABBAABB", { 2, 3, 3, 4 },
              { { "detune", 0.85f }, { "amtCut", 0.6f }, { "cutoff", 200 }, { "chorus", 0.3f }, { "sub", 0.55f }, { "drive", 0.3f }, { "master", -2.0f } } },
            { "Wobble Mid Pesante", { 8, 0, 3, 6 }, "ABCDABCC", { 3, 2, 5, 4 },
              { { "fm", 0.4f }, { "growl", 0.3f }, { "vowels", 3 }, { "amtVowel", 0.5f }, { "drive", 0.5f }, { "cutoff", 160 }, { "fmRatio", 2 }, { "master", -2.6f } } },
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
        const char* letters[] { "A", "B", "C", "D" };
        for (int s = 0; s < numShapes; ++s)
        {
            proc.setShapeKind (s, f.kinds[(size_t) s]);
            set (juce::String ("rate") + letters[s], (float) f.rates[(size_t) s]);
        }
        for (int s = 0; s < seqSlots; ++s)
            set ("seq" + juce::String (s), (float) juce::jlimit (0, 3, f.seq[s] - 'A'));
        for (auto& [id, value] : f.values) set (id, value);
        apvts.state.setProperty ("presetName", f.name, nullptr);
        proc.shapesToState();
    }

    juce::File Presets::userDir() const
    {
        return juce::File::getSpecialLocation (juce::File::userApplicationDataDirectory).getChildFile ("WubForge").getChildFile ("Presets");
    }
    juce::Array<juce::File> Presets::userPresets() const
    {
        auto files = userDir().findChildFiles (juce::File::findFiles, false, "*.wfpreset");
        files.sort();
        return files;
    }
    bool Presets::save (const juce::String& name)
    {
        const auto clean = juce::File::createLegalFileName (name.trim());
        if (clean.isEmpty()) return false;
        userDir().createDirectory();
        apvts.state.setProperty ("presetName", clean, nullptr);
        proc.shapesToState();
        if (auto xml = apvts.copyState().createXml()) return xml->writeTo (userDir().getChildFile (clean + ".wfpreset"));
        return false;
    }
    bool Presets::load (const juce::File& file)
    {
        auto xml = juce::XmlDocument::parse (file);
        if (xml == nullptr || ! xml->hasTagName (apvts.state.getType())) return false;
        apvts.replaceState (juce::ValueTree::fromXml (*xml));
        apvts.state.setProperty ("presetName", file.getFileNameWithoutExtension(), nullptr);
        proc.shapesFromState();
        return true;
    }
}

//==============================================================================
WubForgeAudioProcessor::WubForgeAudioProcessor()
    : AudioProcessor (BusesProperties().withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
      apvts (*this, nullptr, "WubForge", wf::createLayout())
{
    for (auto* id : wf::ids) p.push_back (apvts.getRawParameterValue (id));
    jassert ((int) p.size() == wf::numIds);
    for (int s = 0; s < wf::numShapes; ++s) setShapeKind (s, s == 0 ? 0 : s == 1 ? 1 : s == 2 ? 2 : 3);
    apvts.state.setProperty ("presetName", "Init", nullptr);
    shapesToState();
}

void WubForgeAudioProcessor::prepareToPlay (double sampleRate, int)
{
    synth.prepare (sampleRate);
}

float WubForgeAudioProcessor::readShape (int s, float phase) const
{
    const float x = juce::jlimit (0.0f, 0.99999f, phase) * (float) wf::shapePoints;
    const int i0 = (int) x, i1 = (i0 + 1) % wf::shapePoints;
    const float f = x - (float) i0;
    return getShape (s, i0) + f * (getShape (s, i1) - getShape (s, i0));
}

void WubForgeAudioProcessor::setShapeKind (int s, int kind)
{
    for (int i = 0; i < wf::shapePoints; ++i)
        setShape (s, i, wf::shapeKind (kind, ((float) i + 0.5f) / (float) wf::shapePoints));
}

void WubForgeAudioProcessor::shapesToState()
{
    juce::String text;
    for (int s = 0; s < wf::numShapes; ++s)
        for (int i = 0; i < wf::shapePoints; ++i)
            text << juce::String (getShape (s, i), 3) << ' ';
    apvts.state.setProperty ("shapes", text.trimEnd(), nullptr);
}

void WubForgeAudioProcessor::shapesFromState()
{
    juce::StringArray values;
    values.addTokens (apvts.state.getProperty ("shapes").toString(), " ", {});
    if (values.size() != wf::numShapes * wf::shapePoints) return;
    for (int s = 0; s < wf::numShapes; ++s)
        for (int i = 0; i < wf::shapePoints; ++i)
            setShape (s, i, values[s * wf::shapePoints + i].getFloatValue());
}

wf::BlockParams WubForgeAudioProcessor::readParams()
{
    using namespace wf;
    static constexpr double delayBeats[] { 0.25, 0.5, 0.75, 1.0, 1.5, 2.0 };
    BlockParams b;
    b.shape = p[iShape]->load(); b.detune = p[iDetune]->load(); b.fm = p[iFm]->load(); b.fmRatio = p[iFmRatio]->load();
    b.sub = p[iSub]->load(); b.octave = (int) p[iOctave]->load() - 2; b.glide = p[iGlide]->load();
    b.cutoff = p[iCutoff]->load(); b.reso = p[iReso]->load(); b.vowelQ = p[iVowelQ]->load(); b.vowelPath = (int) p[iVowels]->load();
    b.depth = p[iWobble]->load(); b.growl = p[iGrowl]->load();
    b.amtCut = p[iAmtCut]->load(); b.amtVowel = p[iAmtVowel]->load(); b.amtVol = p[iAmtVol]->load(); b.amtPitch = p[iAmtPitch]->load();
    for (int s = 0; s < numShapes; ++s) b.rate[s] = (int) p[(size_t) (iRate0 + s)]->load();
    for (int s = 0; s < seqSlots; ++s) b.seq[s] = (int) p[(size_t) (iSeq0 + s)]->load();
    b.drive = p[iDrive]->load(); b.aA = p[iA]->load(); b.aR = p[iR]->load();
    b.fx.chorus = p[iChorus]->load(); b.fx.delayMix = p[iDlyMix]->load(); b.fx.delayFeedback = p[iDlyFb]->load();
    b.fx.delayBeats = delayBeats[juce::jlimit (0, 5, (int) p[iDlyTime]->load())];
    b.fx.reverbMix = p[iRevMix]->load(); b.fx.reverbSize = p[iRevSize]->load();
    b.fx.gain = juce::Decibels::decibelsToGain (p[iMaster]->load());
    return b;
}

void WubForgeAudioProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midi)
{
    juce::ScopedNoDenormals nd;
    buffer.clear();
    double bpm = 120.0, ppq = -1.0;
    bool playing = false;
    if (auto* ph = getPlayHead())
        if (auto pos = ph->getPosition())
        {
            if (auto b = pos->getBpm()) bpm = *b;
            playing = pos->getIsPlaying();
            if (playing) if (auto q = pos->getPpqPosition()) ppq = *q;
        }
    const auto b = readParams();
    const double sr = getSampleRate() > 0.0 ? getSampleRate() : 48000.0;
    auto* L = buffer.getWritePointer (0);
    auto* R = buffer.getNumChannels() > 1 ? buffer.getWritePointer (1) : L;
    auto renderTo = [&] (int from, int to)
    {
        if (to <= from) return;
        synth.render (L + from, R + from, to - from, b, bpm, ppq >= 0.0 ? ppq + bpm / 60.0 * from / sr : -1.0, *this);
    };
    int done = 0;
    for (const auto meta : midi)
    {
        const int pos = juce::jlimit (0, buffer.getNumSamples(), meta.samplePosition);
        renderTo (done, pos);
        done = pos;
        const auto m = meta.getMessage();
        if (m.isNoteOn()) synth.noteOn (m.getNoteNumber(), m.getFloatVelocity(), ppq >= 0.0);
        else if (m.isNoteOff()) synth.noteOff (m.getNoteNumber());
        else if (m.isAllNotesOff() || m.isAllSoundOff()) synth.allNotesOff();
    }
    renderTo (done, buffer.getNumSamples());
    playSlot.store (synth.slot);
    playPhase.store (synth.lfoPhase);
    playValue.store (synth.lfoOut);
}

juce::AudioProcessorEditor* WubForgeAudioProcessor::createEditor()   { return new WubForgeEditor (*this); }

void WubForgeAudioProcessor::getStateInformation (juce::MemoryBlock& d)
{
    shapesToState();
    if (auto xml = apvts.copyState().createXml()) copyXmlToBinary (*xml, d);
}

void WubForgeAudioProcessor::setStateInformation (const void* data, int size)
{
    if (auto xml = getXmlFromBinary (data, size))
        if (xml->hasTagName (apvts.state.getType()))
        {
            apvts.replaceState (juce::ValueTree::fromXml (*xml));
            shapesFromState();
        }
}
