"""
Kit 2026: tanti kit di batteria per DrumForge, ognuno con i SUOI colpi (kick, snare, clap, hat, perc...).
Ogni kit nasce da una "base" di genere piu' le sue differenze, cosi' i kit dello stesso genere suonano diversi.

Scrive anche Source/Instruments/DrumForge/GeneratedKits.h (i 16 pad di ogni kit) tramite write_kits_header().
Gli id dei suoni ("x_k_<kit>_<pezzo>") non si cambiano mai: sono salvati nei progetti.
"""
import os
import numpy as np
from dsp import *
from registry import sound
import electro2012 as ex
from electro2012 import polish, kick_x, snare_x, clap_x, hat_x, crash_x, tom_x, DRUM_COL

DRUMS = "Batteria"

# ============================================================================ generatori dei pezzi
def perc_x(kind, d, seed, f=1.0, tau=1.0, dist=1.0):
    """Percussioni di vario tipo; f scala l'intonazione, tau la durata."""
    t = tt(d)
    r = rng(seed)
    if kind == "fm":
        fr = 900 * f
        p = phase_of(fr * 0.4 + fr * 0.6 * np.exp(-t / 0.012))
        x = np.sin(p + 3.0 * np.exp(-t / (0.02 * tau)) * np.sin(p * r.uniform(1.4, 3.6))) * np.exp(-t / (0.05 * tau))
    elif kind == "cowbell":
        x = bp(np.sign(np.sin(2 * np.pi * 540 * f * t)) + np.sign(np.sin(2 * np.pi * 800 * f * t)), 500 * f, 3000 * f) * np.exp(-t / (0.09 * tau))
    elif kind == "shaker":
        x = bp(noise(d, seed), 5000, 13000) * np.clip(t / 0.01, 0, 1) * np.exp(-np.maximum(0, t - 0.01) / (0.035 * tau))
    elif kind == "conga":
        fr = 220 * f
        x = np.sin(phase_of(fr + fr * 0.4 * np.exp(-t / 0.01))) * np.exp(-t / (0.12 * tau)) + bp(noise(d, seed), 1500, 5000) * np.exp(-t / 0.006) * 0.4
    elif kind == "clave":
        x = np.sin(2 * np.pi * 2500 * f * t) * np.exp(-t / (0.02 * tau))
    elif kind == "rim":
        x = (np.sin(2 * np.pi * 1700 * f * t) + 0.6 * np.sin(2 * np.pi * 820 * f * t)) * np.exp(-t / (0.015 * tau)) + ex.click(d, seed, 3000, 10000, 0.001, 0.8)
    elif kind == "block":
        x = modal(700 * f, d, [1.0, 2.76, 5.4], [1.0, 0.5, 0.3], [0.05 * tau, 0.03 * tau, 0.015 * tau], seed=seed)
    elif kind == "zap":
        x = np.sin(phase_of(200 * f + 5000 * f * np.exp(-t / (0.008 * tau)))) * np.exp(-t / (0.04 * tau))
    elif kind == "noise":
        x = bp(noise(d, seed), 800 * f, 6000 * f) * np.exp(-t / (0.03 * tau))
    elif kind == "metal":
        x = metallic(d, 400 * f, 5000 * f, 14, 0.1 * tau, seed)
    elif kind == "tick":
        x = hp(noise(d, seed), 6000) * np.exp(-t / (0.006 * tau)) + np.sin(2 * np.pi * 4200 * f * t) * np.exp(-t / (0.004 * tau)) * 0.6
    else:
        raise ValueError(kind)
    return drive(x * dist, dist) if dist > 1.0 else x


# ============================================================================ basi di genere
# kick: d f0 f1 ptau btau clk dist tok sub tail_tau | snare: d tone drop ttau ntau n_lo n_hi crack dist room clap_layer
# clap: d bursts spacing tail lo hi room | hat/ohat: d tau pitch hpf noise_amt dist metal | tom: d f0 f1 tau dist
# cym: d tau lo hi n nz dist | perc1/perc2: (tipo, f, tau, dist)
BASES = {
    "psy": dict(
        kick=dict(d=0.30, f0=300, f1=50, ptau=0.011, btau=0.09, clk=0.9, dist=2.2, tok=0.3),
        snare=dict(d=0.30, tone=270, drop=0.3, ttau=0.03, ntau=0.08, n_lo=2200, n_hi=11000, crack=1.0, dist=1.6, room=0.08),
        clap=dict(d=0.45, bursts=4, spacing=0.010, tail=0.10, lo=1100, hi=3200, room=0.2),
        hat=dict(d=0.10, tau=0.022, pitch=1.6, hpf=8000, noise_amt=0.25, dist=1.2, metal=1.0),
        ohat=dict(d=0.40, tau=0.13, pitch=1.5, hpf=7500, noise_amt=0.3, dist=1.2, metal=1.0),
        tom=dict(d=0.40, f0=400, f1=220, tau=0.12, dist=1.8),
        cym=dict(d=1.0, tau=0.35, lo=3000, hi=11000, n=24, nz=0.4),
        perc1=("fm", 1.0, 1.0, 1.0), perc2=("tick", 1.0, 1.0, 1.0)),
    "hitech": dict(
        kick=dict(d=0.18, f0=380, f1=60, ptau=0.006, btau=0.05, clk=1.3, dist=2.4, tok=0.5),
        snare=dict(d=0.22, tone=320, drop=0.2, ttau=0.02, ntau=0.05, n_lo=3000, n_hi=12000, crack=1.2, dist=1.8),
        clap=dict(d=0.20, bursts=3, spacing=0.006, tail=0.04, lo=1500, hi=5000),
        hat=dict(d=0.05, tau=0.008, pitch=2.0, hpf=10000, noise_amt=0.3, dist=1.5, metal=0.9),
        ohat=dict(d=0.25, tau=0.07, pitch=1.9, hpf=9000, noise_amt=0.3, dist=1.4, metal=0.9),
        tom=dict(d=0.25, f0=900, f1=300, tau=0.05, dist=2.0),
        cym=dict(d=0.6, tau=0.2, lo=4000, hi=13000, n=20, nz=0.5),
        perc1=("zap", 1.0, 1.0, 1.0), perc2=("fm", 1.6, 0.6, 1.5)),
    "techno": dict(
        kick=dict(d=0.50, f0=190, f1=47, ptau=0.030, btau=0.20, clk=0.5, dist=4.0, sub=0.3, tail_tau=0.2),
        snare=dict(d=0.35, tone=220, drop=0.4, ttau=0.04, ntau=0.10, n_lo=1500, n_hi=9000, crack=0.7, dist=2.5, room=0.12),
        clap=dict(d=0.45, bursts=4, spacing=0.011, tail=0.12, lo=900, hi=2800, room=0.18),
        hat=dict(d=0.12, tau=0.030, pitch=1.0, hpf=7500, noise_amt=0.5, dist=1.5, metal=0.6),
        ohat=dict(d=0.50, tau=0.18, pitch=1.0, hpf=7000, noise_amt=0.5, dist=1.5, metal=0.6),
        tom=dict(d=0.50, f0=220, f1=120, tau=0.18, dist=2.0),
        cym=dict(d=1.0, tau=0.3, lo=3500, hi=12000, n=20, nz=0.4),
        perc1=("metal", 1.0, 1.0, 2.0), perc2=("rim", 1.0, 1.0, 1.0)),
    "house": dict(
        kick=dict(d=0.45, f0=200, f1=52, ptau=0.025, btau=0.16, clk=0.5, dist=1.8, tok=0.2),
        snare=dict(d=0.35, tone=230, drop=0.3, ttau=0.04, ntau=0.10, n_lo=1800, n_hi=10000, crack=0.8, dist=1.5, room=0.12, clap_layer=0.3),
        clap=dict(d=0.45, bursts=4, spacing=0.010, tail=0.12, lo=1000, hi=3000, room=0.18),
        hat=dict(d=0.10, tau=0.025, pitch=1.0, hpf=7500, noise_amt=0.6, dist=1.0, metal=0.5),
        ohat=dict(d=0.45, tau=0.16, pitch=1.0, hpf=7000, noise_amt=0.6, dist=1.0, metal=0.5),
        tom=dict(d=0.45, f0=260, f1=150, tau=0.16, dist=1.5),
        cym=dict(d=1.4, tau=0.45, lo=3000, hi=13000, n=30, nz=0.5),
        perc1=("shaker", 1.0, 1.0, 1.0), perc2=("conga", 1.0, 1.0, 1.0)),
    "dnb": dict(
        kick=dict(d=0.30, f0=280, f1=56, ptau=0.012, btau=0.09, clk=1.0, dist=2.2, tok=0.4),
        snare=dict(d=0.35, tone=200, drop=0.4, ttau=0.05, ntau=0.10, n_lo=1800, n_hi=11000, crack=1.4, dist=2.5, room=0.08),
        clap=dict(d=0.35, bursts=3, spacing=0.009, tail=0.08, lo=1300, hi=3800, room=0.1),
        hat=dict(d=0.08, tau=0.018, pitch=1.15, hpf=8500, noise_amt=0.6, dist=1.3, metal=0.5),
        ohat=dict(d=0.35, tau=0.12, pitch=1.1, hpf=8000, noise_amt=0.6, dist=1.3, metal=0.5),
        tom=dict(d=0.40, f0=300, f1=170, tau=0.12, dist=2.0),
        cym=dict(d=1.2, tau=0.4, lo=3000, hi=13000, n=28, nz=0.5),
        perc1=("rim", 1.0, 1.0, 1.0), perc2=("shaker", 1.0, 1.0, 1.0)),
    "bass": dict(   # dubstep, riddim, trap, phonk, drill
        kick=dict(d=0.60, f0=260, f1=46, ptau=0.030, btau=0.22, clk=0.8, dist=2.2, tok=0.3, sub=0.4, tail_tau=0.25),
        snare=dict(d=0.50, tone=190, drop=0.6, ttau=0.06, ntau=0.15, n_lo=1300, n_hi=8500, crack=0.9, dist=2.6, room=0.18, clap_layer=0.5),
        clap=dict(d=0.50, bursts=4, spacing=0.011, tail=0.13, lo=900, hi=2800, room=0.15),
        hat=dict(d=0.08, tau=0.015, pitch=1.1, hpf=8000, noise_amt=0.5, dist=1.2, metal=0.6),
        ohat=dict(d=0.40, tau=0.15, pitch=1.1, hpf=7500, noise_amt=0.5, dist=1.2, metal=0.6),
        tom=dict(d=0.60, f0=280, f1=90, tau=0.24, dist=2.5),
        cym=dict(d=1.6, tau=0.5, lo=2500, hi=13000, n=36, nz=0.6, dist=1.5),
        perc1=("rim", 1.0, 1.0, 1.0), perc2=("noise", 1.0, 1.0, 1.0)),
    "hard": dict(   # hardstyle, rawstyle, hardcore
        kick=dict(d=0.55, f0=380, f1=52, ptau=0.035, btau=0.25, clk=0.7, dist=7.0, tok=0.2),
        snare=dict(d=0.45, tone=210, drop=0.5, ttau=0.06, ntau=0.14, n_lo=1200, n_hi=9000, crack=0.9, dist=4.0, room=0.2),
        clap=dict(d=0.55, bursts=4, spacing=0.012, tail=0.14, lo=900, hi=2800, room=0.3),
        hat=dict(d=0.12, tau=0.03, pitch=0.95, hpf=7000, noise_amt=0.6, dist=3.0, metal=0.6),
        ohat=dict(d=0.55, tau=0.22, pitch=0.95, hpf=6500, noise_amt=0.6, dist=3.0, metal=0.6),
        tom=dict(d=0.55, f0=260, f1=110, tau=0.22, dist=5.0),
        cym=dict(d=1.6, tau=0.5, lo=2500, hi=12000, n=36, nz=0.6, dist=2.5),
        perc1=("metal", 0.8, 1.0, 3.0), perc2=("noise", 0.7, 1.5, 2.0)),
    "retro": dict(  # garage, breakbeat, synthwave, lo-fi, electro funk
        kick=dict(d=0.40, f0=170, f1=55, ptau=0.025, btau=0.14, clk=0.4, dist=1.4, tok=0.15),
        snare=dict(d=0.40, tone=210, drop=0.3, ttau=0.05, ntau=0.13, n_lo=1200, n_hi=8000, crack=0.6, dist=1.4, room=0.15),
        clap=dict(d=0.45, bursts=4, spacing=0.011, tail=0.12, lo=900, hi=2600, room=0.15),
        hat=dict(d=0.10, tau=0.028, pitch=0.9, hpf=6500, noise_amt=0.5, dist=1.0, metal=0.6),
        ohat=dict(d=0.45, tau=0.17, pitch=0.9, hpf=6000, noise_amt=0.5, dist=1.0, metal=0.6),
        tom=dict(d=0.55, f0=220, f1=120, tau=0.2, dist=1.4),
        cym=dict(d=1.4, tau=0.45, lo=2800, hi=12000, n=30, nz=0.5),
        perc1=("clave", 1.0, 1.0, 1.0), perc2=("conga", 1.0, 1.0, 1.0)),
}

# Kit: (chiave, nome, base, variazioni, extra) -- le variazioni cambiano solo alcuni parametri della base.
# extra: 4 pad presi dalla libreria (basso/fx/voce), con il loro nome
KITS = [
    # --- Psytrance
    ("psyfull", "Psy Full-on", "psy", {}, [("x_bass_psy_roll", "Basso"), ("x_fx_psy_zap_down", "Zap"), ("x_fx_squelch", "Squelch"), ("x_vox_ah_chop", "Vox")]),
    ("psyforest", "Psy Dark Forest", "psy", dict(kick=dict(f0=240, f1=44, btau=0.075, dist=3.5, clk=1.3), snare=dict(tone=180, dist=3.0, room=0.25),
        hat=dict(pitch=1.3, dist=2.5), perc1=("metal", 0.7, 1.4, 2.0), perc2=("noise", 0.6, 1.2, 1.0)),
        [("x_bass_psy_dark", "Basso"), ("x_fx_alien", "Alieno"), ("x_atmo_metal", "Drone"), ("x_vox_whisper", "Sussurro")]),
    ("psygoa", "Psy Goa 90s", "psy", dict(kick=dict(d=0.4, f0=210, btau=0.13, dist=1.6, clk=0.5), snare=dict(room=0.3, clap_layer=0.4),
        clap=dict(room=0.35), hat=dict(metal=0.5, noise_amt=0.6), perc1=("conga", 1.2, 0.8, 1.0), perc2=("shaker", 1.0, 1.2, 1.0)),
        [("x_bass_psy_square", "Basso"), ("x_pluck_goa", "Pluck"), ("x_fx_psy_sweep", "Sweep"), ("x_vox_choir_ooh", "Coro")]),
    ("psyprog", "Psy Progressive", "psy", dict(kick=dict(d=0.42, f0=200, btau=0.15, dist=1.5, clk=0.4, sub=0.2, tail_tau=0.15), snare=dict(dist=1.2, room=0.2),
        hat=dict(pitch=1.2, metal=0.6, dist=1.0), perc1=("shaker", 1.0, 1.0, 1.0), perc2=("block", 1.0, 1.0, 1.0)),
        [("x_bass_psy_fm", "Basso"), ("x_pad_space", "Pad"), ("x_fx_psy_sweep", "Sweep"), ("x_vox_breath", "Respiro")]),
    ("psytwilight", "Psy Twilight", "psy", dict(kick=dict(f0=280, f1=48, btau=0.10, dist=2.6), snare=dict(tone=240, room=0.15), hat=dict(pitch=1.8),
        perc1=("fm", 1.3, 0.8, 1.0), perc2=("zap", 0.7, 1.5, 1.0)),
        [("x_bass_psy_roll", "Basso"), ("x_lead_psy", "Lead"), ("x_fx_laser_psy", "Laser"), ("x_vox_robot_yeah", "Robot")]),
    ("psysuomi", "Psy Suomi (strano)", "psy", dict(kick=dict(f0=320, btau=0.07, tok=0.6), snare=dict(tone=360, crack=1.5), clap=dict(bursts=6, spacing=0.016),
        hat=dict(pitch=2.2, metal=0.4), perc1=("cowbell", 1.3, 0.7, 1.0), perc2=("clave", 0.8, 1.5, 1.0)),
        [("x_bass_psy_square", "Basso"), ("x_fx_alien", "Alieno"), ("x_vox_yo", "Yo"), ("x_fx_glitch_burst", "Glitch")]),
    ("psynight", "Psy Night", "psy", dict(kick=dict(f0=260, f1=47, dist=2.8, clk=0.7), snare=dict(tone=200, room=0.3, dist=2.0), hat=dict(pitch=1.4, tau=0.03),
        ohat=dict(tau=0.2), perc1=("fm", 0.6, 1.2, 1.5), perc2=("noise", 1.2, 0.8, 1.0)),
        [("x_bass_psy_fm", "Basso"), ("x_stab_psy_dark", "Stab"), ("x_atmo_drone_dark", "Drone"), ("x_fx_psy_zap_down", "Zap")]),
    # --- Hi-tech / psycore
    ("hitech200", "Hi-tech 200", "hitech", {}, [("x_bass_hitech", "Basso"), ("x_fx_psy_zap_up", "Zap su"), ("x_fx_laser_psy", "Laser"), ("x_fx_glitch_burst", "Glitch")]),
    ("hitechglitch", "Hi-tech Glitch", "hitech", dict(kick=dict(dist=3.5, clk=1.6), snare=dict(dist=3.0), hat=dict(dist=3.0, pitch=2.4),
        perc1=("tick", 1.5, 0.6, 2.0), perc2=("zap", 1.6, 0.5, 2.0)),
        [("x_bass_hitech", "Basso"), ("x_fx_glitch_burst", "Glitch"), ("x_vox_robot_yeah", "Robot"), ("x_fx_alien", "Alieno")]),
    ("psycore", "Psycore", "hitech", dict(kick=dict(d=0.25, f0=330, f1=52, btau=0.07, dist=4.0), snare=dict(tone=200, dist=4.0, room=0.2),
        clap=dict(room=0.25), hat=dict(pitch=1.4, metal=1.0), perc1=("metal", 1.2, 0.8, 3.0), perc2=("noise", 0.5, 1.0, 2.5)),
        [("x_bass_psy_dark", "Basso"), ("x_atmo_metal", "Drone"), ("x_vox_whisper", "Sussurro"), ("x_fx_sub_drop", "Sub drop")]),
    # --- Techno
    ("techberlin", "Techno Berlino", "techno", {}, [("techno_rumble", "Rumble"), ("x_stab_techno", "Stab"), ("x_atmo_drone_dark", "Drone"), ("x_fx_psy_sweep", "Sweep")]),
    ("techind", "Techno Industriale", "techno", dict(kick=dict(dist=7.0, clk=0.9), snare=dict(dist=5.0, room=0.3), clap=dict(room=0.35),
        hat=dict(dist=4.0), perc1=("metal", 0.7, 1.5, 4.0), perc2=("noise", 0.5, 2.0, 3.0)),
        [("x_perc_industrial", "Industriale"), ("x_perc_metal_hit", "Metallo"), ("x_atmo_metal", "Drone"), ("x_fx_impact", "Impatto")]),
    ("techmin", "Techno Minimal", "techno", dict(kick=dict(d=0.3, btau=0.11, dist=1.5, sub=0.0), snare=dict(d=0.2, ntau=0.05, room=0.0), clap=dict(tail=0.05, room=0.05),
        hat=dict(tau=0.012, noise_amt=0.3), perc1=("clave", 1.0, 0.7, 1.0), perc2=("block", 1.3, 0.6, 1.0)),
        [("x_perc_psy_wood", "Legno"), ("x_perc_psy_drop", "Goccia"), ("x_vox_ah_chop", "Vox"), ("x_stab_techno", "Stab")]),
    ("techacid", "Techno Acid", "techno", dict(kick=dict(f0=230, dist=3.0), hat=dict(metal=0.8, pitch=1.2), perc1=("cowbell", 1.0, 1.0, 1.0), perc2=("rim", 1.1, 1.0, 1.0)),
        [("x_acid_303", "Acid"), ("x_acid_303_accent", "Acid accent"), ("x_fx_squelch", "Squelch"), ("x_vox_go", "Go!")]),
    ("hardtechno", "Hard Techno", "techno", dict(kick=dict(d=0.45, f0=260, ptau=0.02, dist=8.0, clk=1.0), snare=dict(dist=4.0), clap=dict(room=0.25),
        hat=dict(dist=3.0, tau=0.02), ohat=dict(dist=3.0), perc1=("metal", 0.9, 0.8, 3.0), perc2=("zap", 0.5, 1.0, 2.0)),
        [("x_kick_techno_hard", "Kick 3"), ("x_fx_sub_drop", "Sub drop"), ("x_vox_hey_shout", "Hey!"), ("x_crash_dist", "Crash dist.")]),
    # --- House
    ("housedeep", "House Deep", "house", dict(kick=dict(d=0.5, f0=170, btau=0.18, dist=1.3, clk=0.3), snare=dict(room=0.2), hat=dict(noise_amt=0.8, metal=0.3, tau=0.03)),
        [("x_pad_warm", "Pad"), ("x_stab_organ", "Organo"), ("x_vox_oh_long", "Vox"), ("x_atmo_ocean", "Onde")]),
    ("techhouse", "Tech House", "house", dict(kick=dict(f0=220, dist=2.4, clk=0.7), clap=dict(room=0.12), perc1=("rim", 1.0, 1.0, 1.0), perc2=("shaker", 1.1, 0.8, 1.0)),
        [("x_bass_electro_wob", "Basso"), ("x_vox_yo", "Yo"), ("x_perc_cowbell", "Cowbell"), ("x_stab_techno", "Stab")]),
    ("electro09", "Electro House 2009", "house", dict(kick=dict(f0=260, dist=2.8, clk=0.8, tok=0.35), snare=dict(dist=2.5, clap_layer=0.5), hat=dict(dist=2.0),
        perc1=("zap", 1.0, 1.0, 1.0), perc2=("metal", 1.1, 0.8, 1.5)),
        [("x_bass_growl", "Growl"), ("x_stab_dutch", "Stab Dutch"), ("x_vox_hey", "Hey"), ("x_fx_zap", "Zap")]),
    ("bigroom", "Big Room", "house", dict(kick=dict(d=0.7, f0=250, f1=48, btau=0.26, dist=2.4, sub=0.3, tail_tau=0.3), snare=dict(room=0.3, clap_layer=0.6),
        clap=dict(room=0.3), cym=dict(d=2.0, tau=0.7)),
        [("x_lead_bigroom", "Lead"), ("x_fx_impact", "Impatto"), ("x_vox_go", "Go!"), ("x_fx_riser_tone", "Riser")]),
    ("futurehouse", "Future House", "house", dict(kick=dict(d=0.35, f0=240, btau=0.12, clk=0.9, dist=2.0), snare=dict(tone=260, crack=1.1),
        hat=dict(pitch=1.2, tau=0.018), perc1=("fm", 1.2, 1.0, 1.0), perc2=("clave", 1.2, 1.0, 1.0)),
        [("x_bass_wub", "Basso"), ("x_pluck_electro", "Pluck"), ("x_vox_ah_chop", "Vox"), ("x_vox_ee", "Vox Ii")]),
    # --- DnB
    ("dnbliquid", "DnB Liquido", "dnb", dict(kick=dict(dist=1.5, clk=0.6), snare=dict(dist=1.5, room=0.25, crack=0.9), hat=dict(noise_amt=0.8, metal=0.3),
        perc1=("shaker", 1.0, 1.0, 1.0), perc2=("conga", 1.1, 1.0, 1.0)),
        [("x_pad_glass", "Pad"), ("x_bass_neuro_reese", "Reese"), ("x_vox_oh_long", "Vox"), ("x_atmo_rain", "Pioggia")]),
    ("jumpup", "DnB Jump Up", "dnb", dict(kick=dict(f0=300, dist=3.0), snare=dict(dist=3.5, crack=1.6), perc1=("zap", 0.8, 1.0, 1.5), perc2=("noise", 1.0, 1.0, 1.0)),
        [("x_bass_neuro_growl", "Growl"), ("x_bass_wub", "Wub"), ("x_vox_hey_shout", "Hey!"), ("x_fx_siren", "Sirena")]),
    ("neuro2", "Neurofunk Scuro", "dnb", dict(kick=dict(dist=3.5, f1=52), snare=dict(tone=180, dist=4.5, room=0.12), hat=dict(dist=2.5),
        perc1=("metal", 1.0, 0.7, 2.5), perc2=("tick", 1.0, 1.0, 1.5)),
        [("x_bass_neuro_stab", "Basso"), ("x_bass_neuro_growl", "Growl"), ("x_fx_glitch_burst", "Glitch"), ("x_atmo_metal", "Drone")]),
    ("jungle", "Jungle", "dnb", dict(kick=dict(d=0.4, f0=180, btau=0.14, dist=1.3, clk=0.4), snare=dict(tone=240, drop=0.2, dist=1.3, room=0.2, crack=0.8),
        hat=dict(metal=0.4, noise_amt=0.8), perc1=("conga", 0.9, 1.0, 1.0), perc2=("rim", 0.9, 1.0, 1.0)),
        [("x_bass_sub", "Sub"), ("x_vox_yo", "Yo"), ("x_pad_dark", "Pad"), ("x_fx_siren", "Sirena")]),
    # --- Bass music
    ("dubstep11", "Dubstep 2011", "bass", {}, [("x_bass_yoi", "Yoi"), ("x_bass_growl", "Growl"), ("x_fx_impact", "Impatto"), ("x_vox_drop", "Drop")]),
    ("riddim", "Riddim", "bass", dict(kick=dict(d=0.4, btau=0.14, dist=3.0), snare=dict(d=0.35, room=0.08, dist=3.5), hat=dict(tau=0.012),
        perc1=("zap", 0.6, 1.2, 1.5), perc2=("metal", 0.6, 1.0, 2.0)),
        [("x_bass_neuro_stab", "Basso"), ("x_bass_square", "Quadra"), ("x_vox_robot_yeah", "Robot"), ("x_fx_sub_drop", "Sub drop")]),
    ("trapdark", "Trap Scuro", "bass", dict(kick=dict(d=0.8, f0=160, f1=42, btau=0.4, dist=1.4, clk=0.2, sub=0.6, tail_tau=0.5), snare=dict(tone=230, dist=1.4, clap_layer=0.0, room=0.12),
        hat=dict(tau=0.012, pitch=1.15), perc1=("rim", 1.0, 1.0, 1.0), perc2=("noise", 1.5, 0.6, 1.0)),
        [("x_bass_808_long", "808"), ("x_bass_808_dist", "808 dist."), ("x_pad_dark", "Pad"), ("x_vox_yo", "Yo")]),
    ("phonk", "Phonk", "bass", dict(kick=dict(d=0.7, f0=170, f1=45, btau=0.35, dist=3.0, sub=0.5, tail_tau=0.4), snare=dict(tone=210, dist=2.5, room=0.1),
        hat=dict(tau=0.014, dist=2.0), perc1=("cowbell", 1.0, 1.3, 2.0), perc2=("cowbell", 1.25, 1.0, 2.5)),
        [("x_bass_808_dist", "808"), ("x_vox_go", "Go!"), ("x_vox_drop", "Drop"), ("x_crash_reverse", "Crash rev.")]),
    ("drill", "Drill", "bass", dict(kick=dict(d=0.8, f0=150, f1=40, btau=0.45, dist=1.5, sub=0.6, tail_tau=0.5), snare=dict(tone=250, drop=0.2, dist=1.6, room=0.1, clap_layer=0.0),
        hat=dict(tau=0.01, pitch=1.25), perc1=("rim", 1.1, 0.8, 1.0), perc2=("clave", 0.9, 1.0, 1.0)),
        [("x_bass_808_long", "808"), ("x_bass_808_short", "808 corto"), ("x_pad_choir", "Coro"), ("x_vox_hey_shout", "Hey")]),
    # --- Hard
    ("euphoric", "Hardstyle Euphoric", "hard", dict(kick=dict(dist=5.0), clap=dict(room=0.35), hat=dict(dist=1.5), perc1=("fm", 1.0, 1.0, 1.0), perc2=("shaker", 1.0, 1.0, 1.0)),
        [("x_kick_reverse_bass", "Reverse bass"), ("x_lead_hardstyle", "Screech"), ("x_pad_trance", "Pad"), ("x_vox_hey_shout", "Hey!")]),
    ("rawstyle", "Rawstyle", "hard", dict(kick=dict(f0=480, dist=10.0, ptau=0.05), snare=dict(dist=6.0), perc1=("metal", 0.6, 1.5, 4.0), perc2=("noise", 0.5, 2.0, 4.0)),
        [("x_kick_rawstyle", "Kick raw"), ("x_lead_screech", "Screech"), ("x_vox_go", "Go!"), ("x_fx_impact", "Impatto")]),
    ("gabber", "Hardcore Gabber", "hard", dict(kick=dict(d=0.4, f0=300, f1=60, ptau=0.02, btau=0.15, dist=12.0, clk=1.0), snare=dict(tone=240, dist=6.0),
        hat=dict(tau=0.02, dist=4.0), perc1=("zap", 0.8, 1.0, 3.0), perc2=("metal", 1.0, 0.7, 4.0)),
        [("x_kick_hardstyle", "Kick 2"), ("x_vox_hey_shout", "Hey!"), ("x_fx_siren", "Sirena"), ("x_crash_dist", "Crash dist.")]),
    # --- Retro e altro
    ("ukgarage", "UK Garage", "retro", dict(kick=dict(d=0.35, f0=190, btau=0.12, dist=1.6, clk=0.6), snare=dict(tone=250, crack=1.0, room=0.08), hat=dict(tau=0.02, pitch=1.05),
        perc1=("rim", 1.0, 1.0, 1.0), perc2=("shaker", 1.0, 0.8, 1.0)),
        [("x_bass_sub", "Sub"), ("x_stab_organ", "Organo"), ("x_vox_ah_chop", "Vox"), ("x_vox_oh", "Vox Oh")]),
    ("breakbeat", "Breakbeat", "retro", dict(kick=dict(d=0.35, f0=200, dist=2.0, clk=0.7), snare=dict(tone=200, crack=1.0, dist=2.0, room=0.2), hat=dict(noise_amt=0.8, metal=0.3),
        perc1=("conga", 1.0, 1.0, 1.0), perc2=("noise", 1.0, 1.0, 1.0)),
        [("x_bass_saw", "Basso"), ("x_vox_yo", "Yo"), ("x_fx_siren", "Sirena"), ("x_fx_reverse_snare", "Snare rev.")]),
    ("synthwave", "Synthwave", "retro", dict(kick=dict(d=0.45, f0=150, f1=52, btau=0.18, dist=1.2), snare=dict(d=0.9, tone=190, room=0.55, ntau=0.2, clap_layer=0.3),
        clap=dict(room=0.45), tom=dict(d=0.8, f0=300, f1=110, tau=0.3), perc1=("clave", 0.9, 1.0, 1.0), perc2=("conga", 1.2, 0.8, 1.0)),
        [("x_pad_pwm", "Pad"), ("x_lead_square", "Lead"), ("x_pad_strings", "Archi"), ("x_atmo_shimmer", "Shimmer")]),
    ("lofi", "Lo-Fi Hip Hop", "retro", dict(kick=dict(d=0.45, f0=140, f1=55, btau=0.16, dist=1.2, clk=0.2), snare=dict(d=0.4, tone=200, n_hi=6000, crack=0.3, room=0.2),
        hat=dict(hpf=5000, noise_amt=0.8, metal=0.2, tau=0.035), ohat=dict(hpf=5000, metal=0.2), perc1=("shaker", 0.7, 1.4, 1.0), perc2=("block", 0.8, 1.0, 1.0)),
        [("x_pad_warm", "Pad"), ("x_vox_breath", "Respiro"), ("x_atmo_rain", "Pioggia"), ("x_bass_sub", "Sub")]),
    ("electrofunk", "Electro Funk", "retro", dict(kick=dict(d=0.6, f0=170, f1=48, btau=0.25, dist=1.6, sub=0.4, tail_tau=0.3), snare=dict(tone=230, crack=0.9, room=0.1),
        hat=dict(pitch=1.0, metal=0.9, noise_amt=0.2), perc1=("cowbell", 1.0, 1.0, 1.0), perc2=("clave", 1.0, 1.0, 1.0)),
        [("x_bass_square", "Basso"), ("x_vox_robot_yeah", "Robot"), ("x_fx_laser", "Laser"), ("x_stab_brass", "Brass")]),
]

PIECES = [  # (pezzo, famiglia di polish, categoria, nome)
    ("kick", "kick", "Kick", "Kick"), ("snare", "snare", "Rullanti", "Snare"), ("clap", "clap", "Clap e schiocchi", "Clap"),
    ("hat", "hat", "Hi-hat chiusi", "Hi-hat"), ("ohat", "ohat", "Hi-hat aperti", "Hi-hat aperto"), ("tom", "tom", "Tom", "Tom"),
    ("cym", "cym", "Piatti", "Piatto"), ("perc1", "perc", "Percussioni", "Perc"), ("perc2", "perc", "Percussioni", "Perc 2"),
]
COL = {"kick": "kick", "snare": "snare", "clap": "clap", "hat": "hat", "ohat": "ohat", "tom": "tom", "cym": "cym", "perc1": "perc", "perc2": "perc"}


JITTER_KEEP = {"d", "bursts", "n"}


def piece_params(kit_index, base, overrides, piece):
    """Base del genere + variazioni del kit + una piccola variazione "di carattere" diversa per ogni kit
    (intonazione, tempi, saturazione): cosi' due kit dello stesso genere non hanno mai lo stesso colpo."""
    p = BASES[base][piece]
    o = overrides.get(piece)
    r = rng(9000 + kit_index * 101 + sum(map(ord, piece)))
    j = lambda amt: float(np.exp(r.normal(0.0, amt)))
    if isinstance(p, tuple):
        kind, f, tau, dist = o if o is not None else p
        return (kind, f * j(0.12), tau * j(0.15), max(1.0, dist * j(0.15)))
    q = dict(p)
    if o:
        q.update(o)
    for k, v in list(q.items()):
        if k in JITTER_KEEP or not isinstance(v, (int, float)) or v == 0:
            continue
        amt = 0.06 if k in ("f1", "tone", "pitch", "lo", "hi", "n_lo", "n_hi", "hpf") else 0.15
        q[k] = v * j(amt)
    if "dist" in q:
        q["dist"] = max(1.0, q["dist"])
    if "pitch" in q:
        q["pitch"] = min(2.6, q["pitch"])
    return q


def render_piece(piece, params, seed):
    if piece == "kick":
        return kick_x(seed=seed, **params)
    if piece == "snare":
        return snare_x(seed=seed, **params)
    if piece == "clap":
        return clap_x(seed=seed, **params)
    if piece in ("hat", "ohat"):
        return hat_x(seed=seed, **params)
    if piece == "tom":
        return tom_x(seed=seed, **params)
    if piece == "cym":
        return crash_x(seed=seed, **params)
    kind, f, tau, dist = params
    d = {"shaker": 0.15, "conga": 0.35, "clave": 0.12, "rim": 0.15, "block": 0.25, "zap": 0.2, "noise": 0.2, "metal": 0.4, "tick": 0.08, "fm": 0.25, "cowbell": 0.35}[kind] * max(1.0, tau)
    return perc_x(kind, d, seed, f, tau, dist)


def sid_of(key, piece):
    return "x_k_%s_%s" % (key, piece)


for ki, (key, name, base, ov, extra) in enumerate(KITS):
    for pi, (piece, family, cat, pname) in enumerate(PIECES):
        params = piece_params(ki, base, ov, piece)
        seed = 5000 + ki * 37 + pi * 5

        def _make(piece=piece, params=params, seed=seed, family=family):
            return lambda: polish(render_piece(piece, params, seed), family)
        sound(sid_of(key, piece), "%s %s" % (pname, name), DRUMS, cat, colour=DRUM_COL[COL[piece]])(_make())


# ============================================================================ i 16 pad di ogni kit (per DrumForge)
def kit_pads(key, extra):
    s = lambda p: sid_of(key, p)
    pads = [
        (s("kick"), "Kick", 0, 1.0, 0, 0, 0, 0.0),
        (s("kick"), "Kick corto", 2, 0.45, -1, 0, 0, 0.0),
        (s("snare"), "Snare", 0, 1.0, 0, 0, 0, 0.0),
        (s("clap"), "Clap", 0, 1.0, -1, 0, 0, 0.0),
        (s("perc1"), "Perc", 0, 1.0, -3, 0.2, 0, 0.0),
        (s("hat"), "Hi-hat", 0, 1.0, -3, 0.1, 1, 0.0),
        (s("hat"), "Hi-hat corto", 2, 0.45, -4, -0.1, 1, 0.0),
        (s("ohat"), "Hi-hat aperto", 0, 1.0, -3, 0.1, 1, 0.0),
        (s("tom"), "Tom basso", -5, 1.0, -1, -0.3, 0, 0.0),
        (s("tom"), "Tom alto", 4, 1.0, -1, 0.3, 0, 0.0),
        (s("perc2"), "Perc 2", 0, 1.0, -3, -0.2, 0, 0.0),
        (s("cym"), "Piatto", 0, 1.0, -4, 0, 0, 0.0),
    ]
    for sid, label in extra:
        pads.append((sid, label, 0, 1.0, -3, 0, 0, 0.0))
    return pads


def write_kits_header(root):
    lines = ["#pragma once", "// File generato da tools/soundgen/kits2026.py: non modificare a mano.", "// Kit DrumForge 2026 (ogni kit con i suoi colpi).", "",
             "#define DRUMFORGE_KITS_2026 \\"]
    for key, name, base, ov, extra in KITS:
        pads = kit_pads(key, extra)
        cells = ", ".join('{ "%s", "%s", %.1ff, %.2ff, %.1ff, %.1ff, 20000, %d, %.2ff }' % (sid, label, tune, dec, lev, pan, choke, rev)
                          for sid, label, tune, dec, lev, pan, choke, rev in pads)
        lines.append('    { "%s", {{ %s }} }, \\' % (name, cells))
    lines.append("")
    path = os.path.join(root, "Source", "Instruments", "DrumForge", "GeneratedKits.h")
    with open(path, "w") as f:
        f.write("\n".join(lines))
