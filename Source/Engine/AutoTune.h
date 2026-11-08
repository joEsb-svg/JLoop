#pragma once

#include <JuceHeader.h>
#include <algorithm>
#include <cmath>
#include <vector>
#include "MusicTheory.h"

//==============================================================================
/**
    AUTOTUNE per le voci (una voce sola, non accordi), fatto "offline" sul file intero.
    1. Si trova l'altezza della voce ogni ~5 ms (YIN, su una copia a 1/4 del sample rate: veloce anche sul telefono).
    2. Per ogni momento si sceglie la nota della scala piu' vicina (con un po' di isteresi: niente sfarfallio).
    3. La correzione arriva piu' o meno in fretta: Naturale (lenta, il vibrato resta), Pop, Robotico (istantanea, "T-Pain").
    4. Si risintetizza con PSOLA: si ricopiano i singoli periodi della voce piu' vicini o piu' lontani.
       Cosi' la durata non cambia e il timbro (le formanti) resta quello della voce, niente effetto "papera".
    Le parti senza intonazione (s, t, respiri, silenzio) passano invariate.
*/
namespace autotune
{
    enum Mode { off = 0, natural, pop, robotic, numModes };

    inline juce::StringArray modeNames()     { return { "Spento", "Naturale", "Pop", "Robotico" }; }

    /** Tempo per arrivare sulla nota (secondi) e quanta correzione si applica. */
    inline void modeSettings (int mode, double& retuneSeconds, float& amount)
    {
        switch (mode)
        {
            case natural: retuneSeconds = 0.14; amount = 0.85f; break;
            case pop:     retuneSeconds = 0.035; amount = 1.0f; break;
            case robotic: retuneSeconds = 0.0;  amount = 1.0f; break;
            default:      retuneSeconds = 0.0;  amount = 0.0f; break;
        }
    }

    /** La nota (MIDI) della scala piu' vicina a "midi" (scala 0 = tutte le note). */
    inline int nearestInScale (double midi, int root, int scaleType)
    {
        const int base = (int) std::floor (midi);
        int best = base;
        double bestDist = 1.0e9;
        for (int k = base - 3; k <= base + 4; ++k)
        {
            if (! theory::inScale (k, root, scaleType)) continue;
            const double d = std::abs (midi - k);
            if (d < bestDist) { bestDist = d; best = k; }
        }
        return best;
    }

    struct Frame { double midi = 0.0; bool voiced = false; };

    /** Altezza ogni "hop" campioni (al sample rate originale). midi 0 = senza intonazione. */
    inline std::vector<Frame> detect (const float* x, int n, double sr, int hop)
    {
        // copia a 1/4 (filtro passa-basso semplice: due medie mobili)
        const int dec = sr > 30000.0 ? 4 : 2;
        const double dsr = sr / dec;
        std::vector<float> d ((size_t) (n / dec + 1), 0.0f);
        {
            float s1 = 0.0f, s2 = 0.0f;
            const float a = (float) std::exp (-2.0 * juce::MathConstants<double>::pi * 1400.0 / sr);
            for (int i = 0, j = 0; i < n; ++i)
            {
                s1 = x[i] + a * (s1 - x[i]);
                s2 = s1 + a * (s2 - s1);
                if (i % dec == 0 && j < (int) d.size()) d[(size_t) j++] = s2;
            }
        }
        const int W = juce::jmax (128, (int) std::round (0.043 * dsr));   // finestra ~43 ms
        const int tauMin = (int) (dsr / 1000.0), tauMax = juce::jmin (W - 2, (int) (dsr / 70.0));
        const int dn = (int) d.size();
        std::vector<float> diff ((size_t) tauMax + 2), cmnd ((size_t) tauMax + 2);
        std::vector<Frame> frames ((size_t) (n / hop + 1));
        double globalRms = 0.0;
        for (int i = 0; i < n; ++i) globalRms += (double) x[i] * x[i];
        globalRms = std::sqrt (globalRms / juce::jmax (1, n));

        for (size_t f = 0; f < frames.size(); ++f)
        {
            const int c = (int) (f * (size_t) hop / (size_t) dec);
            const int s = c - W / 2;
            if (s < 0 || s + W + tauMax >= dn) continue;
            double e = 0.0;
            for (int i = 0; i < W; ++i) e += (double) d[(size_t) (s + i)] * d[(size_t) (s + i)];
            if (std::sqrt (e / W) < juce::jmax (0.004, globalRms * 0.12)) continue;   // troppo piano
            // YIN: differenza e media cumulativa normalizzata
            cmnd[0] = 1.0f;
            double run = 0.0;
            for (int tau = 1; tau <= tauMax; ++tau)
            {
                double acc = 0.0;
                const float* p = d.data() + s;
                for (int i = 0; i < W; ++i) { const float q = p[i] - p[i + tau]; acc += q * q; }
                diff[(size_t) tau] = (float) acc;
                run += acc;
                cmnd[(size_t) tau] = run > 0.0 ? (float) (acc * tau / run) : 1.0f;
            }
            int best = -1;
            for (int tau = juce::jmax (2, tauMin); tau < tauMax; ++tau)
                if (cmnd[(size_t) tau] < 0.15f)
                {
                    while (tau + 1 < tauMax && cmnd[(size_t) tau + 1] < cmnd[(size_t) tau]) ++tau;
                    best = tau;
                    break;
                }
            if (best < 0) continue;
            // interpolazione parabolica (meno di un campione)
            const float y0 = cmnd[(size_t) best - 1], y1 = cmnd[(size_t) best], y2 = cmnd[(size_t) best + 1];
            const float den = y0 - 2.0f * y1 + y2;
            const double t = best + (std::abs (den) > 1.0e-9f ? 0.5 * (y0 - y2) / den : 0.0);
            const double hz = dsr / t;
            frames[f].midi = 69.0 + 12.0 * std::log2 (hz / 440.0);
            frames[f].voiced = true;
        }
        // mediana su 5 (via gli errori d'ottava isolati) e niente "buchi" di un frame solo
        std::vector<Frame> out = frames;
        for (size_t f = 2; f + 2 < frames.size(); ++f)
        {
            if (! frames[f].voiced) continue;
            double v[5]; int k = 0;
            for (size_t g = f - 2; g <= f + 2; ++g) if (frames[g].voiced) v[k++] = frames[g].midi;
            std::sort (v, v + k);
            const double med = v[k / 2];
            if (std::abs (frames[f].midi - med) > 6.0) out[f].midi = med;
        }
        for (size_t f = 1; f + 1 < out.size(); ++f)
            if (! out[f].voiced && out[f - 1].voiced && out[f + 1].voiced)
                out[f] = { 0.5 * (out[f - 1].midi + out[f + 1].midi), true };
        return out;
    }

    /** Applica l'autotune. root 0..11 (Do..Si), scaleType come theory::scales() (0 = tutte le note). */
    inline juce::AudioBuffer<float> process (const juce::AudioBuffer<float>& in, double sr, int mode, int root, int scaleType)
    {
        double retune = 0.0; float amount = 0.0f;
        modeSettings (mode, retune, amount);
        const int n = in.getNumSamples();
        if (amount <= 0.0f || n < (int) (0.05 * sr)) return in;
        const float* x = in.getReadPointer (0);

        const int hop = juce::jmax (64, (int) std::round (0.005 * sr));
        const auto frames = detect (x, n, sr, hop);

        // correzione (semitoni) per ogni frame, con la velocita' scelta
        std::vector<double> shift (frames.size(), 0.0);
        {
            const double alpha = retune <= 0.0 ? 1.0 : 1.0 - std::exp (-(double) hop / (retune * sr));
            double cur = 0.0; int target = -1000;
            for (size_t f = 0; f < frames.size(); ++f)
            {
                if (! frames[f].voiced) { cur *= 0.8; target = -1000; continue; }
                const double m = frames[f].midi;
                const int near = nearestInScale (m, root, scaleType);
                // isteresi: si cambia nota solo se la voce e' davvero passata all'altra
                if (target == -1000 || std::abs (m - target) > 0.5 + (std::abs (near - target) > 1 ? 0.0 : 0.15))
                    target = near;
                const double want = (target - m) * amount;
                if (target != -1000 && (f == 0 || ! frames[f - 1].voiced)) cur = want;   // inizio di una nota: subito giusto
                else cur += (want - cur) * alpha;
                shift[f] = cur;
            }
        }

        // segni di periodo (pitch marks): sui picchi della voce dove c'e' intonazione, ogni 5 ms altrove
        struct Mark { int pos; int period; double ratio; };
        std::vector<Mark> marks;
        {
            const int unvoicedP = hop;
            int pos = 0;
            while (pos < n)
            {
                const size_t f = juce::jmin (frames.size() - 1, (size_t) (pos / hop));
                if (frames[f].voiced)
                {
                    const double hz = 440.0 * std::pow (2.0, (frames[f].midi - 69.0) / 12.0);
                    const int P = juce::jlimit (16, (int) (sr / 60.0), (int) std::round (sr / hz));
                    // il picco piu' alto nella zona dove dovrebbe stare il prossimo periodo
                    int best = pos; float bv = -1.0e9f;
                    const int a = juce::jmax (0, pos - P / 4), b = juce::jmin (n - 1, pos + P / 4);
                    const bool continuing = ! marks.empty() && marks.back().period != unvoicedP;
                    const int from = continuing ? a : pos, to = continuing ? b : juce::jmin (n - 1, pos + P);
                    for (int i = from; i <= to; ++i)
                        if (x[i] > bv) { bv = x[i]; best = i; }
                    if (! marks.empty() && best <= marks.back().pos) best = marks.back().pos + P / 2;
                    marks.push_back ({ best, P, std::pow (2.0, shift[f] / 12.0) });
                    pos = best + P;
                }
                else
                {
                    marks.push_back ({ pos, unvoicedP, 1.0 });
                    pos += unvoicedP;
                }
            }
        }
        if (marks.size() < 2) return in;

        // PSOLA: ogni "chicco" (2 periodi, finestra di Hann) va rimesso a distanza periodo/ratio
        juce::AudioBuffer<float> out (1, n);
        out.clear();
        float* y = out.getWritePointer (0);
        std::vector<float> norm ((size_t) n, 0.0f);
        double ts = marks[0].pos;
        size_t k = 0;
        while (ts < n)
        {
            while (k + 1 < marks.size() && std::abs (marks[k + 1].pos - ts) <= std::abs (marks[k].pos - ts)) ++k;
            const auto& m = marks[k];
            const int P = m.period;
            const double Ps = P / m.ratio;
            const int c = (int) std::round (ts);
            for (int j = -P; j < P; ++j)
            {
                const int si = m.pos + j, di = c + j;
                if (si < 0 || si >= n || di < 0 || di >= n) continue;
                const float w = 0.5f + 0.5f * std::cos (juce::MathConstants<float>::pi * (float) j / (float) P);
                y[di] += x[si] * w;
                norm[(size_t) di] += w;
            }
            ts += juce::jmax (8.0, Ps);
        }
        // normalizzazione per la sovrapposizione (si evita di dividere quasi per zero)
        for (int i = 0; i < n; ++i)
            y[i] /= juce::jmax (0.5f, norm[(size_t) i]);
        // i bordi (prima del primo segno) restano quelli originali
        for (int i = 0; i < juce::jmin (n, marks[0].pos); ++i) y[i] = x[i];
        return out;
    }
}
