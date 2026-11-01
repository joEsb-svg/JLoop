#pragma once

#include <JuceHeader.h>
#include <array>
#include <atomic>
#include <vector>
#include "Mixer.h"

//==============================================================================
/**
    Automazioni: curve nel tempo della canzone (playlist) che muovono un controllo da sole,
    per esempio il filtro che si apre in un build-up o il volume che sfuma alla fine.

    Ogni curva ("corsia") ha un bersaglio e fino a 256 punti (tick della canzone, valore 0..1);
    tra un punto e l'altro il valore cambia in linea retta. Suonano in modalita' SONG.
    I punti sono impacchettati in 32 bit (tick 16 bit | valore 16 bit), letti senza lock dal thread audio.
*/
namespace automation
{
    inline constexpr int maxLanes  = 32;
    inline constexpr int maxPoints = 256;

    enum Kind { none = 0, master, channelParam, trackParam, effectParam, instrumentParam };

    // channelParam: a = canale, b = quale
    enum ChannelParam { chVolume = 0, chPan, chFilter, chReverb, chDelay, chPitch, numChannelParams };
    // trackParam: a = traccia del mixer, b = quale
    enum TrackParam { trVolume = 0, trPan, trEqLow, trEqMid, trEqHigh, numTrackParams };
    // effectParam: a = traccia, b = slot, c = parametro
    // instrumentParam: a = canale, b = indice del parametro dello strumento

    inline const char* channelParamName (int i)
    {
        static const char* names[] = { "Volume", "Panorama", "Filtro", "Riverbero", "Delay", "Intonazione" };
        return names[juce::jlimit (0, numChannelParams - 1, i)];
    }

    inline const char* trackParamName (int i)
    {
        static const char* names[] = { "Volume", "Panorama", "EQ bassi", "EQ medi", "EQ alti" };
        return names[juce::jlimit (0, numTrackParams - 1, i)];
    }

    struct Target
    {
        int kind = none, a = 0, b = 0, c = 0;
        bool operator== (const Target& o) const { return kind == o.kind && a == o.a && b == o.b && c == o.c; }
    };

    struct Point
    {
        int tick = 0;        // 12 tick per step, 192 per battuta
        float value = 0.5f;  // 0..1
        float curve = 0.0f;  // curvatura del tratto che parte da questo punto: -1 (veloce subito) .. 0 (dritto) .. 1 (lento all'inizio)
    };

    /** Il tratto curvo: t = 0..1 del tempo -> 0..1 della strada fatta. */
    inline float shape (float t, float curve) noexcept
    {
        if (std::abs (curve) < 0.001f) return t;
        return std::pow (t, std::exp2 (curve * 3.0f));
    }

    /** La curvatura che fa passare il tratto, a meta' del tempo, per la frazione 'mid' della strada (per la maniglia). */
    inline float curveForMid (float mid) noexcept
    {
        mid = juce::jlimit (0.01f, 0.99f, mid);
        return juce::jlimit (-1.0f, 1.0f, std::log2 (std::log (mid) / std::log (0.5f)) / 3.0f);
    }

    inline uint32_t pack (const Point& p)
    {
        return (uint32_t) juce::jlimit (0, 65535, p.tick) << 16 | (uint32_t) juce::jlimit (0, 65535, juce::roundToInt (p.value * 65535.0f));
    }

    inline Point unpack (uint32_t v)    { return { (int) (v >> 16), (float) (v & 0xffff) / 65535.0f }; }

    /** Volume (0..1 della curva) -> guadagno: scala in dB, da -40 dB a maxDb; in fondo silenzio. */
    inline float volumeGain (float v, float maxDb)
    {
        if (v < 0.01f) return 0.0f;
        return juce::Decibels::decibelsToGain (-40.0f + (maxDb + 40.0f) * v);
    }

    inline float volumeNorm (float gain, float maxDb)
    {
        if (gain <= 0.0001f) return 0.0f;
        return juce::jlimit (0.0f, 1.0f, (juce::Decibels::gainToDecibels (gain) + 40.0f) / (maxDb + 40.0f));
    }

    struct Lane
    {
        std::atomic<int> kind { none }, a { 0 }, b { 0 }, c { 0 };
        std::atomic<bool> enabled { true };
        std::atomic<int> count { 0 };
        std::array<std::atomic<uint32_t>, maxPoints> points {};
        std::array<std::atomic<float>, maxPoints> curves {};

        Target target() const   { return { kind.load(), a.load(), b.load(), c.load() }; }
        void setTarget (const Target& t)    { a.store (t.a); b.store (t.b); c.store (t.c); kind.store (t.kind); }

        /** Il valore della curva al tick indicato (thread audio; -1 = nessun punto). */
        float valueAt (double tick) const noexcept
        {
            const int n = juce::jmin (count.load(), maxPoints);
            if (n <= 0)
                return -1.0f;
            auto first = unpack (points[0].load());
            if (tick <= first.tick || n == 1)
                return first.value;
            for (int i = 1; i < n; ++i)
            {
                const auto p = unpack (points[(size_t) i].load());
                if (tick <= p.tick)
                {
                    const auto q = unpack (points[(size_t) i - 1].load());
                    const double span = juce::jmax (1, p.tick - q.tick);
                    const float t = shape ((float) ((tick - q.tick) / span), curves[(size_t) i - 1].load());
                    return q.value + (p.value - q.value) * t;
                }
            }
            return unpack (points[(size_t) n - 1].load()).value;
        }
    };

    //==========================================================================
    /** Forme e onde pronte per una corsia. */
    namespace shapes
    {
        enum Id { rampUp = 0, rampDown, upDown, rampUpCurve, rampDownCurve,
                  sine, triangle, sawUp, sawDown, square, randomSteps, randomSmooth, numShapes };

        inline const char* name (int id)
        {
            static const char* names[] { "Sale", "Scende", "Sale e scende", "Sale (curva)", "Scende (curva)",
                                         "Sinusoide", "Triangolo", "Dente di sega su", "Dente di sega giu'", "Quadra",
                                         "Casuale a scalini", "Casuale morbido" };
            return names[juce::jlimit (0, numShapes - 1, id)];
        }

        /** Le onde si ripetono a tempo; le rampe coprono tutto il tratto una volta sola. */
        inline bool isWave (int id)     { return id >= sine; }

        /** Ripetizione delle onde, in tick (192 = una battuta). */
        inline constexpr int cycleChoices[6] { 384, 192, 96, 48, 24, 12 };
        inline const char* cycleName (int i)
        {
            static const char* names[] { "2 battute", "1 battuta", "1/2", "1/4", "1/8", "1/16" };
            return names[juce::jlimit (0, 5, i)];
        }

        inline constexpr int maxShapePoints = 200;

        /** I punti della forma da 'start' per 'length' tick, tra lo e hi (0..1). Le onde troppo fitte si accorciano. */
        inline std::vector<Point> make (int id, int start, int length, int cycle, float lo, float hi, juce::uint32 seed = 1)
        {
            std::vector<Point> pts;
            const float mid = 0.5f * (lo + hi);
            auto add = [&pts] (int t, float v, float c = 0.0f) { pts.push_back ({ t, v, c }); };
            const int end = start + length;
            switch (id)
            {
                case rampUp:        add (start, lo); add (end, hi); return pts;
                case rampDown:      add (start, hi); add (end, lo); return pts;
                case upDown:        add (start, lo); add (start + length / 2, hi); add (end, lo); return pts;
                case rampUpCurve:   add (start, lo, 0.6f); add (end, hi); return pts;     // lenta all'inizio, corre alla fine
                case rampDownCurve: add (start, hi, -0.6f); add (end, lo); return pts;   // scende subito, poi piano
                default: break;
            }

            cycle = juce::jmax (4, cycle);
            const int perCycle = id == sine || id == square ? 4 : id == randomSmooth ? 1 : 2;
            const int maxCycles = juce::jmax (1, maxShapePoints / perCycle);
            const int cycles = juce::jmin (maxCycles, juce::jmax (1, length / cycle));
            const float easeIn = curveForMid (0.293f), easeOut = curveForMid (0.707f);   // un quarto di sinusoide
            juce::Random rnd ((juce::int64) seed);
            for (int k = 0; k < cycles; ++k)
            {
                const int t = start + k * cycle;
                switch (id)
                {
                    case sine:
                        add (t, lo, easeIn); add (t + cycle / 4, mid, easeOut);
                        add (t + cycle / 2, hi, easeIn); add (t + cycle * 3 / 4, mid, easeOut);
                        break;
                    case triangle:   add (t, lo); add (t + cycle / 2, hi); break;
                    case sawUp:      add (t, lo); add (t + cycle - 1, hi); break;
                    case sawDown:    add (t, hi); add (t + cycle - 1, lo); break;
                    case square:     add (t, hi); add (t + cycle / 2 - 1, hi); add (t + cycle / 2, lo); add (t + cycle - 1, lo); break;
                    case randomSteps:
                    {
                        const float v = lo + (hi - lo) * rnd.nextFloat();
                        add (t, v); add (t + cycle - 1, v);
                        break;
                    }
                    case randomSmooth: add (t, lo + (hi - lo) * rnd.nextFloat()); break;
                    default: break;
                }
            }
            // chiusura: si torna al punto di partenza alla fine dell'ultimo giro
            const int last = start + cycles * cycle;
            if (id == sine || id == triangle) add (last, lo);
            else if (id == randomSmooth) add (last, lo + (hi - lo) * rnd.nextFloat());
            return pts;
        }
    }
}
