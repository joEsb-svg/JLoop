#pragma once

#include <JuceHeader.h>
#include <cmath>
#include <vector>

//==============================================================================
/**
    Allunga o accorcia un suono SENZA cambiarne l'intonazione (WSOLA: pezzetti sovrapposti,
    ognuno spostato di poco per incastrarsi con il precedente). Pensato per le voci.
    ratio = durata nuova / durata vecchia (2 = lungo il doppio, cioe' a meta' tempo).
*/
namespace stretch
{
    inline juce::AudioBuffer<float> process (const juce::AudioBuffer<float>& in, double sampleRate, double ratio)
    {
        const int nIn = in.getNumSamples();
        ratio = juce::jlimit (0.25, 4.0, ratio);
        const int nOut = (int) std::llround (nIn * ratio);
        juce::AudioBuffer<float> out (1, juce::jmax (1, nOut));
        out.clear();
        if (nIn < 16 || std::abs (ratio - 1.0) < 1.0e-4)
        {
            out.copyFrom (0, 0, in, 0, 0, juce::jmin (nIn, nOut));
            return out;
        }

        const int N = juce::jmax (256, (int) std::round (0.040 * sampleRate)) & ~1;   // finestra di 40 ms
        const int hop = N / 2;
        const int search = (int) std::round (0.012 * sampleRate);                  // si cerca +-12 ms
        const double hopIn = hop / ratio;
        const float* x = in.getReadPointer (0);
        float* y = out.getWritePointer (0);

        std::vector<float> window ((size_t) N);
        for (int i = 0; i < N; ++i) window[(size_t) i] = 0.5f - 0.5f * std::cos (juce::MathConstants<float>::twoPi * i / N);
        std::vector<float> norm ((size_t) nOut + N, 0.0f);

        auto sampleAt = [x, nIn] (long i) { return i >= 0 && i < nIn ? x[i] : 0.0f; };
        long prevIn = 0;
        for (int k = 0;; ++k)
        {
            const long outPos = (long) k * hop;
            if (outPos >= nOut) break;
            long inPos = (long) std::llround (k * hopIn);
            if (k > 0)
            {
                // il pezzo che continuerebbe "naturalmente" quello di prima: lo si cerca vicino a inPos
                const long natural = prevIn + hop;
                double best = -1.0e30;
                long bestPos = inPos;
                for (long d = -search; d <= search; d += 2)
                {
                    const long cand = inPos + d;
                    if (cand < 0 || cand + N > nIn + N / 2) continue;
                    double c = 0.0;
                    for (int j = 0; j < hop; j += 4)
                        c += (double) sampleAt (cand + j) * sampleAt (natural + j);
                    if (c > best) { best = c; bestPos = cand; }
                }
                inPos = bestPos;
            }
            for (int j = 0; j < N && outPos + j < nOut; ++j)
            {
                y[outPos + j] += sampleAt (inPos + j) * window[(size_t) j];
                norm[(size_t) (outPos + j)] += window[(size_t) j];
            }
            prevIn = inPos;
        }
        for (int i = 0; i < nOut; ++i)
            if (norm[(size_t) i] > 0.05f) y[i] /= norm[(size_t) i];
        return out;
    }

    /** BPM originale probabile di una voce lunga 'seconds': un numero intero di battute che stia tra 75 e 180 BPM,
        il piu' vicino al tempo del progetto. */
    inline double guessBpm (double seconds, double projectBpm)
    {
        double best = projectBpm, bestDist = 1.0e9;
        for (int bars = 1; bars <= 64; ++bars)
        {
            const double bpm = bars * 4 * 60.0 / juce::jmax (0.1, seconds);
            if (bpm < 75.0 || bpm > 180.0) continue;
            const double dist = std::abs (std::log (bpm / projectBpm));
            if (dist < bestDist) { bestDist = dist; best = bpm; }
        }
        return std::round (best * 10.0) / 10.0;
    }
}
