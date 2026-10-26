#include "PsyForge.h"
#include "PsyForgeEditor.h"

namespace psy
{
    static const char* ids[] { "figure", "kickOn", "kickClick", "kickPunch", "kickDecay", "kickTune", "kickLevel",
                               "bassWave", "bassOct", "transpose", "cutoff", "reso", "drive", "bassLevel", "coda", "morso", "snap",
                               "revMix", "revSize", "dlyMix", "dlyTime", "dlyFb", "master" };
    enum { iFigure, iKickOn, iClick, iPunch, iKDecay, iKTune, iKLevel, iWave, iOct, iTranspose, iCutoff, iReso, iDrive, iBLevel,
           iCoda, iMorso, iSnap, iRevMix, iRevSize, iDlyMix, iDlyTime, iDlyFb, iMaster, numIds };

    static inline double mtof (double m) noexcept { return 440.0 * std::pow (2.0, (m - 69.0) / 12.0); }

    juce::String figureGrid (int figure)
    {
        switch (figure)
        {
            case 1:  return "K.BBK.BBK.BBK.BB";                    // Night: in levare, piu' scuro
            case 2:  return "K.B.K.B.K.B.K.B.";                    // Goa / progressive: ottavi in levare
            case 3:  return "K.BBBBBBK.BBBBBBK.BBBBBBK.BBBBBB";    // Hi-tech: trentaduesimi
            default: return "KBBBKBBBKBBBKBBB";                    // Full-on: K-B-B-B
        }
    }

    juce::AudioProcessorValueTreeState::ParameterLayout createLayout()
    {
        using namespace juce;
        std::vector<std::unique_ptr<RangedAudioParameter>> v;
        auto pct = [] (float x, int) { return String (roundToInt (x * 100.0f)) + "%"; };
        auto flt = [&v] (const char* id, const char* name, NormalisableRange<float> r, float def, std::function<String (float, int)> t)
        { v.push_back (std::make_unique<AudioParameterFloat> (ParameterID { id, 1 }, name, r, def, AudioParameterFloatAttributes().withStringFromValueFunction (std::move (t)))); };
        auto skew = [] (float a, float b, float c) { NormalisableRange<float> r (a, b); r.setSkewForCentre (c); return r; };
        auto choice = [&v] (const char* id, const char* name, StringArray o, int def) { v.push_back (std::make_unique<AudioParameterChoice> (ParameterID { id, 1 }, name, o, def)); };
        static const char* names[] { "C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B" };

        choice ("figure", "Figura", { "Full-on", "Night", "Goa", "Hi-tech" }, 0);
        v.push_back (std::make_unique<AudioParameterBool> (ParameterID { "kickOn", 1 }, "Cassa", true));
        flt ("kickClick", "Click", { 0.0f, 1.0f }, 0.5f, pct);
        flt ("kickPunch", "Punch", { 0.0f, 1.0f }, 0.5f, pct);
        flt ("kickDecay", "Coda cassa", { 0.0f, 1.0f }, 0.35f, pct);
        flt ("kickTune", "Accorda cassa", { -12.0f, 12.0f, 1.0f }, 0.0f, [] (float x, int) { return (x > 0 ? "+" : "") + String (roundToInt (x)) + " st"; });
        flt ("kickLevel", "Volume cassa", { 0.0f, 1.0f }, 0.85f, pct);
        choice ("bassWave", "Onda", { "Saw", "Square", "FM" }, 0);
        choice ("bassOct", "Ottava", { "-1", "0", "+1" }, 1);
        flt ("transpose", "Nota", { -12.0f, 12.0f, 1.0f }, 0.0f, [] (float x, int)
        {
            const int s = roundToInt (x);
            return String (names[((s % 12) + 12) % 12]) + "  (" + (s > 0 ? "+" : "") + String (s) + ")";
        });
        flt ("cutoff", "Cutoff", skew (60.0f, 8000.0f, 600.0f), 420.0f, [] (float x, int) { return x >= 1000.0f ? String (x / 1000.0f, 1) + " kHz" : String (roundToInt (x)) + " Hz"; });
        flt ("reso", "Risonanza", { 0.0f, 1.0f }, 0.3f, pct);
        flt ("drive", "Drive", { 0.0f, 1.0f }, 0.3f, pct);
        flt ("bassLevel", "Volume basso", { 0.0f, 1.0f }, 0.8f, pct);
        flt ("coda", "Coda", { 0.0f, 1.0f }, 0.3f, pct);
        flt ("morso", "Morso", { 0.0f, 1.0f }, 0.6f, pct);
        flt ("snap", "Schiocco", { 0.0f, 1.0f }, 0.3f, pct);
        flt ("revMix", "Riverbero", { 0.0f, 1.0f }, 0.0f, pct);
        flt ("revSize", "Stanza", { 0.0f, 1.0f }, 0.4f, pct);
        flt ("dlyMix", "Delay", { 0.0f, 1.0f }, 0.0f, pct);
        choice ("dlyTime", "Tempo delay", { "1/16", "1/8", "3/16", "1/4", "3/8", "1/2" }, 2);
        flt ("dlyFb", "Feedback", { 0.0f, 0.9f }, 0.3f, pct);
        flt ("master", "Volume", { -24.0f, 6.0f }, 0.0f, [] (float x, int) { return (x > 0.05f ? "+" : "") + String (x, 1) + " dB"; });
        return { v.begin(), v.end() };
    }

    //==========================================================================
    void Presets::set (const juce::String& id, float value)
    {
        if (auto* prm = apvts.getParameter (id))
            prm->setValueNotifyingHost (prm->convertTo0to1 (value));
    }

    struct Factory { const char* name; int figure; const char* grid; std::vector<std::pair<const char*, float>> values; };
    static const std::vector<Factory>& factory()
    {
        // bassWave 0 saw 1 square 2 FM | bassOct 0 = -1, 1 = 0, 2 = +1
        static const std::vector<Factory> list
        {
            { "Init", 0, nullptr, {} },
            { "Full-on Rolling", 0, nullptr, { { "cutoff", 380 }, { "reso", 0.35f }, { "morso", 0.65f }, { "coda", 0.28f }, { "snap", 0.3f }, { "drive", 0.35f }, { "master", 0.7f } } },
            { "Dark Forest", 1, nullptr, { { "bassWave", 1 }, { "cutoff", 220 }, { "reso", 0.5f }, { "morso", 0.75f }, { "coda", 0.22f }, { "snap", 0.2f },
                                            { "drive", 0.5f }, { "kickDecay", 0.25f }, { "dlyMix", 0.1f }, { "master", 1.7f } } },
            { "Goa Classic", 2, nullptr, { { "cutoff", 500 }, { "reso", 0.55f }, { "morso", 0.55f }, { "coda", 0.45f }, { "snap", 0.4f }, { "kickDecay", 0.45f },
                                            { "revMix", 0.12f }, { "master", -4.5f } } },
            { "Hi-tech 190", 3, nullptr, { { "bassWave", 2 }, { "cutoff", 700 }, { "reso", 0.3f }, { "morso", 0.8f }, { "coda", 0.1f }, { "snap", 0.12f },
                                            { "kickDecay", 0.18f }, { "kickPunch", 0.7f }, { "drive", 0.45f }, { "master", 2.0f } } },
            { "Twilight", 0, nullptr, { { "cutoff", 650 }, { "reso", 0.25f }, { "morso", 0.45f }, { "coda", 0.35f }, { "snap", 0.35f }, { "drive", 0.2f }, { "master", 0.5f } } },
            { "Progressive Psy", 2, nullptr, { { "bassWave", 1 }, { "cutoff", 300 }, { "reso", 0.2f }, { "morso", 0.4f }, { "coda", 0.5f }, { "snap", 0.5f },
                                                { "kickDecay", 0.5f }, { "revMix", 0.15f }, { "master", -4.9f } } },
            { "Zenonesque", 0, "KB.BKB.BKB.BK.BB", { { "cutoff", 450 }, { "reso", 0.45f }, { "morso", 0.7f }, { "coda", 0.15f }, { "snap", 0.2f },
                                                     { "dlyMix", 0.18f }, { "dlyTime", 2 }, { "master", 0.9f } } },
            { "Forest Night", 1, "K.BBK.BBK.B.KBBB", { { "bassWave", 2 }, { "cutoff", 260 }, { "reso", 0.6f }, { "morso", 0.85f }, { "coda", 0.18f },
                                                        { "drive", 0.55f }, { "kickTune", -2 }, { "master", 1.4f } } },
            { "Squelch 303", 0, nullptr, { { "cutoff", 300 }, { "reso", 0.82f }, { "morso", 0.9f }, { "coda", 0.3f }, { "snap", 0.25f }, { "drive", 0.6f }, { "master", 0.6f } } },
            { "Solo basso", 0, nullptr, { { "kickOn", 0 }, { "cutoff", 400 }, { "reso", 0.35f }, { "morso", 0.65f }, { "coda", 0.3f }, { "master", 3.4f } } },
            { "Hi-tech Twister", 3, "K.BBB.BBK.BBBB.BK.B.BBBBK.BB.BBB", { { "bassWave", 2 }, { "cutoff", 900 }, { "reso", 0.4f }, { "morso", 0.9f },
                                                                              { "coda", 0.08f }, { "snap", 0.1f }, { "drive", 0.6f }, { "kickDecay", 0.15f }, { "master", 2.7f } } },
        };
        return list;
    }

    int Presets::numFactory()                    { return (int) factory().size(); }
    juce::String Presets::factoryName (int i)    { return factory()[(size_t) juce::jlimit (0, numFactory() - 1, i)].name; }

    void Presets::loadFactory (int i)
    {
        const auto& f = factory()[(size_t) juce::jlimit (0, numFactory() - 1, i)];
        proc.lastFigure.store (f.figure);   // prima dei parametri: la figura non deve rimettere la griglia standard
        for (auto* prm : apvts.processor.getParameters())
            if (auto* r = dynamic_cast<juce::RangedAudioParameter*> (prm))
                r->setValueNotifyingHost (r->getDefaultValue());
        set ("figure", (float) f.figure);
        for (auto& [id, value] : f.values) set (id, value);
        proc.setGrid (f.grid != nullptr ? juce::String (f.grid) : figureGrid (f.figure));
        apvts.state.setProperty ("presetName", f.name, nullptr);
        proc.gridToState();
    }

    juce::File Presets::userDir() const
    {
        return juce::File::getSpecialLocation (juce::File::userApplicationDataDirectory).getChildFile ("PsyForge").getChildFile ("Presets");
    }
    juce::Array<juce::File> Presets::userPresets() const
    {
        auto files = userDir().findChildFiles (juce::File::findFiles, false, "*.psypreset");
        files.sort();
        return files;
    }
    bool Presets::save (const juce::String& name)
    {
        const auto clean = juce::File::createLegalFileName (name.trim());
        if (clean.isEmpty()) return false;
        userDir().createDirectory();
        apvts.state.setProperty ("presetName", clean, nullptr);
        proc.gridToState();
        if (auto xml = apvts.copyState().createXml()) return xml->writeTo (userDir().getChildFile (clean + ".psypreset"));
        return false;
    }
    bool Presets::load (const juce::File& file)
    {
        auto xml = juce::XmlDocument::parse (file);
        if (xml == nullptr || ! xml->hasTagName (apvts.state.getType())) return false;
        const auto tree = juce::ValueTree::fromXml (*xml);
        proc.lastFigure.store ((int) tree.getChildWithProperty ("id", "figure").getProperty ("value", 0.0f));
        apvts.replaceState (tree);
        apvts.state.setProperty ("presetName", file.getFileNameWithoutExtension(), nullptr);
        proc.gridFromState();
        return true;
    }
}

//==============================================================================
PsyForgeAudioProcessor::PsyForgeAudioProcessor()
    : AudioProcessor (BusesProperties().withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
      apvts (*this, nullptr, "PsyForge", psy::createLayout())
{
    for (auto* id : psy::ids) p.push_back (apvts.getRawParameterValue (id));
    jassert ((int) p.size() == psy::numIds);
    setGrid (psy::figureGrid (0));
    apvts.state.setProperty ("presetName", "Init", nullptr);
    gridToState();
}

void PsyForgeAudioProcessor::setGrid (const juce::String& g)
{
    for (int i = 0; i < psy::maxCells; ++i)
    {
        const auto c = i < g.length() ? g[i] : '.';
        setCell (i, c == 'K' ? 1 : c == 'B' ? 2 : 0);
    }
}

juce::String PsyForgeAudioProcessor::getGrid() const
{
    juce::String s;
    for (int i = 0; i < psy::maxCells; ++i)
        s << (getCell (i) == 1 ? "K" : getCell (i) == 2 ? "B" : ".");
    return s;
}

void PsyForgeAudioProcessor::gridFromState()
{
    const auto g = apvts.state.getProperty ("grid").toString();
    if (g.isNotEmpty()) setGrid (g);
    lastFigure.store ((int) p[psy::iFigure]->load());
}

int PsyForgeAudioProcessor::numCells() const   { return psy::cellsFor ((int) p[psy::iFigure]->load()); }

void PsyForgeAudioProcessor::prepareToPlay (double sampleRate, int)
{
    sr = sampleRate;
    fx.prepare (sr);
    reset();
}

void PsyForgeAudioProcessor::reset()
{
    held.clear();
    latchEnd = -1.0;
    kT = bT = 1.0e9;
    kEnv = bEnv = fEnv = 0.0f;
    s1 = s2 = s1b = s2b = 0.0f;
    lastCell = -1;
    playCell.store (-1);
    fx.reset();
}

psy::BlockParams PsyForgeAudioProcessor::readParams()
{
    using namespace psy;
    static constexpr double delayBeats[] { 0.25, 0.5, 0.75, 1.0, 1.5, 2.0 };
    BlockParams b;
    b.figure = (int) p[iFigure]->load(); b.kickOn = p[iKickOn]->load() > 0.5f;
    b.kickClick = p[iClick]->load(); b.kickPunch = p[iPunch]->load(); b.kickDecay = p[iKDecay]->load();
    b.kickTune = p[iKTune]->load(); b.kickLevel = p[iKLevel]->load();
    b.bassWave = (int) p[iWave]->load(); b.bassOct = (int) p[iOct]->load(); b.transpose = juce::roundToInt (p[iTranspose]->load());
    b.cutoff = p[iCutoff]->load(); b.reso = p[iReso]->load(); b.drive = p[iDrive]->load(); b.bassLevel = p[iBLevel]->load();
    b.coda = p[iCoda]->load(); b.morso = p[iMorso]->load(); b.snap = p[iSnap]->load();
    b.fx.reverbMix = p[iRevMix]->load(); b.fx.reverbSize = p[iRevSize]->load(); b.fx.delayMix = p[iDlyMix]->load();
    b.fx.delayBeats = delayBeats[juce::jlimit (0, 5, (int) p[iDlyTime]->load())]; b.fx.delayFeedback = p[iDlyFb]->load();
    b.fx.gain = juce::Decibels::decibelsToGain (p[iMaster]->load());
    return b;
}

void PsyForgeAudioProcessor::hit (int what, const psy::BlockParams& b)
{
    if (what == 1 && b.kickOn) { kT = 0.0; kPh = 0.0; }
    if (what == 2) { bT = 0.0; bPh = 0.0; bMod = 0.0; }   // fase da zero: ogni colpo uguale, "morso" pulito
}

void PsyForgeAudioProcessor::render (float* L, float* R, int n, const psy::BlockParams& b, double bpm, double ppq)
{
    if (n <= 0) return;
    constexpr double twoPi = juce::MathConstants<double>::twoPi;
    const int cellsN = psy::cellsFor (b.figure);
    const double cpb = cellsN / 4.0;                 // caselle per quarto
    const double bps = bpm / 60.0;
    const double bassPitch = note - 12 + b.transpose + (b.bassOct - 1) * 12;
    const double kickF0 = psy::mtof (bassPitch - 12 + b.kickTune);
    const double bassF = psy::mtof (bassPitch);
    const double kDecay = 0.05 + 0.5 * b.kickDecay, kSweep = 0.004 + 0.03 * b.kickPunch;
    const double bDecay = 0.015 + 0.2 * b.coda, fDecay = 0.008 + 0.08 * b.snap;
    const float k = 2.0f - 1.9f * b.reso;
    const float drive = 1.0f + b.drive * 6.0f, dNorm = 0.8f / std::tanh (drive * 0.5f);

    for (int i = 0; i < n; ++i)
    {
        double pos;
        if (ppq >= 0.0) pos = ppq + bps * (double) i / sr;
        else { pos = freeBeats; freeBeats += bps / sr; }
        const bool active = ! held.empty() || pos < latchEnd;
        const int cellIdx = (int) std::floor (pos * cpb + 1.0e-9);
        if (noteOnPending)
        {
            noteOnPending = false;
            const double frac = pos * cpb - (double) cellIdx;
            if (frac < 0.3) lastCell = cellIdx - 1;   // nota sul colpo: la casella attuale suona subito
        }
        if (cellIdx != lastCell)
        {
            lastCell = cellIdx;
            const int cell = ((cellIdx % cellsN) + cellsN) % cellsN;
            if (active)
            {
                playCell.store (cell, std::memory_order_relaxed);
                hit (getCell (cell), b);
            }
            else playCell.store (-1, std::memory_order_relaxed);
        }

        float out = 0.0f;
        // --- cassa: sinusoide con discesa d'intonazione rapidissima + click
        if (kT < 1.2 * sr)
        {
            const double t = kT / sr;
            const double f = kickF0 * (1.0 + 9.0 * std::exp (-t / kSweep));
            kPh += f / sr;
            const double body = std::sin (twoPi * kPh) * std::exp (-t / kDecay);
            const double click = (rng.nextFloat() * 2.0f - 1.0f) * std::exp (-t * 900.0) * 0.6 * b.kickClick
                               + std::sin (twoPi * 2400.0 * t) * std::exp (-t * 400.0) * 0.4 * b.kickClick;
            out += (float) (std::tanh ((body + click) * 1.6) * 0.6 * b.kickLevel);
            kT += 1.0;
        }
        // --- basso: colpo cortissimo con filtro che schiocca
        if (bT < 0.6 * sr)
        {
            const double t = bT / sr;
            const double env = juce::jmin (1.0, t / 0.0012) * std::exp (-t / bDecay);
            const double fe = std::exp (-t / fDecay);
            bPh += bassF / sr; bPh -= std::floor (bPh);
            double osc;
            if (b.bassWave == 1) osc = bPh < 0.5 ? 0.8 : -0.8;
            else if (b.bassWave == 2)
            {
                bMod += bassF * 2.0 / sr; bMod -= std::floor (bMod);
                osc = std::sin (twoPi * bPh + (0.6 + 3.5 * fe * b.morso) * std::sin (twoPi * bMod));
            }
            else osc = 2.0 * bPh - 1.0;
            const float cutoff = (float) juce::jlimit (40.0, sr * 0.45, b.cutoff * std::pow (2.0, b.morso * 5.0 * fe));
            const float g = std::tan (juce::MathConstants<float>::pi * cutoff / (float) sr);
            const float a1 = 1.0f / (1.0f + g * (g + k)), a2 = g * a1, a3 = g * a2;
            float x = (float) osc;
            {   // due SVF in fila (24 dB), risonanza sul primo
                const float v3 = x - s2, v1 = a1 * s1 + a2 * v3, v2 = s2 + a2 * s1 + a3 * v3;
                s1 = 2.0f * v1 - s1; s2 = 2.0f * v2 - s2; x = v2;
                const float b1 = 1.0f / (1.0f + g * (g + 1.4f)), b2 = g * b1, b3 = g * b2;
                const float w3 = x - s2b, w1 = b1 * s1b + b2 * w3, w2 = s2b + b2 * s1b + b3 * w3;
                s1b = 2.0f * w1 - s1b; s2b = 2.0f * w2 - s2b; x = w2;
            }
            x = std::tanh (x * drive) * dNorm;
            out += (float) (x * env * b.bassLevel * 0.5);
            bT += 1.0;
        }
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

void PsyForgeAudioProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midi)
{
    juce::ScopedNoDenormals nd;
    buffer.clear();
    double bpm = 145.0, ppq = -1.0;
    if (auto* ph = getPlayHead())
        if (auto pos = ph->getPosition())
        {
            if (auto bp = pos->getBpm()) bpm = *bp;
            if (pos->getIsPlaying()) if (auto q = pos->getPpqPosition()) ppq = *q;
        }
    const auto b = readParams();
    if (b.figure != lastFigure.load())   // figura nuova scelta dall'utente: griglia standard di quella figura
    {
        lastFigure.store (b.figure);
        setGrid (psy::figureGrid (b.figure));
    }
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
            if (held.empty() && ppq < 0.0 && posAt (at) >= latchEnd) { freeBeats = 0.0; lastCell = -1; }   // a trasporto fermo si parte dalla cassa
            held.push_back (m.getNoteNumber());
            note = m.getNoteNumber();
            const double pos = posAt (at);
            latchEnd = juce::jmax (latchEnd, std::floor (pos + 0.03) + 1.0);   // ogni nota tiene acceso almeno il suo quarto
            noteOnPending = true;
        }
        else if (m.isNoteOff())
        {
            held.erase (std::remove (held.begin(), held.end(), m.getNoteNumber()), held.end());
            if (! held.empty()) note = held.back();
        }
        else if (m.isAllNotesOff() || m.isAllSoundOff()) { held.clear(); latchEnd = -1.0; }
    }
    render (L + done, R + done, buffer.getNumSamples() - done, b, bpm, ppq >= 0.0 ? posAt (done) : -1.0);
}

juce::AudioProcessorEditor* PsyForgeAudioProcessor::createEditor()   { return new PsyForgeEditor (*this); }

void PsyForgeAudioProcessor::getStateInformation (juce::MemoryBlock& d)
{
    gridToState();
    if (auto xml = apvts.copyState().createXml()) copyXmlToBinary (*xml, d);
}

void PsyForgeAudioProcessor::setStateInformation (const void* data, int size)
{
    if (auto xml = getXmlFromBinary (data, size))
        if (xml->hasTagName (apvts.state.getType()))
        {
            const auto tree = juce::ValueTree::fromXml (*xml);
            lastFigure.store ((int) tree.getChildWithProperty ("id", "figure").getProperty ("value", 0.0f));
            apvts.replaceState (tree);
            gridFromState();
        }
}
