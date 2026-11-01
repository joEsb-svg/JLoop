#pragma once

#include <JuceHeader.h>
#include <functional>
#include <vector>
#include "SequencerEngine.h"

//==============================================================================
/**
    Effetti pronti per le automazioni: in un tocco JLoop mette l'effetto giusto nel mixer
    (sulla traccia scelta o sul Master) e disegna da solo le curve nella playlist.

    Tutto nel tempo della canzone: startBar = prima battuta, bars = quante battute dura.
    Fuori dal tratto scelto l'effetto resta "spento" (mix a zero, filtro aperto, volume com'era),
    tranne quelli fatti per restare (fade out, sidechain che cresce).
*/
namespace autofx
{
    enum Id
    {
        buildUp = 0, filterOpen, filterClose, silence, pitchUp, stutter, tapeStop,
        fadeIn, fadeOut, gateIn, sidechainUp, widen,
        reverbThrow, delayThrow, reverbWash,
        lofi, vinylIn,
        numIds
    };

    enum Category { buildAndDrop = 0, volumeAndMotion, space, lofiCat, numCategories };

    struct Info
    {
        const char* name;
        const char* detail;
        int category;
        int defaultBars;
        bool usesLength;      // false: l'effetto sta sempre nell'ultima battuta/quarto del tratto
        juce::uint32 colour;
    };

    inline const char* categoryName (int c)
    {
        static const char* names[] { "Build-up e drop", "Volume e movimento", "Spazio", "Lo-fi" };
        return names[juce::jlimit (0, numCategories - 1, c)];
    }

    inline const Info& info (int id)
    {
        static const Info infos[numIds] {
            { "Build-up col filtro",     "Passa-alto che sale fino al drop, poi si riapre di colpo",            buildAndDrop, 8, true,  0xff4fe3ff },
            { "Filtro che si apre",      "Parte cupo e si apre piano (intro, entrata di un suono)",             buildAndDrop, 8, true,  0xff4fe3ff },
            { "Filtro che si chiude",    "Si scurisce piano (break, uscita), alla fine torna aperto",           buildAndDrop, 4, true,  0xff4fe3ff },
            { "Silenzio prima del drop", "L'ultimo quarto del tratto e' muto: il drop esplode",                  buildAndDrop, 4, false, 0xffff6b5b },
            { "Pitch che sale",          "L'intonazione sale di un'ottava, a scalini di semitono",               buildAndDrop, 4, true,  0xffb07cff },
            { "Stutter finale",          "Ripetizioni sempre piu' strette nell'ultima battuta (1/8 > 1/32)",    buildAndDrop, 1, false, 0xff7d8bff },
            { "Tape stop",               "Il nastro rallenta e si ferma nell'ultimo quarto",                     buildAndDrop, 1, false, 0xffff6b5b },
            { "Fade in",                 "Il volume sale da zero",                                              volumeAndMotion, 8, true, 0xff7ed957 },
            { "Fade out",                "Il volume scende fino a zero e resta giu'",                           volumeAndMotion, 8, true, 0xff7ed957 },
            { "Trance gate che entra",   "Il suono comincia a pulsare a sedicesimi, sempre di piu'",            volumeAndMotion, 4, true, 0xff4fe3ff },
            { "Sidechain che cresce",    "La \"pompa\" con la cassa aumenta e poi resta",                       volumeAndMotion, 8, true, 0xfff0922b },
            { "Stereo che si allarga",   "Il suono si apre ai lati fino al drop",                               volumeAndMotion, 8, true, 0xffb07cff },
            { "Lancio di riverbero",     "Una coda di riverbero sull'ultimo quarto che riempie il vuoto",       space, 1, false, 0xff3fb8ff },
            { "Lancio di delay",         "Eco sull'ultimo quarto che continua dopo",                            space, 1, false, 0xff3fb8ff },
            { "Riverbero che cresce",    "Il suono si allontana sempre di piu', poi torna asciutto",            space, 4, true, 0xff3fb8ff },
            { "Lo-fi che sale",          "Bitcrusher: il suono si sgretola (16 > 4 bit), poi torna pulito",     lofiCat, 4, true, 0xffc49360 },
            { "Vinile che entra",        "Fruscio, crepitio e nastro che crescono, poi tornano puliti",         lofiCat, 4, true, 0xffc49360 },
        };
        return infos[juce::jlimit (0, numIds - 1, id)];
    }

    struct Result
    {
        bool ok = false;
        juce::String error;
        int lanes = 0;
    };

    inline constexpr int ticksPerBar  = SequencerEngine::stepsPerBar * SequencerEngine::ticksPerStep;   // 192
    inline constexpr int ticksPerBeat = ticksPerBar / 4;

    //==========================================================================
    namespace detail
    {
        /** Uno slot con quell'effetto sulla traccia (se 'match' lo accetta), altrimenti il primo vuoto. -1 = traccia piena. */
        inline int slotFor (SequencerEngine& e, int track, int type, std::function<bool (const mixer::Slot&)> match = {})
        {
            auto& tr = e.getTrack (track);
            for (int s = 0; s < mixer::numSlots; ++s)
                if (tr.slots[(size_t) s].type.load() == type && (! match || match (tr.slots[(size_t) s])))
                    return s;
            for (int s = 0; s < mixer::numSlots; ++s)
                if (tr.slots[(size_t) s].type.load() == mixer::none)
                {
                    e.setTrackEffect (track, s, type);
                    return s;
                }
            return -1;
        }

        inline void setParam (SequencerEngine& e, int track, int slot, int p, float value)
        {
            e.getTrack (track).slots[(size_t) slot].params[(size_t) p].store (value);
        }

        inline float norm (int type, int p, float value)   { return mixer::toNormalised (mixer::info (type).params[(size_t) p], value); }

        /** Mette i punti nella corsia del bersaglio, togliendo quelli che c'erano tra 'from' e 'to'. */
        inline bool curve (SequencerEngine& e, const automation::Target& t, std::vector<automation::Point> pts, int from, int to)
        {
            const int lane = e.addLane (t);
            if (lane < 0) return false;
            auto old = e.getLanePoints (lane);
            old.erase (std::remove_if (old.begin(), old.end(), [from, to] (const automation::Point& p) { return p.tick >= from && p.tick <= to; }), old.end());
            for (auto& p : pts)
                if (p.tick >= 0) old.push_back (p);
            e.setLanePoints (lane, old);
            return true;
        }

        inline automation::Target volumeTarget (int track)
        {
            return track == 0 ? automation::Target { automation::master, 0, 0, 0 }
                              : automation::Target { automation::trackParam, track, automation::trVolume, 0 };
        }

        inline float currentVolume (SequencerEngine& e, int track)
        {
            return track == 0 ? automation::volumeNorm (e.getMasterVolume(), 3.5f)
                              : automation::volumeNorm (e.getTrack (track).volume.load(), 4.0f);
        }

        inline int findKickTrack (SequencerEngine& e, int notTrack)
        {
            for (int c = 0; c < e.getNumChannels(); ++c)
            {
                const auto name = e.getChannel (c).name.toLowerCase();
                const int t = e.getChannelTrack (c);
                if (t > 0 && t != notTrack && (name.contains ("kick") || name.contains ("cassa")))
                    return t;
            }
            return 0;
        }

        /** Griglia del Glitch: 16 caratteri (. normale, 8/6/3 ripeti, s stop, r reverse, m muto, h rallenta). */
        inline void setGlitchGrid (SequencerEngine& e, int track, int slot, const char* pattern)
        {
            float p[mixer::maxParams] {};
            for (int s = 0; s < 16; ++s)
            {
                const char ch = pattern[s];
                const int m = ch == '8' ? 1 : ch == '6' ? 2 : ch == '3' ? 3 : ch == 's' ? 4 : ch == 'r' ? 5 : ch == 'm' ? 6 : ch == 'h' ? 7 : 0;
                mixer::glitchGrid::set (p, s, m);
            }
            setParam (e, track, slot, 1, p[1]);
            setParam (e, track, slot, 2, p[2]);
        }

        /** Mix acceso solo nel tratto [a, b): zero prima e dopo. */
        inline std::vector<automation::Point> onlyIn (int a, int b, float on, float off = 0.0f)
        {
            std::vector<automation::Point> pts;
            if (a > 0) pts.push_back ({ a - 1, off });
            pts.push_back ({ a, on });
            pts.push_back ({ b - 1, on });
            pts.push_back ({ b, off });
            return pts;
        }
    }

    //==========================================================================
    /** Applica l'effetto pronto. track = traccia del mixer (0 = Master). Thread della UI. */
    inline Result apply (SequencerEngine& e, int id, int track, int startBar, int bars)
    {
        using namespace detail;
        using automation::Target;
        Result r;
        track = juce::jlimit (0, mixer::numTracks - 1, track);
        bars = juce::jlimit (1, 64, bars);
        startBar = juce::jmax (0, startBar);
        const int a = startBar * ticksPerBar, b = (startBar + bars) * ticksPerBar;
        const int lastBar = b - ticksPerBar, lastBeat = b - ticksPerBeat;
        auto fx = [track] (int slot, int p) { return Target { automation::effectParam, track, slot, p }; };
        bool ok = true;
        auto add = [&] (const Target& t, std::vector<automation::Point> pts, int from, int to)
        {
            if (curve (e, t, std::move (pts), from, to)) ++r.lanes;
            else ok = false;
        };
        auto full = [&r] { r.error = "La traccia ha gia' 6 effetti: togline uno (o scegli un'altra traccia)."; return r; };

        switch (id)
        {
            case buildUp:
            case filterOpen:
            case filterClose:
            {
                const float type = id == buildUp ? 1.0f : 0.0f;   // passa-alto / passa-basso
                const int s = slotFor (e, track, mixer::filter, [type] (const mixer::Slot& sl) { return std::abs (sl.params[0].load() - type) < 0.5f; });
                if (s < 0) return full();
                const float open = id == buildUp ? 20.0f : 20000.0f;
                setParam (e, track, s, 0, type);
                setParam (e, track, s, 1, open);
                setParam (e, track, s, 2, id == buildUp ? 0.25f : 0.3f);
                setParam (e, track, s, 3, 0.0f);
                const auto n = [] (float hz) { return norm (mixer::filter, 1, hz); };
                if (id == buildUp)          add (fx (s, 1), { { a, n (20.0f) }, { b - 2, n (1200.0f) }, { b, n (20.0f) } }, a, b);
                else if (id == filterOpen)  add (fx (s, 1), { { a, n (200.0f) }, { b, n (20000.0f) } }, a, b);
                else                        add (fx (s, 1), { { a, n (20000.0f) }, { b - 2, n (250.0f) }, { b, n (20000.0f) } }, a, b);
                break;
            }
            case silence:
            {
                const float v = currentVolume (e, track);
                add (volumeTarget (track), { { lastBeat - 1, v }, { lastBeat, 0.0f }, { b - 1, 0.0f }, { b, v } }, lastBeat - 1, b);
                break;
            }
            case pitchUp:
            {
                const int s = slotFor (e, track, mixer::pitch);
                if (s < 0) return full();
                setParam (e, track, s, 0, 0.0f);
                setParam (e, track, s, 1, 0.0f);
                setParam (e, track, s, 2, 40.0f);
                setParam (e, track, s, 3, 0.0f);
                add (fx (s, 0), { { a, norm (mixer::pitch, 0, 0.0f) }, { b - 2, norm (mixer::pitch, 0, 12.0f) }, { b, norm (mixer::pitch, 0, 0.0f) } }, a, b);
                add (fx (s, 3), onlyIn (a, b, 1.0f), a - 1, b);
                break;
            }
            case stutter:
            case tapeStop:
            {
                const int s = slotFor (e, track, mixer::glitch);
                if (s < 0) return full();
                setGlitchGrid (e, track, s, id == stutter ? "........88663333" : "............ssss");
                setParam (e, track, s, 0, 0.0f);
                add (fx (s, 0), onlyIn (lastBar, b, 1.0f), lastBar - 1, b);
                break;
            }
            case fadeIn:
            case fadeOut:
            {
                const float v = currentVolume (e, track);
                if (id == fadeIn) add (volumeTarget (track), { { a, 0.0f }, { b, v } }, a, b);
                else              add (volumeTarget (track), { { a, v }, { b, 0.0f } }, a, b);
                break;
            }
            case gateIn:
            {
                const int s = slotFor (e, track, mixer::gate);
                if (s < 0) return full();
                setParam (e, track, s, 1, 1.0f);    // 1/16
                setParam (e, track, s, 2, 0.0f);
                setParam (e, track, s, 3, 0.2f);
                add (fx (s, 2), { { a, 0.0f }, { b - 1, 1.0f }, { b, 0.0f } }, a, b);
                break;
            }
            case sidechainUp:
            {
                if (track == 0)
                {
                    r.error = "Sul Master la pompa abbasserebbe anche la cassa: scegli la traccia di un basso, di un pad o degli accordi.";
                    return r;
                }
                const int kick = findKickTrack (e, track);
                if (kick <= 0)
                {
                    r.error = "Non trovo la traccia della cassa (un canale che si chiama Kick o Cassa, con la sua traccia nel mixer).";
                    return r;
                }
                const int s = slotFor (e, track, mixer::sidechain);
                if (s < 0) return full();
                setParam (e, track, s, 0, (float) kick);
                setParam (e, track, s, 1, 0.0f);
                add (fx (s, 1), { { a, 0.0f }, { b, 0.8f } }, a, b);
                break;
            }
            case widen:
            {
                const int s = slotFor (e, track, mixer::stereo);
                if (s < 0) return full();
                setParam (e, track, s, 0, 1.0f);
                add (fx (s, 0), { { a, norm (mixer::stereo, 0, 1.0f) }, { b - 2, norm (mixer::stereo, 0, 2.0f) }, { b, norm (mixer::stereo, 0, 1.0f) } }, a, b);
                break;
            }
            case reverbThrow:
            case reverbWash:
            {
                const int s = slotFor (e, track, mixer::reverb);
                if (s < 0) return full();
                setParam (e, track, s, 0, id == reverbThrow ? 0.85f : 0.9f);
                setParam (e, track, s, 1, 0.4f);
                setParam (e, track, s, 2, 1.0f);
                setParam (e, track, s, 3, 0.0f);
                if (id == reverbThrow)
                    add (fx (s, 3), { { lastBeat - 1, 0.0f }, { lastBeat, 0.85f }, { b, 0.65f }, { b + ticksPerBar / 2, 0.0f } }, lastBeat - 1, b + ticksPerBar / 2);
                else
                    add (fx (s, 3), { { a, 0.0f }, { b - 2, 0.75f }, { b, 0.0f } }, a, b);
                break;
            }
            case delayThrow:
            {
                const int s = slotFor (e, track, mixer::delay);
                if (s < 0) return full();
                setParam (e, track, s, 0, 2.0f);    // 3/16
                setParam (e, track, s, 1, 0.55f);
                setParam (e, track, s, 2, 0.0f);
                setParam (e, track, s, 3, 1.0f);
                setParam (e, track, s, 4, 5000.0f);
                add (fx (s, 2), { { lastBeat - 1, 0.0f }, { lastBeat, 0.7f }, { b, 0.6f }, { b + ticksPerBar * 3 / 4, 0.0f } }, lastBeat - 1, b + ticksPerBar * 3 / 4);
                break;
            }
            case lofi:
            {
                const int s = slotFor (e, track, mixer::bitcrusher);
                if (s < 0) return full();
                setParam (e, track, s, 0, 16.0f);
                setParam (e, track, s, 1, 3.0f);
                setParam (e, track, s, 2, 0.0f);
                add (fx (s, 0), { { a, norm (mixer::bitcrusher, 0, 16.0f) }, { b - 2, norm (mixer::bitcrusher, 0, 4.0f) }, { b, norm (mixer::bitcrusher, 0, 16.0f) } }, a, b);
                add (fx (s, 2), onlyIn (a, b, 1.0f), a - 1, b);
                break;
            }
            case vinylIn:
            {
                const int s = slotFor (e, track, mixer::vinyl);
                if (s < 0) return full();
                setParam (e, track, s, 0, 0.3f);
                setParam (e, track, s, 1, 0.4f);
                setParam (e, track, s, 2, 0.5f);
                setParam (e, track, s, 3, 5000.0f);
                setParam (e, track, s, 4, 0.4f);
                setParam (e, track, s, 5, 0.0f);
                add (fx (s, 5), { { a, 0.0f }, { b - 2, 1.0f }, { b, 0.0f } }, a, b);
                break;
            }
            default:
                r.error = "Effetto sconosciuto.";
                return r;
        }

        if (! ok)
        {
            r.error = "Ci sono gia' troppe automazioni (massimo " + juce::String (automation::maxLanes) + "): eliminane qualcuna.";
            return r;
        }
        r.ok = true;
        return r;
    }
}
