#include "Materia.h"
#include "MateriaEditor.h"

namespace materia
{
    //==========================================================================
    // I tipi di blocco

    const ModuleInfo& sourceInfo (int type)
    {
        static const std::vector<ModuleInfo> infos =
        {
            { "Vuoto", "", 0xff5a6a7c, { nullptr, nullptr, nullptr, nullptr }, { 0, 0, 0, 0 }, nullptr, {}, 0 },
            { "Oscillatore", "Le onde classiche: da sinusoide a quadra, con sync, piega e FM", 0xff44e6ff,
              { "FORMA", "SYNC", "PIEGA", "FM" }, { 0.66f, 0.0f, 0.0f, 0.0f }, nullptr, {}, 0 },
            { "Wavetable", "27 tabelle di forme che si trasformano (le stesse di Prisma)", 0xff6ea8ff,
              { "POSIZ.", "WARP", "PIEGA", "FM" }, { 0.0f, 0.0f, 0.0f, 0.0f },
              "Warp", { "Sync", "Bend +", "Bend -", "Specchio", "Quantizza", "Allunga" }, 0 },
            { "Rumore", "Dal soffio al fruscio, anche intonato sulla nota", 0xffc8d4e0,
              { "COLORE", "TONO", "GRANA", "LARGH." }, { 0.5f, 0.0f, 0.0f, 0.5f }, nullptr, {}, 0 },
            { "Corda", "Una corda vera che vibra: pizzicata, o tenuta con l'archetto", 0xff7dff9b,
              { "DUREZZA", "DURATA", "PUNTO", "ARCHETTO" }, { 0.6f, 0.6f, 0.2f, 0.0f },
              "Materiale", { "Nylon", "Acciaio", "Legno", "Vetro" }, 1 },
        };
        return infos[(size_t) juce::jlimit (0, (int) infos.size() - 1, type)];
    }

    const ModuleInfo& shaperInfo (int type)
    {
        static const std::vector<ModuleInfo> infos =
        {
            { "Vuoto", "", 0xff5a6a7c, { nullptr, nullptr, nullptr, nullptr }, { 0, 0, 0, 0 }, nullptr, {}, 0 },
            { "Filtro", "Toglie gli alti, i bassi o lascia una fascia", 0xffff6b8b,
              { "TAGLIO", "RISON.", "DRIVE", nullptr }, { 0.6f, 0.15f, 0.0f, 0 }, "Tipo", { "LP 12", "LP 24", "HP", "BP", "Notch" }, 1 },
            { "Distorsione", "Sporca e schiaccia il suono", 0xffff9a4d,
              { "QUANTO", "TONO", "BIAS", nullptr }, { 0.35f, 0.8f, 0.0f, 0 }, "Tipo", { "Morbida", "Dura", "Valvola", "Diodo", "Seno" }, 0 },
            { "Piega-onda", "Ripiega l'onda su se stessa: armonici nuovi", 0xffffc94d,
              { "PIEGHE", "ASIMM.", "MORBIDO", nullptr }, { 0.3f, 0.0f, 0.5f, 0 }, nullptr, {}, 0 },
            { "Risonatore", "Il suono risuona dentro una corda o un tubo intonato", 0xffb9a0ff,
              { "NOTA", "DURATA", "SMORZA", nullptr }, { 0.5f, 0.6f, 0.4f, 0 }, "Tipo", { "Corda", "Tubo" }, 0 },
            { "Schiaccia-bit", "Suono a pochi bit e a gradini, da vecchio computer", 0xff9be7c4,
              { "BIT", "GRADINI", nullptr, nullptr }, { 0.4f, 0.2f, 0, 0 }, nullptr, {}, 0 },
            { "Vocale", "Fa \"parlare\" il suono: A E I O U", 0xffff8ad8,
              { "VOCALE", "CARATT.", "SPOSTA", nullptr }, { 0.0f, 0.5f, 0.5f, 0 }, nullptr, {}, 0 },
            { "Chorus", "Allarga e raddoppia (sul suono intero)", 0xff7fd0ff,
              { nullptr, nullptr, nullptr, nullptr }, { 0, 0, 0, 0 }, nullptr, {}, 0 },
            { "Ritardo", "Eco a tempo (sul suono intero)", 0xff7fd0ff,
              { "RIPETI", nullptr, nullptr, nullptr }, { 0.4f, 0, 0, 0 }, "Tempo", { "1/16", "1/8", "1/8 punt.", "1/4", "3/8", "1/2" }, 2 },
            { "Riverbero", "Lo spazio attorno al suono (sul suono intero)", 0xff7fd0ff,
              { "GRANDEZZA", nullptr, nullptr, nullptr }, { 0.6f, 0, 0, 0 }, nullptr, {}, 0 },
        };
        return infos[(size_t) juce::jlimit (0, (int) infos.size() - 1, type)];
    }

    const ModuleInfo& moverInfo (int type)
    {
        static const std::vector<ModuleInfo> infos =
        {
            { "Vuoto", "", 0xff5a6a7c, { nullptr, nullptr, nullptr, nullptr }, { 0, 0, 0, 0 }, nullptr, {}, 0 },
            { "Inviluppo", "Un movimento che parte a ogni nota: attacco, decadimento, tenuta, rilascio", 0xffffe066,
              { "ATTACCO", "DECAD.", "TENUTA", "RILASCIO" }, { 0.0f, 0.45f, 0.0f, 0.4f }, nullptr, {}, 0 },
            { "LFO", "Un'onda lenta che va avanti e indietro, libera o a tempo", 0xffffe066,
              { "VELOCITA'", "A TEMPO", "FASE", nullptr }, { 0.5f, 0.0f, 0.0f, 0 },
              "Forma", { "Sinusoide", "Triangolo", "Sega su", "Sega giu'", "Quadra", "Casuale" }, 0 },
            { "Sequenza", "8 passi disegnati a mano, a tempo", 0xffffe066,
              { "TEMPO", "LISCIO", "PASSI", nullptr }, { 0.4f, 0.0f, 1.0f, 0 }, "Verso", { "Avanti", "Indietro", "Avanti e indietro", "A caso" }, 0 },
            { "Casuale", "Un valore diverso a ogni nota", 0xffffe066, { nullptr, nullptr, nullptr, nullptr }, { 0, 0, 0, 0 }, nullptr, {}, 0 },
            { "Nota", "Segue la nota suonata: basso = meno, alto = piu'", 0xffffe066, { nullptr, nullptr, nullptr, nullptr }, { 0, 0, 0, 0 }, nullptr, {}, 0 },
            { "Forza", "Segue la forza del tocco (velocity)", 0xffffe066, { nullptr, nullptr, nullptr, nullptr }, { 0, 0, 0, 0 }, nullptr, {}, 0 },
            { "Rotella", "Segue la rotella di modulazione", 0xffffe066, { nullptr, nullptr, nullptr, nullptr }, { 0, 0, 0, 0 }, nullptr, {}, 0 },
        };
        return infos[(size_t) juce::jlimit (0, (int) infos.size() - 1, type)];
    }

    juce::String sourceId (int slot, const char* suffix)    { return "s" + juce::String (slot + 1) + suffix; }
    juce::String shaperId (int slot, const char* suffix)    { return "f" + juce::String (slot + 1) + suffix; }
    juce::String moverId (int slot, const char* suffix)     { return "m" + juce::String (slot + 1) + suffix; }

    //==========================================================================
    // Conversioni delle manopole (0..1) nei valori veri

    static inline double envSeconds (float x) noexcept      { return 0.001 + (double) x * x * x * 6.0; }
    static inline double lfoHz (float x) noexcept           { return 0.05 * std::pow (600.0, (double) x); }
    static inline double cutoffHz (float x) noexcept        { return 20.0 * std::pow (1000.0, (double) juce::jlimit (0.0f, 1.0f, x)); }
    static inline int lfoSyncIndex (float x) noexcept       { return juce::jlimit (0, 9, juce::roundToInt (x * 9.0f)); }
    static inline int seqRateIndex (float x) noexcept       { return juce::jlimit (0, 5, juce::roundToInt (x * 5.0f)); }
    static inline int seqLength (float x) noexcept          { return 2 + juce::jlimit (0, 6, juce::roundToInt (x * 6.0f)); }
    static inline int resonatorSemis (float x) noexcept     { return juce::roundToInt ((x - 0.5f) * 48.0f); }
    static constexpr double lfoBeats[] { 0.0, 16.0, 8.0, 4.0, 2.0, 1.0, 0.5, 0.25, 2.0 / 3.0, 1.0 / 3.0 };
    static constexpr const char* lfoBeatNames[] { "Libero", "4 batt.", "2 batt.", "1 batt.", "1/2", "1/4", "1/8", "1/16", "1/4 T", "1/8 T" };
    static constexpr double seqBeats[] { 1.0, 0.5, 0.25, 0.125, 1.0 / 3.0, 1.0 / 6.0 };
    static constexpr const char* seqBeatNames[] { "1/4", "1/8", "1/16", "1/32", "1/8 T", "1/16 T" };
    static constexpr double delayBeats[] { 0.25, 0.5, 0.75, 1.0, 1.5, 2.0 };
    static constexpr int unisonCounts[] { 1, 2, 3, 5, 7 };

    juce::String knobText (int group, int type, int knob, float v)
    {
        auto pct = [] (float x) { return juce::String (juce::roundToInt (x * 100.0f)) + "%"; };
        auto secs = [] (double s) { return s < 1.0 ? juce::String (juce::roundToInt (s * 1000.0)) + " ms" : juce::String (s, 2) + " s"; };
        if (group == 1)
        {
            if (type == shFilter && knob == 0)
            {
                const double hz = cutoffHz (v);
                return hz < 1000.0 ? juce::String (juce::roundToInt (hz)) + " Hz" : juce::String (hz / 1000.0, 1) + " kHz";
            }
            if (type == shResonator && knob == 0) { const int s = resonatorSemis (v); return (s > 0 ? "+" : "") + juce::String (s) + " st"; }
            if (type == shResonator && knob == 1) return secs (0.05 * std::pow (200.0, (double) v));
            if (type == shCrush && knob == 0)     return juce::String (juce::roundToInt (16.0f - v * 14.0f)) + " bit";
            if (type == shVowel && knob == 0)
            {
                static const char* vowels[] { "A", "E", "I", "O", "U" };
                const float p = v * 4.0f;
                const int i = juce::jmin (3, (int) p);
                return p - (float) i < 0.15f ? vowels[i] : p - (float) i > 0.85f ? vowels[i + 1] : juce::String (vowels[i]) + ">" + vowels[i + 1];
            }
            if (type == shFold && knob == 1)      return (v > 0.005f ? "+" : "") + pct (v);
        }
        if (group == 2)
        {
            if (type == mvEnv && knob != 2) return secs (envSeconds (v));
            if (type == mvLfo && knob == 0) { const double hz = lfoHz (v); return juce::String (hz, hz < 1.0 ? 2 : 1) + " Hz"; }
            if (type == mvLfo && knob == 1) return lfoBeatNames[lfoSyncIndex (v)];
            if (type == mvSeq && knob == 0) return seqBeatNames[seqRateIndex (v)];
            if (type == mvSeq && knob == 2) return juce::String (seqLength (v));
        }
        if (group == 0 && type == srcString && knob == 1) return secs (0.1 * std::pow (100.0, (double) v));
        return pct (v);
    }

    juce::String destName (juce::AudioProcessorValueTreeState& s, int dest)
    {
        if (dest <= 0 || dest >= numDests) return {};
        auto typeOf = [&s] (const juce::String& id) { return (int) s.getRawParameterValue (id)->load(); };
        const int d = dest - 1;
        if (d < numSources * destsPerSource)
        {
            const int slot = d / destsPerSource, k = d % destsPerSource;
            const int type = typeOf (sourceId (slot, "Type"));
            if (type == srcEmpty) return {};
            const auto& info = sourceInfo (type);
            static const char* common[] { "DETUNE", "NOTA", "VOLUME", "PAN" };
            const char* label = k < numKnobs ? info.knobs[k] : common[k - numKnobs];
            if (label == nullptr) return {};
            return juce::String (info.name) + " " + juce::String (slot + 1) + ": " + label;
        }
        const int e = d - numSources * destsPerSource;
        const int slot = e / destsPerShaper, k = e % destsPerShaper;
        const int type = typeOf (shaperId (slot, "Type"));
        if (type == shEmpty) return {};
        const auto& info = shaperInfo (type);
        const char* label = k < 3 ? info.knobs[k] : "MIX";
        if (label == nullptr) return {};
        return juce::String (info.name) + " " + juce::String (slot + 1) + ": " + label;
    }

    //==========================================================================
    // Parametri

    static const char* const sourceSuffixes[] { "Type", "Sel", "Mode", "A", "B", "C", "D", "Uni", "Det", "Oct", "Semi", "Fine", "Lvl", "Pan" };
    enum { sType, sSel, sMode, sA, sB, sC, sD, sUni, sDet, sOct, sSemi, sFine, sLvl, sPan, numSourceParams };
    static const char* const shaperSuffixes[] { "Type", "Mode", "A", "B", "C", "Mix" };
    enum { fType, fMode, fA, fB, fC, fMix, numShaperParams };
    static const char* const moverSuffixes[] { "Type", "Mode", "A", "B", "C", "D", "S1", "S2", "S3", "S4", "S5", "S6", "S7", "S8",
                                               "T1", "T2", "T3", "Q1", "Q2", "Q3" };
    enum { mType, mMode, mA, mB, mC, mD, mS1, mT1 = mS1 + numSteps, mQ1 = mT1 + numTargets, numMoverParams = mQ1 + numTargets };
    static const char* const globalIds[] { "ampA", "ampD", "ampS", "ampR", "voiceMode", "glide", "master" };
    enum { gAmpA, gAmpD, gAmpS, gAmpR, gVoiceMode, gGlide, gMaster, numGlobals };

    juce::AudioProcessorValueTreeState::ParameterLayout createLayout()
    {
        using namespace juce;
        std::vector<std::unique_ptr<RangedAudioParameter>> v;
        auto pct = [] (float x, int) { return String (roundToInt (x * 100.0f)) + "%"; };
        auto bip = [] (float x, int) { return (x > 0.005f ? "+" : "") + String (roundToInt (x * 100.0f)) + "%"; };
        auto secs = [] (float x, int) { return x < 1.0f ? String (roundToInt (x * 1000.0f)) + " ms" : String (x, 2) + " s"; };
        auto flt = [&v] (String id, String name, NormalisableRange<float> r, float def, std::function<String (float, int)> t)
        { v.push_back (std::make_unique<AudioParameterFloat> (ParameterID { id, 1 }, name, r, def, AudioParameterFloatAttributes().withStringFromValueFunction (std::move (t)))); };
        auto choice = [&v] (String id, String name, StringArray o, int def) { v.push_back (std::make_unique<AudioParameterChoice> (ParameterID { id, 1 }, name, o, def)); };
        auto integer = [&v] (String id, String name, int lo, int hi, int def) { v.push_back (std::make_unique<AudioParameterInt> (ParameterID { id, 1 }, name, lo, hi, def)); };
        auto timeRange = [] (float lo, float hi) { NormalisableRange<float> r (lo, hi); r.setSkewForCentre (lo + (hi - lo) * 0.08f); return r; };
        auto names = [] (int count, const ModuleInfo& (*info) (int)) { StringArray a; for (int i = 0; i < count; ++i) a.add (info (i).name); return a; };
        static const char* letters[] { "A", "B", "C", "D" };

        for (int s = 0; s < numSources; ++s)
        {
            const String N = "Sorgente " + String (s + 1) + " ";
            choice (sourceId (s, "Type"), N + "tipo", names (numSourceTypes, sourceInfo), 0);
            integer (sourceId (s, "Sel"), N + "tabella", 0, prisma::maxTables - 1, 0);
            integer (sourceId (s, "Mode"), N + "modo", 0, 9, 0);
            for (auto* l : letters) flt (sourceId (s, l), N + l, { 0.0f, 1.0f }, 0.0f, pct);
            choice (sourceId (s, "Uni"), N + "unisono", { "1 voce", "2 voci", "3 voci", "5 voci", "7 voci" }, 0);
            flt (sourceId (s, "Det"), N + "detune", { 0.0f, 1.0f }, 0.25f, pct);
            choice (sourceId (s, "Oct"), N + "ottava", { "OTT -3", "OTT -2", "OTT -1", "OTT 0", "OTT +1", "OTT +2", "OTT +3" }, 3);
            flt (sourceId (s, "Semi"), N + "semitoni", { -12.0f, 12.0f, 1.0f }, 0.0f, [] (float x, int) { return (x > 0 ? "+" : "") + String (roundToInt (x)) + " st"; });
            flt (sourceId (s, "Fine"), N + "fine", { -100.0f, 100.0f }, 0.0f, [] (float x, int) { return (x > 0 ? "+" : "") + String (roundToInt (x)) + " ct"; });
            flt (sourceId (s, "Lvl"), N + "volume", { 0.0f, 1.0f }, 0.75f, pct);
            flt (sourceId (s, "Pan"), N + "pan", { -1.0f, 1.0f }, 0.0f, bip);
        }
        for (int s = 0; s < numShapers; ++s)
        {
            const String N = "Forma " + String (s + 1) + " ";
            choice (shaperId (s, "Type"), N + "tipo", names (numShaperTypes, shaperInfo), 0);
            integer (shaperId (s, "Mode"), N + "modo", 0, 9, 0);
            for (int k = 0; k < 3; ++k) flt (shaperId (s, letters[k]), N + letters[k], { 0.0f, 1.0f }, 0.0f, pct);
            flt (shaperId (s, "Mix"), N + "mix", { 0.0f, 1.0f }, 1.0f, pct);
        }
        for (int s = 0; s < numMovers; ++s)
        {
            const String N = "Movimento " + String (s + 1) + " ";
            choice (moverId (s, "Type"), N + "tipo", names (numMoverTypes, moverInfo), 0);
            integer (moverId (s, "Mode"), N + "modo", 0, 9, 0);
            for (auto* l : letters) flt (moverId (s, l), N + l, { 0.0f, 1.0f }, 0.0f, pct);
            for (int k = 0; k < numSteps; ++k)
                flt (moverId (s, ("S" + String (k + 1)).toRawUTF8()), N + "passo " + String (k + 1), { -1.0f, 1.0f }, 0.0f, bip);
            for (int k = 0; k < numTargets; ++k)
                integer (moverId (s, ("T" + String (k + 1)).toRawUTF8()), N + "verso " + String (k + 1), 0, numDests - 1, 0);
            for (int k = 0; k < numTargets; ++k)
                flt (moverId (s, ("Q" + String (k + 1)).toRawUTF8()), N + "quanto " + String (k + 1), { -1.0f, 1.0f }, 0.0f, bip);
        }
        flt ("ampA", "Volume: attacco", timeRange (0.001f, 4.0f), 0.004f, secs);
        flt ("ampD", "Volume: decadimento", timeRange (0.005f, 6.0f), 0.4f, secs);
        flt ("ampS", "Volume: tenuta", { 0.0f, 1.0f }, 0.8f, pct);
        flt ("ampR", "Volume: rilascio", timeRange (0.005f, 8.0f), 0.25f, secs);
        choice ("voiceMode", "Voci", { "POLY", "MONO", "LEGATO" }, 0);
        flt ("glide", "Glide", timeRange (0.0f, 1.5f), 0.0f, secs);
        flt ("master", "Volume", { -24.0f, 12.0f }, 0.0f, [] (float x, int) { return String (x, 1) + " dB"; });
        return { v.begin(), v.end() };
    }

    //==========================================================================
    // Preset: punti di partenza (si costruisce tutto da qui)

    void Presets::set (const juce::String& id, float value)
    {
        if (auto* prm = apvts.getParameter (id))
            prm->setValueNotifyingHost (prm->convertTo0to1 (value));
    }

    struct Factory { const char* name; std::vector<std::pair<const char*, float>> values; };
    static const std::vector<Factory>& factory()
    {
        // Sorgenti: 1 oscillatore 2 wavetable 3 rumore 4 corda | Forma: 1 filtro 2 distorsione 3 piega 4 risonatore 5 bit 6 vocale 7 chorus 8 ritardo 9 riverbero
        // Movimento: 1 inviluppo 2 LFO 3 sequenza 4 casuale 5 nota 6 forza 7 rotella
        // Destinazioni: sorgente n (0..3): 1 + n*8 + (A0 B1 C2 D3 detune4 nota5 volume6 pan7) | forma n (0..5): 33 + n*4 + (A0 B1 C2 mix3)
        static const std::vector<Factory> f =
        {
            { "Un oscillatore", { { "s1Type", 1 }, { "s1A", 0.66f } } },
            { "Vuoto", {} },
            { "Corda pizzicata", { { "s1Type", 4 }, { "s1Mode", 1 }, { "s1A", 0.6f }, { "s1B", 0.65f }, { "s1C", 0.2f }, { "ampS", 1.0f }, { "ampR", 0.6f },
                                   { "f1Type", 9 }, { "f1A", 0.5f }, { "f1Mix", 0.2f } } },
            { "Vetro e archetto", { { "s1Type", 4 }, { "s1Mode", 3 }, { "s1A", 0.3f }, { "s1B", 0.9f }, { "s1C", 0.35f }, { "s1D", 0.6f }, { "s1Oct", 4 },
                                    { "ampA", 0.5f }, { "ampS", 1.0f }, { "ampR", 1.5f }, { "f1Type", 9 }, { "f1A", 0.85f }, { "f1Mix", 0.45f },
                                    { "m1Type", 2 }, { "m1A", 0.45f }, { "m1T1", 4 }, { "m1Q1", 0.3f } } },
            { "Voce che si muove", { { "s1Type", 2 }, { "s1Sel", 22 }, { "s1A", 0.5f }, { "s1Uni", 2 }, { "s1Det", 0.2f }, { "f1Type", 1 }, { "f1Mode", 1 }, { "f1A", 0.75f },
                                     { "m1Type", 2 }, { "m1Mode", 1 }, { "m1B", 0.45f }, { "m1T1", 1 }, { "m1Q1", 0.5f }, { "ampR", 0.4f } } },
            { "Tubo che soffia", { { "s1Type", 3 }, { "s1A", 0.55f }, { "s1Lvl", 0.6f }, { "f1Type", 4 }, { "f1Mode", 1 }, { "f1A", 0.5f }, { "f1B", 0.8f }, { "f1C", 0.5f },
                                   { "f2Type", 9 }, { "f2A", 0.7f }, { "f2Mix", 0.3f }, { "ampA", 0.08f }, { "ampS", 1.0f }, { "ampR", 0.5f } } },
            { "Passi sul filtro", { { "s1Type", 1 }, { "s1A", 0.66f }, { "s1Oct", 2 }, { "voiceMode", 1 }, { "f1Type", 1 }, { "f1Mode", 1 }, { "f1A", 0.35f }, { "f1B", 0.55f },
                                    { "m1Type", 3 }, { "m1A", 0.4f }, { "m1C", 1.0f }, { "m1S1", 1.0f }, { "m1S2", -0.2f }, { "m1S3", 0.5f }, { "m1S4", -0.6f },
                                    { "m1S5", 0.8f }, { "m1S6", 0.0f }, { "m1S7", -0.4f }, { "m1S8", 0.3f }, { "m1T1", 33 }, { "m1Q1", 0.35f } } },
            { "Campana FM", { { "s1Type", 1 }, { "s1A", 0.0f }, { "s1Semi", 7 }, { "s1Oct", 4 }, { "s1Lvl", 0.0f }, { "s2Type", 1 }, { "s2A", 0.0f }, { "s2D", 0.15f },
                              { "m1Type", 1 }, { "m1B", 0.6f }, { "m1T1", 12 }, { "m1Q1", 0.5f },
                              { "ampD", 2.5f }, { "ampS", 0.0f }, { "ampR", 2.0f }, { "f1Type", 9 }, { "f1A", 0.7f }, { "f1Mix", 0.3f }, { "master", -5.0f } } },
        };
        return f;
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
        return juce::File::getSpecialLocation (juce::File::userApplicationDataDirectory).getChildFile ("Materia").getChildFile ("Presets");
    }
    juce::Array<juce::File> Presets::userPresets() const
    {
        auto files = userDir().findChildFiles (juce::File::findFiles, false, "*.materiapreset");
        files.sort();
        return files;
    }
    bool Presets::save (const juce::String& name)
    {
        const auto clean = juce::File::createLegalFileName (name.trim());
        if (clean.isEmpty()) return false;
        userDir().createDirectory();
        apvts.state.setProperty ("presetName", clean, nullptr);
        if (auto xml = apvts.copyState().createXml()) return xml->writeTo (userDir().getChildFile (clean + ".materiapreset"));
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
struct MateriaAudioProcessor::Ids
{
    std::vector<std::atomic<float>*> src, shp, mov, global;
};

struct MateriaAudioProcessor::Block
{
    struct Src { int type, sel, mode, uni, oct; float k[materia::numKnobs], detune, semi, fine, level, pan; };
    struct Shp { int type, mode; float k[3], mix; };
    struct Mov { int type, mode; float k[materia::numKnobs], step[materia::numSteps]; int dst[materia::numTargets]; float amt[materia::numTargets]; };
    Src src[materia::numSources];
    Shp shp[materia::numShapers];
    Mov mov[materia::numMovers];
    float amp[4];
    int voiceMode;
    float glide, gain;
};

MateriaAudioProcessor::MateriaAudioProcessor()
    : AudioProcessor (BusesProperties().withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
      apvts (*this, nullptr, "Materia", materia::createLayout())
{
    using namespace materia;
    prisma::Bank::get();
    ids = std::make_unique<Ids>();
    for (int s = 0; s < numSources; ++s) for (auto* x : sourceSuffixes) ids->src.push_back (apvts.getRawParameterValue (sourceId (s, x)));
    for (int s = 0; s < numShapers; ++s) for (auto* x : shaperSuffixes) ids->shp.push_back (apvts.getRawParameterValue (shaperId (s, x)));
    for (int s = 0; s < numMovers; ++s)  for (auto* x : moverSuffixes)  ids->mov.push_back (apvts.getRawParameterValue (moverId (s, x)));
    for (auto* x : globalIds) ids->global.push_back (apvts.getRawParameterValue (x));
    for (auto* list : { &ids->src, &ids->shp, &ids->mov, &ids->global }) for (auto* x : *list) { jassert (x != nullptr); juce::ignoreUnused (x); }
    for (int s = 0; s < numSources; ++s)
    {
        apvts.addParameterListener (sourceId (s, "Sel"), this);
        apvts.addParameterListener (sourceId (s, "Type"), this);
    }
    stringLines.assign ((size_t) maxVoices * numSources * lineSize, 0.0f);
    combLines.assign ((size_t) maxVoices * numShapers * 2 * lineSize, 0.0f);
    presets.loadFactory (0);
    ensureTables();
}

MateriaAudioProcessor::~MateriaAudioProcessor()
{
    for (int s = 0; s < materia::numSources; ++s)
    {
        apvts.removeParameterListener (materia::sourceId (s, "Sel"), this);
        apvts.removeParameterListener (materia::sourceId (s, "Type"), this);
    }
}

void MateriaAudioProcessor::ensureTables()
{
    for (int s = 0; s < materia::numSources; ++s)
        if ((int) apvts.getRawParameterValue (materia::sourceId (s, "Type"))->load() == materia::srcWave)
            prisma::Bank::get().ensure ((int) apvts.getRawParameterValue (materia::sourceId (s, "Sel"))->load());
}

void MateriaAudioProcessor::parameterChanged (const juce::String&, float)   { ensureTables(); }

//==============================================================================
// Aggiungere, togliere, spostare i blocchi (dal thread dell'interfaccia)

void MateriaAudioProcessor::resetSlot (const juce::String& prefix, const materia::ModuleInfo& info, int type, bool isMover)
{
    auto set = [this] (const juce::String& id, float v) { presets.set (id, v); };
    static const char* letters[] { "A", "B", "C", "D" };
    set (prefix + "Mode", (float) info.defaultMode);
    for (int k = 0; k < materia::numKnobs; ++k)
        if (apvts.getParameter (prefix + letters[k]) != nullptr)
            set (prefix + letters[k], info.defaults[k]);
    if (isMover)
    {
        for (int k = 0; k < materia::numSteps; ++k)
            set (prefix + "S" + juce::String (k + 1), type == materia::mvSeq ? (k % 2 == 0 ? 0.8f : -0.4f) * (k < 4 ? 1.0f : 0.6f) : 0.0f);
        for (int k = 0; k < materia::numTargets; ++k) { set (prefix + "T" + juce::String (k + 1), 0.0f); set (prefix + "Q" + juce::String (k + 1), 0.0f); }
    }
    set (prefix + "Type", (float) type);
}

int MateriaAudioProcessor::addSource (int type)
{
    using namespace materia;
    for (int s = 0; s < numSources; ++s)
        if ((int) apvts.getRawParameterValue (sourceId (s, "Type"))->load() == srcEmpty)
        {
            const auto p = sourceId (s, "");
            for (auto* id : { "Sel", "Uni", "Semi", "Fine", "Pan" }) presets.set (p + id, 0.0f);
            presets.set (p + "Det", 0.25f); presets.set (p + "Oct", 3.0f); presets.set (p + "Lvl", 0.75f);
            resetSlot (p, sourceInfo (type), type, false);
            return s;
        }
    return -1;
}

int MateriaAudioProcessor::addShaper (int type)
{
    using namespace materia;
    // i blocchi "spazio" (chorus, ritardo, riverbero) vanno comunque in fondo: si prende il primo posto libero
    for (int s = 0; s < numShapers; ++s)
        if ((int) apvts.getRawParameterValue (shaperId (s, "Type"))->load() == shEmpty)
        {
            presets.set (shaperId (s, "Mix"), isSpace (type) ? 0.3f : 1.0f);
            resetSlot (shaperId (s, ""), shaperInfo (type), type, false);
            return s;
        }
    return -1;
}

int MateriaAudioProcessor::addMover (int type)
{
    using namespace materia;
    for (int s = 0; s < numMovers; ++s)
        if ((int) apvts.getRawParameterValue (moverId (s, "Type"))->load() == mvEmpty)
        {
            resetSlot (moverId (s, ""), moverInfo (type), type, true);
            return s;
        }
    return -1;
}

void MateriaAudioProcessor::removeSource (int slot)
{
    using namespace materia;
    presets.set (sourceId (slot, "Type"), 0.0f);
    // i movimenti che lo muovevano si scollegano
    for (int m = 0; m < numMovers; ++m)
        for (int k = 0; k < numTargets; ++k)
        {
            const auto id = moverId (m, "T") + juce::String (k + 1);
            const int d = (int) apvts.getRawParameterValue (id)->load();
            if (d >= sourceDest (slot, 0) && d < sourceDest (slot, 0) + destsPerSource) presets.set (id, 0.0f);
        }
}

void MateriaAudioProcessor::removeShaper (int slot)
{
    using namespace materia;
    presets.set (shaperId (slot, "Type"), 0.0f);
    for (int m = 0; m < numMovers; ++m)
        for (int k = 0; k < numTargets; ++k)
        {
            const auto id = moverId (m, "T") + juce::String (k + 1);
            const int d = (int) apvts.getRawParameterValue (id)->load();
            if (d >= shaperDest (slot, 0) && d < shaperDest (slot, 0) + destsPerShaper) presets.set (id, 0.0f);
        }
}

void MateriaAudioProcessor::removeMover (int slot)     { presets.set (materia::moverId (slot, "Type"), 0.0f); }

void MateriaAudioProcessor::moveShaper (int slot, int delta)
{
    using namespace materia;
    // si scambia col primo blocco non vuoto in quella direzione
    int other = slot + delta;
    while (other >= 0 && other < numShapers && (int) apvts.getRawParameterValue (shaperId (other, "Type"))->load() == shEmpty) other += delta;
    if (other < 0 || other >= numShapers) return;
    for (auto* suffix : shaperSuffixes)
    {
        auto* a = apvts.getParameter (shaperId (slot, suffix));
        auto* b = apvts.getParameter (shaperId (other, suffix));
        const float va = a->getValue(), vb = b->getValue();
        a->setValueNotifyingHost (vb);
        b->setValueNotifyingHost (va);
    }
    // i movimenti seguono il loro blocco
    for (int m = 0; m < numMovers; ++m)
        for (int k = 0; k < numTargets; ++k)
        {
            const auto id = moverId (m, "T") + juce::String (k + 1);
            const int d = (int) apvts.getRawParameterValue (id)->load();
            const int a0 = shaperDest (slot, 0), b0 = shaperDest (other, 0);
            if (d >= a0 && d < a0 + destsPerShaper) presets.set (id, (float) (d - a0 + b0));
            else if (d >= b0 && d < b0 + destsPerShaper) presets.set (id, (float) (d - b0 + a0));
        }
}

bool MateriaAudioProcessor::connect (int mover, int dest, float amount)
{
    using namespace materia;
    for (int k = 0; k < numTargets; ++k)
    {
        const auto t = moverId (mover, "T") + juce::String (k + 1);
        const int d = (int) apvts.getRawParameterValue (t)->load();
        if (d == 0 || d == dest)
        {
            presets.set (t, (float) dest);
            presets.set (moverId (mover, "Q") + juce::String (k + 1), amount);
            return true;
        }
    }
    return false;
}

//==============================================================================
void MateriaAudioProcessor::readBlock (Block& b)
{
    using namespace materia;
    for (int s = 0; s < numSources; ++s)
    {
        auto P = [&] (int i) { return ids->src[(size_t) (s * numSourceParams + i)]->load(); };
        auto& x = b.src[s];
        x.type = (int) P (sType); x.sel = (int) P (sSel); x.mode = (int) P (sMode);
        for (int k = 0; k < numKnobs; ++k) x.k[k] = P (sA + k);
        x.uni = unisonCounts[juce::jlimit (0, 4, (int) P (sUni))]; x.detune = P (sDet); x.oct = (int) P (sOct) - 3;
        x.semi = P (sSemi); x.fine = P (sFine); x.level = P (sLvl); x.pan = P (sPan);
    }
    for (int s = 0; s < numShapers; ++s)
    {
        auto P = [&] (int i) { return ids->shp[(size_t) (s * numShaperParams + i)]->load(); };
        auto& x = b.shp[s];
        x.type = (int) P (fType); x.mode = (int) P (fMode);
        for (int k = 0; k < 3; ++k) x.k[k] = P (fA + k);
        x.mix = P (fMix);
    }
    for (int s = 0; s < numMovers; ++s)
    {
        auto P = [&] (int i) { return ids->mov[(size_t) (s * numMoverParams + i)]->load(); };
        auto& x = b.mov[s];
        x.type = (int) P (mType); x.mode = (int) P (mMode);
        for (int k = 0; k < numKnobs; ++k) x.k[k] = P (mA + k);
        for (int k = 0; k < numSteps; ++k) x.step[k] = P (mS1 + k);
        for (int k = 0; k < numTargets; ++k) { x.dst[k] = juce::jlimit (0, numDests - 1, (int) P (mT1 + k)); x.amt[k] = P (mQ1 + k); }
    }
    for (int k = 0; k < 4; ++k) b.amp[k] = ids->global[(size_t) (gAmpA + k)]->load();
    b.voiceMode = (int) ids->global[gVoiceMode]->load();
    b.glide = ids->global[gGlide]->load();
    b.gain = juce::Decibels::decibelsToGain (ids->global[gMaster]->load());
}

void MateriaAudioProcessor::prepareToPlay (double sampleRate, int)
{
    sr = sampleRate;
    for (auto& f : space) f.prepare (sr);
    reset();
}

void MateriaAudioProcessor::reset()
{
    for (auto& v : voices) v = {};
    held.clear();
    for (auto& f : space) f.reset();
    for (auto& m : movers) m = {};
    std::fill (stringLines.begin(), stringLines.end(), 0.0f);
    std::fill (combLines.begin(), combLines.end(), 0.0f);
}

void MateriaAudioProcessor::noteOn (int note, float velocity, const Block& b)
{
    using namespace materia;
    const bool anyHeld = ! held.empty();
    held.push_back (note);

    auto start = [&] (Voice& v, bool retrigger)
    {
        const bool wasActive = v.active;
        v.note = note;
        v.velocity = velocity;
        v.targetPitch = note;
        if (! wasActive || b.glide <= 0.0005f) v.pitch = note;
        if (retrigger || ! wasActive)
        {
            v.active = true;
            for (auto& r : v.random) r = rng.nextFloat() * 2.0f - 1.0f;
            if (! wasActive)
            {
                for (auto& s : v.src) { for (auto& ph : s.phase) ph = rng.nextDouble(); s = Voice::Src { s.phase }; }
                for (auto& s : v.shp) { s = {}; s.count = -1; }   // -1: le linee del risonatore vanno svuotate
                v.amp.value = 0.0f;
                for (auto& e : v.env) e.value = 0.0f;
            }
            for (auto& s : v.src) s.primed = false;   // la corda si ripizzica
            v.amp.start();
            for (auto& e : v.env) e.start();
            v.age = ++ageCounter;
        }
    };

    if (b.voiceMode != 0)
    {
        auto& v = voices[0];
        start (v, b.voiceMode == 1 || ! anyHeld || v.amp.stage == prisma::Env::release || ! v.active);
        return;
    }
    Voice* pick = nullptr;
    for (auto& v : voices) if (! v.active) { pick = &v; break; }
    if (pick == nullptr)
    {
        pick = &voices[0];
        for (auto& v : voices) if (v.age < pick->age) pick = &v;
        pick->active = false;
    }
    start (*pick, true);
}

void MateriaAudioProcessor::noteOff (int note, const Block& b)
{
    held.erase (std::remove (held.begin(), held.end(), note), held.end());
    if (b.voiceMode != 0)
    {
        auto& v = voices[0];
        if (! held.empty())
        {
            v.note = held.back();
            v.targetPitch = held.back();
            if (b.voiceMode == 1) { v.amp.start(); for (auto& e : v.env) e.start(); for (auto& s : v.src) s.primed = false; }
        }
        else { v.amp.stop(); for (auto& e : v.env) e.stop(); }
        return;
    }
    for (auto& v : voices)
        if (v.active && v.note == note && v.amp.stage != prisma::Env::release) { v.amp.stop(); for (auto& e : v.env) e.stop(); }
}

//==============================================================================
namespace
{
    inline double mtof (double m) noexcept     { return 440.0 * std::pow (2.0, (m - 69.0) / 12.0); }

    inline float lfoShape (int shape, double ph, float hold) noexcept
    {
        switch (shape)
        {
            case 0:  return (float) std::sin (ph * juce::MathConstants<double>::twoPi);
            case 1:  return (float) (ph < 0.25 ? 4.0 * ph : ph < 0.75 ? 2.0 - 4.0 * ph : 4.0 * ph - 4.0);
            case 2:  return (float) (2.0 * ph - 1.0);
            case 3:  return (float) (1.0 - 2.0 * ph);
            case 4:  return ph < 0.5 ? 1.0f : -1.0f;
            default: return hold;
        }
    }

    /** Numero "a caso" ma sempre uguale per lo stesso indice (sequenza casuale che si ripete uguale nel brano). */
    inline int hashStep (juce::int64 i, int n) noexcept
    {
        auto x = (juce::uint64) i * 0x9E3779B97F4A7C15ull;
        x ^= x >> 29; x *= 0xBF58476D1CE4E5B9ull; x ^= x >> 32;
        return (int) (x % (juce::uint64) n);
    }

    /** Filtro a stato variabile (TPT): ritorna il passa-basso, lascia hp e bp. */
    struct Svf
    {
        float g = 0, kq = 2, a1 = 0, a2 = 0, a3 = 0;
        void set (double hz, float res, double sr) noexcept
        {
            g = (float) std::tan (juce::MathConstants<double>::pi * juce::jlimit (15.0, sr * 0.45, hz) / sr);
            kq = 2.0f - 1.96f * juce::jlimit (0.0f, 1.0f, res);
            a1 = 1.0f / (1.0f + g * (g + kq)); a2 = g * a1; a3 = g * a2;
        }
        inline float run (float x, float& ic1, float& ic2, float& hp, float& bp) const noexcept
        {
            const float v3 = x - ic2;
            const float v1 = a1 * ic1 + a2 * v3;
            const float v2 = ic2 + a2 * ic1 + a3 * v3;
            ic1 = 2.0f * v1 - ic1;
            ic2 = 2.0f * v2 - ic2;
            bp = v1;
            hp = x - kq * v1 - v2;
            return v2;
        }
    };

    inline float shape (int mode, float x) noexcept
    {
        switch (mode)
        {
            case 1:  return juce::jlimit (-1.0f, 1.0f, x);                                         // dura
            case 2:  return x > 0.0f ? std::tanh (x) : std::tanh (x * 0.55f) * 1.4f;               // valvola (asimmetrica)
            case 3:  return std::tanh (x - 0.35f * std::abs (x));                                  // diodo
            case 4:  return std::sin (juce::jlimit (-4.0f, 4.0f, x) * juce::MathConstants<float>::halfPi);   // seno
            default: return std::tanh (x);
        }
    }

    inline float fold (float x, float soft) noexcept
    {
        // piega a triangolo (dura) e a seno (morbida), mescolate
        const float t = x * 0.25f + 0.25f;
        const float tri = 4.0f * std::abs (t - std::floor (t + 0.5f)) - 1.0f;
        const float sn = std::sin (x * juce::MathConstants<float>::halfPi);
        return tri + (sn - tri) * soft;
    }
}

void MateriaAudioProcessor::render (float* L, float* R, int n, const Block& b, double bpm, double ppq)
{
    using namespace materia;
    if (n <= 0) return;
    const auto& bank = prisma::Bank::get();
    constexpr int chunk = 16;
    const int mask = lineSize - 1;
    const float voiceGain = b.voiceMode != 0 ? 0.7f : 0.35f;
    const float glideCoef = b.glide <= 0.0005f ? 1.0f : 1.0f - std::exp (-1.0f / (b.glide * 0.35f * (float) sr));
    juce::uint32 newest = 0;
    float shown[numMovers];
    for (int m = 0; m < numMovers; ++m) shown[m] = movers[(size_t) m].value;
    float globalMod[numDests] {};

    for (int start = 0; start < n; start += chunk)
    {
        const int len = juce::jmin (chunk, n - start);
        const double chunkSec = len / sr;
        const double songPos = ppq >= 0.0 ? ppq + bpm / 60.0 * start / sr : -1.0;

        // --- movimenti uguali per tutte le note: LFO, sequenza, rotella
        for (int m = 0; m < numMovers; ++m)
        {
            const auto& mv = b.mov[m];
            auto& st = movers[(size_t) m];
            if (mv.type == mvLfo)
            {
                const int sync = lfoSyncIndex (mv.k[1]);
                double ph;
                if (sync > 0)
                {
                    const double beats = lfoBeats[sync];
                    if (songPos >= 0.0) ph = songPos / beats;
                    else { st.phase += chunkSec * bpm / 60.0 / beats; ph = st.phase; }
                }
                else { st.phase += chunkSec * lfoHz (mv.k[0]); ph = st.phase; }
                ph += mv.k[2];
                const juce::int64 turn = (juce::int64) std::floor (ph);
                if ((int) (turn & 0x7fffffff) != st.step) { st.step = (int) (turn & 0x7fffffff); st.hold = (float) hashStep (turn + m * 977, 2001) / 1000.0f - 1.0f; }
                ph -= std::floor (ph);
                if (st.phase > 1.0e6) st.phase -= 1.0e6;
                st.value = lfoShape (mv.mode, ph, st.hold);
                shownPhase[(size_t) m].store ((float) ph, std::memory_order_relaxed);
            }
            else if (mv.type == mvSeq)
            {
                const double beats = seqBeats[seqRateIndex (mv.k[0])];
                double pos;
                if (songPos >= 0.0) pos = songPos / beats;
                else { st.phase += chunkSec * bpm / 60.0 / beats; pos = st.phase; }
                const int count = seqLength (mv.k[2]);
                const juce::int64 idx = (juce::int64) std::floor (pos);
                int step;
                switch (mv.mode)
                {
                    case 1:  step = count - 1 - (int) (idx % count); break;
                    case 2:  { const int period = juce::jmax (1, 2 * count - 2); const int q = (int) (idx % period); step = q < count ? q : period - q; break; }
                    case 3:  step = hashStep (idx + m * 131, count); break;
                    default: step = (int) (idx % count); break;
                }
                const float target = mv.step[juce::jlimit (0, numSteps - 1, step)];
                // LISCIO: quanto tempo ci mette ad arrivare al passo nuovo (fino a tutto il passo)
                const double stepSec = beats * 60.0 / juce::jmax (40.0, bpm);
                const float k = mv.k[1] <= 0.01f ? 1.0f : (float) (1.0 - std::exp (-chunkSec / (mv.k[1] * stepSec * 0.35)));
                st.smooth += (target - st.smooth) * k;
                st.value = st.smooth;
                shownPhase[(size_t) m].store ((float) step, std::memory_order_relaxed);
            }
            else if (mv.type == mvWheel) st.value = modWheel;
            else if (mv.type == mvEmpty) st.value = 0.0f;
        }
        // quanto muovono i blocchi "spazio" (solo i movimenti uguali per tutte le note)
        std::fill (std::begin (globalMod), std::end (globalMod), 0.0f);
        for (int m = 0; m < numMovers; ++m)
            if (b.mov[m].type == mvLfo || b.mov[m].type == mvSeq || b.mov[m].type == mvWheel)
                for (int k = 0; k < numTargets; ++k)
                    globalMod[b.mov[m].dst[k]] += movers[(size_t) m].value * b.mov[m].amt[k];

        for (int vi = 0; vi < maxVoices; ++vi)
        {
            auto& v = voices[(size_t) vi];
            if (! v.active) continue;

            // --- quanto vale ogni movimento per questa nota, e cosa muove
            float mod[numDests] {};
            float mval[numMovers];
            for (int m = 0; m < numMovers; ++m)
            {
                const auto& mv = b.mov[m];
                float val = 0.0f;
                switch (mv.type)
                {
                    case mvEnv:      val = v.env[(size_t) m].next ((float) envSeconds (mv.k[0]), (float) envSeconds (mv.k[1]) + 0.004f, mv.k[2],
                                                                      (float) envSeconds (mv.k[3]) + 0.004f, sr / len); break;
                    case mvLfo: case mvSeq: case mvWheel: val = movers[(size_t) m].value; break;
                    case mvRandom:   val = v.random[(size_t) m]; break;
                    case mvNote:     val = juce::jlimit (-1.0f, 1.0f, (float) ((v.pitch - 60.0) / 24.0)); break;
                    case mvVelocity: val = v.velocity; break;
                    default: break;
                }
                mval[m] = val;
                if (mv.type != mvEmpty)
                    for (int k = 0; k < numTargets; ++k)
                        mod[mv.dst[k]] += val * mv.amt[k];
            }
            mod[0] = 0.0f;
            if (v.age >= newest) { newest = v.age; for (int m = 0; m < numMovers; ++m) shown[m] = mval[m]; }

            v.pitch += (v.targetPitch - v.pitch) * (1.0 - std::pow (1.0 - (double) glideCoef, (double) len));

            float bufL[chunk] {}, bufR[chunk] {}, monoPrev[chunk] {}, monoCur[chunk];

            // ================= SORGENTI
            for (int k = 0; k < numSources; ++k)
            {
                const auto& s = b.src[k];
                auto& vs = v.src[(size_t) k];
                if (s.type == srcEmpty) continue;
                const int d0 = sourceDest (k, 0);
                auto K = [&] (int i) { return juce::jlimit (0.0f, 1.0f, s.k[i] + mod[d0 + i]); };
                const float level = juce::jlimit (0.0f, 1.5f, s.level * (1.0f + mod[d0 + sdLevel]));
                const float pan = juce::jlimit (-1.0f, 1.0f, s.pan + mod[d0 + sdPan]);
                const double midi = v.pitch + s.oct * 12 + s.semi + s.fine / 100.0 + pitchBend * 2.0 + mod[d0 + sdPitch] * 24.0;
                const double f = juce::jlimit (8.0, sr * 0.45, mtof (midi));
                std::fill (monoCur, monoCur + len, 0.0f);

                if (s.type == srcOsc || s.type == srcWave)
                {
                    const bool osc = s.type == srcOsc;
                    const int table = osc ? 0 : s.sel;
                    const float pos = K (0), warp = K (1), bend = K (2), fm = K (3);
                    const int warpMode = osc ? 1 : juce::jlimit (0, 5, s.mode) + 1;
                    const int uni = s.uni;
                    const float detune = juce::jlimit (0.0f, 1.0f, s.detune + mod[d0 + sdDetune]);
                    const float gain = level / std::sqrt ((float) uni);
                    double inc[maxUnison]; float gl[maxUnison], gr[maxUnison];
                    double maxF = f;
                    for (int u = 0; u < uni; ++u)
                    {
                        const float spread = uni == 1 ? 0.0f : (float) u / (float) (uni - 1) * 2.0f - 1.0f;
                        const double fu = f * std::pow (2.0, spread * detune * 0.5 / 12.0);
                        inc[u] = fu / sr;
                        maxF = juce::jmax (maxF, fu);
                        const float panU = juce::jlimit (-1.0f, 1.0f, spread * 0.8f + pan);
                        gl[u] = std::cos ((panU + 1.0f) * juce::MathConstants<float>::pi * 0.25f) * gain;
                        gr[u] = std::sin ((panU + 1.0f) * juce::MathConstants<float>::pi * 0.25f) * gain;
                    }
                    const double warpMul = (warpMode == 1 ? 1.0 + warp * 7.0 : 1.0 + warp * 2.0) * (1.0 + bend * 1.5) * (1.0 + fm * 3.0);
                    const int lev = prisma::Bank::levelFor (maxF * warpMul, sr);
                    const bool plain = warp <= 0.0005f && bend <= 0.0005f && fm <= 0.0005f;
                    for (int i = 0; i < len; ++i)
                    {
                        float l = 0.0f, r = 0.0f, first = 0.0f;
                        const double fmOff = fm > 0.0005f ? (double) fm * 0.75 * monoPrev[i] : 0.0;
                        for (int u = 0; u < uni; ++u)
                        {
                            double& ph = vs.phase[(size_t) u];
                            double rp = ph;
                            if (! plain)
                            {
                                if (warp > 0.0005f) rp = prisma::warpPhase (warpMode, warp, rp);
                                if (bend > 0.0005f) rp = prisma::warpPhase (2, bend, rp);
                                rp += fmOff;
                                rp -= std::floor (rp);
                            }
                            const float x = bank.read (table, pos, rp, lev);
                            l += x * gl[u];
                            r += x * gr[u];
                            if (u == 0) first = x;
                            ph += inc[u];
                            if (ph >= 1.0) ph -= 1.0;
                        }
                        bufL[i] += l; bufR[i] += r;
                        monoCur[i] = first;
                    }
                }
                else if (s.type == srcNoise)
                {
                    const float colour = K (0), tone = K (1), grain = K (2), width = K (3);
                    const float kc = colour < 0.5f ? 0.002f * std::pow (500.0f, colour * 2.0f) : 0.002f * std::pow (400.0f, (colour - 0.5f) * 2.0f);
                    const bool high = colour > 0.5f;
                    const float comp = high ? 1.0f : juce::jmin (5.0f, 1.0f / std::sqrt (juce::jmax (0.02f, kc)));
                    const int period = 1 + (int) (grain * grain * 220.0f);
                    Svf bp; bp.set (f, 0.985f, sr);
                    const float gl = std::cos ((pan + 1.0f) * juce::MathConstants<float>::pi * 0.25f) * level * 0.6f;
                    const float gr = std::sin ((pan + 1.0f) * juce::MathConstants<float>::pi * 0.25f) * level * 0.6f;
                    for (int i = 0; i < len; ++i)
                    {
                        if (--vs.holdCount <= 0) { vs.hold = rng.nextFloat() * 2.0f - 1.0f; vs.holdCount = period; }
                        const float wn = vs.hold;
                        vs.lp += kc * (wn - vs.lp);
                        float y = (high ? wn - vs.lp : vs.lp) * comp;
                        // seconda voce di rumore per la larghezza stereo
                        const float wn2 = rng.nextFloat() * 2.0f - 1.0f;
                        vs.lp2 += kc * (wn2 - vs.lp2);
                        const float y2 = (high ? wn2 - vs.lp2 : vs.lp2) * comp;
                        float left = y, right = y + (y2 - y) * width;
                        if (tone > 0.001f)
                        {
                            float hp, band;
                            bp.run ((left + right) * 0.5f, vs.bp1, vs.bp2, hp, band);
                            const float pitched = band * bp.kq * 9.0f;
                            left += (pitched - left) * tone; right += (pitched - right) * tone;
                        }
                        bufL[i] += left * gl; bufR[i] += right * gr;
                        monoCur[i] = left;
                    }
                }
                else if (s.type == srcString)
                {
                    float* line = stringLines.data() + ((size_t) vi * numSources + (size_t) k) * lineSize;
                    static constexpr float materialDamp[] { 0.45f, 0.85f, 0.2f, 0.97f };
                    static constexpr float materialLife[] { 0.6f, 1.0f, 0.25f, 1.6f };
                    const int mat = juce::jlimit (0, 3, s.mode);
                    const float hard = K (0), life = K (1), point = K (2), bow = K (3);
                    const float kd = juce::jlimit (0.05f, 0.995f, materialDamp[mat] * (0.75f + 0.25f * hard));
                    const double period = juce::jlimit (4.0, (double) lineSize - 4.0, sr / f - (1.0 - kd) / kd);
                    const double t60 = 0.1 * std::pow (100.0, (double) life) * materialLife[mat];
                    const float fb = (float) std::pow (0.001, 1.0 / (f * t60));
                    const float kh = 0.03f + 0.97f * hard * hard;
                    const int pick = (int) (period * (0.02 + 0.48 * point));
                    if (! vs.primed)
                    {
                        std::fill (line, line + lineSize, 0.0f);
                        vs.write = 0; vs.damp = 0.0f; vs.exLp = 0.0f;
                        vs.excite = (int) period;
                        vs.primed = true;
                    }
                    const float gl = std::cos ((pan + 1.0f) * juce::MathConstants<float>::pi * 0.25f) * level * 0.8f;
                    const float gr = std::sin ((pan + 1.0f) * juce::MathConstants<float>::pi * 0.25f) * level * 0.8f;
                    const int whole = (int) period; const float frac = (float) (period - whole);
                    const float velGain = 0.4f + 0.6f * v.velocity;
                    for (int i = 0; i < len; ++i)
                    {
                        const int r0 = (vs.write - whole) & mask, r1 = (vs.write - whole - 1) & mask;
                        const float y = line[r0] + (line[r1] - line[r0]) * frac;
                        vs.damp += kd * (y - vs.damp);
                        float in = vs.damp * fb;
                        if (vs.excite > 0 || bow > 0.001f)
                        {
                            const float wn = rng.nextFloat() * 2.0f - 1.0f;
                            vs.exLp += kh * (wn - vs.exLp);
                            if (vs.excite > 0) { in += vs.exLp * velGain; --vs.excite; }
                            in += vs.exLp * bow * 0.06f;
                        }
                        line[vs.write] = in;
                        // punto dove si pizzica: toglie gli armonici che hanno un nodo li'
                        const float out = y - line[(vs.write - pick) & mask] * 0.85f * (point > 0.01f ? 1.0f : 0.0f);
                        vs.write = (vs.write + 1) & mask;
                        bufL[i] += out * gl; bufR[i] += out * gr;
                        monoCur[i] = out;
                    }
                }
                vs.last = monoCur[len - 1];
                std::copy (monoCur, monoCur + len, monoPrev);
            }

            // ================= FORMA (i blocchi "per nota", in ordine)
            for (int k = 0; k < numShapers; ++k)
            {
                const auto& s = b.shp[k];
                if (s.type == shEmpty || isSpace (s.type)) continue;
                auto& st = v.shp[(size_t) k];
                const int d0 = shaperDest (k, 0);
                auto K = [&] (int i) { return juce::jlimit (0.0f, 1.0f, s.k[i] + mod[d0 + i]); };
                const float mix = juce::jlimit (0.0f, 1.0f, s.mix + mod[d0 + 3]);
                float* io[2] { bufL, bufR };

                switch (s.type)
                {
                    case shFilter:
                    {
                        Svf f; f.set (cutoffHz (K (0)), K (1), sr);
                        const float drive = K (2), dg = 1.0f + drive * 9.0f, dn = 1.0f / (1.0f + drive * 2.5f);
                        for (int c = 0; c < 2; ++c)
                            for (int i = 0; i < len; ++i)
                            {
                                const float x = io[c][i];
                                const float in = drive > 0.001f ? std::tanh (x * dg) * dn : x;
                                float hp, bp, y;
                                const float lp = f.run (in, st.s[c][0], st.s[c][1], hp, bp);
                                switch (s.mode)
                                {
                                    case 1:  { float h2, b2; y = f.run (lp, st.s[c][2], st.s[c][3], h2, b2); break; }
                                    case 2:  y = hp; break;
                                    case 3:  y = bp * f.kq; break;
                                    case 4:  y = lp + hp; break;
                                    default: y = lp; break;
                                }
                                io[c][i] = x + (y - x) * mix;
                            }
                        break;
                    }
                    case shDrive:
                    {
                        const float a = K (0), pg = 1.0f + a * a * 40.0f, tone = 0.04f + 0.96f * K (1) * K (1), bias = K (2) * 0.6f;
                        const float comp = 1.0f / (1.0f + a * 1.5f), dc = shape (s.mode, bias);
                        for (int c = 0; c < 2; ++c)
                            for (int i = 0; i < len; ++i)
                            {
                                const float x = io[c][i];
                                float y = (shape (s.mode, x * pg + bias) - dc) * comp;
                                st.s[c][0] += tone * (y - st.s[c][0]);
                                y = st.s[c][0];
                                io[c][i] = x + (y - x) * mix;
                            }
                        break;
                    }
                    case shFold:
                    {
                        const float g = 1.0f + K (0) * 10.0f, offs = K (1) * 0.8f, soft = K (2);
                        const float dc = fold (offs, soft);
                        for (int c = 0; c < 2; ++c)
                            for (int i = 0; i < len; ++i)
                            {
                                const float x = io[c][i];
                                const float y = (fold (x * g + offs, soft) - dc) * 0.8f;
                                io[c][i] = x + (y - x) * mix;
                            }
                        break;
                    }
                    case shResonator:
                    {
                        float* lines = combLines.data() + ((size_t) vi * numShapers + (size_t) k) * 2 * lineSize;
                        if (st.count < 0) { std::fill (lines, lines + 2 * lineSize, 0.0f); st.count = 0; st.write = 0; }
                        const bool tube = s.mode == 1;
                        const float kd = juce::jlimit (0.03f, 0.99f, 1.0f - K (2) * 0.95f);
                        const double f = juce::jlimit (12.0, sr * 0.4, mtof (v.pitch + pitchBend * 2.0 + resonatorSemis (s.k[0]) + mod[d0] * 24.0));
                        // il tubo (chiuso da una parte) inverte il segno: suona un'ottava sotto, quindi si accorcia della meta'
                        const double period = juce::jlimit (3.0, (double) lineSize - 4.0, sr / f * (tube ? 0.5 : 1.0) - (1.0 - kd) / kd);
                        const double t60 = 0.05 * std::pow (200.0, (double) K (1));
                        const float fb = (float) std::pow (0.001, 1.0 / (f * t60)) * (tube ? -1.0f : 1.0f);
                        const int whole = (int) period; const float frac = (float) (period - whole);
                        for (int i = 0; i < len; ++i)
                        {
                            for (int c = 0; c < 2; ++c)
                            {
                                float* line = lines + c * lineSize;
                                const int r0 = (st.write - whole) & mask, r1 = (st.write - whole - 1) & mask;
                                const float d = line[r0] + (line[r1] - line[r0]) * frac;
                                st.s[c][0] += kd * (d - st.s[c][0]);
                                const float x = io[c][i];
                                const float y = x + st.s[c][0] * fb;
                                line[st.write] = y;
                                io[c][i] = x + (y * 0.6f - x) * mix;
                            }
                            st.write = (st.write + 1) & mask;
                        }
                        break;
                    }
                    case shCrush:
                    {
                        const float levels = std::pow (2.0f, 16.0f - K (0) * 14.0f) * 0.5f;
                        const int period = 1 + (int) (K (1) * K (1) * 60.0f);
                        for (int i = 0; i < len; ++i)
                        {
                            if (--st.count <= 0)
                            {
                                st.count = period;
                                for (int c = 0; c < 2; ++c) st.s[c][0] = std::round (io[c][i] * levels) / levels;
                            }
                            for (int c = 0; c < 2; ++c) io[c][i] += (st.s[c][0] - io[c][i]) * mix;
                        }
                        break;
                    }
                    case shVowel:
                    {
                        static constexpr float F1[] { 800.0f, 500.0f, 300.0f, 500.0f, 350.0f }, F2[] { 1200.0f, 1900.0f, 2300.0f, 900.0f, 700.0f };
                        const float p = K (0) * 4.0f;
                        const int i0 = juce::jmin (3, (int) p);
                        const float fr = p - (float) i0;
                        const double shift = std::pow (2.0, ((double) K (2) - 0.5) * 2.0);
                        const float res = 0.8f + 0.18f * K (1);
                        Svf a, c2;
                        a.set ((F1[i0] + (F1[i0 + 1] - F1[i0]) * fr) * shift, res, sr);
                        c2.set ((F2[i0] + (F2[i0 + 1] - F2[i0]) * fr) * shift, res, sr);
                        for (int c = 0; c < 2; ++c)
                            for (int i = 0; i < len; ++i)
                            {
                                const float x = io[c][i];
                                float hp, b1, b2;
                                a.run (x, st.s[c][0], st.s[c][1], hp, b1);
                                c2.run (x, st.s[c][2], st.s[c][3], hp, b2);
                                const float y = (b1 * a.kq + b2 * c2.kq * 0.7f) * 2.2f;
                                io[c][i] = x + (y - x) * mix;
                            }
                        break;
                    }
                    default: break;
                }
            }

            // ================= VOLUME
            const float velGain = 0.45f + 0.55f * v.velocity;
            for (int i = 0; i < len; ++i)
            {
                const float g = v.amp.next (b.amp[0], b.amp[1], b.amp[2], b.amp[3], sr) * velGain * voiceGain;
                L[start + i] += bufL[i] * g;
                R[start + i] += bufR[i] * g;
                if (v.amp.stage == prisma::Env::idle) { v.active = false; break; }
            }
        }
    }
    for (int m = 0; m < numMovers; ++m) shownMover[(size_t) m].store (shown[m], std::memory_order_relaxed);

    // ================= SPAZIO (chorus, ritardo, riverbero: sul suono intero, in ordine)
    for (int k = 0; k < numShapers; ++k)
    {
        const auto& s = b.shp[k];
        if (! isSpace (s.type)) continue;
        const int d0 = shaperDest (k, 0);
        const float mix = juce::jlimit (0.0f, 1.0f, s.mix + globalMod[d0 + 3]);
        const float a = juce::jlimit (0.0f, 1.0f, s.k[0] + globalMod[d0]);
        forge::FxParams p;
        if (s.type == shChorus) p.chorus = mix;
        else if (s.type == shDelay) { p.delayMix = mix; p.delayFeedback = a * 0.9f; p.delayBeats = delayBeats[juce::jlimit (0, 5, s.mode)]; }
        else { p.reverbMix = mix; p.reverbSize = a; }
        space[(size_t) k].process (L, R, n, p, bpm);
    }

    for (int i = 0; i < n; ++i)
    {
        auto soft = [] (float x) { const float a = std::abs (x); return a < 0.85f ? x : std::copysign (0.85f + 0.15f * std::tanh ((a - 0.85f) * 6.0f), x); };
        L[i] = soft (L[i] * b.gain);
        if (R != L) R[i] = soft (R[i] * b.gain);
    }
}

void MateriaAudioProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midi)
{
    juce::ScopedNoDenormals nd;
    buffer.clear();
    double bpm = 128.0, ppq = -1.0;
    if (auto* ph = getPlayHead())
        if (auto pos = ph->getPosition())
        {
            if (auto bp = pos->getBpm()) bpm = *bp;
            if (pos->getIsPlaying()) if (auto q = pos->getPpqPosition()) ppq = *q;
        }
    Block b;
    readBlock (b);
    auto* L = buffer.getWritePointer (0);
    auto* R = buffer.getNumChannels() > 1 ? buffer.getWritePointer (1) : L;
    int done = 0;
    for (const auto meta : midi)
    {
        const int at = juce::jlimit (0, buffer.getNumSamples(), meta.samplePosition);
        render (L + done, R + done, at - done, b, bpm, ppq >= 0.0 ? ppq + bpm / 60.0 * done / sr : -1.0);
        done = at;
        const auto m = meta.getMessage();
        if (m.isNoteOn()) noteOn (m.getNoteNumber(), m.getFloatVelocity(), b);
        else if (m.isNoteOff()) noteOff (m.getNoteNumber(), b);
        else if (m.isPitchWheel()) pitchBend = (float) (m.getPitchWheelValue() - 8192) / 8192.0f;
        else if (m.isControllerOfType (1)) modWheel = (float) m.getControllerValue() / 127.0f;
        else if (m.isAllNotesOff() || m.isAllSoundOff())
        {
            held.clear();
            for (auto& v : voices) { v.amp.stop(); for (auto& e : v.env) e.stop(); }
        }
    }
    render (L + done, R + done, buffer.getNumSamples() - done, b, bpm, ppq >= 0.0 ? ppq + bpm / 60.0 * done / sr : -1.0);
}

juce::AudioProcessorEditor* MateriaAudioProcessor::createEditor()   { return new MateriaEditor (*this); }

void MateriaAudioProcessor::getStateInformation (juce::MemoryBlock& d)
{
    if (auto xml = apvts.copyState().createXml()) copyXmlToBinary (*xml, d);
}

void MateriaAudioProcessor::setStateInformation (const void* data, int size)
{
    if (auto xml = getXmlFromBinary (data, size))
        if (xml->hasTagName (apvts.state.getType()))
        {
            apvts.replaceState (juce::ValueTree::fromXml (*xml));
            ensureTables();
        }
}
