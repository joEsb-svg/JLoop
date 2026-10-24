#pragma once

#include <JuceHeader.h>
#include "SequencerEngine.h"
#include "ProjectIO.h"
#include "SampleStore.h"
#include "../Instruments/Instruments.h"

//==============================================================================
/**
    "Congela traccia": calcola in un thread a parte l'audio di un canale synth, un giro per ogni pattern
    in cui suona. Come l'esportazione usa una copia del progetto (secondo motore, copie degli strumenti):
    l'app continua a funzionare mentre lavora.

    Per ogni pattern suona qualche giro di "riscaldamento" (code di riverbero e delay del giro prima)
    e registra l'ultimo: ripetuto in loop e' identico a quello che suonerebbe lo strumento.
*/
class ChannelFreezer : public juce::Thread
{
public:
    ChannelFreezer (const juce::var& projectJson, SampleStore& store, int channelIndex, double sampleRateToUse)
        : juce::Thread ("JLoop freeze"), channel (channelIndex), sampleRate (sampleRateToUse > 0.0 ? sampleRateToUse : 48000.0)
    {
        engine = std::make_unique<SequencerEngine>();
        ProjectIO::fromJson (*engine, projectJson);
        engine->setMetronome (false);
        engine->setSongMode (false);

        // Solo il canale da congelare: gli altri si svuotano (niente lavoro inutile)
        for (int p = 0; p < SequencerEngine::numPatterns; ++p)
        {
            engine->selectPattern (p);
            bool has = engine->hasNotes (channel);
            for (int s = 0; s < engine->getPatternLength() && ! has; ++s)
                has = engine->isStepOn (channel, s);
            used.push_back (has);
            for (int o = 0; o < engine->getNumChannels(); ++o)
                if (o != channel)
                {
                    engine->clearRow (o);
                    engine->setNotes (o, {});
                }
        }
        instruments = std::make_unique<Instruments::Host> (*engine);
        instruments->resolve();
        for (int c = 0; c < engine->getNumChannels(); ++c)
            if (c != channel && ! engine->isSynth (c))
                engine->getChannel (c).sample.store (nullptr);
        juce::ignoreUnused (store);

        result = std::make_unique<SequencerEngine::FrozenAudio>();
        result->bpm = engine->getBpm();
        result->swing = engine->getSwing();
        result->sampleRate = sampleRate;
        result->loops.resize ((size_t) SequencerEngine::numPatterns);
        result->lengths.assign ((size_t) SequencerEngine::numPatterns, 0);
        result->loopSamples.assign ((size_t) SequencerEngine::numPatterns, 0.0);
    }

    ~ChannelFreezer() override  { stopThread (10000); }

    int getChannel() const          { return channel; }
    float getProgress() const       { return progress.load(); }
    bool isFinished() const         { return finished.load(); }
    bool wasCancelled() const       { return cancelled.load(); }
    bool hasAnything() const        { return std::find (used.begin(), used.end(), true) != used.end(); }

    /** A lavoro finito (sul thread della UI): l'audio congelato, da dare al canale. */
    std::unique_ptr<SequencerEngine::FrozenAudio> takeResult()  { return std::move (result); }

    void run() override
    {
        engine->prepare (sampleRate);
        const double sps = sampleRate * 60.0 / engine->getBpm() / 4.0;
        const double sw = engine->getSwing() / 3.0;
        const int total = (int) std::count (used.begin(), used.end(), true);
        int done = 0;
        constexpr int block = 256;
        juce::AudioBuffer<float> scratch (2, block);

        auto renderSamples = [&] (juce::int64 count) -> bool
        {
            for (juce::int64 pos = 0; pos < count; pos += block)
            {
                if (threadShouldExit()) return false;
                const int n = (int) juce::jmin ((juce::int64) block, count - pos);
                scratch.clear();
                engine->process (scratch.getArrayOfWritePointers(), 2, n);
            }
            return true;
        };

        for (int p = 0; p < SequencerEngine::numPatterns; ++p)
        {
            if (! used[(size_t) p])
                continue;

            // Silenzio tra un pattern e l'altro: le code del precedente si spengono
            engine->setPlaying (false);
            if (! renderSamples ((juce::int64) (sampleRate * 3.0))) { cancelled.store (true); break; }

            engine->selectPattern (p);
            const int len = engine->getPatternLength();
            double loop = 0.0;
            for (int k = 0; k < len; ++k)
                loop += sps * ((k % 2) == 0 ? 1.0 + sw : 1.0 - sw);
            const int warm = juce::jmax (1, (int) std::ceil (2.5 * sampleRate / loop));
            const auto loopLen = (int) std::llround (loop);

            juce::AudioBuffer<float> capture (2, loopLen);
            capture.clear();
            engine->setCapture (channel, &capture, (juce::int64) std::llround (loop * warm));
            engine->setPlaying (true);
            const bool ok = renderSamples ((juce::int64) std::llround (loop * (warm + 1)));
            engine->setCapture (-1, nullptr, 0);
            if (! ok) { cancelled.store (true); break; }

            result->loops[(size_t) p] = std::move (capture);
            result->lengths[(size_t) p] = len;
            result->loopSamples[(size_t) p] = loop;
            progress.store ((float) ++done / (float) juce::jmax (1, total));
        }
        engine->setPlaying (false);
        finished.store (true);
    }

private:
    const int channel;
    const double sampleRate;
    std::unique_ptr<SequencerEngine> engine;
    std::unique_ptr<Instruments::Host> instruments;   // distrutto prima del motore
    std::vector<bool> used;
    std::unique_ptr<SequencerEngine::FrozenAudio> result;
    std::atomic<float> progress { 0.0f };
    std::atomic<bool> finished { false }, cancelled { false };

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (ChannelFreezer)
};
