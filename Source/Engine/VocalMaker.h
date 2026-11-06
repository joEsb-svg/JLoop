#pragma once

#include <JuceHeader.h>
#include <map>
#include <vector>
#include "SequencerEngine.h"
#include "SampleStore.h"
#include "MusicTheory.h"
#include "VocalSound.h"
#include "TimeStretch.h"

//==============================================================================
/**
    FRASI VOCALI A TEMPO: una libreria di hook cantati, grida, build-up e voci strane, fatte da JLoop.
    Ogni frase e' scritta in battiti: viene "cantata" al BPM del progetto e nella sua tonalita',
    e quando cambi i BPM si rifa' da sola (come riser e downer: la ricetta resta nel progetto).
*/
namespace vocals
{
    enum Category { hooks = 0, shouts, buildups, weird, numCategories };

    inline const char* categoryName (int c)
    {
        static const char* names[] { "Hook cantati", "Grida e adlib", "Build-up", "Psy, hi-tech e strane" };
        return names[juce::jlimit (0, numCategories - 1, c)];
    }

    struct Phrase
    {
        const char* id;
        const char* name;
        int category;
        int bars;
        int voice;            // voce proposta
        bool pitched;         // cantata (nella scala) o parlata/gridata
        std::vector<vocal::Syl> syls;
    };

    inline const std::vector<Phrase>& phrases()
    {
        using vocal::Syl;
        auto repeat = [] (const char* text, double from, double step, int count, float degree, float accent = 1.0f, double len = -1.0)
        {
            std::vector<Syl> v;
            for (int i = 0; i < count; ++i) v.push_back ({ text, from + i * step, len > 0 ? len : step * 0.8, degree, accent });
            return v;
        };
        auto join = [] (std::vector<Syl> a, const std::vector<Syl>& b) { a.insert (a.end(), b.begin(), b.end()); return a; };

        static const std::vector<Phrase> list
        {
            // --- Hook cantati (nella scala del progetto)
            { "oh_anthem", "Oh-oh-oh (anthem)", hooks, 2, vocal::female, true, {
                { "o", 0, 1.4, 4 }, { "w o", 1.5, 0.5, 4, 0.8f }, { "w o", 2, 2, 5 }, { "w o", 4, 1, 4, 0.9f }, { "w o", 5, 1, 2, 0.8f }, { "w o", 6, 2, 0, 1.0f, -0.3f } } },
            { "oh_wo_oh", "Oh-wo-oh (trance)", hooks, 2, vocal::female, true, {
                { "o", 0, 0.75, 7 }, { "w o", 0.75, 0.75, 6 }, { "w o", 1.5, 2.5, 4 }, { "o", 4, 0.75, 7 }, { "w o", 4.75, 0.75, 6 }, { "w o", 5.5, 2.5, 2, 1.0f, -0.4f } } },
            { "ah_chop", "Ah ah (vocal chop)", hooks, 1, vocal::female, true, {
                { "a", 0, 0.35, 0 }, { "a", 0.5, 0.35, 0, 0.8f }, { "a", 1.5, 0.35, 2 }, { "a", 2, 0.35, 4 }, { "a", 2.75, 0.5, 3, 0.9f }, { "a", 3.5, 0.4, 2, 0.7f } } },
            { "eh_chop", "Eh-yeh chop", hooks, 1, vocal::female, true, {
                { "eh", 0, 0.25, 4 }, { "y eh", 0.5, 0.25, 4, 0.8f }, { "eh", 1, 0.5, 2 }, { "y eh", 2, 0.25, 4 }, { "eh", 2.5, 0.25, 5, 0.8f }, { "y eh", 3, 0.9, 4, 1.0f, -1.0f } } },
            { "yeah_eh", "Yeah-eh-eh", hooks, 2, vocal::female, true, {
                { "y eh", 0, 1.5, 4 }, { "eh", 1.5, 0.5, 5, 0.8f }, { "eh", 2, 2, 4, 1.0f, -0.5f }, { "y eh", 4, 1.5, 2 }, { "eh", 5.5, 0.5, 1, 0.8f }, { "eh", 6, 2, 0 } } },
            { "na_na", "Na na na", hooks, 2, vocal::female, true, join (repeat ("n a", 0, 0.5, 6, 4, 0.9f, 0.4), {
                { "n a", 3, 1, 5 }, { "n a", 4, 0.5, 4 }, { "n a", 4.5, 0.5, 2 }, { "n a", 5, 0.5, 4 }, { "n a", 5.5, 0.5, 2 }, { "n a", 6, 2, 0 } }) },
            { "la_la", "La la la", hooks, 2, vocal::female, true, {
                { "l a", 0, 0.5, 0 }, { "l a", 0.5, 0.5, 2 }, { "l a", 1, 1, 4 }, { "l a", 2, 0.5, 4 }, { "l a", 2.5, 0.5, 5 }, { "l a", 3, 1, 4 },
                { "l a", 4, 0.5, 2 }, { "l a", 4.5, 0.5, 4 }, { "l a", 5, 1, 2 }, { "l a", 6, 2, 0 } } },
            { "hey_ey", "Hey-ey-ey (cantato)", hooks, 1, vocal::female, true, {
                { "h ey", 0, 1, 4 }, { "ey", 1, 0.5, 5, 0.8f }, { "ey", 1.5, 1.5, 4 }, { "ey", 3, 1, 2, 0.8f, -1.0f } } },
            { "tonight", "Tonight (parola cantata)", hooks, 2, vocal::female, true, {
                { "t uh", 0, 1, 2 }, { "n ay t", 1, 3, 4, 1.0f }, { "t uh", 4, 1, 2 }, { "n ay t", 5, 3, 0, 1.0f } } },
            { "feel_it", "Feel it (parola cantata)", hooks, 1, vocal::female, true, {
                { "f ii l", 0, 1.5, 4 }, { "ih t", 1.5, 0.5, 2, 0.8f }, { "f ii l", 2, 1.5, 4 }, { "ih t", 3.5, 0.5, 0, 0.8f } } },
            { "ooh_pad", "Ooh lungo (pad)", hooks, 4, vocal::choir, true, {
                { "u", 0, 7.8, 0 }, { "u", 8, 7.8, -1 } } },
            { "aah_choir", "Aah coro", hooks, 4, vocal::choir, true, {
                { "a", 0, 3.8, 0 }, { "a", 4, 3.8, 2 }, { "a", 8, 3.8, -2 }, { "a", 12, 3.9, 0 } } },

            // --- Grida e adlib (parlate)
            { "hey", "Hey!", shouts, 1, vocal::male, false, { { "h ey", 0, 0.9, 0 } } },
            { "hey_x3", "Hey! Hey! Hey!", shouts, 1, vocal::male, false, { { "h ey", 0, 0.6, 0 }, { "h ey", 1, 0.6, 0 }, { "h ey", 2, 1.2, 1 } } },
            { "oh", "Oh!", shouts, 1, vocal::male, false, { { "o", 0, 0.8, 0, 1.0f, -2.0f } } },
            { "yeah", "Yeah!", shouts, 1, vocal::male, false, { { "y eh", 0, 1.0, 1 } } },
            { "go", "Go!", shouts, 1, vocal::male, false, { { "g ow", 0, 0.8, 1 } } },
            { "come_on", "Come on!", shouts, 1, vocal::male, false, { { "k uh m", 0, 0.5, 0 }, { "aa n", 0.5, 1.0, 2 } } },
            { "lets_go", "Let's go!", shouts, 1, vocal::male, false, { { "l eh t s", 0, 0.5, 0 }, { "g ow", 0.5, 1.2, 2 } } },
            { "drop", "Drop!", shouts, 1, vocal::male, false, { { "d r aa p", 0, 0.8, 1 } } },
            { "jump", "Jump!", shouts, 1, vocal::male, false, { { "j uh m p", 0, 0.7, 1 } } },
            { "get_up", "Get up!", shouts, 1, vocal::male, false, { { "g eh t", 0, 0.4, 0 }, { "uh p", 0.5, 0.6, 2 } } },
            { "uh", "Uh!", shouts, 1, vocal::male, false, { { "uh", 0, 0.4, 0, 1.0f, -3.0f } } },
            { "woo", "Woo!", shouts, 1, vocal::female, false, { { "w u", 0, 1.2, 6, 1.0f, 3.0f } } },
            { "make_noise", "Make some noise!", shouts, 1, vocal::male, false, {
                { "m ey k", 0, 0.5, 1 }, { "s uh m", 0.5, 0.5, 0 }, { "n oy z", 1, 1.4, 2 } } },

            // --- Build-up (finiscono sul drop)
            { "hey_build", "Hey hey hey sempre piu' veloce", buildups, 4, vocal::male, false,
                join (join (repeat ("h ey", 0, 1, 8, 0, 0.9f), repeat ("h ey", 8, 0.5, 4, 1, 0.9f)), repeat ("h ey", 10, 0.25, 8, 2, 0.8f, 0.2)) },
            { "come_on_x2", "Come on, come on!", buildups, 2, vocal::male, false, {
                { "k uh m", 0, 0.5, 0 }, { "aa n", 0.5, 1.3, 2 }, { "k uh m", 2, 0.5, 0 }, { "aa n", 2.5, 1.3, 3 },
                { "k uh m", 4, 0.5, 1 }, { "aa n", 4.5, 0.5, 3 }, { "k uh m", 5, 0.5, 2 }, { "aa n", 5.5, 0.5, 4 }, { "k uh m", 6, 0.5, 3 }, { "aa n", 6.5, 1.3, 6 } } },
            { "oh_rising", "Oh-oh che sale", buildups, 4, vocal::female, true, {
                { "o", 0, 1.8, 0 }, { "w o", 2, 1.8, 1 }, { "w o", 4, 1.8, 2 }, { "w o", 6, 1.8, 3 }, { "w o", 8, 0.9, 4 }, { "w o", 9, 0.9, 5 },
                { "w o", 10, 0.9, 6 }, { "w o", 11, 0.9, 7 }, { "w o", 12, 0.45, 7 }, { "w o", 12.5, 0.45, 8 }, { "w o", 13, 0.45, 9 }, { "w o", 13.5, 0.45, 10 },
                { "w o", 14, 1.9, 11, 1.0f, 1.0f } } },
            { "lets_go_build", "Let's go! (prima del drop)", buildups, 2, vocal::male, false, {
                { "y eh", 0, 1.5, 0 }, { "k uh m", 4, 0.5, 0 }, { "aa n", 4.5, 1.0, 2 }, { "l eh t s", 6.5, 0.5, 2 }, { "g ow", 7, 1.0, 4 } } },

            // --- Psy, hi-tech e strane
            { "robot_ah", "Robot ah-ah (sedicesimi)", weird, 1, vocal::robot, true,
                join (repeat ("a", 0, 0.25, 8, 0, 0.9f, 0.18), repeat ("a", 2, 0.25, 8, 7, 0.9f, 0.18)) },
            { "robot_yoyo", "Yo-yo alieno", weird, 1, vocal::robot, true, {
                { "y o", 0, 0.5, 7 }, { "y o", 0.75, 0.5, 5 }, { "y o", 1.5, 0.5, 7 }, { "y o", 2, 0.5, 10 }, { "y o", 3, 0.9, 0, 1.0f, -5.0f } } },
            { "uh_uh_robot", "Uh-uh-uh robotico", weird, 1, vocal::robot, true, repeat ("uh", 0, 0.5, 8, 0, 1.0f, 0.2) },
            { "wow_wow", "Wow-wow (formanti)", weird, 1, vocal::robot, true, {
                { "w a w", 0, 0.5, 0 }, { "w a w", 0.5, 0.5, 0 }, { "w a w", 1, 0.5, 3 }, { "w a w", 1.5, 0.5, 0 },
                { "w a w", 2, 0.5, 5 }, { "w a w", 2.5, 0.5, 3 }, { "w a w", 3, 1.0, 0, 1.0f, -2.0f } } },
            { "whisper_trip", "Sussurro: \"trip\"", weird, 1, vocal::whisper, false, { { "t r ih p", 0, 1.5, 0 } } },
            { "whisper_shh", "Sussurro: shh-ah", weird, 1, vocal::whisper, false, { { "sh", 0, 0.5, 0 }, { "h a", 2, 1.5, 0 } } },
            { "alien_choir", "Coro alieno", weird, 2, vocal::choir, true, {
                { "u", 0, 1.9, 0, 1.0f, 1.0f }, { "i", 2, 1.9, 1, 1.0f, -1.0f }, { "a", 4, 1.9, 3, 1.0f, 2.0f }, { "u", 6, 1.9, 0, 1.0f, -12.0f } } },
        };
        return list;
    }

    inline int indexOf (const juce::String& id)
    {
        const auto& p = phrases();
        for (int i = 0; i < (int) p.size(); ++i)
            if (id == p[(size_t) i].id) return i;
        return -1;
    }

    struct Options
    {
        int phrase = 0;
        int voice = -1;       // -1 = quella proposta dalla frase
        int octave = 0;
        int fx = vocal::dry;
    };

    inline int voiceOf (const Options& o)    { return o.voice >= 0 ? o.voice : phrases()[(size_t) juce::jlimit (0, (int) phrases().size() - 1, o.phrase)].voice; }

    /** Grado della scala -> semitoni (scala del progetto; senza scala: minore). */
    inline float degreeToSemis (float degree, int scaleType)
    {
        const auto& all = theory::scales();
        const auto& iv = all[(size_t) (scaleType > 0 && scaleType < (int) all.size() ? scaleType : 2)].intervals;
        const int n = (int) iv.size();
        const int d = (int) std::floor (degree);
        const int oct = (int) std::floor ((double) d / n);
        const int idx = d - oct * n;
        return (float) (iv[(size_t) idx] + 12 * oct) + (degree - (float) d) * 2.0f;
    }

    /** L'audio della frase (mono, a sr): dura le sue battute piu' la coda. */
    inline juce::AudioBuffer<float> render (const Options& o, double bpm, double sr, int root, int scaleType)
    {
        const auto& ph = phrases()[(size_t) juce::jlimit (0, (int) phrases().size() - 1, o.phrase)];
        const int voice = voiceOf (o);
        const bool fem = voice != vocal::male;
        const double spb = 60.0 / juce::jmax (20.0, bpm);
        const double tail = o.fx == vocal::dry || o.fx == vocal::radio ? 0.35 : 1.8;
        juce::AudioBuffer<float> out (1, (int) std::ceil ((ph.bars * 4 * spb + tail) * sr));
        out.clear();

        // Note di ogni sillaba
        const int rootPc = ((root % 12) + 12) % 12;
        const float baseSung = (float) (57 + ((rootPc + 3) % 12)) + 12.0f * o.octave;   // tonica tra La2+1 e Sol#3+1 (voce femminile ~ La3-Sol#4)
        const float baseSpoken = (fem ? 62.0f : 52.0f) + 12.0f * o.octave;
        std::vector<std::pair<float, float>> pitches;
        for (auto& s : ph.syls)
        {
            if (ph.pitched)
            {
                const float p = baseSung + degreeToSemis (s.degree, scaleType) - (fem ? 0.0f : 12.0f);
                pitches.push_back ({ p, p + s.bend });
            }
            else
            {
                // parlata/gridata: parte un po' su e scende (come una voce vera che grida)
                const float p = baseSpoken + s.degree;
                pitches.push_back ({ p + 2.0f, p - 3.0f + s.bend });
            }
        }

        const auto addSinger = [&] (vocal::Singer who, float gain, float transpose)
        {
            auto segs = vocal::layout (ph.syls, bpm, pitches, who.fem);
            for (auto& s : segs) { s.pitchA += transpose; s.pitchB += transpose; }
            juce::AudioBuffer<float> one (1, out.getNumSamples());
            one.clear();
            vocal::sing (segs, who, sr, one);
            out.addFrom (0, 0, one, 0, 0, out.getNumSamples(), gain);
        };

        const float tension = ph.pitched ? 0.0f : 0.65f;
        switch (voice)
        {
            case vocal::female: addSinger ({ true, 0, 0, 0, 1.0f, 30.0f, 0.08f, tension, 11 }, 1.0f, 0.0f); break;
            case vocal::male:   addSinger ({ false, 0, 0, 0, 1.0f, 22.0f, 0.05f, tension, 12 }, 1.0f, 0.0f); break;
            case vocal::robot:  addSinger ({ true, 1, 0, 0, 1.0f, 0.0f, 0.0f, 0.0f, 13 }, 1.0f, 0.0f); break;
            case vocal::whisper: addSinger ({ true, 2, 0, 0, 1.0f, 0.0f, 0.3f, 0.0f, 14 }, 1.0f, 0.0f); break;
            case vocal::choir:
            default:
            {
                // 4 voci femminili leggermente stonate e sfasate + 2 maschili un'ottava sotto
                const float det[4] { -9.0f, 7.0f, -3.0f, 12.0f };
                const float off[4] { 0.0f, 0.014f, 0.006f, 0.021f };
                const float fs[4] { 1.0f, 0.97f, 1.03f, 0.99f };
                for (int k = 0; k < 4; ++k)
                    addSinger ({ true, 0, det[k], off[k], fs[k], 26.0f, 0.07f, 0.0f, (juce::uint32) (21 + k) }, 0.45f, 0.0f);
                for (int k = 0; k < 2; ++k)
                    addSinger ({ false, 0, k == 0 ? -6.0f : 8.0f, 0.01f * (k + 1), 1.0f, 20.0f, 0.05f, 0.0f, (juce::uint32) (31 + k) }, 0.4f, ph.pitched ? -12.0f : 0.0f);
                break;
            }
        }
        vocal::finish (out, sr, bpm, o.fx);
        return out;
    }

    //==========================================================================
    inline juce::String recipeToString (const Options& o, double bpm, int root, int scaleType)
    {
        auto* d = new juce::DynamicObject();
        d->setProperty ("kind", "vocal");
        d->setProperty ("phrase", phrases()[(size_t) juce::jlimit (0, (int) phrases().size() - 1, o.phrase)].id);
        d->setProperty ("voice", o.voice);
        d->setProperty ("octave", o.octave);
        d->setProperty ("fx", o.fx);
        d->setProperty ("bpm", bpm);
        d->setProperty ("root", root);
        d->setProperty ("scale", scaleType);
        return juce::JSON::toString (juce::var (d), true);
    }

    inline bool isVocalRecipe (const juce::String& text)    { return text.contains ("\"vocal\"") || text.contains ("\"vocalStretch\""); }

    inline bool recipeFromString (const juce::String& text, Options& o, double& bpm, int& root, int& scaleType)
    {
        const auto v = juce::JSON::parse (text);
        if (! v.isObject() || v.getProperty ("kind", {}).toString() != "vocal") return false;
        o.phrase = indexOf (v.getProperty ("phrase", {}).toString());
        if (o.phrase < 0) return false;
        o.voice = juce::jlimit (-1, vocal::numVoices - 1, (int) v.getProperty ("voice", -1));
        o.octave = juce::jlimit (-1, 1, (int) v.getProperty ("octave", 0));
        o.fx = juce::jlimit (0, vocal::numFx - 1, (int) v.getProperty ("fx", 0));
        bpm = (double) v.getProperty ("bpm", 120.0);
        root = (int) v.getProperty ("root", 0);
        scaleType = (int) v.getProperty ("scale", 0);
        return true;
    }

    /** Nome della frase pulito per un nome di file. */
    inline juce::String displayName (const Options& o)
    {
        return juce::String (phrases()[(size_t) juce::jlimit (0, (int) phrases().size() - 1, o.phrase)].name)
                   .removeCharacters ("!\"?").replace ("'", "").trim();
    }

    /** Scrive la frase come campione (nome con battute e BPM) e la carica. Ritorna il nome del file, vuoto se fallisce. */
    inline juce::String writeSample (SampleStore& store, const Options& o, double bpm, int root, int scaleType, SampleData*& data)
    {
        data = nullptr;
        const auto& ph = phrases()[(size_t) juce::jlimit (0, (int) phrases().size() - 1, o.phrase)];
        auto audio = render (o, bpm, 44100.0, root, scaleType);
        auto folder = SampleStore::getFolder();
        folder.createDirectory();
        const juce::String bpmText = std::abs (bpm - std::round (bpm)) < 0.05 ? juce::String (juce::roundToInt (bpm)) : juce::String (bpm, 1);
        const auto base = juce::File::createLegalFileName ("Vocal " + displayName (o) + " " + vocal::voiceName (voiceOf (o)) + " "
                                                           + juce::String (ph.bars) + (ph.bars == 1 ? " battuta " : " battute ") + bpmText + " BPM");
        juce::File file = folder.getChildFile (base + ".wav");
        for (int i = 2; file.exists(); ++i) file = folder.getChildFile (base + " (" + juce::String (i) + ").wav");
        {
            juce::WavAudioFormat wav;
            std::unique_ptr<juce::OutputStream> os = std::make_unique<juce::FileOutputStream> (file);
            auto w = wav.createWriterFor (os, juce::AudioFormatWriterOptions{}.withSampleRate (44100.0).withNumChannels (1).withBitsPerSample (16));
            if (w == nullptr || ! w->writeFromAudioSampleBuffer (audio, 0, audio.getNumSamples()))
                return {};
        }
        data = store.load (file.getFileName());
        return data != nullptr ? file.getFileName() : juce::String();
    }

    //==========================================================================
    // LE TUE VOCI A TEMPO: un campione importato, suonato a un certo BPM, stirato al BPM del progetto (stessa intonazione)

    inline juce::String stretchRecipe (const juce::String& source, double srcBpm, double bpm)
    {
        auto* d = new juce::DynamicObject();
        d->setProperty ("kind", "vocalStretch");
        d->setProperty ("source", source);
        d->setProperty ("srcBpm", srcBpm);
        d->setProperty ("bpm", bpm);
        return juce::JSON::toString (juce::var (d), true);
    }

    inline bool stretchFromString (const juce::String& text, juce::String& source, double& srcBpm, double& bpm)
    {
        const auto v = juce::JSON::parse (text);
        if (! v.isObject() || v.getProperty ("kind", {}).toString() != "vocalStretch") return false;
        source = v.getProperty ("source", {}).toString();
        srcBpm = juce::jlimit (20.0, 300.0, (double) v.getProperty ("srcBpm", 120.0));
        bpm = (double) v.getProperty ("bpm", 120.0);
        return source.isNotEmpty();
    }

    /** Stira il campione 'source' (suonato a srcBpm) al tempo bpm e lo salva. Ritorna il nome, vuoto se non riesce. */
    inline juce::String writeStretched (SampleStore& store, const juce::String& source, double srcBpm, double bpm, SampleData*& data)
    {
        data = nullptr;
        auto* src = store.load (source);
        if (src == nullptr) return {};
        auto audio = stretch::process (src->audio, src->sampleRate, srcBpm / juce::jmax (20.0, bpm));
        const float peak = audio.getMagnitude (0, 0, audio.getNumSamples());
        if (peak > 0.99f) audio.applyGain (0.99f / peak);
        auto folder = SampleStore::getFolder();
        folder.createDirectory();
        const juce::String bpmText = std::abs (bpm - std::round (bpm)) < 0.05 ? juce::String (juce::roundToInt (bpm)) : juce::String (bpm, 1);
        const auto base = juce::File::createLegalFileName ("Vocal " + juce::File (source).getFileNameWithoutExtension() + " a tempo " + bpmText + " BPM");
        juce::File file = folder.getChildFile (base + ".wav");
        for (int i = 2; file.exists(); ++i) file = folder.getChildFile (base + " (" + juce::String (i) + ").wav");
        {
            juce::WavAudioFormat wav;
            std::unique_ptr<juce::OutputStream> os = std::make_unique<juce::FileOutputStream> (file);
            auto w = wav.createWriterFor (os, juce::AudioFormatWriterOptions{}.withSampleRate (src->sampleRate).withNumChannels (1).withBitsPerSample (24));
            if (w == nullptr || ! w->writeFromAudioSampleBuffer (audio, 0, audio.getNumSamples()))
                return {};
        }
        data = store.load (file.getFileName());
        return data != nullptr ? file.getFileName() : juce::String();
    }

    /** I BPM sono cambiati: la frase (se la ricetta e' una frase vocale) si rifa'. Ritorna il nuovo nome, vuoto se non serve o non riesce. */
    inline juce::String retime (const juce::String& recipe, double bpm, SampleStore& store, juce::String& newRecipe)
    {
        {
            juce::String source; double srcBpm = 0.0, madeAt = 0.0;
            if (stretchFromString (recipe, source, srcBpm, madeAt))
            {
                if (std::abs (madeAt - bpm) < 0.05) return {};
                newRecipe = stretchRecipe (source, srcBpm, bpm);
                SampleData* data = nullptr;
                return writeStretched (store, source, srcBpm, bpm, data);
            }
        }
        Options o; double madeAt = 0.0; int root = 0, scale = 0;
        if (! recipeFromString (recipe, o, madeAt, root, scale) || std::abs (madeAt - bpm) < 0.05) return {};
        newRecipe = recipeToString (o, bpm, root, scale);
        SampleData* data = nullptr;
        return writeSample (store, o, bpm, root, scale, data);
    }

    struct Result
    {
        bool ok = false;
        juce::String error, sampleName;
        int channel = -1, pattern = -1, startBar = -1;
    };

    inline Result addToProject (SequencerEngine& engine, const juce::String& fileName, SampleData* data, const juce::String& name, int bars, int bar);

    /** Crea campione, canale e un pattern lungo quanto la frase (e, se chiesto, la clip nella playlist dalla battuta 'bar'). */
    inline Result apply (SequencerEngine& engine, SampleStore& store, const Options& o, int bar = -1)
    {
        Result r;
        const auto& ph = phrases()[(size_t) juce::jlimit (0, (int) phrases().size() - 1, o.phrase)];
        if (engine.getFirstFreePattern() < 0) { r.error = "Tutti i pattern sono gia' usati."; return r; }
        const double bpm = engine.getBpm();
        const int root = engine.getScaleRoot(), scale = engine.getScaleType();
        SampleData* data = nullptr;
        const auto fileName = writeSample (store, o, bpm, root, scale, data);
        if (fileName.isEmpty()) { r.error = "Non riesco a salvare il campione."; return r; }
        engine.sampleRecipes[fileName] = recipeToString (o, bpm, root, scale);
        return addToProject (engine, fileName, data, "Vocal " + displayName (o), ph.bars, bar);
    }

    /** La tua voce (campione importato, suonato a srcBpm) a tempo col progetto: canale + pattern (+ clip). */
    inline Result applyStretched (SequencerEngine& engine, SampleStore& store, const juce::String& source, double srcBpm, int bar = -1)
    {
        Result r;
        if (engine.getFirstFreePattern() < 0) { r.error = "Tutti i pattern sono gia' usati."; return r; }
        const double bpm = engine.getBpm();
        SampleData* data = nullptr;
        const auto fileName = writeStretched (store, source, srcBpm, bpm, data);
        if (fileName.isEmpty()) { r.error = "Non riesco a leggere o a salvare la voce."; return r; }
        engine.sampleRecipes[fileName] = stretchRecipe (source, srcBpm, bpm);
        const double seconds = data->audio.getNumSamples() / data->sampleRate;
        const int bars = juce::jlimit (1, SequencerEngine::maxSteps / SequencerEngine::stepsPerBar,
                                       (int) std::ceil (seconds * bpm / 240.0 - 0.05));
        return addToProject (engine, fileName, data, "Vocal " + juce::File (source).getFileNameWithoutExtension(), bars, bar);
    }

    inline Result addToProject (SequencerEngine& engine, const juce::String& fileName, SampleData* data, const juce::String& name, int bars, int bar)
    {
        Result r;
        r.pattern = engine.getFirstFreePattern();
        if (r.pattern < 0) { r.error = "Tutti i pattern sono gia' usati."; return r; }
        r.channel = engine.addChannel();
        if (r.channel < 0) { r.error = "Hai raggiunto il massimo di canali."; return r; }
        auto& ch = engine.getChannel (r.channel);
        ch.type.store (SequencerEngine::drumOrSample);
        ch.sampleName = fileName;
        ch.sample.store (data);
        ch.colour.store (0xffe36fb8u);
        ch.name = name;
        ch.customName = true;
        ch.touchSound();
        r.sampleName = fileName;

        auto& pat = engine.getPattern (r.pattern);
        pat.length.store (juce::jmin (SequencerEngine::maxSteps, bars * SequencerEngine::stepsPerBar));
        pat.velocity[(size_t) r.channel][0].store ((uint8_t) 110);
        pat.probability[(size_t) r.channel][0].store ((uint8_t) 100);
        pat.ratchet[(size_t) r.channel][0].store ((uint8_t) 1);
        engine.setPatternName (r.pattern, name);

        if (bar >= 0)
        {
            auto clips = engine.getClips();
            // prima traccia libera in quelle battute
            int track = -1;
            for (int t = 0; t < SequencerEngine::numTracks && track < 0; ++t)
            {
                bool free = true;
                for (auto& c : clips)
                    if (c.track == t)
                    {
                        const int s = c.startStep(), e = s + engine.getClipLength (c);
                        if (s < (bar + bars) * SequencerEngine::stepsPerBar && e > bar * SequencerEngine::stepsPerBar) free = false;
                    }
                if (free) track = t;
            }
            if (track >= 0)
            {
                SequencerEngine::Clip c;
                c.track = track; c.bar = bar; c.pattern = r.pattern; c.length = 0;
                clips.push_back (c);
                engine.setClips (clips);
                r.startBar = bar;
            }
        }
        r.ok = true;
        return r;
    }
}
