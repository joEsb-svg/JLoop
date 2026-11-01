#pragma once

#include <JuceHeader.h>
#include "Performance.h"
#include <array>
#include <atomic>
#include <cmath>
#include <complex>
#include <memory>
#include <vector>

//==============================================================================
/**
    Il mixer di JLoop (come in FL Studio): una traccia Master e 32 tracce "insert".

    Ogni canale del channel rack manda il suono a una traccia. Ogni traccia ha:
    - volume, panorama, muto, solo e il suo misuratore
    - un equalizzatore a 3 bande (bassi, medi con la frequenza, alti)
    - fino a 6 effetti in fila (compressore, saturazione, filtro, delay, riverbero, chorus,
      bitcrusher, sidechain, ampiezza stereo)

    Il sidechain abbassa il volume della traccia a ogni colpo di un'altra traccia (es. la cassa):
    l'effetto "pompa" della musica elettronica.

    Gli effetti sono oggetti creati dal thread della UI (e mai nel callback audio): i loro parametri
    vivono nello slot della traccia (atomic), cosi' l'interfaccia li puo' cambiare in ogni momento.
*/
namespace mixer
{
    inline constexpr int numInserts = 32;
    inline constexpr int numTracks  = numInserts + 1;   // 0 = Master
    inline constexpr int numSlots   = 6;
    inline constexpr int maxParams  = 12;

    //==========================================================================
    // Tipi di effetto e i loro parametri

    enum FxType { none = 0, compressor, saturation, filter, delay, reverb, chorus, bitcrusher, sidechain, stereo, ott, glitch, trash, vowel,
                  gate, tremolo, phaser, flanger, peq, transient, exciter, vinyl, limiter, pitch, ringmod, shimmer, numFxTypes };

    enum class Unit { percent, dB, ms, hz, ratio, choice, track, bits, times, semis };

    struct ParamDef
    {
        const char* name;
        float minValue, maxValue, defaultValue;
        Unit unit;
        float centre = 0.0f;                 // per le scale "a curva" (frequenze, tempi): il valore a meta' corsa
        const char* choices = nullptr;       // per Unit::choice: voci separate da '|'
    };

    struct FxInfo
    {
        const char* id;                      // nome salvato nei progetti (non cambiarlo)
        const char* name;
        const char* detail;
        int numParams;
        std::array<ParamDef, maxParams> params;
    };

    inline const FxInfo& info (int type)
    {
        static const std::array<FxInfo, numFxTypes> infos =
        {{
            { "none", "Vuoto", "", 0, {} },
            { "comp", "Compressore", "Controlla la dinamica: suoni piu' compatti e presenti", 5, {{
                { "Soglia", -40.0f, 0.0f, -18.0f, Unit::dB },
                { "Rapporto", 1.0f, 20.0f, 4.0f, Unit::ratio, 4.0f },
                { "Attacco", 0.1f, 100.0f, 5.0f, Unit::ms, 10.0f },
                { "Rilascio", 10.0f, 1000.0f, 120.0f, Unit::ms, 150.0f },
                { "Guadagno", 0.0f, 24.0f, 4.0f, Unit::dB } }} },
            { "drive", "Saturazione", "Calore e distorsione", 4, {{
                { "Drive", 0.0f, 1.0f, 0.4f, Unit::percent },
                { "Tono", 500.0f, 20000.0f, 12000.0f, Unit::hz, 4000.0f },
                { "Mix", 0.0f, 1.0f, 1.0f, Unit::percent },
                { "Uscita", -12.0f, 6.0f, 0.0f, Unit::dB } }} },
            { "filter", "Filtro", "Passa-basso, passa-alto o passa-banda, con LFO", 5, {{
                { "Tipo", 0.0f, 2.0f, 0.0f, Unit::choice, 0.0f, "Passa-basso|Passa-alto|Passa-banda" },
                { "Cutoff", 20.0f, 20000.0f, 2000.0f, Unit::hz, 1000.0f },
                { "Risonanza", 0.0f, 1.0f, 0.2f, Unit::percent },
                { "LFO", 0.0f, 1.0f, 0.0f, Unit::percent },
                { "Velocita' LFO", 0.05f, 10.0f, 0.5f, Unit::hz, 1.0f } }} },
            { "delay", "Delay", "Eco a tempo", 5, {{
                { "Tempo", 0.0f, 5.0f, 2.0f, Unit::choice, 0.0f, "1/16|1/8|3/16|1/4|3/8|1/2" },
                { "Feedback", 0.0f, 0.95f, 0.4f, Unit::percent },
                { "Mix", 0.0f, 1.0f, 0.3f, Unit::percent },
                { "Ping-pong", 0.0f, 1.0f, 1.0f, Unit::choice, 0.0f, "No|Si'" },
                { "Tono", 500.0f, 20000.0f, 6000.0f, Unit::hz, 4000.0f } }} },
            { "reverb", "Riverbero", "Spazio: stanza, sala, cattedrale", 4, {{
                { "Stanza", 0.0f, 1.0f, 0.6f, Unit::percent },
                { "Smorzamento", 0.0f, 1.0f, 0.5f, Unit::percent },
                { "Ampiezza", 0.0f, 1.0f, 1.0f, Unit::percent },
                { "Mix", 0.0f, 1.0f, 0.3f, Unit::percent } }} },
            { "chorus", "Chorus", "Suono piu' largo e mosso", 3, {{
                { "Velocita'", 0.05f, 5.0f, 0.8f, Unit::hz, 1.0f },
                { "Profondita'", 0.0f, 1.0f, 0.5f, Unit::percent },
                { "Mix", 0.0f, 1.0f, 0.5f, Unit::percent } }} },
            { "crush", "Bitcrusher", "Suono sporco, a 8 bit, lo-fi", 3, {{
                { "Bit", 2.0f, 16.0f, 8.0f, Unit::bits },
                { "Riduzione", 1.0f, 32.0f, 4.0f, Unit::times, 4.0f },
                { "Mix", 0.0f, 1.0f, 1.0f, Unit::percent } }} },
            { "sidechain", "Sidechain", "Abbassa il volume a ogni colpo di un'altra traccia (la cassa)", 4, {{
                { "Sorgente", 1.0f, (float) numInserts, 1.0f, Unit::track },
                { "Quantita'", 0.0f, 1.0f, 0.7f, Unit::percent },
                { "Rilascio", 20.0f, 800.0f, 250.0f, Unit::ms, 200.0f },
                { "Soglia", -48.0f, 0.0f, -30.0f, Unit::dB } }} },
            { "stereo", "Stereo", "Allarga o stringe l'immagine stereo", 1, {{
                { "Ampiezza", 0.0f, 2.0f, 1.0f, Unit::percent } }} },
            { "ott", "OTT", "Compressore multibanda aggressivo: bassi e lead grossi e in faccia", 5, {{
                { "Profondita'", 0.0f, 1.0f, 0.6f, Unit::percent },
                { "Tempo", 0.0f, 1.0f, 0.5f, Unit::percent },
                { "Su (upward)", 0.0f, 1.0f, 1.0f, Unit::percent },
                { "Giu' (downward)", 0.0f, 1.0f, 1.0f, Unit::percent },
                { "Uscita", -12.0f, 12.0f, 0.0f, Unit::dB } }} },
            { "glitch", "Glitch", "Ripetizioni, tape stop e reverse a tempo, disegnati su una griglia", 3, {{
                { "Mix", 0.0f, 1.0f, 1.0f, Unit::percent },
                { "Step 1-8", 0.0f, 16777215.0f, 0.0f, Unit::times },
                { "Step 9-16", 0.0f, 16777215.0f, 0.0f, Unit::times } }} },
            { "trash", "Distorsione trash", "Sporca e grossa (stile complextro): distorce i medi e gli alti, il basso resta pulito", 5, {{
                { "Drive", 0.0f, 1.0f, 0.5f, Unit::percent },
                { "Tono", 500.0f, 16000.0f, 6000.0f, Unit::hz, 3000.0f },
                { "Basso pulito", 0.0f, 1.0f, 0.8f, Unit::percent },
                { "Mix", 0.0f, 1.0f, 1.0f, Unit::percent },
                { "Uscita", -18.0f, 6.0f, -3.0f, Unit::dB } }} },
            { "vowel", "Filtro a vocali", "Fa \"parlare\" il suono (a-e-i-o-u), anche a tempo: yoi, wow, talking bass", 5, {{
                { "Vocale", 0.0f, 4.0f, 0.0f, Unit::percent },
                { "LFO", 0.0f, 1.0f, 0.5f, Unit::percent },
                { "Tempo LFO", 0.0f, 4.0f, 2.0f, Unit::choice, 0.0f, "1/1|1/2|1/4|1/8|1/16" },
                { "Risonanza", 0.0f, 1.0f, 0.5f, Unit::percent },
                { "Mix", 0.0f, 1.0f, 1.0f, Unit::percent } }} },
            { "gate", "Trance gate", "Taglia il suono a tempo con un ritmo di 16 step (pad e accordi che pulsano)", 4, {{
                { "Ritmo", 0.0f, 65535.0f, 46811.0f, Unit::times },
                { "Velocita'", 0.0f, 2.0f, 1.0f, Unit::choice, 0.0f, "1/8|1/16|1/32" },
                { "Profondita'", 0.0f, 1.0f, 1.0f, Unit::percent },
                { "Morbidezza", 0.0f, 1.0f, 0.25f, Unit::percent } }} },
            { "tremolo", "Tremolo / Auto-pan", "Volume o posizione destra-sinistra che ondeggiano a tempo", 4, {{
                { "Modo", 0.0f, 1.0f, 0.0f, Unit::choice, 0.0f, "Tremolo|Auto-pan" },
                { "Tempo", 0.0f, 6.0f, 3.0f, Unit::choice, 0.0f, "1/1|1/2|1/4|1/8|1/16|1/4T|1/8T" },
                { "Profondita'", 0.0f, 1.0f, 0.7f, Unit::percent },
                { "Forma", 0.0f, 2.0f, 0.0f, Unit::choice, 0.0f, "Sinusoide|Triangolo|Quadra" } }} },
            { "phaser", "Phaser", "Il \"fruscio che gira\": da lento e profondo a velocissimo", 5, {{
                { "Velocita'", 0.05f, 8.0f, 0.5f, Unit::hz, 0.8f },
                { "Profondita'", 0.0f, 1.0f, 0.7f, Unit::percent },
                { "Feedback", 0.0f, 0.9f, 0.5f, Unit::percent },
                { "Stadi", 0.0f, 3.0f, 1.0f, Unit::choice, 0.0f, "4|6|8|12" },
                { "Mix", 0.0f, 1.0f, 0.5f, Unit::percent } }} },
            { "flanger", "Flanger", "Effetto jet, metallico; anche a tempo", 6, {{
                { "Velocita'", 0.02f, 5.0f, 0.25f, Unit::hz, 0.5f },
                { "Profondita'", 0.0f, 1.0f, 0.7f, Unit::percent },
                { "Feedback", -0.95f, 0.95f, 0.6f, Unit::percent },
                { "Ritardo", 0.5f, 10.0f, 2.0f, Unit::ms, 2.5f },
                { "Mix", 0.0f, 1.0f, 0.5f, Unit::percent },
                { "A tempo", 0.0f, 4.0f, 0.0f, Unit::choice, 0.0f, "Libero|2/1|1/1|1/2|1/4" } }} },
            { "peq", "EQ parametrico", "4 bande + tagli: togli il fango, alza il click, pulisci i bassi", 12, {{
                { "Taglia bassi", 20.0f, 1000.0f, 20.0f, Unit::hz, 120.0f },
                { "Bassi", -18.0f, 18.0f, 0.0f, Unit::dB },
                { "Freq. bassi", 30.0f, 500.0f, 100.0f, Unit::hz, 120.0f },
                { "Medi 1", -18.0f, 18.0f, 0.0f, Unit::dB },
                { "Freq. medi 1", 80.0f, 5000.0f, 400.0f, Unit::hz, 600.0f },
                { "Q medi 1", 0.3f, 10.0f, 1.0f, Unit::ratio, 1.5f },
                { "Medi 2", -18.0f, 18.0f, 0.0f, Unit::dB },
                { "Freq. medi 2", 300.0f, 12000.0f, 2500.0f, Unit::hz, 2500.0f },
                { "Q medi 2", 0.3f, 10.0f, 1.0f, Unit::ratio, 1.5f },
                { "Alti", -18.0f, 18.0f, 0.0f, Unit::dB },
                { "Freq. alti", 1500.0f, 16000.0f, 8000.0f, Unit::hz, 6000.0f },
                { "Taglia alti", 1000.0f, 20000.0f, 20000.0f, Unit::hz, 6000.0f } }} },
            { "transient", "Transient shaper", "Piu' attacco o piu' coda a cassa, rullante e bassi (senza compressore)", 4, {{
                { "Attacco", -1.0f, 1.0f, 0.4f, Unit::percent },
                { "Coda", -1.0f, 1.0f, 0.0f, Unit::percent },
                { "Velocita'", 0.0f, 1.0f, 0.5f, Unit::percent },
                { "Uscita", -12.0f, 12.0f, 0.0f, Unit::dB } }} },
            { "exciter", "Exciter", "Brillantezza e presenza: armoniche nuove sugli acuti", 4, {{
                { "Quantita'", 0.0f, 1.0f, 0.35f, Unit::percent },
                { "Da", 800.0f, 12000.0f, 3000.0f, Unit::hz, 3000.0f },
                { "Carattere", 0.0f, 1.0f, 0.0f, Unit::choice, 0.0f, "Morbido|Brillante" },
                { "Ampiezza", 0.0f, 1.0f, 0.5f, Unit::percent } }} },
            { "vinyl", "Vinile / nastro", "Lo-fi: fruscio, crepitio, nastro che ondeggia, toni scuri", 6, {{
                { "Fruscio", 0.0f, 1.0f, 0.2f, Unit::percent },
                { "Crepitio", 0.0f, 1.0f, 0.3f, Unit::percent },
                { "Nastro", 0.0f, 1.0f, 0.3f, Unit::percent },
                { "Tono", 1000.0f, 20000.0f, 7000.0f, Unit::hz, 5000.0f },
                { "Eta'", 0.0f, 1.0f, 0.3f, Unit::percent },
                { "Mix", 0.0f, 1.0f, 1.0f, Unit::percent } }} },
            { "limiter", "Limiter / Maximizer", "Alza il volume senza superare il tetto (sul Master, per ultimo)", 3, {{
                { "Guadagno", 0.0f, 24.0f, 3.0f, Unit::dB },
                { "Tetto", -12.0f, 0.0f, -0.3f, Unit::dB },
                { "Rilascio", 10.0f, 1000.0f, 120.0f, Unit::ms, 120.0f } }} },
            { "pitch", "Pitch shifter", "Sposta l'intonazione di semitoni, anche aggiungendo un'ottava o una quinta", 4, {{
                { "Semitoni", -24.0f, 24.0f, 12.0f, Unit::semis },
                { "Armonia", 0.0f, 3.0f, 0.0f, Unit::choice, 0.0f, "No|Ottava sotto|Ottava sopra|Quinta sopra" },
                { "Grana", 15.0f, 120.0f, 50.0f, Unit::ms, 50.0f },
                { "Mix", 0.0f, 1.0f, 1.0f, Unit::percent } }} },
            { "ringmod", "Ring mod / Freq shifter", "Suoni metallici, robotici, fuori di testa (psy, hi-tech)", 5, {{
                { "Modo", 0.0f, 1.0f, 0.0f, Unit::choice, 0.0f, "Ring mod|Frequency shifter" },
                { "Frequenza", 1.0f, 5000.0f, 300.0f, Unit::hz, 250.0f },
                { "Movimento", 0.0f, 1.0f, 0.0f, Unit::percent },
                { "Velocita' LFO", 0.05f, 10.0f, 0.5f, Unit::hz, 1.0f },
                { "Mix", 0.0f, 1.0f, 0.5f, Unit::percent } }} },
            { "shimmer", "Shimmer reverb", "Riverbero che sale di ottava: code eteree per pad e break", 4, {{
                { "Stanza", 0.0f, 1.0f, 0.8f, Unit::percent },
                { "Shimmer", 0.0f, 1.0f, 0.5f, Unit::percent },
                { "Smorzamento", 0.0f, 1.0f, 0.4f, Unit::percent },
                { "Mix", 0.0f, 1.0f, 0.35f, Unit::percent } }} },
        }};
        return infos[(size_t) juce::jlimit (0, numFxTypes - 1, type)];
    }

    inline int typeFromId (const juce::String& id)
    {
        for (int t = 0; t < numFxTypes; ++t)
            if (id == info (t).id)
                return t;
        return none;
    }

    /** Valore 0..1 di uno slider <-> valore reale del parametro (scala a curva se c'e' il centro). */
    inline float toNormalised (const ParamDef& p, float v)
    {
        juce::NormalisableRange<float> r (p.minValue, p.maxValue);
        if (p.centre > p.minValue && p.centre < p.maxValue) r.setSkewForCentre (p.centre);
        return r.convertTo0to1 (juce::jlimit (p.minValue, p.maxValue, v));
    }

    inline float fromNormalised (const ParamDef& p, float n)
    {
        juce::NormalisableRange<float> r (p.minValue, p.maxValue);
        if (p.centre > p.minValue && p.centre < p.maxValue) r.setSkewForCentre (p.centre);
        float v = r.convertFrom0to1 (juce::jlimit (0.0f, 1.0f, n));
        if (p.unit == Unit::choice || p.unit == Unit::track || p.unit == Unit::bits || p.unit == Unit::semis)
            v = std::round (v);
        return v;
    }

    inline juce::String formatValue (const ParamDef& p, float v)
    {
        switch (p.unit)
        {
            case Unit::percent: return juce::String (juce::roundToInt (v * 100.0f)) + "%";
            case Unit::dB:      return (v > 0.05f ? "+" : "") + juce::String (v, 1) + " dB";
            case Unit::ms:      return v < 1000.0f ? juce::String (v, v < 10.0f ? 1 : 0) + " ms" : juce::String (v / 1000.0f, 2) + " s";
            case Unit::hz:      return v >= 1000.0f ? juce::String (v / 1000.0f, v >= 10000.0f ? 1 : 2) + " kHz" : juce::String (v, v < 10.0f ? 2 : 0) + " Hz";
            case Unit::ratio:   return juce::String (v, v < 10.0f ? 1 : 0) + ":1";
            case Unit::times:   return juce::String (juce::roundToInt (v)) + "x";
            case Unit::semis:   return (v > 0.5f ? "+" : "") + juce::String (juce::roundToInt (v)) + " st";
            case Unit::bits:    return juce::String (juce::roundToInt (v)) + " bit";
            case Unit::track:   return "Insert " + juce::String (juce::roundToInt (v));
            case Unit::choice:
            {
                const auto list = juce::StringArray::fromTokens (p.choices != nullptr ? p.choices : "", "|", "");
                return list[juce::jlimit (0, list.size() - 1, juce::roundToInt (v))];
            }
        }
        return juce::String (v, 2);
    }

    //==========================================================================
    /** Quello che un effetto sa del resto del mixer, blocco per blocco. */
    struct Context
    {
        double sampleRate = 48000.0, bpm = 120.0;
        double ppq = 0.0;              // posizione (in quarti) all'inizio del blocco
        bool playing = false;
        const float* keyL = nullptr;   // sidechain: l'ingresso della traccia sorgente
        const float* keyR = nullptr;
    };

    /** Un effetto. prepare() e il costruttore girano sul thread della UI; process() nel callback audio. */
    class Effect
    {
    public:
        virtual ~Effect() = default;
        virtual void prepare (double sampleRate) = 0;
        virtual void reset() {}
        virtual void process (float* L, float* R, int n, const float* params, const Context& ctx) = 0;
    };

    std::unique_ptr<Effect> createEffect (int type);

    //==========================================================================
    /** Uno slot effetti di una traccia: il tipo, i parametri e l'oggetto che fa il lavoro. */
    struct Slot
    {
        std::atomic<int> type { none };
        std::atomic<bool> bypass { false };
        std::array<std::atomic<float>, maxParams> params {};
        Effect* effect = nullptr;                 // letto dal thread audio (sotto lo structureLock)
        std::unique_ptr<Effect> owned;            // posseduto qui, cambiato solo sotto lo structureLock
    };

    struct Track
    {
        std::atomic<float> volume { 1.0f };       // guadagno lineare (fader), 0..~1.6
        std::atomic<float> pan    { 0.0f };
        std::atomic<bool>  muted  { false };
        std::atomic<bool>  solo   { false };
        std::atomic<float> eqLow  { 0.0f }, eqMid { 0.0f }, eqMidFreq { 1000.0f }, eqHigh { 0.0f };   // dB, Hz
        std::atomic<float> peakL  { 0.0f }, peakR { 0.0f };   // misuratore (la UI li legge e li azzera)
        std::array<Slot, numSlots> slots;

        // Solo thread della UI
        juce::String name;                        // vuoto = nome automatico (il canale collegato)
        juce::uint32 colour = 0;                  // 0 = colore automatico
    };

    //==========================================================================
    /** Filtro biquad (RBJ), stereo. */
    struct Biquad
    {
        float b0 = 1, b1 = 0, b2 = 0, a1 = 0, a2 = 0;
        float z1[2] {}, z2[2] {};

        void setLowShelf (double sr, double f, double gainDb)   { shelf (sr, f, gainDb, true); }
        void setHighShelf (double sr, double f, double gainDb)  { shelf (sr, f, gainDb, false); }

        void setPeak (double sr, double f, double q, double gainDb)
        {
            const double A = std::pow (10.0, gainDb / 40.0), w = juce::MathConstants<double>::twoPi * f / sr;
            const double alpha = std::sin (w) / (2.0 * q), c = std::cos (w);
            set (1 + alpha * A, -2 * c, 1 - alpha * A, 1 + alpha / A, -2 * c, 1 - alpha / A);
        }

        void shelf (double sr, double f, double gainDb, bool low)
        {
            const double A = std::pow (10.0, gainDb / 40.0), w = juce::MathConstants<double>::twoPi * f / sr;
            const double c = std::cos (w), s = std::sin (w), alpha = s / 2.0 * std::sqrt (2.0), sq = 2.0 * std::sqrt (A) * alpha;
            if (low)
                set (A * ((A + 1) - (A - 1) * c + sq), 2 * A * ((A - 1) - (A + 1) * c), A * ((A + 1) - (A - 1) * c - sq),
                     (A + 1) + (A - 1) * c + sq, -2 * ((A - 1) + (A + 1) * c), (A + 1) + (A - 1) * c - sq);
            else
                set (A * ((A + 1) + (A - 1) * c + sq), -2 * A * ((A - 1) + (A + 1) * c), A * ((A + 1) + (A - 1) * c - sq),
                     (A + 1) - (A - 1) * c + sq, 2 * ((A - 1) - (A + 1) * c), (A + 1) - (A - 1) * c - sq);
        }

        void set (double nb0, double nb1, double nb2, double na0, double na1, double na2)
        {
            b0 = (float) (nb0 / na0); b1 = (float) (nb1 / na0); b2 = (float) (nb2 / na0);
            a1 = (float) (na1 / na0); a2 = (float) (na2 / na0);
        }

        void reset()    { z1[0] = z1[1] = z2[0] = z2[1] = 0.0f; }

        inline float process (float x, int ch) noexcept
        {
            const float y = b0 * x + z1[ch];
            z1[ch] = b1 * x - a1 * y + z2[ch];
            z2[ch] = b2 * x - a2 * y;
            return y;
        }
    };

    /** L'equalizzatore a 3 bande di una traccia (stato del thread audio). */
    struct TrackEq
    {
        Biquad low, mid, high;
        float shown[4] { 99.0f, 99.0f, 99.0f, 99.0f };
        bool active = false;

        void update (double sr, float lo, float md, float freq, float hi)
        {
            if (lo == shown[0] && md == shown[1] && freq == shown[2] && hi == shown[3])
                return;
            shown[0] = lo; shown[1] = md; shown[2] = freq; shown[3] = hi;
            low.setLowShelf (sr, 120.0, lo);
            mid.setPeak (sr, juce::jlimit (60.0, sr * 0.4, (double) freq), 0.9, md);
            high.setHighShelf (sr, 8000.0, hi);
            active = std::abs (lo) > 0.05f || std::abs (md) > 0.05f || std::abs (hi) > 0.05f;
        }

        void process (float* L, float* R, int n) noexcept
        {
            if (! active)
                return;
            for (int i = 0; i < n; ++i)
            {
                L[i] = high.process (mid.process (low.process (L[i], 0), 0), 0);
                R[i] = high.process (mid.process (low.process (R[i], 1), 1), 1);
            }
        }

        void reset()    { low.reset(); mid.reset(); high.reset(); for (auto& s : shown) s = 99.0f; }
    };

    //==========================================================================
    // Gli effetti

    class Compressor : public Effect
    {
    public:
        void prepare (double sampleRate) override   { sr = sampleRate; env = 0.0f; }
        void reset() override                         { env = 0.0f; }

        void process (float* L, float* R, int n, const float* p, const Context&) override
        {
            const float threshold = p[0], ratio = juce::jmax (1.0f, p[1]);
            const float att = (float) std::exp (-1.0 / (juce::jmax (0.0001, (double) p[2] / 1000.0) * sr));
            const float rel = (float) std::exp (-1.0 / (juce::jmax (0.001, (double) p[3] / 1000.0) * sr));
            const float makeup = juce::Decibels::decibelsToGain (p[4]);

            for (int i = 0; i < n; ++i)
            {
                const float in = juce::jmax (std::abs (L[i]), std::abs (R[i]));
                env = in > env ? att * env + (1.0f - att) * in : rel * env + (1.0f - rel) * in;
                const float levelDb = juce::Decibels::gainToDecibels (env, -100.0f);
                const float over = levelDb - threshold;
                const float gain = over > 0.0f ? juce::Decibels::decibelsToGain (-over * (1.0f - 1.0f / ratio)) : 1.0f;
                L[i] *= gain * makeup;
                R[i] *= gain * makeup;
            }
        }

    private:
        double sr = 48000.0;
        float env = 0.0f;
    };

    class Saturation : public Effect
    {
    public:
        void prepare (double sampleRate) override   { sr = sampleRate; lp[0] = lp[1] = 0.0f; }

        void process (float* L, float* R, int n, const float* p, const Context&) override
        {
            const float drive = 1.0f + p[0] * 14.0f;
            // Volume compensato: un segnale a livello tipico (0.3) esce allo stesso livello con qualunque drive
            // (prima il drive alzava il volume fino a +20 dB e il limiter del master schiacciava tutto)
            const float norm = 0.3f / std::tanh (0.3f * drive);
            const float coef = 1.0f - (float) std::exp (-juce::MathConstants<double>::twoPi * p[1] / sr);
            const float mix = p[2], out = juce::Decibels::decibelsToGain (p[3]);
            float* ch[2] { L, R };
            for (int c = 0; c < 2; ++c)
                for (int i = 0; i < n; ++i)
                {
                    const float dry = ch[c][i];
                    float wet = std::tanh (dry * drive) * norm;
                    lp[c] += coef * (wet - lp[c]);
                    wet = lp[c];
                    ch[c][i] = (dry + (wet - dry) * mix) * out;
                }
        }

    private:
        double sr = 48000.0;
        float lp[2] {};
    };

    class Filter : public Effect
    {
    public:
        void prepare (double sampleRate) override   { sr = sampleRate; reset(); }
        void reset() override                         { s1[0] = s1[1] = s2[0] = s2[1] = 0.0f; phase = 0.0; }

        void process (float* L, float* R, int n, const float* p, const Context&) override
        {
            const int type = juce::roundToInt (p[0]);
            const float k = 2.0f - 1.9f * p[2];
            const double lfoInc = (double) p[4] / sr;
            float* ch[2] { L, R };

            for (int start = 0; start < n; start += 16)   // coefficienti ogni 16 campioni
            {
                const int len = juce::jmin (16, n - start);
                double cutoff = p[1];
                if (p[3] > 0.001f)
                {
                    cutoff *= std::exp2 (std::sin (phase * juce::MathConstants<double>::twoPi) * 3.0 * p[3]);
                    phase += lfoInc * len;
                    if (phase >= 1.0) phase -= 1.0;
                }
                const float g = (float) std::tan (juce::MathConstants<double>::pi * juce::jlimit (20.0, sr * 0.45, cutoff) / sr);
                const float a1 = 1.0f / (1.0f + g * (g + k)), a2 = g * a1, a3 = g * a2;

                for (int c = 0; c < 2; ++c)
                    for (int i = start; i < start + len; ++i)
                    {
                        const float x = ch[c][i];
                        const float v3 = x - s2[c];
                        const float v1 = a1 * s1[c] + a2 * v3;
                        const float v2 = s2[c] + a2 * s1[c] + a3 * v3;
                        s1[c] = 2.0f * v1 - s1[c];
                        s2[c] = 2.0f * v2 - s2[c];
                        ch[c][i] = type == 1 ? x - k * v1 - v2 : type == 2 ? v1 * k : v2;
                    }
            }
        }

    private:
        double sr = 48000.0, phase = 0.0;
        float s1[2] {}, s2[2] {};
    };

    class Delay : public Effect
    {
    public:
        void prepare (double sampleRate) override
        {
            sr = sampleRate;
            buffer[0].assign ((size_t) (sr * 2.6) + 1, 0.0f);
            buffer[1].assign ((size_t) (sr * 2.6) + 1, 0.0f);
            reset();
        }
        void reset() override
        {
            for (auto& b : buffer) std::fill (b.begin(), b.end(), 0.0f);
            pos = 0; lp[0] = lp[1] = 0.0f;
        }

        void process (float* L, float* R, int n, const float* p, const Context& ctx) override
        {
            static constexpr double sixteenths[] = { 1.0, 2.0, 3.0, 4.0, 6.0, 8.0 };
            const double steps = sixteenths[juce::jlimit (0, 5, juce::roundToInt (p[0]))];
            const int size = (int) buffer[0].size();
            const int d = juce::jlimit (1, size - 1, (int) (steps * 15.0 / juce::jmax (40.0, ctx.bpm) * sr));
            const float fb = p[1], mix = p[2];
            const bool ping = p[3] > 0.5f;
            const float coef = 1.0f - (float) std::exp (-juce::MathConstants<double>::twoPi * p[4] / sr);

            for (int i = 0; i < n; ++i)
            {
                const int rd = (pos - d + size) % size;
                const float dl = buffer[0][(size_t) rd], dr = buffer[1][(size_t) rd];
                lp[0] += coef * (dl - lp[0]);
                lp[1] += coef * (dr - lp[1]);
                if (ping)
                {
                    buffer[0][(size_t) pos] = (L[i] + R[i]) * 0.5f + lp[1] * fb;
                    buffer[1][(size_t) pos] = lp[0] * fb;
                }
                else
                {
                    buffer[0][(size_t) pos] = L[i] + lp[0] * fb;
                    buffer[1][(size_t) pos] = R[i] + lp[1] * fb;
                }
                pos = (pos + 1) % size;
                L[i] += dl * mix;
                R[i] += dr * mix;
            }
        }

    private:
        double sr = 48000.0;
        std::vector<float> buffer[2];
        int pos = 0;
        float lp[2] {};
    };

    class Reverb : public Effect
    {
    public:
        void prepare (double sampleRate) override   { reverb.setSampleRate (sampleRate); reverb.reset(); }
        void reset() override                         { reverb.reset(); }

        void process (float* L, float* R, int n, const float* p, const Context&) override
        {
            juce::Reverb::Parameters rp;
            rp.roomSize = p[0]; rp.damping = p[1]; rp.width = p[2];
            rp.wetLevel = p[3] * 0.7f; rp.dryLevel = 1.0f - p[3] * 0.5f;
            if (rp.roomSize != last.roomSize || rp.damping != last.damping || rp.width != last.width || rp.wetLevel != last.wetLevel)
            {
                reverb.setParameters (rp);
                last = rp;
            }
            reverb.processStereo (L, R, n);
        }

    private:
        juce::Reverb reverb;
        juce::Reverb::Parameters last;
    };

    class Chorus : public Effect
    {
    public:
        void prepare (double sampleRate) override
        {
            sr = sampleRate;
            buffer[0].assign (4096, 0.0f);
            buffer[1].assign (4096, 0.0f);
            pos = 0; phase = 0.0;
        }

        void process (float* L, float* R, int n, const float* p, const Context&) override
        {
            const float base = (float) (0.012 * sr), depth = (float) (0.006 * sr) * p[1], mix = p[2];
            const int size = (int) buffer[0].size();
            for (int i = 0; i < n; ++i)
            {
                buffer[0][(size_t) pos] = L[i];
                buffer[1][(size_t) pos] = R[i];
                phase += p[0] / sr;
                if (phase >= 1.0) phase -= 1.0;
                const float mL = perf::fastSin ((float) phase);
                const float mR = perf::fastSin ((float) phase + 0.25f);
                const float wl = read (buffer[0], base + depth * mL, size);
                const float wr = read (buffer[1], base + depth * mR, size);
                L[i] = L[i] * (1.0f - 0.4f * mix) + wr * 0.8f * mix;
                R[i] = R[i] * (1.0f - 0.4f * mix) + wl * 0.8f * mix;
                pos = (pos + 1) % size;
            }
        }

    private:
        float read (const std::vector<float>& b, float delay, int size) const
        {
            float idx = (float) pos - delay;
            while (idx < 0) idx += (float) size;
            const int i0 = (int) idx;
            const float fr = idx - (float) i0;
            return b[(size_t) i0] + fr * (b[(size_t) ((i0 + 1) % size)] - b[(size_t) i0]);
        }

        double sr = 48000.0, phase = 0.0;
        std::vector<float> buffer[2];
        int pos = 0;
    };

    class Bitcrusher : public Effect
    {
    public:
        void prepare (double) override  { counter = 0; held[0] = held[1] = 0.0f; }

        void process (float* L, float* R, int n, const float* p, const Context&) override
        {
            const float q = std::exp2 (juce::jlimit (2.0f, 16.0f, p[0]) - 1.0f);
            const int hold = juce::jmax (1, juce::roundToInt (p[1]));
            const float mix = p[2];
            for (int i = 0; i < n; ++i)
            {
                if (counter <= 0)
                {
                    held[0] = std::round (L[i] * q) / q;
                    held[1] = std::round (R[i] * q) / q;
                    counter = hold;
                }
                --counter;
                L[i] += (held[0] - L[i]) * mix;
                R[i] += (held[1] - R[i]) * mix;
            }
        }

    private:
        int counter = 0;
        float held[2] {};
    };

    /**
        Sidechain "a colpi": riconosce ogni attacco della traccia sorgente (la cassa) e abbassa il volume,
        che poi risale nel tempo di "Rilascio". Cosi' pompa a tempo anche con casse lunghe (808).
    */
    class Sidechain : public Effect
    {
    public:
        void prepare (double sampleRate) override   { sr = sampleRate; reset(); }
        void reset() override                         { fast = slow = 0.0f; phase = 1.0f; refractory = 0; }

        void process (float* L, float* R, int n, const float* p, const Context& ctx) override
        {
            if (ctx.keyL == nullptr)
                return;
            const float fastAtt = (float) std::exp (-1.0 / (0.0005 * sr)), fastRel = (float) std::exp (-1.0 / (0.010 * sr));
            const float slowCoef = (float) std::exp (-1.0 / (0.060 * sr));
            const float threshold = juce::Decibels::decibelsToGain (p[3]);
            const float amount = p[1];
            const float step = (float) (1.0 / (juce::jmax (0.02, (double) p[2] / 1000.0) * sr));
            const int minGap = (int) (0.06 * sr);

            for (int i = 0; i < n; ++i)
            {
                const float key = juce::jmax (std::abs (ctx.keyL[i]), std::abs (ctx.keyR[i]));
                fast = key > fast ? fastAtt * fast + (1.0f - fastAtt) * key : fastRel * fast;
                slow = slowCoef * slow + (1.0f - slowCoef) * fast;

                // Un attacco: il livello sale di colpo sopra la soglia
                if (refractory > 0)
                    --refractory;
                else if (fast > threshold && fast > slow * 1.6f)
                {
                    phase = 0.0f;
                    refractory = minGap;
                }

                float duck = 0.0f;
                if (phase < 1.0f)
                {
                    const float x = phase;
                    duck = amount * (1.0f - x) * (1.0f - x);
                    phase += step;
                }
                // Discesa morbida (3 ms) per non fare click
                current += (duck - current) * 0.02f;
                L[i] *= 1.0f - current;
                R[i] *= 1.0f - current;
            }
        }

    private:
        double sr = 48000.0;
        float fast = 0.0f, slow = 0.0f, phase = 1.0f, current = 0.0f;
        int refractory = 0;
    };

    class Stereo : public Effect
    {
    public:
        void prepare (double) override {}
        void process (float* L, float* R, int n, const float* p, const Context&) override
        {
            const float w = p[0];
            for (int i = 0; i < n; ++i)
            {
                const float m = (L[i] + R[i]) * 0.5f, s = (L[i] - R[i]) * 0.5f * w;
                L[i] = m + s;
                R[i] = m - s;
            }
        }
    };

    //==========================================================================
    /**
        OTT: compressore multibanda "su e giu'" (stile Xfer OTT). Tre bande (88 Hz / 2.5 kHz):
        i suoni forti vengono abbassati (downward), quelli deboli alzati (upward): tutto diventa denso e in faccia.
        Profondita' = quanto del suono compresso si mescola all'originale.
    */
    class Ott : public Effect
    {
    public:
        void prepare (double sampleRate) override
        {
            sr = sampleRate;
            for (auto* f : { &lowSplit, &highSplit, &lowAllpass })
            {
                f->prepare ({ sr, 4096, 2 });
                f->setType (juce::dsp::LinkwitzRileyFilterType::lowpass);
            }
            lowSplit.setCutoffFrequency (88.0f);
            highSplit.setCutoffFrequency (2500.0f);
            lowAllpass.setCutoffFrequency (2500.0f);
            reset();
        }

        void reset() override
        {
            lowSplit.reset(); highSplit.reset(); lowAllpass.reset();
            env.fill (0.0f);
            gainDb.fill (0.0f);
        }

        void process (float* L, float* R, int n, const float* p, const Context&) override
        {
            const float depth = p[0], timeScale = 0.15f + 0.85f * p[1], up = p[2], down = p[3];
            const float out = juce::Decibels::decibelsToGain (p[4]);
            if (depth <= 0.001f)
            {
                if (std::abs (p[4]) > 0.01f) for (int i = 0; i < n; ++i) { L[i] *= out; R[i] *= out; }
                return;
            }

            // Tempi, soglie e guadagni per banda (basso, medio, alto), presi dai valori classici dell'OTT
            static constexpr float attMs[3] { 47.8f, 22.4f, 13.5f }, relMs[3] { 282.0f, 132.0f, 67.0f };
            static constexpr float downThr[3] { -33.8f, -30.2f, -35.5f }, upThr[3] { -40.8f, -41.8f, -40.8f };
            static constexpr float makeupDb[3] { 18.0f, 16.0f, 18.0f };   // come i guadagni di banda dell'OTT: il suono compresso torna su
            float att[3], rel[3];
            for (int b = 0; b < 3; ++b)
            {
                att[b] = (float) std::exp (-1.0 / (attMs[b] * timeScale * 0.001 * sr));
                rel[b] = (float) std::exp (-1.0 / (relMs[b] * timeScale * 0.001 * sr));
            }
            const float downSlope = (1.0f - 1.0f / 66.7f) * down;   // sopra la soglia: quasi un limiter
            const float upSlope = (1.0f - 1.0f / 4.17f) * up;       // sotto la soglia: alza i dettagli

            for (int i = 0; i < n; ++i)
            {
                float band[3][2];
                for (int ch = 0; ch < 2; ++ch)
                {
                    const float x = ch == 0 ? L[i] : R[i];
                    float lo, rest, mid, hi, loA, loB;
                    lowSplit.processSample (ch, x, lo, rest);
                    highSplit.processSample (ch, rest, mid, hi);
                    lowAllpass.processSample (ch, lo, loA, loB);   // stessa fase delle altre bande
                    band[0][ch] = loA + loB;
                    band[1][ch] = mid;
                    band[2][ch] = hi;
                }

                float yl = 0.0f, yr = 0.0f;
                for (int b = 0; b < 3; ++b)
                {
                    const float level = juce::jmax (std::abs (band[b][0]), std::abs (band[b][1]));
                    env[(size_t) b] = level > env[(size_t) b] ? att[b] * env[(size_t) b] + (1.0f - att[b]) * level
                                                              : rel[b] * env[(size_t) b] + (1.0f - rel[b]) * level;
                    if ((i & 3) == 0)   // guadagno ricalcolato ogni 4 campioni (log/exp costano)
                    {
                        const float db = juce::Decibels::gainToDecibels (env[(size_t) b], -120.0f);
                        float g = 0.0f;
                        if (db > downThr[b])
                            g = -(db - downThr[b]) * downSlope;
                        else if (db < upThr[b] && db > -78.0f)
                        {
                            // l'aumento si spegne nel silenzio (sotto -60 dB): niente fruscio alzato
                            const float fade = juce::jlimit (0.0f, 1.0f, (db + 78.0f) / 18.0f);
                            g = juce::jmin (24.0f, (upThr[b] - db) * upSlope) * fade;
                        }
                        gainDb[(size_t) b] = juce::Decibels::decibelsToGain (g + makeupDb[b]);
                    }
                    yl += band[b][0] * gainDb[(size_t) b];
                    yr += band[b][1] * gainDb[(size_t) b];
                }
                L[i] = (L[i] + depth * (yl - L[i])) * out;
                R[i] = (R[i] + depth * (yr - R[i])) * out;
            }
        }

    private:
        double sr = 48000.0;
        juce::dsp::LinkwitzRileyFilter<float> lowSplit, highSplit, lowAllpass;
        std::array<float, 3> env {}, gainDb { 1.0f, 1.0f, 1.0f };
    };

    //==========================================================================
    /**
        Glitch (stile Gross Beat): una griglia di 16 step (una battuta) e per ogni step un effetto:
        0 normale, 1 ripeti 1/8, 2 ripeti 1/16, 3 ripeti 1/32, 4 tape stop, 5 reverse, 6 muto, 7 rallenta.
        Step consecutivi con lo stesso effetto formano un unico gesto (es. 4 step "ripeti 1/32" = una rullata
        che ripete l'inizio del primo step). Funziona a tempo, mentre il progetto suona.
        La griglia sta in due parametri: 8 step x 3 bit ciascuno.
    */
    namespace glitchGrid
    {
        enum Mode { normal = 0, repeat8, repeat16, repeat32, tapeStop, reverse, mute, halfSpeed, numModes };
        inline int get (const float* p, int step)
        {
            const auto code = (uint32_t) juce::jlimit (0.0f, 16777215.0f, p[step < 8 ? 1 : 2]);
            return (int) ((code >> (3 * (step % 8))) & 7u);
        }
        inline void set (float* p, int step, int mode)
        {
            auto code = (uint32_t) juce::jlimit (0.0f, 16777215.0f, p[step < 8 ? 1 : 2]);
            const uint32_t shift = (uint32_t) (3 * (step % 8));
            code = (code & ~(7u << shift)) | ((uint32_t) (mode & 7) << shift);
            p[step < 8 ? 1 : 2] = (float) code;
        }
        inline const char* name (int mode)
        {
            static const char* names[] { "Normale", "Ripeti 1/8", "Ripeti 1/16", "Ripeti 1/32", "Tape stop", "Reverse", "Muto", "Rallenta" };
            return names[juce::jlimit (0, 7, mode)];
        }
        inline const char* shortName (int mode)
        {
            static const char* names[] { "", "1/8", "1/16", "1/32", "STOP", "REV", "MUTO", "1/2x" };
            return names[juce::jlimit (0, 7, mode)];
        }
    }

    class Glitch : public Effect
    {
    public:
        void prepare (double sampleRate) override
        {
            sr = sampleRate;
            const size_t size = (size_t) (sr * 8.0);
            buf[0].assign (size, 0.0f);
            buf[1].assign (size, 0.0f);
            reset();
        }

        void reset() override
        {
            for (auto& b : buf) std::fill (b.begin(), b.end(), 0.0f);
            writePos = 0; runStart = 0; runMode = 0; runStep = -1; sampleInRun = 0; wet = 0.0f;
        }

        void process (float* L, float* R, int n, const float* p, const Context& ctx) override
        {
            if (buf[0].empty())
                return;
            const int size = (int) buf[0].size();
            const double beatsPerSample = ctx.bpm / 60.0 / sr;
            const double samplesPerStep = sr * 60.0 / ctx.bpm / 4.0;
            const float mix = p[0];
            const float fadeStep = (float) (1.0 / (0.002 * sr));    // 2 ms

            for (int i = 0; i < n; ++i)
            {
                buf[0][(size_t) writePos] = L[i];
                buf[1][(size_t) writePos] = R[i];

                int mode = glitchGrid::normal;
                if (ctx.playing && ctx.ppq >= 0.0)
                {
                    const double beat = ctx.ppq + beatsPerSample * i;
                    const double inBar = std::fmod (beat, 4.0);
                    const int step = juce::jlimit (0, 15, (int) (inBar * 4.0));
                    mode = glitchGrid::get (p, step);

                    // Nuovo "gesto": lo step precedente aveva un effetto diverso (o si e' ricominciato)
                    const bool newRun = mode != runMode || step < runStep || step >= runStep + runLength;
                    if (newRun)
                    {
                        runMode = mode;
                        runStep = step;
                        runStart = writePos;
                        sampleInRun = 0;
                        runLength = 1;
                        while (step + runLength < 16 && glitchGrid::get (p, step + runLength) == mode)
                            ++runLength;
                        runSamples = samplesPerStep * runLength;
                    }
                }
                else
                {
                    runMode = glitchGrid::normal;
                    runStep = -1;
                }

                // Lettura dal registro secondo l'effetto
                float wl = L[i], wr = R[i], env = 1.0f;
                const double t = (double) sampleInRun;
                double readOffset = 0.0;    // campioni da runStart (puo' essere negativo: reverse)
                bool useBuffer = true;
                switch (runMode)
                {
                    case glitchGrid::repeat8:
                    case glitchGrid::repeat16:
                    case glitchGrid::repeat32:
                    {
                        const double len = samplesPerStep * (runMode == glitchGrid::repeat8 ? 2.0 : runMode == glitchGrid::repeat16 ? 1.0 : 0.5);
                        const double pos = std::fmod (t, len);
                        readOffset = pos;
                        // piccola sfumatura a inizio e fine di ogni ripetizione: niente click
                        const double edge = 0.0015 * sr;
                        env = (float) juce::jmin (1.0, juce::jmin (pos / edge, (len - pos) / edge));
                        break;
                    }
                    case glitchGrid::tapeStop:
                    {
                        const double L0 = juce::jmax (1.0, runSamples);
                        const double tt = juce::jmin (t, L0);
                        readOffset = tt - tt * tt / (2.0 * L0);        // velocita' da 1 a 0
                        env = (float) juce::jlimit (0.0, 1.0, (L0 - t) / (0.01 * sr));
                        break;
                    }
                    case glitchGrid::reverse:
                        readOffset = -1.0 - t;                         // quello appena suonato, al contrario
                        env = (float) juce::jmin (1.0, juce::jmin (t / (0.003 * sr), (runSamples - t) / (0.003 * sr)));
                        break;
                    case glitchGrid::halfSpeed:
                        readOffset = t * 0.5;
                        break;
                    case glitchGrid::mute:
                        wl = wr = 0.0f;
                        useBuffer = false;
                        break;
                    default:
                        useBuffer = false;
                        break;
                }

                if (useBuffer)
                {
                    double pos = (double) runStart + readOffset;
                    pos -= std::floor (pos / size) * size;
                    const int i0 = (int) pos % size, i1 = (i0 + 1) % size;
                    const float f = (float) (pos - std::floor (pos));
                    wl = (buf[0][(size_t) i0] + f * (buf[0][(size_t) i1] - buf[0][(size_t) i0])) * env;
                    wr = (buf[1][(size_t) i0] + f * (buf[1][(size_t) i1] - buf[1][(size_t) i0])) * env;
                }

                // Passaggio morbido tra suono normale ed effetto
                const float target = runMode == glitchGrid::normal ? 0.0f : mix;
                wet += juce::jlimit (-fadeStep, fadeStep, target - wet);
                L[i] += wet * (wl - L[i]);
                R[i] += wet * (wr - R[i]);

                writePos = (writePos + 1) % size;
                ++sampleInRun;
            }
        }

    private:
        double sr = 48000.0;
        std::array<std::vector<float>, 2> buf;
        int writePos = 0, runStart = 0, runMode = 0, runStep = -1, runLength = 1;
        long sampleInRun = 0;
        double runSamples = 1.0;
        float wet = 0.0f;
    };

    //==========================================================================
    /** Distorsione "trash": il basso (sotto 150 Hz) resta quasi pulito, medi e alti vengono saturati forte. */
    class Trash : public Effect
    {
    public:
        void prepare (double sampleRate) override
        {
            sr = sampleRate;
            split.prepare ({ sr, 4096, 2 });
            split.setType (juce::dsp::LinkwitzRileyFilterType::lowpass);
            split.setCutoffFrequency (150.0f);
            reset();
        }
        void reset() override   { split.reset(); lp[0] = lp[1] = 0.0f; }

        void process (float* L, float* R, int n, const float* p, const Context&) override
        {
            const float drive = 1.0f + p[0] * p[0] * 40.0f, clean = p[2], mix = p[3];
            const float out = juce::Decibels::decibelsToGain (p[4]);
            const float a = 1.0f - std::exp (-juce::MathConstants<float>::twoPi * p[1] / (float) sr);
            const float norm = 1.0f / std::tanh (juce::jmin (drive, 3.0f));
            for (int i = 0; i < n; ++i)
                for (int ch = 0; ch < 2; ++ch)
                {
                    float& x = ch == 0 ? L[i] : R[i];
                    float lo, hi;
                    split.processSample (ch, x, lo, hi);
                    // asimmetria leggera: armoniche pari, suono "sporco"
                    float d = std::tanh (hi * drive + 0.15f * p[0]) - std::tanh (0.15f * p[0]);
                    d = d * norm * 0.6f;
                    lp[ch] += a * (d - lp[ch]);                      // tono: toglie la "sabbia" in cima
                    const float lowPart = lo * clean + std::tanh (lo * drive * 0.5f) * 0.5f * (1.0f - clean);
                    const float wet = lowPart + lp[ch];
                    x = (x + mix * (wet - x)) * out;
                }
        }

    private:
        double sr = 48000.0;
        juce::dsp::LinkwitzRileyFilter<float> split;
        float lp[2] {};
    };

    //==========================================================================
    /** Filtro a vocali: due formanti (F1, F2) che passano da a-e-i-o-u, mossi da un LFO a tempo. */
    class Vowel : public Effect
    {
    public:
        void prepare (double sampleRate) override   { sr = sampleRate; reset(); }
        void reset() override                        { for (auto& f : bands) f = {}; phase = 0.0; }

        void process (float* L, float* R, int n, const float* p, const Context& ctx) override
        {
            static constexpr float F1[5] { 800.0f, 450.0f, 300.0f, 450.0f, 330.0f };   // a e i o u
            static constexpr float F2[5] { 1150.0f, 1900.0f, 2300.0f, 800.0f, 700.0f };
            static constexpr double beatsPer[5] { 4.0, 2.0, 1.0, 0.5, 0.25 };
            const double beats = beatsPer[juce::jlimit (0, 4, juce::roundToInt (p[2]))];
            const float q = 2.0f + p[3] * 14.0f, mix = p[4];
            for (int start = 0; start < n; start += 16)
            {
                const int m = juce::jmin (16, n - start);
                // LFO: a tempo col progetto quando suona, libero quando e' fermo
                double ph;
                if (ctx.playing) ph = std::fmod ((ctx.ppq + ctx.bpm / 60.0 * start / sr) / beats, 1.0);
                else { phase = std::fmod (phase + ctx.bpm / 60.0 / beats * m / sr, 1.0); ph = phase; }
                const float lfo = 0.5f - 0.5f * std::cos ((float) ph * juce::MathConstants<float>::twoPi);
                const float v = juce::jlimit (0.0f, 4.0f, p[0] + p[1] * 4.0f * lfo);
                const int i0 = juce::jmin (3, (int) v);
                const float f = v - (float) i0;
                const float f1 = F1[i0] + f * (F1[i0 + 1] - F1[i0]), f2 = F2[i0] + f * (F2[i0 + 1] - F2[i0]);
                for (int b = 0; b < 2; ++b)
                {
                    const float g = std::tan (juce::MathConstants<float>::pi * juce::jmin (b == 0 ? f1 : f2, (float) sr * 0.45f) / (float) sr);
                    for (auto ch = 0; ch < 2; ++ch) bands[(size_t) (b * 2 + ch)].set (g, 1.0f / q);
                }
                for (int i = start; i < start + m; ++i)
                    for (int ch = 0; ch < 2; ++ch)
                    {
                        float& x = ch == 0 ? L[i] : R[i];
                        const float y = bands[(size_t) ch].bp (x) * 1.6f + bands[(size_t) (2 + ch)].bp (x) * 1.1f;
                        x += mix * (y * 1.4f - x);
                    }
            }
        }

    private:
        struct Svf
        {
            float g = 0.1f, k = 0.5f, s1 = 0.0f, s2 = 0.0f;
            void set (float g_, float k_) { g = g_; k = k_; }
            float bp (float x)
            {
                const float a1 = 1.0f / (1.0f + g * (g + k)), a2 = g * a1, a3 = g * a2;
                const float v3 = x - s2, v1 = a1 * s1 + a2 * v3, v2 = s2 + a2 * s1 + a3 * v3;
                s1 = 2.0f * v1 - s1; s2 = 2.0f * v2 - s2;
                return v1 * k;
            }
        };
        double sr = 48000.0, phase = 0.0;
        std::array<Svf, 4> bands;
    };

    //==========================================================================
    // Effetti aggiunti nella 0.63

    /** Dove siamo (in quarti) al campione i del blocco: a tempo col progetto se suona, altrimenti un orologio libero. */
    struct TempoClock
    {
        double freeBeat = 0.0;
        double beatAt (const Context& ctx, double sr, int i) const
        {
            const double b = ctx.bpm / 60.0 * i / sr;
            return ctx.playing ? ctx.ppq + b : freeBeat + b;
        }
        void advance (const Context& ctx, double sr, int n)
        {
            if (! ctx.playing) freeBeat = std::fmod (freeBeat + ctx.bpm / 60.0 * n / sr, 64.0);
        }
    };

    inline float frac01 (double x) noexcept   { return (float) (x - std::floor (x)); }

    /** Il ritmo (16 step) del Trance gate, salvato come bit nel parametro 0. */
    namespace gateGrid
    {
        inline bool get (const float* p, int step)   { return ((juce::uint32) juce::roundToInt (p[0]) >> (step & 15)) & 1u; }
        inline float set (float v, int step, bool on)
        {
            auto bits = (juce::uint32) juce::roundToInt (v);
            bits = on ? (bits | (1u << step)) : (bits & ~(1u << step));
            return (float) (bits & 0xffffu);
        }
        inline constexpr int stepsPerBeat[3] { 2, 4, 8 };
    }

    class Gate : public Effect
    {
    public:
        void prepare (double sampleRate) override   { sr = sampleRate; reset(); }
        void reset() override                        { gain = 1.0f; clock = {}; }

        void process (float* L, float* R, int n, const float* p, const Context& ctx) override
        {
            const int spb = gateGrid::stepsPerBeat[juce::jlimit (0, 2, juce::roundToInt (p[1]))];
            const auto bits = (juce::uint32) juce::roundToInt (p[0]);
            const float low = 1.0f - p[2];
            const float coef = 1.0f - std::exp (-1.0f / ((0.0007f + p[3] * p[3] * 0.04f) * (float) sr));
            for (int i = 0; i < n; ++i)
            {
                const double beat = clock.beatAt (ctx, sr, i);
                int step = (int) std::floor (beat * spb) % 16;
                if (step < 0) step += 16;
                const float target = ((bits >> step) & 1u) ? 1.0f : low;
                gain += (target - gain) * coef;
                L[i] *= gain; R[i] *= gain;
            }
            clock.advance (ctx, sr, n);
        }

    private:
        double sr = 48000.0;
        float gain = 1.0f;
        TempoClock clock;
    };

    class Tremolo : public Effect
    {
    public:
        void prepare (double sampleRate) override   { sr = sampleRate; reset(); }
        void reset() override                        { clock = {}; }

        void process (float* L, float* R, int n, const float* p, const Context& ctx) override
        {
            static constexpr double beatsPer[7] { 4.0, 2.0, 1.0, 0.5, 0.25, 2.0 / 3.0, 1.0 / 3.0 };
            const double beats = beatsPer[juce::jlimit (0, 6, juce::roundToInt (p[1]))];
            const bool pan = p[0] > 0.5f;
            const int shape = juce::jlimit (0, 2, juce::roundToInt (p[3]));
            const float depth = p[2];
            for (int i = 0; i < n; ++i)
            {
                const float ph = frac01 (clock.beatAt (ctx, sr, i) / beats);
                float lfo;   // -1..1
                if (shape == 0)      lfo = perf::fastSin (ph);
                else if (shape == 1) lfo = 4.0f * std::abs (ph - 0.5f) - 1.0f;
                else                 lfo = std::tanh (perf::fastSin (ph) * 7.0f);
                if (pan)
                {
                    const float a = (lfo * depth + 1.0f) * juce::MathConstants<float>::pi * 0.25f;
                    L[i] *= std::cos (a) * juce::MathConstants<float>::sqrt2;
                    R[i] *= std::sin (a) * juce::MathConstants<float>::sqrt2;
                }
                else
                {
                    const float g = 1.0f - depth * (0.5f - 0.5f * lfo);
                    L[i] *= g; R[i] *= g;
                }
            }
            clock.advance (ctx, sr, n);
        }

    private:
        double sr = 48000.0;
        TempoClock clock;
    };

    class Phaser : public Effect
    {
    public:
        void prepare (double sampleRate) override   { sr = sampleRate; reset(); }
        void reset() override
        {
            for (auto& c : state) for (auto& s : c) s = 0.0f;
            fb[0] = fb[1] = 0.0f; phase = 0.0;
        }

        void process (float* L, float* R, int n, const float* p, const Context&) override
        {
            static constexpr int stagesFor[4] { 4, 6, 8, 12 };
            const int stages = stagesFor[juce::jlimit (0, 3, juce::roundToInt (p[3]))];
            const float depth = p[1], feedback = p[2], mix = p[4], norm = 1.0f / (1.0f + mix);
            float a[2] {};
            for (int i = 0; i < n; ++i)
            {
                if ((i & 7) == 0)
                {
                    for (int ch = 0; ch < 2; ++ch)
                    {
                        const float lfo = 0.5f + 0.5f * perf::fastSin ((float) phase + 0.25f * (float) ch);
                        const double f = juce::jmin (sr * 0.45, 180.0 * std::pow (2.0, (double) (lfo * depth * 6.0f)));
                        const double t = std::tan (juce::MathConstants<double>::pi * f / sr);
                        a[ch] = (float) ((t - 1.0) / (t + 1.0));
                    }
                }
                phase += p[0] / sr;
                if (phase >= 1.0) phase -= 1.0;
                for (int ch = 0; ch < 2; ++ch)
                {
                    float& x = ch == 0 ? L[i] : R[i];
                    float y = x + std::tanh (fb[ch] * feedback);
                    for (int s = 0; s < stages; ++s)
                    {
                        const float out = a[ch] * y + state[ch][(size_t) s];
                        state[ch][(size_t) s] = y - a[ch] * out;
                        y = out;
                    }
                    fb[ch] = y;
                    x = (x + mix * y) * norm;
                }
            }
        }

    private:
        double sr = 48000.0, phase = 0.0;
        std::array<float, 12> state[2] {};
        float fb[2] {};
    };

    /** Linea di ritardo stereo con lettura interpolata (per flanger, nastro e pitch shifter). */
    struct DelayLine
    {
        std::vector<float> buf[2];
        int pos = 0, mask = 0;

        void prepare (int minSize)
        {
            int size = 1;
            while (size < minSize) size <<= 1;
            buf[0].assign ((size_t) size, 0.0f);
            buf[1].assign ((size_t) size, 0.0f);
            mask = size - 1; pos = 0;
        }
        void clear()                                { for (auto& b : buf) std::fill (b.begin(), b.end(), 0.0f); }
        void write (float l, float r) noexcept      { buf[0][(size_t) pos] = l; buf[1][(size_t) pos] = r; pos = (pos + 1) & mask; }
        /** Legge il campione di 'delay' campioni fa (delay >= 1). */
        float read (int ch, float delay) const noexcept
        {
            const float idx = (float) pos - delay;
            const int i0 = (int) std::floor (idx);
            const float fr = idx - (float) i0;
            const float a = buf[ch][(size_t) (i0 & mask)], b = buf[ch][(size_t) ((i0 + 1) & mask)];
            return a + fr * (b - a);
        }
    };

    class Flanger : public Effect
    {
    public:
        void prepare (double sampleRate) override   { sr = sampleRate; line.prepare ((int) (sr * 0.03) + 8); reset(); }
        void reset() override                        { line.clear(); fb[0] = fb[1] = 0.0f; phase = 0.0; clock = {}; }

        void process (float* L, float* R, int n, const float* p, const Context& ctx) override
        {
            static constexpr double beatsPer[5] { 0.0, 8.0, 4.0, 2.0, 1.0 };
            const int sync = juce::jlimit (0, 4, juce::roundToInt (p[5]));
            const float maxD = p[3] * 0.001f * (float) sr, depth = p[1], feedback = p[2], mix = p[4];
            const float minD = juce::jmax (1.5f, 0.0001f * (float) sr);
            const float norm = 1.0f / (1.0f + mix * 0.6f);
            for (int i = 0; i < n; ++i)
            {
                double ph;
                if (sync > 0) ph = clock.beatAt (ctx, sr, i) / beatsPer[sync];
                else { phase += p[0] / sr; if (phase >= 1.0) phase -= 1.0; ph = phase; }
                float out[2];
                for (int ch = 0; ch < 2; ++ch)
                {
                    const float lfo = 0.5f + 0.5f * perf::fastSin ((float) ph + 0.12f * (float) ch);
                    const float d = juce::jmax (minD, maxD * (1.0f - depth * lfo));
                    out[ch] = line.read (ch, d);
                }
                line.write (L[i] + std::tanh (out[0] * feedback), R[i] + std::tanh (out[1] * feedback));
                const float wetGain = 1.0f - std::abs (feedback) * 0.45f;
                L[i] = (L[i] + mix * out[0] * wetGain) * norm;
                R[i] = (R[i] + mix * out[1] * wetGain) * norm;
            }
            clock.advance (ctx, sr, n);
        }

    private:
        double sr = 48000.0, phase = 0.0;
        DelayLine line;
        float fb[2] {};
        TempoClock clock;
    };

    /** Filtri passa-alto / passa-basso (Butterworth 12 dB) per l'EQ parametrico e gli altri. */
    inline void setPass (Biquad& b, double sr, double f, bool high)
    {
        const double w = juce::MathConstants<double>::twoPi * juce::jlimit (10.0, sr * 0.45, f) / sr;
        const double c = std::cos (w), alpha = std::sin (w) / (2.0 * 0.7071);
        if (high) b.set ((1 + c) / 2, -(1 + c), (1 + c) / 2, 1 + alpha, -2 * c, 1 - alpha);
        else      b.set ((1 - c) / 2, 1 - c, (1 - c) / 2, 1 + alpha, -2 * c, 1 - alpha);
    }

    class ParamEq : public Effect
    {
    public:
        void prepare (double sampleRate) override   { sr = sampleRate; reset(); }
        void reset() override                        { for (auto& b : bands) b.reset(); for (auto& c : cache) c = -999.0f; }

        void process (float* L, float* R, int n, const float* p, const Context&) override
        {
            bool changed = false;
            for (int k = 0; k < 12; ++k)
                if (cache[(size_t) k] != p[k]) { cache[(size_t) k] = p[k]; changed = true; }
            if (changed)
            {
                const double top = sr * 0.45;
                on[0] = p[0] > 20.5f;                 setPass (bands[0], sr, p[0], true);
                on[1] = std::abs (p[1]) > 0.05f;      bands[1].setLowShelf (sr, juce::jmin (top, (double) p[2]), p[1]);
                on[2] = std::abs (p[3]) > 0.05f;      bands[2].setPeak (sr, juce::jmin (top, (double) p[4]), juce::jmax (0.1f, p[5]), p[3]);
                on[3] = std::abs (p[6]) > 0.05f;      bands[3].setPeak (sr, juce::jmin (top, (double) p[7]), juce::jmax (0.1f, p[8]), p[6]);
                on[4] = std::abs (p[9]) > 0.05f;      bands[4].setHighShelf (sr, juce::jmin (top, (double) p[10]), p[9]);
                on[5] = p[11] < 19900.0f && p[11] < top;   setPass (bands[5], sr, p[11], false);
            }
            for (int b = 0; b < 6; ++b)
            {
                if (! on[b]) continue;
                auto& f = bands[(size_t) b];
                for (int i = 0; i < n; ++i) { L[i] = f.process (L[i], 0); R[i] = f.process (R[i], 1); }
            }
        }

        /** Risposta in dB a una frequenza (per il disegno della curva). */
        static float responseDb (const float* p, double sr, double f)
        {
            Biquad b[6];
            bool active[6];
            const double top = sr * 0.45;
            active[0] = p[0] > 20.5f;            setPass (b[0], sr, p[0], true);
            active[1] = std::abs (p[1]) > 0.05f; b[1].setLowShelf (sr, juce::jmin (top, (double) p[2]), p[1]);
            active[2] = std::abs (p[3]) > 0.05f; b[2].setPeak (sr, juce::jmin (top, (double) p[4]), juce::jmax (0.1f, p[5]), p[3]);
            active[3] = std::abs (p[6]) > 0.05f; b[3].setPeak (sr, juce::jmin (top, (double) p[7]), juce::jmax (0.1f, p[8]), p[6]);
            active[4] = std::abs (p[9]) > 0.05f; b[4].setHighShelf (sr, juce::jmin (top, (double) p[10]), p[9]);
            active[5] = p[11] < 19900.0f;        setPass (b[5], sr, p[11], false);
            const double w = juce::MathConstants<double>::twoPi * f / sr;
            const std::complex<double> z1 = std::polar (1.0, -w), z2 = z1 * z1;
            double mag = 1.0;
            for (int k = 0; k < 6; ++k)
            {
                if (! active[k]) continue;
                const auto num = (double) b[k].b0 + (double) b[k].b1 * z1 + (double) b[k].b2 * z2;
                const auto den = 1.0 + (double) b[k].a1 * z1 + (double) b[k].a2 * z2;
                mag *= std::abs (num / den);
            }
            return (float) (20.0 * std::log10 (juce::jmax (1.0e-6, mag)));
        }

    private:
        double sr = 48000.0;
        std::array<Biquad, 6> bands;
        std::array<float, 12> cache {};
        bool on[6] {};
    };

    class Transient : public Effect
    {
    public:
        void prepare (double sampleRate) override   { sr = sampleRate; reset(); }
        void reset() override                        { fastA = slowA = fastS = slowS = det = 0.0f; }

        void process (float* L, float* R, int n, const float* p, const Context&) override
        {
            const float speed = 1.5f - p[2];   // 0.5 (veloce) .. 1.5 (lento)
            auto c = [this] (float ms) { return 1.0f - std::exp (-1.0f / (ms * 0.001f * (float) sr)); };
            const float aFast = c (0.6f * speed), aSlow = c (25.0f * speed), relLong = c (400.0f);
            const float relFast = c (40.0f * speed), relSlow = c (350.0f * speed), att = c (1.0f);
            const float detAtt = c (0.3f), detRel = c (12.0f);
            const float out = juce::Decibels::decibelsToGain (p[3]);
            for (int i = 0; i < n; ++i)
            {
                const float in = juce::jmax (std::abs (L[i]), std::abs (R[i]));
                det += (in - det) * (in > det ? detAtt : detRel);   // livello liscio (niente "rumore" nel rumore)
                const float x = det + 1.0e-6f;
                // attacco: inseguitore veloce contro lento (salgono a velocita' diverse)
                fastA += (x - fastA) * (x > fastA ? aFast : relLong);
                slowA += (x - slowA) * (x > slowA ? aSlow : relLong);
                // coda: stessa salita, discese diverse
                fastS += (x - fastS) * (x > fastS ? att : relFast);
                slowS += (x - slowS) * (x > slowS ? att : relSlow);
                const float atkDb = juce::jlimit (0.0f, 24.0f, 20.0f * std::log10 (fastA / slowA));
                const float susDb = juce::jlimit (0.0f, 24.0f, 20.0f * std::log10 (slowS / fastS));
                const float gDb = juce::jlimit (-30.0f, 12.0f, p[0] * atkDb * 1.2f + p[1] * susDb * 0.9f);
                const float g = juce::Decibels::decibelsToGain (gDb) * out;
                L[i] *= g; R[i] *= g;
            }
        }

    private:
        double sr = 48000.0;
        float fastA = 0, slowA = 0, fastS = 0, slowS = 0, det = 0;
    };

    class Exciter : public Effect
    {
    public:
        void prepare (double sampleRate) override   { sr = sampleRate; reset(); }
        void reset() override                        { hp1.reset(); hp2.reset(); lastF = -1.0f; }

        void process (float* L, float* R, int n, const float* p, const Context&) override
        {
            if (p[1] != lastF)
            {
                lastF = p[1];
                setPass (hp1, sr, p[1], true);
                setPass (hp2, sr, p[1] * 0.8, true);
            }
            const float amount = p[0], drive = 2.0f + amount * 12.0f;
            const bool bright = p[2] > 0.5f;
            const float width = p[3] * 2.0f;
            for (int i = 0; i < n; ++i)
            {
                float e[2];
                for (int ch = 0; ch < 2; ++ch)
                {
                    const float h = hp1.process (ch == 0 ? L[i] : R[i], ch) * drive;
                    const float s = bright ? std::tanh (h + 0.45f * h * std::abs (h)) : std::tanh (h);
                    e[ch] = hp2.process (s / drive * 2.0f, ch);
                }
                const float m = 0.5f * (e[0] + e[1]), sd = 0.5f * (e[0] - e[1]) * width;
                L[i] += amount * (m + sd);
                R[i] += amount * (m - sd);
            }
        }

    private:
        double sr = 48000.0;
        Biquad hp1, hp2;
        float lastF = -1.0f;
    };

    /** Rumore bianco veloce (per vinile e simili): il thread audio non usa juce::Random condiviso. */
    struct Noise
    {
        juce::uint32 s = 0x12345678u;
        float next() noexcept    { s = s * 1664525u + 1013904223u; return (float) (s >> 8) * (2.0f / 16777216.0f) - 1.0f; }
        float uni() noexcept     { return 0.5f + 0.5f * next(); }
    };

    class Vinyl : public Effect
    {
    public:
        void prepare (double sampleRate) override   { sr = sampleRate; line.prepare ((int) (sr * 0.03) + 8); reset(); }
        void reset() override
        {
            line.clear(); wow = flutter = 0.0;
            for (auto& v : lp1) v = 0.0f;
            for (auto& v : lp2) v = 0.0f;
            for (auto& v : hs) v = 0.0f;
            for (auto& v : click) v = 0.0f;
            for (auto& v : hissLp) v = 0.0f;
            hp.reset(); lastHp = -1.0f;
        }

        void process (float* L, float* R, int n, const float* p, const Context&) override
        {
            const float hiss = p[0] * p[0] * 0.035f, crackle = p[1], tape = p[2], age = p[4], mix = p[5];
            const float tone = 1.0f - std::exp (-juce::MathConstants<float>::twoPi * juce::jmin (p[3], (float) sr * 0.45f) / (float) sr);
            const float hpF = 25.0f + age * 140.0f;
            if (hpF != lastHp) { lastHp = hpF; setPass (hp, sr, hpF, true); }
            const float clickRate = crackle * crackle * 60.0f / (float) sr;   // fino a ~60 al secondo
            const float base = 0.006f * (float) sr;
            const float drive = 1.0f + age * 3.0f, driveNorm = 1.0f / std::tanh (drive);
            for (int i = 0; i < n; ++i)
            {
                wow += 0.55 / sr;     if (wow >= 1.0) wow -= 1.0;
                flutter += 6.3 / sr;  if (flutter >= 1.0) flutter -= 1.0;
                const float mod = tape * (0.0022f * perf::fastSin ((float) wow) + 0.00018f * perf::fastSin ((float) flutter)) * (float) sr;
                line.write (L[i], R[i]);
                float w[2];
                for (int ch = 0; ch < 2; ++ch)
                {
                    const float dry = ch == 0 ? L[i] : R[i];
                    float y = tape > 0.001f ? line.read (ch, base + mod) : dry;
                    // eta': saturazione e meno stereo
                    y = std::tanh (y * drive) * driveNorm;
                    // tono: due poli
                    lp1[ch] += (y - lp1[ch]) * tone;
                    lp2[ch] += (lp1[ch] - lp2[ch]) * tone;
                    y = hp.process (lp2[ch], ch);
                    // fruscio
                    const float nz = noise.next();
                    hissLp[ch] += (nz - hissLp[ch]) * 0.35f;
                    y += hissLp[ch] * hiss;
                    // crepitio: piccoli impulsi che si spengono subito
                    if (noise.uni() < clickRate)
                        click[ch] = (noise.next() > 0.0f ? 1.0f : -1.0f) * (0.05f + 0.35f * noise.uni()) * (0.4f + 0.6f * crackle);
                    y += click[ch];
                    click[ch] *= 0.55f;
                    w[ch] = y;
                }
                const float m = 0.5f * (w[0] + w[1]), s = 0.5f * (w[0] - w[1]) * (1.0f - age * 0.6f);
                L[i] += mix * (m + s - L[i]);
                R[i] += mix * (m - s - R[i]);
            }
        }

    private:
        double sr = 48000.0, wow = 0.0, flutter = 0.0;
        DelayLine line;
        Biquad hp;
        float lastHp = -1.0f;
        float lp1[2] {}, lp2[2] {}, hs[2] {}, click[2] {}, hissLp[2] {};
        Noise noise;
    };

    class Limiter : public Effect
    {
    public:
        void prepare (double sampleRate) override
        {
            sr = sampleRate;
            look = juce::jmax (1, (int) (0.0015 * sr));
            line.prepare (look + 4);
            reset();
        }
        void reset() override    { line.clear(); env = 1.0f; smooth = 1.0f; }

        void process (float* L, float* R, int n, const float* p, const Context&) override
        {
            const float in = juce::Decibels::decibelsToGain (p[0]);
            const float ceiling = juce::Decibels::decibelsToGain (p[1]);
            const float rel = 1.0f - std::exp (-1.0f / (p[2] * 0.001f * (float) sr));
            const float att = 1.0f - std::exp (-1.0f / ((float) look / 2.5f));
            for (int i = 0; i < n; ++i)
            {
                const float l = L[i] * in, r = R[i] * in;
                const float peak = juce::jmax (std::abs (l), std::abs (r));
                const float need = peak > ceiling ? ceiling / peak : 1.0f;
                env = need < env ? need : env + (need - env) * rel;
                smooth += (env - smooth) * (env < smooth ? att : 1.0f);
                line.write (l, r);
                const float g = smooth;
                L[i] = juce::jlimit (-ceiling, ceiling, line.read (0, (float) look) * g);
                R[i] = juce::jlimit (-ceiling, ceiling, line.read (1, (float) look) * g);
            }
        }

        int getLookahead() const   { return look; }

    private:
        double sr = 48000.0;
        int look = 64;
        DelayLine line;
        float env = 1.0f, smooth = 1.0f;
    };

    /** Pitch shifter a due "testine" che scorrono nella linea di ritardo e si scambiano in dissolvenza. */
    struct PitchVoice
    {
        double phase = 0.0;
        /** ratio = 2^(semitoni/12), window = grana in campioni. Ritorna i due canali spostati. */
        inline void process (const DelayLine& line, float ratio, float window, float& outL, float& outR) noexcept
        {
            phase += (1.0 - ratio) / window;
            phase -= std::floor (phase);
            const float p1 = (float) phase, p2 = frac01 (phase + 0.5);
            const float d1 = 2.0f + p1 * window, d2 = 2.0f + p2 * window;
            const float g1 = std::sin (juce::MathConstants<float>::pi * p1), g2 = std::sin (juce::MathConstants<float>::pi * p2);
            const float w1 = g1 * g1, w2 = g2 * g2;   // somma = 1
            outL = line.read (0, d1) * w1 + line.read (0, d2) * w2;
            outR = line.read (1, d1) * w1 + line.read (1, d2) * w2;
        }
    };

    class PitchShift : public Effect
    {
    public:
        void prepare (double sampleRate) override   { sr = sampleRate; line.prepare ((int) (sr * 0.13) + 8); reset(); }
        void reset() override                        { line.clear(); main = {}; harm = {}; }

        void process (float* L, float* R, int n, const float* p, const Context&) override
        {
            static constexpr float harmSemis[4] { 0.0f, -12.0f, 12.0f, 7.0f };
            const int h = juce::jlimit (0, 3, juce::roundToInt (p[1]));
            const float ratio = std::pow (2.0f, std::round (p[0]) / 12.0f);
            const float hRatio = std::pow (2.0f, harmSemis[h] / 12.0f);
            const float window = p[2] * 0.001f * (float) sr, mix = p[3];
            for (int i = 0; i < n; ++i)
            {
                line.write (L[i], R[i]);
                float l, r;
                main.process (line, ratio, window, l, r);
                if (h > 0)
                {
                    float hl, hr;
                    harm.process (line, hRatio, window, hl, hr);
                    l = (l + hl) * 0.7f; r = (r + hr) * 0.7f;
                }
                L[i] += mix * (l - L[i]);
                R[i] += mix * (r - R[i]);
            }
        }

    private:
        double sr = 48000.0;
        DelayLine line;
        PitchVoice main, harm;
    };

    /** Trasformata di Hilbert a due catene di passa-tutto (coefficienti di Olli Niemitalo): due uscite a 90 gradi. */
    struct Hilbert
    {
        static constexpr double k1[4] { 0.6923878, 0.9360654322959, 0.9882295226860, 0.9987488452737 };
        static constexpr double k2[4] { 0.4021921162426, 0.8561710882420, 0.9722909545651, 0.9952884791278 };
        float x1[4][2] {}, y1[4][2] {}, x2[4][2] {}, y2[4][2] {};   // [stadio][campione -1, -2]
        float delayed = 0.0f;

        void reset()    { *this = {}; }

        static float stage (float a2, float x, float (&xs)[2], float (&ys)[2]) noexcept
        {
            const float y = a2 * (x + ys[1]) - xs[1];
            xs[1] = xs[0]; xs[0] = x;
            ys[1] = ys[0]; ys[0] = y;
            return y;
        }

        void process (float x, float& re, float& im) noexcept
        {
            float a = x, b = x;
            for (int s = 0; s < 4; ++s)
            {
                a = stage ((float) (k1[s] * k1[s]), a, x1[s], y1[s]);
                b = stage ((float) (k2[s] * k2[s]), b, x2[s], y2[s]);
            }
            re = delayed; delayed = a;
            im = b;
        }
    };

    class RingMod : public Effect
    {
    public:
        void prepare (double sampleRate) override   { sr = sampleRate; reset(); }
        void reset() override                        { carrier = 0.0; lfo = 0.0; hil[0].reset(); hil[1].reset(); }

        void process (float* L, float* R, int n, const float* p, const Context&) override
        {
            const bool shifter = p[0] > 0.5f;
            const float mix = p[4];
            for (int i = 0; i < n; ++i)
            {
                lfo += p[3] / sr; if (lfo >= 1.0) lfo -= 1.0;
                const double f = p[1] * std::pow (2.0, (double) (p[2] * 2.0f * perf::fastSin ((float) lfo)));
                carrier += f / sr; carrier -= std::floor (carrier);
                const float s = perf::fastSin ((float) carrier), c = perf::fastSin ((float) carrier + 0.25f);
                for (int ch = 0; ch < 2; ++ch)
                {
                    float& x = ch == 0 ? L[i] : R[i];
                    float y;
                    if (shifter)
                    {
                        float re, im;
                        hil[ch].process (x, re, im);
                        y = re * c - im * s;
                    }
                    else
                        y = x * (ch == 0 ? s : perf::fastSin ((float) carrier + 0.03f));
                    x += mix * (y - x);
                }
            }
        }

    private:
        double sr = 48000.0, carrier = 0.0, lfo = 0.0;
        Hilbert hil[2];
    };

    class Shimmer : public Effect
    {
    public:
        void prepare (double sampleRate) override
        {
            sr = sampleRate;
            reverb.setSampleRate (sr);
            line.prepare ((int) (sr * 0.1) + 8);
            reset();
        }
        void reset() override
        {
            reverb.reset(); line.clear(); voice = {};
            for (auto& b : back) std::fill (std::begin (b), std::end (b), 0.0f);
            hpz[0] = hpz[1] = hpx[0] = hpx[1] = 0.0f;
        }

        void process (float* L, float* R, int n, const float* p, const Context&) override
        {
            juce::Reverb::Parameters rp;
            rp.roomSize = 0.55f + p[0] * 0.43f; rp.damping = p[2]; rp.width = 1.0f;
            rp.wetLevel = 1.0f; rp.dryLevel = 0.0f;
            if (rp.roomSize != last.roomSize || rp.damping != last.damping)
            {
                reverb.setParameters (rp);
                last = rp;
            }
            const float shimmer = p[1] * 0.55f, mix = p[3];
            const float window = 0.06f * (float) sr;
            const float hpc = 1.0f - std::exp (-juce::MathConstants<float>::twoPi * 250.0f / (float) sr);
            for (int start = 0; start < n; start += chunk)
            {
                const int m = juce::jmin (chunk, n - start);
                float wl[chunk], wr[chunk];
                for (int i = 0; i < m; ++i)
                {
                    wl[i] = (L[start + i] + back[0][i] * shimmer) * 0.5f;
                    wr[i] = (R[start + i] + back[1][i] * shimmer) * 0.5f;
                }
                reverb.processStereo (wl, wr, m);
                // la coda passa dal pitch shifter (+1 ottava) e torna dentro al prossimo pezzo
                for (int i = 0; i < m; ++i)
                {
                    line.write (wl[i], wr[i]);
                    float ul, ur;
                    voice.process (line, 2.0f, window, ul, ur);
                    hpz[0] += (ul - hpz[0]) * hpc;   // via i bassi dal giro
                    hpz[1] += (ur - hpz[1]) * hpc;
                    back[0][i] = std::tanh (ul - hpz[0]);
                    back[1][i] = std::tanh (ur - hpz[1]);
                    L[start + i] = L[start + i] * (1.0f - mix * 0.5f) + wl[i] * mix * 1.4f;
                    R[start + i] = R[start + i] * (1.0f - mix * 0.5f) + wr[i] * mix * 1.4f;
                }
                for (int i = m; i < chunk; ++i) back[0][i] = back[1][i] = 0.0f;
            }
        }

    private:
        static constexpr int chunk = 32;
        double sr = 48000.0;
        juce::Reverb reverb;
        juce::Reverb::Parameters last;
        DelayLine line;
        PitchVoice voice;
        float back[2][chunk] {};
        float hpz[2] {}, hpx[2] {};
    };

    inline std::unique_ptr<Effect> createEffect (int type)
    {
        switch (type)
        {
            case compressor: return std::make_unique<Compressor>();
            case saturation: return std::make_unique<Saturation>();
            case filter:     return std::make_unique<Filter>();
            case delay:      return std::make_unique<Delay>();
            case reverb:     return std::make_unique<Reverb>();
            case chorus:     return std::make_unique<Chorus>();
            case bitcrusher: return std::make_unique<Bitcrusher>();
            case sidechain:  return std::make_unique<Sidechain>();
            case stereo:     return std::make_unique<Stereo>();
            case ott:        return std::make_unique<Ott>();
            case glitch:     return std::make_unique<Glitch>();
            case trash:      return std::make_unique<Trash>();
            case vowel:      return std::make_unique<Vowel>();
            case gate:       return std::make_unique<Gate>();
            case tremolo:    return std::make_unique<Tremolo>();
            case phaser:     return std::make_unique<Phaser>();
            case flanger:    return std::make_unique<Flanger>();
            case peq:        return std::make_unique<ParamEq>();
            case transient:  return std::make_unique<Transient>();
            case exciter:    return std::make_unique<Exciter>();
            case vinyl:      return std::make_unique<Vinyl>();
            case limiter:    return std::make_unique<Limiter>();
            case pitch:      return std::make_unique<PitchShift>();
            case ringmod:    return std::make_unique<RingMod>();
            case shimmer:    return std::make_unique<Shimmer>();
            default:         return nullptr;
        }
    }
}
