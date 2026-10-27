#pragma once

#include <JuceHeader.h>
#include <vector>

//==============================================================================
/**
    I SUONI del riser automatico: ogni ingrediente ha diversi "caratteri" e qualche regolazione.
    Un preset sceglie il carattere di ogni ingrediente; "Casuale" ne inventa uno nuovo (da un numero, ripetibile).
    Quali ingredienti suonano lo decidono comunque i pulsanti della finestra (rumore, nota, rullata, impatto).
*/
namespace riser
{
    struct Sound
    {
        // Rumore: 0 sweep (passa-banda che sale), 1 whoosh (passa-alto), 2 jet (phaser), 3 vento
        int noiseType = 0;
        float noiseFrom = 250.0f, noiseOctaves = 5.2f, noiseQ = 1.2f, noiseGain = 0.9f;
        // Nota: 0 supersaw, 1 quadra PWM, 2 sirena, 3 arpeggio, 4 hoover, 5 laser FM
        int toneType = 0;
        float rise = 24.0f, curve = 1.6f, toneGain = 1.0f, detune = 0.12f;
        int octave = 0;           // -1, 0, +1
        bool stutter = false;     // la nota a scatti, sempre piu' veloci
        // Rullata: 0 rullante, 1 clap, 2 cassa, 3 hi-hat, 4 tom
        int rollType = 0, rollFrom = 0, rollTo = 3;   // divisioni: 0 = 1/4, 1 = 1/8, 2 = 1/16, 3 = 1/32
        float rollStart = 0.0f;   // da che punto della salita parte (0 = subito, 0.5 = a meta')
        float rollPitch = 7.0f, rollGain = 1.0f;
        // Impatto: 0 boom, 1 crash, 2 downlifter, 3 sub che scende, 4 boom + crash
        int impactType = 0;
        // Extra
        bool gap = false;         // mezzo quarto di silenzio prima del drop
        bool pump = false;        // sidechain a ogni quarto
        bool reverseCymbal = false;
        int seed = 1234;
    };

    struct NamedSound { const char* name; Sound sound; };

    inline const std::vector<NamedSound>& soundPresets()
    {
        auto s = [] (std::function<void (Sound&)> f) { Sound x; f (x); return x; };
        static const std::vector<NamedSound> list
        {
            { "Classico EDM", Sound {} },
            { "Big Room", s ([] (Sound& x) { x.toneType = 0; x.rise = 12; x.curve = 2.2f; x.rollType = 2; x.rollFrom = 1; x.rollTo = 3; x.rollStart = 0.5f;
                                             x.impactType = 4; x.gap = true; x.noiseType = 1; x.seed = 11; }) },
            { "Electro 2010", s ([] (Sound& x) { x.toneType = 1; x.rise = 12; x.stutter = true; x.rollType = 1; x.impactType = 1; x.noiseType = 0;
                                                 x.noiseQ = 2.0f; x.seed = 21; }) },
            { "Complextro Laser", s ([] (Sound& x) { x.toneType = 5; x.rise = 36; x.curve = 2.5f; x.rollType = 3; x.rollFrom = 2; x.rollTo = 3;
                                                     x.impactType = 2; x.noiseType = 2; x.seed = 31; }) },
            { "Dubstep Sirena", s ([] (Sound& x) { x.toneType = 2; x.rise = 19; x.octave = 1; x.rollType = 0; x.impactType = 3; x.gap = true;
                                                   x.noiseType = 3; x.seed = 41; }) },
            { "Hoover Rave", s ([] (Sound& x) { x.toneType = 4; x.rise = 12; x.curve = 1.2f; x.octave = -1; x.rollType = 0; x.rollPitch = 12;
                                                x.impactType = 1; x.noiseType = 0; x.seed = 51; }) },
            { "Whoosh di rumore", s ([] (Sound& x) { x.noiseType = 1; x.noiseOctaves = 6.5f; x.toneGain = 0.0f; x.rollType = 3; x.rollStart = 0.5f;
                                                     x.impactType = 1; x.reverseCymbal = true; x.seed = 61; }) },
            { "Arpeggio che accelera", s ([] (Sound& x) { x.toneType = 3; x.rise = 12; x.rollType = 3; x.rollFrom = 1; x.impactType = 0; x.noiseGain = 0.5f; x.seed = 71; }) },
            { "Pompa sidechain", s ([] (Sound& x) { x.toneType = 0; x.rise = 12; x.pump = true; x.rollType = 2; x.rollTo = 2; x.impactType = 4; x.seed = 81; }) },
            { "Vento scuro", s ([] (Sound& x) { x.noiseType = 3; x.noiseFrom = 120; x.toneType = 2; x.octave = -1; x.rise = 7; x.toneGain = 0.6f;
                                                x.rollType = 4; x.rollFrom = 0; x.rollTo = 2; x.impactType = 3; x.seed = 91; }) },
            { "Fidget a scatti", s ([] (Sound& x) { x.toneType = 1; x.stutter = true; x.rise = 24; x.curve = 1.0f; x.rollType = 1; x.rollFrom = 1;
                                                    x.impactType = 2; x.gap = true; x.noiseType = 2; x.seed = 101; }) },
            { "Piatto al contrario", s ([] (Sound& x) { x.reverseCymbal = true; x.noiseType = 1; x.toneGain = 0.5f; x.toneType = 2; x.rise = 12;
                                                        x.rollType = 0; x.rollStart = 0.75f; x.impactType = 0; x.seed = 111; }) },
            { "Tensione lenta", s ([] (Sound& x) { x.curve = 3.0f; x.toneType = 0; x.rise = 24; x.rollFrom = 1; x.rollTo = 2; x.rollStart = 0.25f;
                                                   x.impactType = 1; x.noiseOctaves = 4.5f; x.seed = 121; }) },
            { "Hardstyle cassa", s ([] (Sound& x) { x.rollType = 2; x.rollFrom = 0; x.rollTo = 3; x.rollPitch = 12; x.toneType = 4; x.rise = 12;
                                                    x.gap = true; x.impactType = 3; x.seed = 131; }) },
            { "Trance supersaw", s ([] (Sound& x) { x.toneType = 0; x.detune = 0.25f; x.rise = 12; x.octave = 1; x.rollType = 0; x.rollFrom = 1;
                                                    x.impactType = 1; x.reverseCymbal = true; x.noiseType = 0; x.seed = 141; }) },
            { "Jet e laser", s ([] (Sound& x) { x.noiseType = 2; x.noiseQ = 3.0f; x.toneType = 5; x.rise = 24; x.rollType = 3; x.rollFrom = 2;
                                                x.impactType = 2; x.pump = true; x.seed = 151; }) },
        };
        return list;
    }

    /** Un suono inventato: ogni ingrediente pesca un carattere e delle regolazioni sensate. */
    inline Sound randomSound (int seed)
    {
        juce::Random r ((juce::int64) seed * 7919 + 17);
        Sound x;
        x.seed = seed;
        x.noiseType = r.nextInt (4);
        x.noiseFrom = 120.0f + r.nextFloat() * 400.0f;
        x.noiseOctaves = 3.8f + r.nextFloat() * 2.8f;
        x.noiseQ = 0.8f + r.nextFloat() * 2.5f;
        x.noiseGain = 0.5f + r.nextFloat() * 0.5f;
        x.toneType = r.nextInt (6);
        static constexpr float rises[] { 7.0f, 12.0f, 12.0f, 19.0f, 24.0f, 24.0f, 36.0f };
        x.rise = rises[r.nextInt (7)];
        x.curve = 1.0f + r.nextFloat() * 2.0f;
        x.toneGain = r.nextFloat() < 0.15f ? 0.5f : 1.0f;
        x.detune = 0.06f + r.nextFloat() * 0.22f;
        x.octave = r.nextInt (3) - 1;
        x.stutter = r.nextFloat() < 0.3f;
        x.rollType = r.nextInt (5);
        x.rollFrom = r.nextInt (2);
        x.rollTo = 2 + r.nextInt (2);
        static constexpr float starts[] { 0.0f, 0.0f, 0.25f, 0.5f, 0.75f };
        x.rollStart = starts[r.nextInt (5)];
        x.rollPitch = (float) r.nextInt (13);
        x.impactType = r.nextInt (5);
        x.gap = r.nextFloat() < 0.35f;
        x.pump = r.nextFloat() < 0.25f;
        x.reverseCymbal = r.nextFloat() < 0.3f;
        return x;
    }

    namespace detail
    {
        struct Svf
        {
            double s1 = 0, s2 = 0;
            double run (double x, double fc, double q, double sr, int mode)   // 0 LP, 1 BP, 2 HP
            {
                const double g = std::tan (juce::MathConstants<double>::pi * juce::jlimit (20.0, sr * 0.45, fc) / sr), k = 1.0 / juce::jmax (0.3, q);
                const double a1 = 1.0 / (1.0 + g * (g + k)), a2 = g * a1, a3 = g * a2;
                const double v3 = x - s2, v1 = a1 * s1 + a2 * v3, v2 = s2 + a2 * s1 + a3 * v3;
                s1 = 2 * v1 - s1; s2 = 2 * v2 - s2;
                return mode == 0 ? v2 : mode == 1 ? v1 * k : x - k * v1 - v2;
            }
        };

        inline double mtof (double m) { return 440.0 * std::pow (2.0, (m - 69.0) / 12.0); }
        inline double noise (juce::Random& r) { return r.nextFloat() * 2.0f - 1.0f; }

        /** Rumore che sale (T campioni, x = 0..1 lungo la salita). */
        inline void noiseLayer (float* y, int T, double sr, const Sound& s, juce::Random& rnd)
        {
            Svf f1, f2;
            std::vector<float> comb ((size_t) (0.02 * sr) + 4, 0.0f);
            size_t w = 0;
            double lfo = 0.0;
            for (int i = 0; i < T; ++i)
            {
                const double x = (double) i / T;
                const double fc = s.noiseFrom * std::pow (2.0, x * s.noiseOctaves);
                const double n = noise (rnd);
                double v = 0.0, amp = 0.08 + 0.9 * x * x;
                switch (s.noiseType)
                {
                    case 1:   // whoosh: passa-alto che sale, aria piena
                        v = f2.run (f1.run (n, fc * 0.5, 0.7, sr, 2), 14000.0, 0.7, sr, 0) * 0.8;
                        amp = 0.05 + 0.95 * std::pow (x, 1.6);
                        break;
                    case 2:   // jet: filtro a pettine con ritardo che si accorcia (effetto phaser/aereo)
                    {
                        const double delay = (0.012 * std::pow (0.06, x)) * sr;
                        const size_t len = comb.size();
                        double rp = (double) w - delay;
                        while (rp < 0) rp += (double) len;
                        const size_t i0 = (size_t) rp % len, i1 = (i0 + 1) % len;
                        const double fr = rp - std::floor (rp);
                        const double d = comb[i0] + fr * (comb[i1] - comb[i0]);
                        const double in = f1.run (n, 9000.0, 0.7, sr, 0);
                        const double out = in + d * juce::jlimit (0.3, 0.92, 0.55 + s.noiseQ * 0.1);
                        comb[w] = (float) out;
                        w = (w + 1) % len;
                        v = out * 0.35;
                        break;
                    }
                    case 3:   // vento: passa-banda che vaga lentamente e sale
                    {
                        lfo += (0.2 + 1.5 * x) / sr;
                        const double wander = std::pow (2.0, std::sin (juce::MathConstants<double>::twoPi * lfo) * 0.8);
                        v = f2.run (f1.run (n, fc * wander, s.noiseQ * 1.5, sr, 1), 3000.0 + 8000.0 * x, 0.7, sr, 0) * 1.3;
                        amp = 0.2 + 0.8 * x;
                        break;
                    }
                    default:  // sweep: due passa-banda che salgono
                        v = f2.run (f1.run (n, fc, s.noiseQ, sr, 1), fc * 1.5, 0.8, sr, 1);
                        break;
                }
                y[i] += (float) (v * amp * s.noiseGain);
            }
        }

        /** Nota che sale (dalla tonica del progetto). */
        inline void toneLayer (float* y, int T, double sr, double bpm, const Sound& s, int rootPitchClass, juce::Random& rnd)
        {
            constexpr double twoPi = juce::MathConstants<double>::twoPi;
            const double base = 48 + 12 * s.octave + ((rootPitchClass % 12) + 12) % 12;
            Svf lp;
            double ph[6] {};
            for (auto& p : ph) p = rnd.nextDouble();
            double pwm = 0.0, vib = 0.0, arpPos = 0.0, env = 0.0, mod = 0.0;
            int arpIdx = 0;
            const double beat = 60.0 / bpm * sr;
            static constexpr int triad[] { 0, 7, 12, 4, 7, 16, 12, 19 };
            for (int i = 0; i < T; ++i)
            {
                const double x = (double) i / T;
                const double pitch = base + s.rise * std::pow (x, s.curve);
                double v = 0.0;
                switch (s.toneType)
                {
                    case 1:   // quadra con PWM che accelera
                    {
                        pwm += (0.3 + 6.0 * x) / sr;
                        const double width = 0.5 - 0.4 * (0.5 + 0.5 * std::sin (twoPi * pwm));
                        for (int u = 0; u < 2; ++u)
                        {
                            ph[u] += mtof (pitch + (u == 0 ? -s.detune : s.detune)) / sr; ph[u] -= std::floor (ph[u]);
                            v += ph[u] < width ? 0.5 : -0.5;
                        }
                        v = lp.run (v * 0.6, 600.0 * std::pow (2.0, x * 4.0), 1.0, sr, 0);
                        break;
                    }
                    case 2:   // sirena: sinusoide + ottava con vibrato sempre piu' veloce
                    {
                        vib += (2.0 + 10.0 * x * x) / sr;
                        const double p2 = pitch + std::sin (twoPi * vib) * (0.3 + 0.7 * x);
                        ph[0] += mtof (p2) / sr; ph[0] -= std::floor (ph[0]);
                        ph[1] += mtof (p2 + 12.0) / sr; ph[1] -= std::floor (ph[1]);
                        v = std::sin (twoPi * ph[0]) * 0.7 + std::sin (twoPi * ph[1]) * 0.25;
                        v = std::tanh (v * (1.0 + 1.5 * x));
                        break;
                    }
                    case 3:   // arpeggio: note della triade, sempre piu' veloci
                    {
                        const double rate = beat / (2.0 * std::pow (2.0, std::floor (x * 3.0)));   // 1/8 -> 1/16 -> 1/32
                        arpPos += 1.0;
                        if (arpPos >= rate) { arpPos -= rate; arpIdx = (arpIdx + 1) % 8; env = 1.0; }
                        env *= std::exp (-1.0 / (0.12 * sr));
                        const double p2 = pitch + triad[arpIdx];
                        for (int u = 0; u < 3; ++u)
                        {
                            ph[u] += mtof (p2 + (u - 1) * s.detune) / sr; ph[u] -= std::floor (ph[u]);
                            v += 2.0 * ph[u] - 1.0;
                        }
                        v = lp.run (v * 0.3 * (0.25 + 0.75 * env), 900.0 + 7000.0 * env * (0.3 + 0.7 * x), 0.9, sr, 0) * 1.4;
                        break;
                    }
                    case 4:   // hoover: saw + quadra PWM + ottava sotto, con "scoop"
                    {
                        pwm += 0.9 / sr;
                        const double width = 0.5 - 0.35 * (0.5 + 0.5 * std::sin (twoPi * pwm));
                        for (int u = 0; u < 3; ++u)
                        {
                            ph[u] += mtof (pitch + (u - 1) * s.detune * 2.0) / sr; ph[u] -= std::floor (ph[u]);
                            v += (2.0 * ph[u] - 1.0) * 0.4 + (ph[u] < width ? 0.3 : -0.3);
                        }
                        ph[3] += mtof (pitch - 12.0) / sr; ph[3] -= std::floor (ph[3]);
                        v += (2.0 * ph[3] - 1.0) * 0.4;
                        v = lp.run (v * 0.35, 1500.0 * std::pow (2.0, x * 3.0), 0.8, sr, 0);
                        break;
                    }
                    case 5:   // laser FM: indice che cresce, salita piu' cattiva
                    {
                        const double f = mtof (pitch);
                        mod += f * 2.01 / sr; mod -= std::floor (mod);
                        ph[0] += f / sr; ph[0] -= std::floor (ph[0]);
                        v = std::sin (twoPi * ph[0] + (0.5 + 5.0 * x) * std::sin (twoPi * mod)) * 0.7;
                        break;
                    }
                    default:  // supersaw
                    {
                        for (int u = 0; u < 5; ++u)
                        {
                            ph[u] += mtof (pitch + (u - 2) * s.detune) / sr; ph[u] -= std::floor (ph[u]);
                            v += 2.0 * ph[u] - 1.0;
                        }
                        v = lp.run (v * 0.2, 400.0 * std::pow (2.0, x * 4.5), 0.9, sr, 0);
                        break;
                    }
                }
                if (s.stutter)   // a scatti: 1/8 -> 1/16 -> 1/32
                {
                    const double div = beat / (2.0 * std::pow (2.0, std::floor (x * 3.0)));
                    const double inSlice = std::fmod ((double) i, div) / div;
                    v *= inSlice < 0.55 ? 1.0 : 0.0;
                }
                y[i] += (float) (v * (0.15 + 0.45 * x) * s.toneGain);
            }
        }

        /** Un colpo della rullata (vel 0..1, pitch in semitoni sopra il suono di base). */
        inline void rollHit (float* y, int count, double sr, int type, double vel, double semis, juce::Random& rnd)
        {
            constexpr double twoPi = juce::MathConstants<double>::twoPi;
            const double up = std::pow (2.0, semis / 12.0);
            Svf f;
            double ph = 0.0;
            const int len = juce::jmin (count, (int) (0.25 * sr));
            for (int i = 0; i < len; ++i)
            {
                const double t = (double) i / sr;
                double v = 0.0;
                switch (type)
                {
                    case 1:   // clap: tre schiocchi ravvicinati e una coda
                    {
                        const double e = (t < 0.01 ? 1.0 : t < 0.02 ? 0.8 : 0.6) * std::exp (-std::fmod (t, 0.01) * 300.0) * (t < 0.03 ? 1.0 : 0.0)
                                       + (t >= 0.02 ? std::exp (-(t - 0.02) * 22.0) * 0.6 : 0.0);
                        v = f.run (noise (rnd), 1300.0 * up, 1.2, sr, 1) * e * 1.6;
                        break;
                    }
                    case 2:   // cassa
                    {
                        const double fr = (50.0 + 90.0 * std::exp (-t * 30.0)) * up;
                        ph += fr / sr;
                        v = std::sin (twoPi * ph) * std::exp (-t * 12.0) + noise (rnd) * std::exp (-t * 200.0) * 0.2;
                        break;
                    }
                    case 3:   // hi-hat
                        v = f.run (noise (rnd), 7500.0, 0.8, sr, 2) * std::exp (-t * 45.0) * 1.2;
                        break;
                    case 4:   // tom
                    {
                        const double fr = (110.0 + 40.0 * std::exp (-t * 20.0)) * up;
                        ph += fr / sr;
                        v = std::sin (twoPi * ph) * std::exp (-t * 9.0) * 0.9 + f.run (noise (rnd), 2000.0, 0.8, sr, 1) * std::exp (-t * 40.0) * 0.3;
                        break;
                    }
                    default:  // rullante
                    {
                        ph += 180.0 * up / sr;
                        v = f.run (noise (rnd), 3500.0, 0.7, sr, 1) * std::exp (-t * 28.0) * 1.4 + std::sin (twoPi * ph) * std::exp (-t * 40.0) * 0.6;
                        break;
                    }
                }
                y[i] += (float) (v * vel * 0.55);
            }
        }

        /** Rullata che accelera verso la fine; con 'reverse' e' specchiata: fitta all'inizio e rallenta (downer). */
        inline void rollLayer (float* y, int T, double sr, double bpm, double totalBeats, const Sound& s, juce::Random& rnd, bool reverse = false)
        {
            static constexpr double every[4] { 1.0, 0.5, 0.25, 0.125 };
            const double beatSec = 60.0 / bpm;
            const int from = juce::jlimit (0, 3, s.rollFrom), to = juce::jlimit (from, 3, s.rollTo);
            const double startBeat = std::floor (totalBeats * juce::jlimit (0.0f, 0.95f, s.rollStart));
            const int sections = to - from + 1;
            for (double b = startBeat; b < totalBeats - 1.0e-6;)
            {
                const double x = (b - startBeat) / juce::jmax (1.0e-6, totalBeats - startBeat);
                const int section = juce::jlimit (0, sections - 1, (int) (x * sections));
                const double step = every[from + section];
                const double at = reverse ? totalBeats - b - step : b;
                const int start = (int) (juce::jmax (0.0, at) * beatSec * sr);
                if (start < T)
                    rollHit (y + start, T - start, sr, s.rollType, (0.25 + 0.75 * x) * s.rollGain, s.rollPitch * x, rnd);
                else if (! reverse)
                    break;
                b += step;
            }
        }

        inline void impactLayer (float* y, int len, double sr, int type, juce::Random& rnd)
        {
            constexpr double twoPi = juce::MathConstants<double>::twoPi;
            Svf a, b, c;
            double ph = 0.0, ph2 = 0.0;
            for (int i = 0; i < len; ++i)
            {
                const double t = (double) i / sr;
                double v = 0.0;
                const bool boom = type == 0 || type == 4, crash = type == 1 || type == 4;
                if (boom)
                {
                    ph += (30.0 + 60.0 * std::exp (-t * 6.0)) / sr;
                    v += std::sin (twoPi * ph) * std::exp (-t * 1.6) * 0.9
                       + a.run (noise (rnd), 3000.0, 0.7, sr, 0) * std::exp (-t * 9.0) * 0.8
                       + b.run (noise (rnd), 1800.0, 0.7, sr, 0) * std::exp (-t * 1.4) * 0.12;
                }
                if (crash)   // piatto: rumore acuto lungo + qualche parziale metallica
                {
                    const double n = c.run (noise (rnd), 3500.0, 0.7, sr, 2);
                    ph2 += 1.0 / sr;
                    const double metal = std::sin (twoPi * 3217.0 * ph2) * 0.15 + std::sin (twoPi * 4531.0 * ph2) * 0.1 + std::sin (twoPi * 6007.0 * ph2) * 0.08;
                    v += (n * 0.9 + metal * std::exp (-t * 3.0)) * std::exp (-t * 1.2) * (type == 4 ? 0.6 : 1.0);
                }
                if (type == 2)   // downlifter: rumore che scende e un tono che cala
                {
                    const double fc = 7000.0 * std::pow (0.03, juce::jmin (1.0, t / 2.2));
                    ph += 220.0 * std::pow (0.25, juce::jmin (1.0, t / 2.0)) / sr;
                    v += a.run (noise (rnd), fc, 1.5, sr, 1) * std::exp (-t * 0.9) * 1.2
                       + std::sin (twoPi * ph) * std::exp (-t * 1.5) * 0.35
                       + b.run (noise (rnd), 2500.0, 0.7, sr, 0) * std::exp (-t * 12.0) * 0.6;
                }
                if (type == 3)   // sub che scende lungo, con un colpo secco
                {
                    ph += (28.0 + 45.0 * std::exp (-t * 2.0)) / sr;
                    v += std::sin (twoPi * ph) * std::exp (-t * 0.9) * 1.0
                       + a.run (noise (rnd), 5000.0, 0.7, sr, 0) * std::exp (-t * 40.0) * 0.7;
                }
                y[i] += (float) v;
            }
        }

        /** Piatto al contrario che culmina sul drop (ultime 2 battute o tutta la salita se piu' corta). */
        inline void reverseCymbalLayer (float* y, int T, double sr, double bpm, juce::Random& rnd)
        {
            const int len = juce::jmin (T, (int) (8.0 * 60.0 / bpm * sr));
            Svf f;
            for (int i = 0; i < len; ++i)
            {
                const double t = (double) (len - i) / sr;   // tempo all'indietro
                const double v = f.run (noise (rnd), 4000.0, 0.7, sr, 2) * std::exp (-t * 1.3);
                y[T - len + i] += (float) (v * 0.8);
            }
        }
    }
}
