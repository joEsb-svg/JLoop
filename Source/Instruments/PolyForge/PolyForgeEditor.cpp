#include "PolyForgeEditor.h"

PolyForgeEditor::PolyForgeEditor (PolyForgeAudioProcessor& p)
    : NeonEditor (p, p.apvts, "PolyForge", juce::Colour (0xffb07cff), { "OSC", "FILTRO", "ENV", "LFO", "FX" },
                  { pf::ids::cutoff, "CUTOFF", {}, pf::ids::fltType },
                  { pf::ids::space, "SPAZIO", "RIVERBERO + DELAY", {} })
{
    using namespace pf::ids;

    // --- OSC
    auto& s1 = section (0, "OSCILLATORE 1", 1.0f, 2);
    choice (s1, osc1Wave);
    knob (s1, osc1Lvl, "LIVELLO");
    knob (s1, osc1Uni, "UNISON");
    knob (s1, osc1Det, "DETUNE");
    knob (s1, osc1Pw, "PW");

    auto& s2 = section (0, "OSCILLATORE 2", 1.2f, 3);
    choice (s2, osc2Wave);
    knob (s2, osc2Lvl, "LIVELLO");
    knob (s2, osc2Semi, "SEMITONI");
    knob (s2, osc2Fine, "FINE");
    knob (s2, osc2Uni, "UNISON");
    knob (s2, osc2Det, "DETUNE");

    auto& s3 = section (0, "SUB E RUMORE", 0.6f, 1);
    knob (s3, subLvl, "SUB");
    knob (s3, noiseLvl, "RUMORE");
    knob (s3, spread, "STEREO");

    // --- FILTRO
    auto& f = section (1, "FILTRO", 1.0f, 3);
    choice (f, fltType, "TIPO: ");
    knob (f, cutoff, "CUTOFF");
    knob (f, reso, "RISONANZA");
    knob (f, fltDrive, "DRIVE");
    knob (f, keyTrack, "KEYTRACK");
    knob (f, fltVel, "VELOCITY");

    auto& e = section (1, "INVILUPPO DEL FILTRO", 1.2f, 5);
    adsr (e, fltA, fltD, fltS, fltR);
    knob (e, fltEnv, "QUANTITA'");
    knob (e, fltA, "ATTACCO");
    knob (e, fltD, "DECAY");
    knob (e, fltS, "SUSTAIN");
    knob (e, fltR, "RILASCIO");

    // --- ENV
    auto& a = section (2, "VOLUME (ADSR)", 1.5f, 5);
    adsr (a, ampA, ampD, ampS, ampR);
    knob (a, ampA, "ATTACCO");
    knob (a, ampD, "DECAY");
    knob (a, ampS, "SUSTAIN");
    knob (a, ampR, "RILASCIO");
    knob (a, velSens, "VELOCITY");

    auto& v = section (2, "VOCE", 0.6f, 1);
    choice (v, mode);
    knob (v, glide, "GLIDE");

    // --- LFO
    auto& l = section (3, "LFO", 1.0f, 2);
    choice (l, lfoWave, "FORMA: ");
    choice (l, lfoSync, "A TEMPO");
    choice (l, lfoDiv, "DIVISIONE: ");
    knob (l, lfoRate, "VELOCITA'");

    auto& d = section (3, "COSA MUOVE", 1.0f, 4);
    knob (d, lfoPitch, "VIBRATO");
    knob (d, lfoCut, "FILTRO");
    knob (d, lfoAmp, "VOLUME");
    knob (d, lfoPwm, "PW");

    // --- FX
    auto& c = section (4, "CHORUS E DELAY", 1.2f, 3);
    choice (c, dlyTime, "DELAY ");
    knob (c, chorus, "CHORUS");
    knob (c, dlyMix, "DELAY");
    knob (c, dlyFb, "FEEDBACK");
    auto& r = section (4, "RIVERBERO E USCITA", 1.0f, 3);
    knob (r, revMix, "RIVERBERO");
    knob (r, revSize, "STANZA");
    knob (r, master, "VOLUME");

    showTab (0);
}
