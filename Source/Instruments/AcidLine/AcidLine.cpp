#include "AcidLine.h"
#include "AcidLineEditor.h"

namespace acid
{
    static const char* ids[] { "wave", "mode", "cutoff", "reso", "envMod", "decay", "accent", "tune", "squelch", "acid",
                               "slideTime", "gateLen", "level", "revMix", "revSize", "dlyMix", "dlyTime", "dlyFb", "master" };
    enum { iWave, iMode, iCutoff, iReso, iEnvMod, iDecay, iAccent, iTune, iSquelch, iAcid, iSlide, iGate, iLevel,
           iRevMix, iRevSize, iDlyMix, iDlyTime, iDlyFb, iMaster, numIds };

    static inline double mtof (double m) noexcept { return 440.0 * std::pow (2.0, (m - 69.0) / 12.0); }
    static inline double blep (double t, double dt) noexcept
    {
        if (t < dt)        { const double x = t / dt; return x + x - x * x - 1.0; }
        if (t > 1.0 - dt)  { const double x = (t - 1.0) / dt; return x * x + x + x + 1.0; }
        return 0.0;
    }

    static int pack (const Step& s)
    {
        return (juce::jlimit (0, 12, s.note)) | ((juce::jlimit (-1, 1, s.octave) + 1) << 4) | (s.gate ? 64 : 0) | (s.accent ? 128 : 0) | (s.slide ? 256 : 0);
    }
    static Step unpack (int v)
    {
        Step s;
        s.note = v & 15; s.octave = ((v >> 4) & 3) - 1; s.gate = (v & 64) != 0; s.accent = (v & 128) != 0; s.slide = (v & 256) != 0;
        return s;
    }

    // Formato testo: 16 gruppi da 5 caratteri: nota (0-c), ottava (- 0 +), x/. (acceso), A/. (accent), S/. (slide)
    juce::String encode (const std::array<Step, numSteps>& line)
    {
        juce::String t;
        for (auto& s : line)
            t << juce::String::toHexString (s.note) << (s.octave < 0 ? "-" : s.octave > 0 ? "+" : "0")
              << (s.gate ? "x" : ".") << (s.accent ? "A" : ".") << (s.slide ? "S" : ".");
        return t;
    }

    std::array<Step, numSteps> decode (const juce::String& t)
    {
        std::array<Step, numSteps> line {};
        for (int i = 0; i < numSteps && i * 5 + 4 < t.length(); ++i)
        {
            auto& s = line[(size_t) i];
            s.note = juce::jlimit (0, 12, t.substring (i * 5, i * 5 + 1).getHexValue32());
            s.octave = t[i * 5 + 1] == '-' ? -1 : t[i * 5 + 1] == '+' ? 1 : 0;
            s.gate = t[i * 5 + 2] == 'x';
            s.accent = t[i * 5 + 3] == 'A';
            s.slide = t[i * 5 + 4] == 'S';
        }
        return line;
    }

    std::array<Step, numSteps> randomLine (int seed)
    {
        juce::Random r ((juce::int64) seed * 104729 + 3);
        static constexpr int scale[] { 0, 0, 0, 3, 5, 7, 7, 10, 12, 1 };   // minore con un po' di frigio (il "1")
        std::array<Step, numSteps> line {};
        for (int i = 0; i < numSteps; ++i)
        {
            auto& s = line[(size_t) i];
            s.note = scale[r.nextInt (10)];
            const float o = r.nextFloat();
            s.octave = o < 0.15f ? 1 : o < 0.25f ? -1 : 0;
            s.gate = i % 4 == 0 ? r.nextFloat() < 0.95f : r.nextFloat() < 0.75f;
            s.accent = s.gate && r.nextFloat() < (i % 4 == 0 ? 0.45f : 0.22f);
            s.slide = s.gate && r.nextFloat() < 0.25f;
        }
        return line;
    }

    juce::AudioProcessorValueTreeState::ParameterLayout createLayout()
    {
        using namespace juce;
        std::vector<std::unique_ptr<RangedAudioParameter>> v;
        auto pct = [] (float x, int) { return String (roundToInt (x * 100.0f)) + "%"; };
        auto ms = [] (float x, int) { return String (roundToInt (x * 1000.0f)) + " ms"; };
        auto flt = [&v] (const char* id, const char* name, NormalisableRange<float> r, float def, std::function<String (float, int)> t)
        { v.push_back (std::make_unique<AudioParameterFloat> (ParameterID { id, 1 }, name, r, def, AudioParameterFloatAttributes().withStringFromValueFunction (std::move (t)))); };
        auto skew = [] (float a, float b, float c) { NormalisableRange<float> r (a, b); r.setSkewForCentre (c); return r; };
        auto choice = [&v] (const char* id, const char* name, StringArray o, int def) { v.push_back (std::make_unique<AudioParameterChoice> (ParameterID { id, 1 }, name, o, def)); };

        choice ("wave", "Onda", { "Saw", "Square" }, 0);
        choice ("mode", "Suona", { "Sequenza", "Tastiera" }, 0);
        flt ("cutoff", "Cutoff", skew (60.0f, 6000.0f, 500.0f), 380.0f, [] (float x, int) { return x >= 1000.0f ? String (x / 1000.0f, 1) + " kHz" : String (roundToInt (x)) + " Hz"; });
        flt ("reso", "Risonanza", { 0.0f, 1.0f }, 0.7f, pct);
        flt ("envMod", "Env mod", { 0.0f, 1.0f }, 0.55f, pct);
        flt ("decay", "Decay", { 0.0f, 1.0f }, 0.35f, pct);
        flt ("accent", "Accent", { 0.0f, 1.0f }, 0.65f, pct);
        flt ("tune", "Tune", { -12.0f, 12.0f, 1.0f }, 0.0f, [] (float x, int) { return (x > 0 ? "+" : "") + String (roundToInt (x)) + " st"; });
        flt ("squelch", "Squelch", { 0.0f, 1.0f }, 0.4f, pct);
        flt ("acid", "Acido", { 0.0f, 1.0f }, 0.3f, pct);
        flt ("slideTime", "Slide", { 0.02f, 0.25f }, 0.065f, ms);
        flt ("gateLen", "Durata note", { 0.15f, 0.95f }, 0.55f, pct);
        flt ("level", "Livello", { 0.0f, 1.0f }, 0.8f, pct);
        flt ("revMix", "Riverbero", { 0.0f, 1.0f }, 0.0f, pct);
        flt ("revSize", "Stanza", { 0.0f, 1.0f }, 0.5f, pct);
        flt ("dlyMix", "Delay", { 0.0f, 1.0f }, 0.15f, pct);
        choice ("dlyTime", "Tempo delay", { "1/16", "1/8", "3/16", "1/4", "3/8", "1/2" }, 2);
        flt ("dlyFb", "Feedback", { 0.0f, 0.9f }, 0.35f, pct);
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
        // linee: nota ottava acceso accent slide (5 caratteri per step)
        static const std::vector<Factory> list
        {
            { "Init", nullptr, {} },
            { "Acid Classico", "00xA.00x.S30x..00...70xA.00x..a0x..00x.S00xA.50x..00x.S30xA.70x..0+x.Sa0x..30xA.", { { "master", 1.8f } } },
            { "Psy Acid 145", "00xA.00x..00x.S0+x..00xA.00x..30x.S00x..00xA.00x..70x.S00x..00xA.30x..00x.S5+x..",
              { { "cutoff", 300 }, { "reso", 0.8f }, { "envMod", 0.7f }, { "decay", 0.25f }, { "dlyMix", 0.25f }, { "dlyTime", 2 }, { "acid", 0.35f }, { "master", 0.6f } } },
            { "Acid Techno", "00xA.00...00x..00xAS00x..00...30xA.00x..00xA.00...70x.S50x..00xA.00x..3+x.S00x..",
              { { "cutoff", 450 }, { "reso", 0.75f }, { "acid", 0.6f }, { "squelch", 0.55f }, { "dlyMix", 0.1f } } },
            { "Gomma Acida", "00x.S30x.S50x.S70x..00xA.00...a0x.S70x..00x.S30x..00xA.50x.S70x..00...a0xAS70x..",
              { { "wave", 1 }, { "cutoff", 250 }, { "reso", 0.65f }, { "envMod", 0.45f }, { "decay", 0.5f }, { "slideTime", 0.1f }, { "master", -1.1f } } },
            { "Hi-tech Acid", "00xAS7+x.S00x.S3+xA.00x.S50x.S0+x.S10xA.00x.S7+x.S00xAS30x.S0+x.S50xA.10x.S00x.S",
              { { "cutoff", 700 }, { "reso", 0.85f }, { "envMod", 0.8f }, { "decay", 0.15f }, { "slideTime", 0.035f }, { "gateLen", 0.4f }, { "acid", 0.5f }, { "master", -0.4f } } },
            { "Goa Acid", "00xA.70x..0+x..70x..00xA.30x..50x.S70x..00xA.70x..0+xA.a0x.S70x..50x..30x.S00x..",
              { { "cutoff", 520 }, { "reso", 0.7f }, { "envMod", 0.5f }, { "decay", 0.45f }, { "revMix", 0.2f }, { "dlyMix", 0.25f }, { "master", -4.3f } } },
            { "Acido Scuro", "0-xA.0-x..00x.S0-x..1-x..0-xA.00...00x.S0-xA.0-x..10x.S0-x..0-xA.00...3-x.S0-x..",
              { { "cutoff", 180 }, { "reso", 0.75f }, { "envMod", 0.6f }, { "decay", 0.3f }, { "acid", 0.7f }, { "tune", -2 }, { "master", 0.8f } } },
            { "Bleep Quadra", "0+xA.00...7+x..00...3+xA.00...a0x..00...0+xA.00...7+x.S5+x..3+xA.00...0+x..00...",
              { { "wave", 1 }, { "cutoff", 1200 }, { "reso", 0.5f }, { "envMod", 0.4f }, { "decay", 0.2f }, { "gateLen", 0.3f }, { "dlyMix", 0.3f }, { "master", 3.5f } } },
            { "Squelch Totale", "00xAS00xAS30xAS00x.S70xA.00xAS30x.S00xAS00xAS50xA.00x.S70xAS00xAS30xAS00xA.a0x.S",
              { { "squelch", 0.95f }, { "reso", 0.85f }, { "envMod", 0.8f }, { "acid", 0.55f }, { "decay", 0.4f }, { "master", -1.6f } } },
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
        return juce::File::getSpecialLocation (juce::File::userApplicationDataDirectory).getChildFile ("AcidLine").getChildFile ("Presets");
    }
    juce::Array<juce::File> Presets::userPresets() const
    {
        auto files = userDir().findChildFiles (juce::File::findFiles, false, "*.acidpreset");
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
        if (auto xml = apvts.copyState().createXml()) return xml->writeTo (userDir().getChildFile (clean + ".acidpreset"));
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
AcidLineAudioProcessor::AcidLineAudioProcessor()
    : AudioProcessor (BusesProperties().withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
      apvts (*this, nullptr, "AcidLine", acid::createLayout())
{
    for (auto* id : acid::ids) p.push_back (apvts.getRawParameterValue (id));
    jassert ((int) p.size() == acid::numIds);
    presets.loadFactory (0);
}

acid::Step AcidLineAudioProcessor::getStep (int i) const   { return acid::unpack (packed[(size_t) juce::jlimit (0, acid::numSteps - 1, i)].load (std::memory_order_relaxed)); }
void AcidLineAudioProcessor::setStep (int i, const acid::Step& s)   { packed[(size_t) juce::jlimit (0, acid::numSteps - 1, i)].store (acid::pack (s), std::memory_order_relaxed); }

std::array<acid::Step, acid::numSteps> AcidLineAudioProcessor::getLine() const
{
    std::array<acid::Step, acid::numSteps> l {};
    for (int i = 0; i < acid::numSteps; ++i) l[(size_t) i] = getStep (i);
    return l;
}

void AcidLineAudioProcessor::setLine (const std::array<acid::Step, acid::numSteps>& l)
{
    for (int i = 0; i < acid::numSteps; ++i) setStep (i, l[(size_t) i]);
}

void AcidLineAudioProcessor::lineFromState()
{
    const auto t = apvts.state.getProperty ("line").toString();
    if (t.length() >= acid::numSteps * 5) setLine (acid::decode (t));
}

void AcidLineAudioProcessor::prepareToPlay (double sampleRate, int)
{
    sr = sampleRate;
    fx.prepare (sr);
    reset();
}

void AcidLineAudioProcessor::reset()
{
    held.clear();
    latchEnd = -1.0;
    gateOpen = false;
    amp = fEnv = 0.0f;
    for (auto& v : z) v = 0.0f;
    lastStep = -1;
    playStep.store (-1);
    fx.reset();
}

acid::BlockParams AcidLineAudioProcessor::readParams()
{
    using namespace acid;
    static constexpr double delayBeats[] { 0.25, 0.5, 0.75, 1.0, 1.5, 2.0 };
    BlockParams b;
    b.wave = (int) p[iWave]->load(); b.mode = (int) p[iMode]->load();
    b.cutoff = p[iCutoff]->load(); b.reso = p[iReso]->load(); b.envMod = p[iEnvMod]->load(); b.decay = p[iDecay]->load();
    b.accent = p[iAccent]->load(); b.tune = p[iTune]->load(); b.squelch = p[iSquelch]->load(); b.acid = p[iAcid]->load();
    b.slideTime = p[iSlide]->load(); b.gateLen = p[iGate]->load(); b.level = p[iLevel]->load();
    b.fx.reverbMix = p[iRevMix]->load(); b.fx.reverbSize = p[iRevSize]->load(); b.fx.delayMix = p[iDlyMix]->load();
    b.fx.delayBeats = delayBeats[juce::jlimit (0, 5, (int) p[iDlyTime]->load())]; b.fx.delayFeedback = p[iDlyFb]->load();
    b.fx.gain = juce::Decibels::decibelsToGain (p[iMaster]->load());
    return b;
}

void AcidLineAudioProcessor::startNote (double newPitch, bool accent, bool slide, const acid::BlockParams&)
{
    target = newPitch;
    if (slide && gateOpen) return;   // scivola: niente nuovo attacco ne' inviluppo
    pitch = newPitch;
    accentNow = accent ? 1.0f : 0.0f;
    fEnv = 1.0f;
    gateOpen = true;
}

void AcidLineAudioProcessor::render (float* L, float* R, int n, const acid::BlockParams& b, double bpm, double ppq)
{
    if (n <= 0) return;
    const double bps = bpm / 60.0;
    const double reso = juce::jlimit (0.0, 0.97, (double) b.reso + b.squelch * 0.3);
    const double envAmt = juce::jlimit (0.0, 1.0, (double) b.envMod + b.squelch * 0.4);
    const double k = reso * 4.1;
    const float drive = 1.0f + b.acid * 9.0f, dNorm = 0.85f / std::tanh (drive * 0.6f);
    const double glide = std::exp (-1.0 / (b.slideTime * 0.35 * sr));
    const float attack = (float) (1.0 / (0.003 * sr)), release = (float) std::exp (-1.0 / (0.008 * sr));
    const float holdDecay = (float) std::exp (-1.0 / (2.5 * sr));

    for (int i = 0; i < n; ++i)
    {
        double pos;
        if (ppq >= 0.0) pos = ppq + bps * (double) i / sr;
        else { pos = freeBeats; freeBeats += bps / sr; }

        if (b.mode == 0)   // --- sequenza
        {
            const bool active = ! held.empty() || pos < latchEnd - 0.001;   // (tolleranza: l'inizio della battuta dopo non e' compreso)
            const int stepIdx = (int) std::floor (pos * 4.0 + 1.0e-9);
            if (noteOnPending)
            {
                noteOnPending = false;
                if (pos * 4.0 - (double) stepIdx < 0.3) lastStep = stepIdx - 1;
            }
            if (stepIdx != lastStep)
            {
                lastStep = stepIdx;
                const int s = ((stepIdx % acid::numSteps) + acid::numSteps) % acid::numSteps;
                if (active)
                {
                    playStep.store (s, std::memory_order_relaxed);
                    const auto st = getStep (s);
                    const auto prev = getStep ((s + acid::numSteps - 1) % acid::numSteps);
                    if (st.gate)
                    {
                        const double np = rootNote - 12 + b.tune + st.note + 12 * st.octave;
                        startNote (np, st.accent, prev.gate && prev.slide, b);
                        // con lo slide la nota resta aperta fino alla prossima; senza, dura gateLen dello step
                        gateOff = (double) stepIdx / 4.0 + (st.slide ? 0.26 : 0.25 * b.gateLen);
                    }
                }
                else
                {
                    playStep.store (-1, std::memory_order_relaxed);
                    gateOpen = false;
                }
            }
            if (gateOpen && pos >= gateOff) gateOpen = false;
        }

        // --- voce
        pitch = target + (pitch - target) * glide;
        const double f = acid::mtof (pitch);
        const double dt = f / sr;
        ph += dt; if (ph >= 1.0) ph -= 1.0;
        double osc;
        if (b.wave == 1)
        {
            double t2 = ph + 0.5; t2 -= std::floor (t2);
            osc = (ph < 0.5 ? 1.0 : -1.0) + acid::blep (ph, dt) - acid::blep (t2, dt);
        }
        else osc = 2.0 * ph - 1.0 - acid::blep (ph, dt);

        if (gateOpen) amp = juce::jmin (1.0f, amp + attack) * holdDecay + (1.0f - holdDecay) * 0.0f;
        else amp *= release;
        const double decayT = accentNow > 0.5f ? 0.18 : 0.06 + 1.2 * b.decay * b.decay;
        fEnv *= (float) std::exp (-1.0 / (decayT * sr));
        const double accBoost = accentNow * b.accent;
        const double cutoff = juce::jlimit (30.0, sr * 0.42, b.cutoff * std::pow (2.0, (envAmt * (1.0 + accBoost * 0.8)) * fEnv * 5.5));
        const double g = 1.0 - std::exp (-juce::MathConstants<double>::twoPi * cutoff / sr);
        // filtro a 4 poli con risonanza (ladder), saturazione morbida
        const double in = std::tanh (osc * 0.9 - k * z[3]);
        z[0] += (float) (g * (in - z[0]));
        z[1] += (float) (g * (std::tanh (z[0]) - z[1]));
        z[2] += (float) (g * (std::tanh (z[1]) - z[2]));
        z[3] += (float) (g * (std::tanh (z[2]) - z[3]));
        double y = z[3] * (1.0 + k * 0.45);
        y = std::tanh (y * drive) * dNorm;
        const float out = (float) (y * amp * (1.0 + accBoost * 0.5) * b.level * 0.45);
        L[i] += out;
        R[i] += out;
    }
    fx.process (L, R, n, b.fx, bpm);
    for (int i = 0; i < n; ++i)
    {
        auto soft = [] (float v) { const float a = std::abs (v); return a < 0.85f ? v : std::copysign (0.85f + 0.15f * std::tanh ((a - 0.85f) * 6.0f), v); };
        L[i] = soft (L[i]);
        if (R != L) R[i] = soft (R[i]);
    }
}

void AcidLineAudioProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midi)
{
    juce::ScopedNoDenormals nd;
    buffer.clear();
    double bpm = 130.0, ppq = -1.0;
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
            if (b.mode == 0)
            {
                rootNote = m.getNoteNumber();
                const double pos = posAt (at);
                latchEnd = juce::jmax (latchEnd, std::floor ((pos + 0.03) / 4.0) * 4.0 + 4.0);   // la nota tiene accesa la sua battuta
                noteOnPending = true;
            }
            else   // tastiera: legato = slide, velocity alta = accent
                startNote (m.getNoteNumber() - 12 + b.tune, m.getFloatVelocity() > 0.8f, wasHeld, b);
        }
        else if (m.isNoteOff())
        {
            held.erase (std::remove (held.begin(), held.end(), m.getNoteNumber()), held.end());
            if (b.mode == 1)
            {
                if (held.empty()) gateOpen = false;
                else target = held.back() - 12 + b.tune;
            }
            else if (! held.empty()) rootNote = held.back();
        }
        else if (m.isAllNotesOff() || m.isAllSoundOff()) { held.clear(); latchEnd = -1.0; gateOpen = false; }
    }
    render (L + done, R + done, buffer.getNumSamples() - done, b, bpm, ppq >= 0.0 ? posAt (done) : -1.0);
}

juce::AudioProcessorEditor* AcidLineAudioProcessor::createEditor()   { return new AcidLineEditor (*this); }

void AcidLineAudioProcessor::getStateInformation (juce::MemoryBlock& d)
{
    lineToState();
    if (auto xml = apvts.copyState().createXml()) copyXmlToBinary (*xml, d);
}

void AcidLineAudioProcessor::setStateInformation (const void* data, int size)
{
    if (auto xml = getXmlFromBinary (data, size))
        if (xml->hasTagName (apvts.state.getType()))
        {
            apvts.replaceState (juce::ValueTree::fromXml (*xml));
            lineFromState();
        }
}
