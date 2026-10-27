#include "ZapFM.h"
#include "ZapFMEditor.h"

namespace zap
{
    static const char* ids[] { "mode", "rate", "octave", "ratio", "index", "metal", "feedback", "spread",
                               "zap", "zapTime", "decay", "glitch", "crush", "tune", "level",
                               "revMix", "revSize", "dlyMix", "dlyTime", "dlyFb", "master" };
    enum { iMode, iRate, iOctave, iRatio, iIndex, iMetal, iFeedback, iSpread, iZap, iZapTime, iDecay, iGlitch, iCrush, iTune, iLevel,
           iRevMix, iRevSize, iDlyMix, iDlyTime, iDlyFb, iMaster, numIds };

    static inline double mtof (double m) noexcept { return 440.0 * std::pow (2.0, (m - 69.0) / 12.0); }

    static int pack (const Step& s)     { return juce::jlimit (0, maxHeight, s.height) | (s.gate ? 16 : 0) | (s.glitch ? 32 : 0); }
    static Step unpack (int v)          { Step s; s.height = v & 15; s.gate = (v & 16) != 0; s.glitch = (v & 32) != 0; return s; }

    // Formato testo: 16 gruppi da 3 caratteri: altezza (0-f), x/. (acceso), G/. (glitch)
    juce::String encode (const std::array<Step, numSteps>& line)
    {
        juce::String t;
        for (auto& s : line)
            t << juce::String::toHexString (juce::jlimit (0, maxHeight, s.height)) << (s.gate ? "x" : ".") << (s.glitch ? "G" : ".");
        return t;
    }

    std::array<Step, numSteps> decode (const juce::String& t)
    {
        std::array<Step, numSteps> line {};
        for (int i = 0; i < numSteps && i * 3 + 2 < t.length(); ++i)
        {
            auto& s = line[(size_t) i];
            s.height = juce::jlimit (0, maxHeight, t.substring (i * 3, i * 3 + 1).getHexValue32());
            s.gate = t[i * 3 + 1] == 'x';
            s.glitch = t[i * 3 + 2] == 'G';
        }
        return line;
    }

    std::array<Step, numSteps> randomLine (int seed)
    {
        juce::Random r ((juce::int64) seed * 7919 + 11);
        std::array<Step, numSteps> line {};
        for (int i = 0; i < numSteps; ++i)
        {
            auto& s = line[(size_t) i];
            s.gate = i % 4 == 0 ? r.nextFloat() < 0.9f : r.nextFloat() < 0.55f;
            s.height = s.gate ? (r.nextFloat() < 0.3f ? 12 + r.nextInt (4) : 3 + r.nextInt (10)) : 8;
            s.glitch = s.gate && r.nextFloat() < 0.18f;
        }
        return line;
    }

    juce::AudioProcessorValueTreeState::ParameterLayout createLayout()
    {
        using namespace juce;
        std::vector<std::unique_ptr<RangedAudioParameter>> v;
        auto pct = [] (float x, int) { return String (roundToInt (x * 100.0f)) + "%"; };
        auto flt = [&v] (const char* id, const char* name, NormalisableRange<float> r, float def, std::function<String (float, int)> t)
        { v.push_back (std::make_unique<AudioParameterFloat> (ParameterID { id, 1 }, name, r, def, AudioParameterFloatAttributes().withStringFromValueFunction (std::move (t)))); };
        auto choice = [&v] (const char* id, const char* name, StringArray o, int def) { v.push_back (std::make_unique<AudioParameterChoice> (ParameterID { id, 1 }, name, o, def)); };

        choice ("mode", "Suona", { "Sequenza", "Tastiera" }, 0);
        choice ("rate", "Velocita'", { "1/8", "1/16", "1/32" }, 1);
        choice ("octave", "Ottava", { "OTT -1", "OTT 0", "OTT +1", "OTT +2" }, 1);
        choice ("ratio", "Rapporto", { "x0.5", "x1", "x1.5", "x2", "x2.5", "x3", "x3.5", "x4", "x5", "x7", "x11" }, 3);
        flt ("index", "Indice", { 0.0f, 1.0f }, 0.45f, pct);
        flt ("metal", "Metallo", { 0.0f, 1.0f }, 0.5f, pct);
        flt ("feedback", "Feedback", { 0.0f, 1.0f }, 0.15f, pct);
        flt ("spread", "Stereo", { 0.0f, 1.0f }, 0.3f, pct);
        flt ("zap", "Zap", { 0.0f, 1.0f }, 0.55f, pct);
        flt ("zapTime", "Caduta", { 0.0f, 1.0f }, 0.3f, [] (float x, int) { return String (roundToInt ((0.005f + 0.15f * std::pow (x, 1.5f)) * 1000.0f)) + " ms"; });
        flt ("decay", "Durata", { 0.0f, 1.0f }, 0.3f, [] (float x, int) { return String (roundToInt ((0.02f + 0.8f * x * x) * 1000.0f)) + " ms"; });
        flt ("glitch", "Glitch", { 0.0f, 1.0f }, 0.2f, pct);
        flt ("crush", "Crush", { 0.0f, 1.0f }, 0.0f, pct);
        flt ("tune", "Tune", { -12.0f, 12.0f, 1.0f }, 0.0f, [] (float x, int) { return (x > 0 ? "+" : "") + String (roundToInt (x)) + " st"; });
        flt ("level", "Livello", { 0.0f, 1.0f }, 0.8f, pct);
        flt ("revMix", "Riverbero", { 0.0f, 1.0f }, 0.1f, pct);
        flt ("revSize", "Stanza", { 0.0f, 1.0f }, 0.5f, pct);
        flt ("dlyMix", "Delay", { 0.0f, 1.0f }, 0.2f, pct);
        choice ("dlyTime", "Tempo delay", { "1/16", "1/8", "3/16", "1/4", "3/8", "1/2" }, 2);
        flt ("dlyFb", "Feedback delay", { 0.0f, 0.9f }, 0.35f, pct);
        flt ("master", "Volume", { -24.0f, 6.0f }, 0.0f, [] (float x, int) { return (x > 0.05f ? "+" : "") + String (x, 1) + " dB"; });
        return { v.begin(), v.end() };
    }

    //==========================================================================
    void Presets::set (const juce::String& id, float value)
    {
        if (auto* prm = apvts.getParameter (id))
            prm->setValueNotifyingHost (prm->convertTo0to1 (value));
    }

    struct Factory { const char* name; const char* line; std::vector<std::pair<const char*, float>> values; };
    static const std::vector<Factory>& factory()
    {
        // linee: altezza (0-f) acceso (x/.) glitch (G/.) per 16 colpi | ratio: 0 x0.5 1 x1 2 x1.5 3 x2 4 x2.5 5 x3 6 x3.5 7 x4 8 x5 9 x7 10 x11
        static const std::vector<Factory> list
        {
            { "Init", nullptr, {} },
            { "Laser Rain", "fx.8..cx.6x.8..ax.fx.4x.8..dx.7x.8..ex.5xG8..bx.",
              { { "zap", 0.7f }, { "zapTime", 0.35f }, { "decay", 0.25f }, { "index", 0.35f }, { "ratio", 3 }, { "dlyMix", 0.3f }, { "dlyTime", 2 }, { "master", 0 } } },
            { "Hi-tech Pew", "fx.fx.8..fx.cx.8..fxGfx.fx.8..fx.cx.8..fx.fxG8..",
              { { "rate", 2 }, { "octave", 2 }, { "zap", 0.85f }, { "zapTime", 0.15f }, { "decay", 0.1f }, { "index", 0.25f }, { "ratio", 1 },
                { "glitch", 0.3f }, { "master", 2.8f } } },
            { "Zap Machine", "cx.8..cx.ax.cx.8..cxGax.cx.8..cx.ax.cx.8..cxGex.",
              { { "rate", 2 }, { "zap", 0.6f }, { "zapTime", 0.25f }, { "decay", 0.12f }, { "index", 0.6f }, { "metal", 0.7f }, { "ratio", 5 },
                { "feedback", 0.35f }, { "glitch", 0.35f }, { "master", 0.4f } } },
            { "Metallo Liquido", "6x.8..9x.8..4x.8..bx.8..6x.8..9x.8..4xG8..dx.8..",
              { { "zap", 0.25f }, { "zapTime", 0.6f }, { "decay", 0.55f }, { "index", 0.8f }, { "metal", 0.85f }, { "ratio", 6 }, { "feedback", 0.2f },
                { "spread", 0.7f }, { "revMix", 0.25f }, { "dlyMix", 0.25f }, { "master", -2.9f } } },
            { "Glitch Storm", "fxGax.5xGfx.cxG8..exGfx.9xG4x.fxGax.6xGfx.cxGbxG",
              { { "rate", 2 }, { "zap", 0.75f }, { "zapTime", 0.2f }, { "decay", 0.15f }, { "index", 0.55f }, { "ratio", 8 }, { "glitch", 0.9f },
                { "crush", 0.35f }, { "master", -2.6f } } },
            { "Psy Bleep", "8x.8..8..bx.8..8x.8..dx.8x.8..8..bx.8..8x.ex.8..",
              { { "rate", 1 }, { "octave", 2 }, { "zap", 0.2f }, { "zapTime", 0.1f }, { "decay", 0.18f }, { "index", 0.3f }, { "ratio", 3 },
                { "dlyMix", 0.35f }, { "dlyTime", 2 }, { "dlyFb", 0.5f }, { "master", 2.7f } } },
            { "Alien Talk", "9x.cx.6x.ex.8x.4x.bx.7x.9x.cxG6x.ex.8x.4x.bx.7xG",
              { { "rate", 1 }, { "zap", 0.35f }, { "zapTime", 0.7f }, { "decay", 0.4f }, { "index", 0.7f }, { "metal", 0.3f }, { "ratio", 2 },
                { "feedback", 0.6f }, { "spread", 0.5f }, { "master", -4.5f } } },
            { "Twister FX", "fx.ex.dx.cx.bx.ax.9x.8x.7x.6x.5x.4x.3x.2xG1x.0xG",
              { { "rate", 2 }, { "octave", 2 }, { "zap", 0.5f }, { "zapTime", 0.2f }, { "decay", 0.08f }, { "index", 0.45f }, { "ratio", 7 },
                { "glitch", 0.15f }, { "master", 0.8f } } },
            { "Crush Laser", "fx.8..8..fx.8..8..fxG8..fx.8..8..fx.8..cx.8..fxG",
              { { "zap", 0.9f }, { "zapTime", 0.45f }, { "decay", 0.3f }, { "index", 0.4f }, { "ratio", 1 }, { "crush", 0.7f }, { "master", 0 } } },
            { "Campana Goa", "ax.8..8..8..ax.8..8..8..ax.8..8..8..ax.8..8..8..",
              { { "rate", 0 }, { "zap", 0.15f }, { "zapTime", 0.3f }, { "decay", 0.7f }, { "index", 0.55f }, { "metal", 0.6f }, { "ratio", 6 },
                { "spread", 0.6f }, { "revMix", 0.3f }, { "dlyMix", 0.3f }, { "dlyTime", 1 }, { "master", 0 } } },
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
        proc.setLine (f.line != nullptr ? decode (f.line) : decode (factory()[1].line));
        apvts.state.setProperty ("presetName", f.name, nullptr);
        proc.lineToState();
    }

    juce::File Presets::userDir() const
    {
        return juce::File::getSpecialLocation (juce::File::userApplicationDataDirectory).getChildFile ("ZapFM").getChildFile ("Presets");
    }
    juce::Array<juce::File> Presets::userPresets() const
    {
        auto files = userDir().findChildFiles (juce::File::findFiles, false, "*.zappreset");
        files.sort();
        return files;
    }
    bool Presets::save (const juce::String& name)
    {
        const auto clean = juce::File::createLegalFileName (name.trim());
        if (clean.isEmpty()) return false;
        userDir().createDirectory();
        apvts.state.setProperty ("presetName", clean, nullptr);
        proc.lineToState();
        if (auto xml = apvts.copyState().createXml()) return xml->writeTo (userDir().getChildFile (clean + ".zappreset"));
        return false;
    }
    bool Presets::load (const juce::File& file)
    {
        auto xml = juce::XmlDocument::parse (file);
        if (xml == nullptr || ! xml->hasTagName (apvts.state.getType())) return false;
        apvts.replaceState (juce::ValueTree::fromXml (*xml));
        apvts.state.setProperty ("presetName", file.getFileNameWithoutExtension(), nullptr);
        proc.lineFromState();
        return true;
    }
}

//==============================================================================
ZapFMAudioProcessor::ZapFMAudioProcessor()
    : AudioProcessor (BusesProperties().withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
      apvts (*this, nullptr, "ZapFM", zap::createLayout())
{
    for (auto* id : zap::ids) p.push_back (apvts.getRawParameterValue (id));
    jassert ((int) p.size() == zap::numIds);
    presets.loadFactory (0);
}

zap::Step ZapFMAudioProcessor::getStep (int i) const   { return zap::unpack (packed[(size_t) juce::jlimit (0, zap::numSteps - 1, i)].load (std::memory_order_relaxed)); }
void ZapFMAudioProcessor::setStep (int i, const zap::Step& s)   { packed[(size_t) juce::jlimit (0, zap::numSteps - 1, i)].store (zap::pack (s), std::memory_order_relaxed); }

std::array<zap::Step, zap::numSteps> ZapFMAudioProcessor::getLine() const
{
    std::array<zap::Step, zap::numSteps> l {};
    for (int i = 0; i < zap::numSteps; ++i) l[(size_t) i] = getStep (i);
    return l;
}

void ZapFMAudioProcessor::setLine (const std::array<zap::Step, zap::numSteps>& l)
{
    for (int i = 0; i < zap::numSteps; ++i) setStep (i, l[(size_t) i]);
}

void ZapFMAudioProcessor::lineFromState()
{
    const auto t2 = apvts.state.getProperty ("line").toString();
    if (t2.length() >= zap::numSteps * 3) setLine (zap::decode (t2));
}

void ZapFMAudioProcessor::prepareToPlay (double sampleRate, int)
{
    sr = sampleRate;
    fx.prepare (sr);
    reset();
}

void ZapFMAudioProcessor::reset()
{
    held.clear();
    latchEnd = -1.0;
    t = 1.0e9;
    ratchetLeft = 0;
    lastStep = -1;
    playStep.store (-1);
    fx.reset();
}

zap::BlockParams ZapFMAudioProcessor::readParams()
{
    using namespace zap;
    static constexpr double delayBeats[] { 0.25, 0.5, 0.75, 1.0, 1.5, 2.0 };
    BlockParams b;
    b.mode = (int) p[iMode]->load(); b.rate = (int) p[iRate]->load(); b.octave = (int) p[iOctave]->load(); b.ratio = (int) p[iRatio]->load();
    b.index = p[iIndex]->load(); b.metal = p[iMetal]->load(); b.feedback = p[iFeedback]->load(); b.spread = p[iSpread]->load();
    b.zap = p[iZap]->load(); b.zapTime = p[iZapTime]->load(); b.decay = p[iDecay]->load(); b.glitch = p[iGlitch]->load();
    b.crush = p[iCrush]->load(); b.tune = p[iTune]->load(); b.level = p[iLevel]->load();
    b.fx.reverbMix = p[iRevMix]->load(); b.fx.reverbSize = p[iRevSize]->load(); b.fx.delayMix = p[iDlyMix]->load();
    b.fx.delayBeats = delayBeats[juce::jlimit (0, 5, (int) p[iDlyTime]->load())]; b.fx.delayFeedback = p[iDlyFb]->load();
    b.fx.gain = juce::Decibels::decibelsToGain (p[iMaster]->load());
    return b;
}

// glitchType: 0 niente, 1 ripetizioni, 2 laser al contrario, 3 salto di rapporto, 4 rotto (crush)
void ZapFMAudioProcessor::trigger (double heightNorm, int glitchType, const zap::BlockParams& b)
{
    t = 0.0;
    carL = carR = modPh = 0.0;
    fbPrev = 0.0;
    zapSemis = juce::jlimit (0.0, 1.0, heightNorm) * (6.0 + 42.0 * b.zap);
    zapDir = glitchType == 2 ? -1 : 1;
    ratioNow = zap::ratios[(size_t) juce::jlimit (0, (int) std::size (zap::ratios) - 1, b.ratio)];
    if (glitchType == 3) ratioNow = zap::ratios[(size_t) rng.nextInt ((int) std::size (zap::ratios))] * (rng.nextBool() ? 1.0 : 1.013);
    crushHold = 1 + juce::roundToInt (b.crush * 20.0f) + (glitchType == 4 ? 18 : 0);
    crushCount = 0;
}

void ZapFMAudioProcessor::render (float* L, float* R, int n, const zap::BlockParams& b, double bpm, double ppq)
{
    if (n <= 0) return;
    constexpr double twoPi = juce::MathConstants<double>::twoPi;
    const double bps = bpm / 60.0;
    const double spb = b.rate == 0 ? 2.0 : b.rate == 2 ? 8.0 : 4.0;   // colpi per quarto
    const double stepSec = 1.0 / (bps * spb);
    const double decayT = 0.02 + 0.8 * b.decay * b.decay;
    const double zapTau = 0.005 + 0.15 * std::pow ((double) b.zapTime, 1.5);
    const double metalTau = decayT * 0.35 + 0.01;
    const double maxIndex = 10.0 * b.index;
    const double fb = b.feedback * 1.4;
    const double detune = 1.0 + b.spread * 0.012;
    const double lpA = 1.0 - std::exp (-juce::MathConstants<double>::twoPi * 9000.0 / sr);
    const double crushLevels = std::pow (2.0, 16.0 - 12.0 * b.crush);

    for (int i = 0; i < n; ++i)
    {
        double pos;
        if (ppq >= 0.0) pos = ppq + bps * (double) i / sr;
        else { pos = freeBeats; freeBeats += bps / sr; }

        if (b.mode == 0)   // --- sequenza
        {
            const bool active = ! held.empty() || pos < latchEnd - 0.001;
            const int stepIdx = (int) std::floor (pos * spb + 1.0e-9);
            if (noteOnPending)
            {
                noteOnPending = false;
                if (pos * spb - (double) stepIdx < 0.3) lastStep = stepIdx - 1;
            }
            if (stepIdx != lastStep)
            {
                lastStep = stepIdx;
                const int s = ((stepIdx % zap::numSteps) + zap::numSteps) % zap::numSteps;
                if (active)
                {
                    playStep.store (s, std::memory_order_relaxed);
                    const auto st = getStep (s);
                    if (st.gate)
                    {
                        keyNote = rootNote;
                        int g = 0;
                        if (st.glitch || rng.nextFloat() < b.glitch * 0.45f)
                            g = 1 + rng.nextInt (4);
                        const double h = (double) st.height / zap::maxHeight;
                        trigger (h, g, b);
                        ratchetLeft = 0;
                        if (g == 1)
                        {
                            const int count = 2 + rng.nextInt (b.glitch > 0.6f ? 4 : 3);
                            ratchetLeft = count - 1;
                            ratchetGap = stepSec / count * sr;
                            ratchetNext = ratchetGap;
                            ratchetHeight = h;
                        }
                        if (g > 0) lastGlitch.store (s * 8 + g, std::memory_order_relaxed);
                    }
                }
                else
                {
                    playStep.store (-1, std::memory_order_relaxed);
                    ratchetLeft = 0;
                }
            }
        }

        if (ratchetLeft > 0 && (ratchetNext -= 1.0) <= 0.0)
        {
            --ratchetLeft;
            ratchetNext = ratchetGap;
            ratchetHeight *= 0.85;
            trigger (ratchetHeight, 0, b);
        }

        // --- voce FM
        float outL = 0.0f, outR = 0.0f;
        const double ts = t / sr;
        if (ts < decayT * 9.0 + 0.05)
        {
            const double zapEnv = std::exp (-ts / zapTau);
            const double off = zapDir > 0 ? zapSemis * zapEnv : zapSemis * (1.0 - zapEnv) * 0.6;
            const double base = keyNote + b.tune + (b.octave - 1) * 12.0;
            const double f = zap::mtof (juce::jmin (base + off, 135.0));
            const double amp = juce::jmin (1.0, ts / 0.0006) * std::exp (-ts / decayT);
            const double idx = maxIndex * ((1.0 - b.metal) + b.metal * std::exp (-ts / metalTau))
                              * juce::jmin (1.0, 2500.0 / f);   // in alto meno indice: niente fruscio (aliasing)
            modPh += f * ratioNow / sr; modPh -= std::floor (modPh);
            const double m = std::sin (twoPi * modPh + fb * fbPrev);
            fbPrev = m;
            carL += f / sr; carL -= std::floor (carL);
            carR += f * detune / sr; carR -= std::floor (carR);
            double yl = std::sin (twoPi * carL + idx * m), yr = std::sin (twoPi * carR + idx * m);
            yl = std::tanh (yl * 1.3) * amp; yr = std::tanh (yr * 1.3) * amp;
            if (crushHold > 1 || b.crush > 0.0f)
            {
                if (crushCount++ % crushHold == 0)
                {
                    heldL = (float) (std::round (yl * crushLevels) / crushLevels);
                    heldR = (float) (std::round (yr * crushLevels) / crushLevels);
                }
                yl = heldL; yr = heldR;
            }
            const double g = b.level * 0.42;
            lpL += lpA * (yl * g - lpL);   // passa-basso morbido: toglie la "sabbia" in cima
            lpR += lpA * (yr * g - lpR);
            outL = (float) lpL;
            outR = (float) lpR;
            t += 1.0;
        }
        L[i] += outL;
        R[i] += outR;
    }
    fx.process (L, R, n, b.fx, bpm);
    for (int i = 0; i < n; ++i)
    {
        auto soft = [] (float v) { const float a = std::abs (v); return a < 0.85f ? v : std::copysign (0.85f + 0.15f * std::tanh ((a - 0.85f) * 6.0f), v); };
        L[i] = soft (L[i]);
        if (R != L) R[i] = soft (R[i]);
    }
}

void ZapFMAudioProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midi)
{
    juce::ScopedNoDenormals nd;
    buffer.clear();
    double bpm = 150.0, ppq = -1.0;
    if (auto* ph2 = getPlayHead())
        if (auto pos = ph2->getPosition())
        {
            if (auto bp = pos->getBpm()) bpm = *bp;
            if (pos->getIsPlaying()) if (auto q = pos->getPpqPosition()) ppq = *q;
        }
    const auto b = readParams();
    auto* L = buffer.getWritePointer (0);
    auto* R = buffer.getNumChannels() > 1 ? buffer.getWritePointer (1) : L;
    auto posAt = [&] (int offset) { return ppq >= 0.0 ? ppq + bpm / 60.0 * offset / sr : freeBeats; };
    int done = 0;
    for (const auto meta : midi)
    {
        const int at = juce::jlimit (0, buffer.getNumSamples(), meta.samplePosition);
        render (L + done, R + done, at - done, b, bpm, ppq >= 0.0 ? posAt (done) : -1.0);
        done = at;
        const auto m = meta.getMessage();
        if (m.isNoteOn())
        {
            const bool wasHeld = ! held.empty();
            if (b.mode == 0 && ! wasHeld && ppq < 0.0 && posAt (at) >= latchEnd) { freeBeats = 0.0; lastStep = -1; }
            held.push_back (m.getNoteNumber());
            rootNote = m.getNoteNumber();
            if (b.mode == 0)
            {
                const double pos = posAt (at);
                latchEnd = juce::jmax (latchEnd, std::floor ((pos + 0.03) / 4.0) * 4.0 + 4.0);   // la nota tiene accesa la sua battuta
                noteOnPending = true;
            }
            else   // tastiera: ogni nota un colpo, la velocity decide l'altezza dello zap
            {
                keyNote = rootNote;
                ratchetLeft = 0;
                trigger (m.getFloatVelocity(), 0, b);
            }
        }
        else if (m.isNoteOff())
        {
            held.erase (std::remove (held.begin(), held.end(), m.getNoteNumber()), held.end());
            if (! held.empty()) rootNote = held.back();
        }
        else if (m.isAllNotesOff() || m.isAllSoundOff()) { held.clear(); latchEnd = -1.0; ratchetLeft = 0; }
    }
    render (L + done, R + done, buffer.getNumSamples() - done, b, bpm, ppq >= 0.0 ? posAt (done) : -1.0);
}

juce::AudioProcessorEditor* ZapFMAudioProcessor::createEditor()   { return new ZapFMEditor (*this); }

void ZapFMAudioProcessor::getStateInformation (juce::MemoryBlock& d)
{
    lineToState();
    if (auto xml = apvts.copyState().createXml()) copyXmlToBinary (*xml, d);
}

void ZapFMAudioProcessor::setStateInformation (const void* data, int size)
{
    if (auto xml = getXmlFromBinary (data, size))
        if (xml->hasTagName (apvts.state.getType()))
        {
            apvts.replaceState (juce::ValueTree::fromXml (*xml));
            lineFromState();
        }
}
