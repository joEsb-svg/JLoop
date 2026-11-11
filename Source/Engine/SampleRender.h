#pragma once

#include <JuceHeader.h>
#include "SampleStore.h"
#include "DrumVoice.h"
#include "SoundLibrary.h"
#include "SequencerEngine.h"

//==============================================================================
/**
    Suoni come forme d'onda: per disegnarle (browser dei suoni, impostazioni del canale) e per girarle al contrario.
    - renderDrum: un colpo della batteria sintetizzata registrato come campione.
    - install(): insegna al SampleStore a caricare "drum:<tipo>:<kit>" (e quindi anche "rev:drum:...").
*/
namespace samplerender
{
    inline SampleData::Ptr renderDrum (int type, int kit)
    {
        constexpr double sr = 44100.0;
        DrumVoice v;
        v.prepare (sr);
        v.setParams (SoundLibrary::get (type, kit));
        v.trigger (0.9f);
        const int maxLen = (int) (sr * 4.0);
        juce::AudioBuffer<float> tmp (1, maxLen);
        auto* d = tmp.getWritePointer (0);
        int n = 0;
        while (n < maxLen && (v.isActive() || n == 0))
            d[n++] = v.render();
        SampleData::Ptr data = new SampleData();
        data->sampleRate = sr;
        data->rootNote = 60;
        data->audio.setSize (1, juce::jmax (16, n));
        data->audio.clear();
        data->audio.copyFrom (0, 0, tmp, 0, 0, juce::jmax (1, n));
        const float peak = data->audio.getMagnitude (0, 0, data->audio.getNumSamples());
        if (peak > 0.0001f) data->audio.applyGain (0.89f / peak);
        return data;
    }

    inline void install()
    {
        SampleStore::drumRenderer = [] (int type, int kit) { return renderDrum (type, kit); };
    }

    /** Il campione che rappresenta il suono di un canale (per disegnarlo): il suo campione o il colpo di batteria. */
    inline const SampleData* soundOf (SequencerEngine& e, SampleStore& store, int channel)
    {
        auto& ch = e.getChannel (channel);
        if (e.isSynth (channel)) return nullptr;
        if (auto* s = ch.sample.load()) return s;
        return store.load (SampleStore::drumName (ch.drumType.load(), e.getChannelKit (channel)));
    }

    /** Gira al contrario (o rimette dritto) il suono di un canale. false = non si puo' (synth). */
    inline bool setReversed (SequencerEngine& e, SampleStore& store, int channel, bool reversed)
    {
        if (e.isSynth (channel)) return false;
        auto& ch = e.getChannel (channel);
        const bool now = SampleStore::isReversed (ch.sampleName);
        if (now == reversed) return true;
        if (reversed)
        {
            const auto base = ch.sampleName.isNotEmpty() ? ch.sampleName : SampleStore::drumName (ch.drumType.load(), e.getChannelKit (channel));
            const auto name = SampleStore::reversedOf (base);
            auto* data = store.load (name);
            if (data == nullptr) return false;
            ch.sampleName = name;
            ch.sample.store (data);
        }
        else
        {
            const auto base = ch.sampleName.substring (4);
            if (base.startsWith ("drum:"))   // torna la batteria sintetizzata
            {
                ch.sampleName.clear();
                ch.sample.store (nullptr);
            }
            else
            {
                auto* data = store.load (base);
                if (data == nullptr) return false;
                ch.sampleName = base;
                ch.sample.store (data);
            }
        }
        ch.touchSound();
        return true;
    }
}
