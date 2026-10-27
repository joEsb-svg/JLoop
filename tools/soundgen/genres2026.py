"""
Libreria 2026: Psytrance e Hi-tech, pad / atmosfere / voci, e altri generi (DnB e neurofunk, hardstyle, trap, techno).
Solo colpi singoli e note (niente loop con i BPM gia' decisi).

Stesse regole di electro2012: ogni suono esce "da produzione" (polish) e gli id "x_" non si cambiano mai.
"""
import numpy as np
from dsp import *
from registry import sound
import electro2012 as ex
from electro2012 import polish, kick_x, snare_x, clap_x, hat_x, crash_x, detuned_saw, bass_note, chord, vox, DRUM_COL

DRUMS = "Batteria"
INSTR = "Strumenti"
BASS_COL, SYNTH_COL, FX_COL, VOX_COL = ex.BASS_COL, ex.SYNTH_COL, ex.FX_COL, ex.VOX_COL
PAD_COL = 0xff5aa6c9
ATMO_COL = 0xff4f8fa0

# famiglie nuove: pad lunghi (morbidi, compressione leggera) e atmosfere (piu' basse di livello)
ex.FAMILY.setdefault("pad", (80, [(300, 1.0, -2.0), (2500, 0.8, 1.0)], 15000, (-20, 2.0, 0.02, 0.25, 1.0), 0.20))
ex.FAMILY.setdefault("atmo", (40, [], 15000, None, 0.15))


def _reg(table, family, category, colour, fn):
    for sid, name, p in table:
        def _make(p=p):
            return lambda: polish(fn(**p), family)
        sound(sid, name, DRUMS, category, colour=colour)(_make())


# ============================================================================ PSY / HI-TECH: BATTERIA
_reg([
    ("x_kick_psy_fullon",   "Kick Psy Full-on",     dict(d=0.30, f0=300, f1=50, ptau=0.010, btau=0.090, clk=0.9, dist=2.0, seed=1101, tok=0.3)),
    ("x_kick_psy_forest",   "Kick Psy Forest",      dict(d=0.28, f0=260, f1=46, ptau=0.012, btau=0.080, clk=1.1, dist=3.0, seed=1102, tok=0.2)),
    ("x_kick_psy_goa",      "Kick Goa",             dict(d=0.35, f0=220, f1=52, ptau=0.018, btau=0.120, clk=0.5, dist=1.6, seed=1103)),
    ("x_kick_psy_prog",     "Kick Psy Progressive", dict(d=0.40, f0=200, f1=50, ptau=0.020, btau=0.140, clk=0.4, dist=1.5, seed=1104, sub=0.2, tail_tau=0.15)),
    ("x_kick_hitech",       "Kick Hi-tech",         dict(d=0.18, f0=380, f1=60, ptau=0.006, btau=0.050, clk=1.3, dist=2.2, seed=1105, tok=0.5)),
    ("x_kick_psy_twilight", "Kick Psy Twilight",    dict(d=0.32, f0=280, f1=48, ptau=0.014, btau=0.100, clk=0.8, dist=2.6, seed=1106, tok=0.25)),
], "kick", "Kick", DRUM_COL["kick"], kick_x)

_reg([
    ("x_hat_psy_metal", "Hi-hat Psy Metallico", dict(d=0.10, tau=0.025, pitch=1.6, hpf=8000, noise_amt=0.2, dist=1.2, seed=1111, metal=1.0)),
    ("x_hat_psy_tick",  "Hi-hat Psy Tick",      dict(d=0.06, tau=0.010, pitch=1.4, hpf=9500, noise_amt=0.4, dist=1.0, seed=1112)),
    ("x_hat_hitech",    "Hi-hat Hi-tech",       dict(d=0.05, tau=0.008, pitch=2.0, hpf=10000, noise_amt=0.3, dist=1.5, seed=1113, metal=0.9)),
], "hat", "Hi-hat chiusi", DRUM_COL["hat"], hat_x)

_reg([
    ("x_ohat_psy",      "Hi-hat aperto Psy",       dict(d=0.35, tau=0.12, pitch=1.5, hpf=7500, noise_amt=0.3, dist=1.2, seed=1121, metal=1.0)),
    ("x_ohat_psy_long", "Hi-hat aperto Psy lungo", dict(d=0.70, tau=0.25, pitch=1.6, hpf=7000, noise_amt=0.4, dist=1.0, seed=1122, metal=0.9)),
], "ohat", "Hi-hat aperti", DRUM_COL["ohat"], hat_x)

_reg([
    ("x_snare_psy",      "Snare Psy",            dict(d=0.30, tone=280, drop=0.3, ttau=0.03, ntau=0.08, n_lo=2200, n_hi=11000, crack=1.0, dist=1.6, seed=1131, room=0.05)),
    ("x_snare_psy_room", "Snare Psy riverberata", dict(d=0.80, tone=240, drop=0.4, ttau=0.04, ntau=0.10, n_lo=1800, n_hi=10000, crack=0.8, dist=1.4, seed=1132, room=0.35)),
], "snare", "Rullanti", DRUM_COL["snare"], snare_x)

_reg([
    ("x_clap_psy",    "Clap Psy",     dict(d=0.50, bursts=4, spacing=0.010, tail=0.10, lo=1100, hi=3200, seed=1141, room=0.25)),
    ("x_clap_hitech", "Clap Hi-tech", dict(d=0.18, bursts=3, spacing=0.006, tail=0.035, lo=1500, hi=5000, seed=1142)),
], "clap", "Clap e schiocchi", DRUM_COL["clap"], clap_x)


@sound("x_ride_psy", "Ride Psy", DRUMS, "Piatti", colour=DRUM_COL["cym"])
def _():
    d = 0.9; t = tt(d)
    x = metallic(d, 3000, 11000, 24, 0.35, 1151) * 0.6 + np.sin(2 * np.pi * 4100 * t) * np.exp(-t / 0.18) * 0.3
    return polish(x * np.clip(t / 0.001, 0, 1), "cym")


def _fm_perc(d, f0, f1, ptau, ratio, idx, tau, seed, noise_amt=0.0):
    t = tt(d)
    f = f1 + (f0 - f1) * np.exp(-t / ptau)
    ph = phase_of(f)
    x = np.sin(ph + idx * np.exp(-t / (tau * 0.5)) * np.sin(ph * ratio)) * np.exp(-t / tau)
    if noise_amt:
        x += hp(noise(d, seed), 5000) * np.exp(-t / 0.004) * noise_amt
    return x


for sid, name, p in [
    ("x_perc_psy_drop",   "Perc FM Goccia",     dict(d=0.20, f0=1800, f1=500, ptau=0.015, ratio=1.0, idx=2.0, tau=0.05, seed=1161)),
    ("x_perc_psy_metal",  "Perc FM Metallo",    dict(d=0.25, f0=900, f1=880, ptau=0.05, ratio=3.53, idx=4.0, tau=0.06, seed=1162, noise_amt=0.2)),
    ("x_perc_psy_tic",    "Perc Tic Psy",       dict(d=0.06, f0=4000, f1=2500, ptau=0.005, ratio=2.0, idx=1.5, tau=0.012, seed=1163, noise_amt=0.4)),
    ("x_perc_psy_bubble", "Perc Bolla",         dict(d=0.18, f0=400, f1=1400, ptau=0.03, ratio=1.0, idx=0.5, tau=0.05, seed=1164)),
    ("x_perc_hitech_zap", "Perc Hi-tech Zap",   dict(d=0.10, f0=6000, f1=300, ptau=0.008, ratio=2.0, idx=3.0, tau=0.03, seed=1165)),
    ("x_perc_psy_wood",   "Perc Legno FM",      dict(d=0.12, f0=1200, f1=1100, ptau=0.01, ratio=2.76, idx=1.8, tau=0.025, seed=1166, noise_amt=0.2)),
]:
    def _make(p=p):
        return lambda: polish(_fm_perc(**p), "perc")
    sound(sid, name, DRUMS, "Percussioni", colour=DRUM_COL["perc"])(_make())


@sound("x_perc_psy_shaker", "Shaker Psy", DRUMS, "Percussioni", colour=DRUM_COL["perc"])
def _():
    d = 0.12; t = tt(d)
    x = bp(noise(d, 1167), 5000, 12000) * np.clip(t / 0.012, 0, 1) * np.exp(-np.maximum(0, t - 0.012) / 0.03)
    return polish(x, "perc")


# ============================================================================ PSY / HI-TECH: STRUMENTI
def _psy_bass_hit(wave, d, cutoff, env_oct, ftau, res, drv, seed, root=36):
    t = tt(d)
    f = mtof(root)
    if wave == "saw":
        osc = saw(f, d, seed=seed, rand_phase=False)
    elif wave == "square":
        osc = square(f, d, seed=seed, rand_phase=False)
    else:
        osc = fm(f, d, 2.0, 0.8 + 3.0 * np.exp(-t / 0.02))
    x = svf(osc, cutoff * 2 ** (env_oct * np.exp(-t / ftau)), res)
    x = drive(x, drv) * np.clip(t / 0.001, 0, 1) * np.exp(-t / (d * 0.35))
    return x


for sid, name, p in [
    ("x_bass_psy_roll",   "Basso Psy Rolling (colpo)", dict(wave="saw", d=0.18, cutoff=300, env_oct=4.5, ftau=0.020, res=1.4, drv=2.0, seed=1201)),
    ("x_bass_psy_fm",     "Basso Psy FM (colpo)",      dict(wave="fm", d=0.18, cutoff=500, env_oct=3.5, ftau=0.020, res=1.0, drv=2.5, seed=1202)),
    ("x_bass_psy_square", "Basso Psy Quadra (colpo)",  dict(wave="square", d=0.20, cutoff=260, env_oct=4.0, ftau=0.025, res=1.6, drv=1.8, seed=1203)),
    ("x_bass_psy_dark",   "Basso Psy Forest (colpo)",  dict(wave="saw", d=0.14, cutoff=180, env_oct=5.0, ftau=0.012, res=2.5, drv=3.5, seed=1204)),
    ("x_bass_hitech",     "Basso Hi-tech (colpo)",     dict(wave="fm", d=0.10, cutoff=900, env_oct=3.0, ftau=0.010, res=1.2, drv=2.5, seed=1205, root=48)),
]:
    def _make(p=p):
        return lambda: polish(_psy_bass_hit(**p), "bass")
    sound(sid, name, INSTR, "Bassi", root=p.get("root", 36), colour=BASS_COL)(_make())


def _acid(d, accent, seed):
    t = tt(d)
    osc = saw(mtof(48), d, seed=seed, rand_phase=False)
    tau = 0.18 if accent else 0.4
    x = ladder(osc, 250 * 2 ** ((5.0 if accent else 3.5) * np.exp(-t / tau)), 0.85 if accent else 0.7, 1.5)
    x = drive(x * (1.6 if accent else 1.2), 2.0) * adsr(d, 0.002, 0.2, 0.9)
    return x


@sound("x_acid_303", "Acid 303 (nota)", INSTR, "Synth e stab", root=48, gated=True, release=0.03, colour=SYNTH_COL)
def _():
    return polish(_acid(0.9, False, 1211), "synth")


@sound("x_acid_303_accent", "Acid 303 accent", INSTR, "Synth e stab", root=48, gated=True, release=0.03, colour=SYNTH_COL)
def _():
    return polish(_acid(0.9, True, 1212), "synth")


@sound("x_lead_psy", "Lead Psy Saw", INSTR, "Synth e stab", root=60, gated=True, release=0.08, colour=SYNTH_COL)
def _():
    d = 1.5; t = tt(d)
    x = detuned_saw(mtof(60), d, 3, 0.006, 1213)
    x = svf(x, 2500 + 1500 * np.sin(2 * np.pi * 0.7 * t), 1.6) * adsr(d, 0.005, 0.2, 0.85)
    return polish(echo(x, 0.17, 0.3, 0.2), "synth")


@sound("x_pluck_goa", "Pluck Goa", INSTR, "Synth e stab", root=60, colour=SYNTH_COL)
def _():
    d = 0.6; t = tt(d)
    x = saw(mtof(60), d, seed=1214) + 0.5 * square(mtof(72), d, seed=1215)
    x = svf(x, 400 + 6000 * np.exp(-t / 0.05), 2.0) * np.exp(-t / 0.15)
    return polish(reverb(x, 0.2, 1.2, 0.3), "synth")


@sound("x_lead_hitech_fm", "Lead Hi-tech FM", INSTR, "Synth e stab", root=60, gated=True, release=0.04, colour=SYNTH_COL)
def _():
    d = 1.0; t = tt(d)
    x = fm(mtof(60), d, 3.5, 2.5 + 2.0 * np.sin(2 * np.pi * 8 * t)) * adsr(d, 0.002, 0.1, 0.8)
    return polish(drive(x, 1.8), "synth")


@sound("x_stab_psy_dark", "Stab Psy Scuro", INSTR, "Synth e stab", root=60, colour=SYNTH_COL)
def _():
    d = 0.8; t = tt(d)
    x = chord(lambda f, d: saw(f, d, seed=1216), 60, [0, 1, 7], d)   # seconda minore: suono "minaccioso"
    x = svf(x, 300 + 3000 * np.exp(-t / 0.08), 1.5) * np.exp(-t / 0.25)
    return polish(echo(x, 0.25, 0.35, 0.25), "synth")


def _zap(d, f0, f1, tau, seed, ratio=2.0, idx=2.0):
    t = tt(d)
    f = f1 + (f0 - f1) * np.exp(-t / tau)
    ph = phase_of(f)
    return np.sin(ph + idx * np.sin(ph * ratio)) * np.exp(-t / (d * 0.4))


@sound("x_fx_psy_zap_down", "Zap che cade", INSTR, "FX", colour=FX_COL)
def _():
    return polish(echo(_zap(0.5, 4000, 80, 0.08, 1221), 0.12, 0.35, 0.3), "fx")


@sound("x_fx_psy_zap_up", "Zap che sale", INSTR, "FX", colour=FX_COL)
def _():
    d = 0.5; t = tt(d)
    f = 100 * (40 ** np.clip(t / 0.35, 0, 1))
    x = np.sin(phase_of(f) + 1.5 * np.sin(phase_of(f * 2))) * np.clip(t / 0.01, 0, 1) * np.exp(-np.maximum(0, t - 0.35) / 0.03)
    return polish(x, "fx")


@sound("x_fx_laser_psy", "Laser Psy", INSTR, "FX", colour=FX_COL)
def _():
    return polish(echo(_zap(0.35, 7000, 600, 0.03, 1223, 1.0, 0.8), 0.09, 0.45, 0.35), "fx")


@sound("x_fx_alien", "Alieno", INSTR, "FX", colour=FX_COL)
def _():
    d = 1.2; t = tt(d)
    f = 900 + 600 * np.sin(2 * np.pi * 7 * t) * np.sin(2 * np.pi * 0.8 * t)
    ph = phase_of(f)
    x = np.sin(ph + (2 + 2 * np.sin(2 * np.pi * 3 * t)) * np.sin(ph * 1.5)) * adsr(d, 0.02, 0.2, 0.8, rel_start=d - 0.2, rel=0.15)
    return polish(x, "fx")


@sound("x_fx_squelch", "Squelch acido", INSTR, "FX", colour=FX_COL)
def _():
    d = 0.8; t = tt(d)
    osc = saw(mtof(43), d, seed=1225, rand_phase=False)
    x = ladder(osc, 200 * 2 ** (5 * (0.5 + 0.5 * np.sin(2 * np.pi * 2.5 * t - 1.5))), 0.9, 1.5)
    return polish(drive(x, 2.0) * adsr(d, 0.005, 0.2, 0.9, rel_start=d - 0.1, rel=0.08), "fx")


@sound("x_fx_glitch_burst", "Raffica glitch", INSTR, "FX", colour=FX_COL)
def _():
    d = 0.6
    out = np.zeros(secs(d))
    r = rng(1226)
    pos = 0
    while pos < len(out) - secs(0.02):
        n = secs(r.choice([0.015, 0.03, 0.06]))
        f = r.uniform(200, 5000)
        seg = _zap(n / SR, f * 2, f, 0.005, 1226 + pos, r.uniform(1, 5), r.uniform(0.5, 4))
        seg = np.round(seg * 8) / 8 if r.uniform() < 0.4 else seg
        out[pos:pos + n] += seg[: len(out) - pos] * r.uniform(0.4, 1.0)
        pos += n
    return polish(out, "fx")


@sound("x_fx_psy_sweep", "Sweep Psy", INSTR, "FX", colour=FX_COL)
def _():
    d = 2.0; t = tt(d)
    x = svf(noise(d, 1227), 300 * 2 ** (5 * np.sin(np.pi * t / d)), 3.0, "bp")
    return polish(x * np.sin(np.pi * t / d), "fx")


@sound("x_fx_sub_drop", "Sub drop", INSTR, "FX", colour=FX_COL)
def _():
    d = 2.0; t = tt(d)
    f = 30 + 90 * np.exp(-t / 0.5)
    return polish(drive(np.sin(phase_of(f)), 1.5) * np.clip(t / 0.003, 0, 1) * np.exp(-t / 0.7), "fx")


# ============================================================================ PAD (tenuti, nota C4)
def _pad(x, d, att=0.4, rel=0.6):
    return x * adsr(d, att, 0.5, 0.9, rel_start=d - rel, rel=rel * 0.6)


def _pad_sound(sid, name, fn):
    sound(sid, name, INSTR, "Pad e atmosfere", root=60, gated=True, release=0.6, colour=PAD_COL)(lambda: polish(fn(), "pad"))


def _p_warm():
    d = 4.0
    x = chord(lambda f, d: detuned_saw(f, d, 5, 0.01, 1301), 60, [0], d) + 0.5 * detuned_saw(mtof(48), d, 3, 0.008, 1306)
    return chorus(_pad(lp(x, 1600), d), 6, 0.5, 0.4, seed=1302)


def _p_glass():
    d = 4.0; t = tt(d)
    f = mtof(60)
    x = fm(f, d, 3.0, 1.2 * np.exp(-t / 1.5) + 0.3) * 0.6 + np.sin(phase_of(np.full(len(t), f * 2.0))) * 0.25
    return reverb(_pad(x, d, 0.15), 0.35, 2.5, 0.8, 8000, seed=1303)


def _p_dark():
    d = 4.0
    x = detuned_saw(mtof(48), d, 5, 0.012, 1304) + 0.6 * detuned_saw(mtof(60), d, 3, 0.01, 1305)
    return _pad(lp(x, 650), d, 0.6)


def _p_choir():
    d = 4.0
    x = sum(vox(["a"], d, mtof(60) * (1 + dt), 1310 + k) for k, dt in enumerate([-0.006, 0.0, 0.007]))
    return reverb(chorus(_pad(x, d, 0.3), 8, 0.4, 0.5, seed=1311), 0.3, 2.5, 0.7, seed=1312)


def _p_strings():
    d = 4.0; t = tt(d)
    x = detuned_saw(mtof(60), d, 7, 0.009, 1320) * (1 + 0.05 * np.sin(2 * np.pi * 5 * t))
    return chorus(_pad(lp(hp(x, 200), 3500), d, 0.5), 5, 0.7, 0.5, seed=1321)


def _p_pwm():
    d = 4.0; t = tt(d)
    w = 0.5 + 0.35 * np.sin(2 * np.pi * 0.6 * t)
    ph = (phase_of(np.full(len(t), mtof(60))) / (2 * np.pi)) % 1
    x = np.where(ph < w, 1.0, -1.0) - (2 * w - 1)
    return chorus(_pad(lp(x, 2200), d), 6, 0.4, 0.5, seed=1322)


def _p_sweep():
    d = 4.0; t = tt(d)
    x = detuned_saw(mtof(60), d, 5, 0.012, 1323)
    return _pad(svf(x, 300 * 2 ** (4 * (0.5 - 0.5 * np.cos(2 * np.pi * t / d))), 1.8), d)


def _p_space():
    d = 4.0; t = tt(d)
    x = detuned_saw(mtof(60), d, 3, 0.01, 1324) * 0.6 + np.sin(phase_of(np.full(len(t), mtof(72)))) * 0.3
    x = svf(x, 900 + 600 * np.sin(2 * np.pi * 0.3 * t), 1.2)
    return reverb(echo(_pad(x, d, 0.5), 0.375, 0.5, 0.3), 0.35, 3.0, 0.9, 7000, seed=1325)


def _p_trance():
    d = 4.0
    x = chord(lambda f, d: detuned_saw(f, d, 7, 0.02, 1326), 60, [0, 12], d)
    return reverb(_pad(lp(x, 5000), d, 0.08), 0.25, 2.0, 0.6, seed=1327)


for sid, name, fn in [
    ("x_pad_warm", "Pad Caldo", _p_warm), ("x_pad_glass", "Pad Vetro", _p_glass), ("x_pad_dark", "Pad Scuro", _p_dark),
    ("x_pad_choir", "Pad Coro Aah", _p_choir), ("x_pad_strings", "Pad Archi Synth", _p_strings), ("x_pad_pwm", "Pad PWM", _p_pwm),
    ("x_pad_sweep", "Pad Sweep", _p_sweep), ("x_pad_space", "Pad Spazio Psy", _p_space), ("x_pad_trance", "Pad Trance Supersaw", _p_trance),
]:
    _pad_sound(sid, name, fn)


# ============================================================================ ATMOSFERE (lunghe, tenute)
def _atmo_sound(sid, name, fn, root=60):
    sound(sid, name, INSTR, "Pad e atmosfere", root=root, gated=True, release=1.0, colour=ATMO_COL)(lambda: polish(fn(), "atmo"))


def _swell(d, att=1.2, rel=1.5):
    return adsr(d, att, 0.5, 1.0, rel_start=d - rel, rel=rel * 0.6)


def _a_drone():
    d = 6.0; t = tt(d)
    x = detuned_saw(mtof(36), d, 4, 0.006, 1401) + 0.5 * saw(mtof(43), d, seed=1402)
    x = svf(x, 250 + 150 * np.sin(2 * np.pi * 0.15 * t), 2.5)
    return x * _swell(d)


def _a_wind():
    d = 6.0; t = tt(d)
    x = svf(noise(d, 1403), 600 * 2 ** (1.5 * np.sin(2 * np.pi * 0.2 * t) + 0.8 * np.sin(2 * np.pi * 0.53 * t)), 4.0, "bp")
    return x * _swell(d, 1.5)


def _a_space():
    d = 6.0; t = tt(d)
    r = rng(1404)
    x = np.zeros(len(t))
    for k in range(10):
        f = mtof(60 + r.choice([0, 7, 12, 19, 24, 31]))
        x += np.sin(phase_of(np.full(len(t), f * (1 + r.uniform(-0.003, 0.003))))) * (0.5 + 0.5 * np.sin(2 * np.pi * r.uniform(0.05, 0.3) * t + r.uniform(0, 6)))
    return reverb(x / 10 * _swell(d), 0.4, 3.0, 1.0, 9000, seed=1405)


def _a_rain():
    d = 6.0
    r = rng(1406)
    x = np.zeros(secs(d))
    for _ in range(260):
        at = secs(r.uniform(0, d - 0.1))
        n = secs(0.03)
        f = r.uniform(1500, 6000)
        x[at:at + n] += np.sin(2 * np.pi * f * tt(n / SR)) * np.exp(-tt(n / SR) / 0.006) * r.uniform(0.2, 1.0)
    x += hp(noise(d, 1407), 3000) * 0.08
    return reverb(x * _swell(d, 0.8), 0.3, 2.0, 0.6, seed=1408)


def _a_shimmer():
    d = 6.0; t = tt(d)
    x = sum(np.sin(phase_of(np.full(len(t), mtof(72 + s)))) * (0.5 + 0.5 * np.sin(2 * np.pi * (0.4 + 0.13 * k) * t)) for k, s in enumerate([0, 7, 12, 16]))
    return reverb(chorus(x / 4 * _swell(d), 9, 0.3, 0.6, seed=1409), 0.45, 3.5, 1.2, 10000, seed=1410)


def _a_metal():
    d = 6.0; t = tt(d)
    x = metallic(d, 200, 3000, 30, 4.0, 1411, 0.2)
    x = svf(x, 800 + 500 * np.sin(2 * np.pi * 0.25 * t), 3.0)
    return x * _swell(d, 1.0)


def _a_ocean():
    d = 6.0; t = tt(d)
    wave = 0.5 - 0.5 * np.cos(2 * np.pi * t / 3.0)
    x = svf(noise(d, 1412), 300 + 2500 * wave, 0.8)
    return x * (0.3 + 0.7 * wave) * _swell(d, 0.5, 0.8)


for sid, name, fn, *root in [
    ("x_atmo_drone_dark", "Drone Scuro", _a_drone, 36), ("x_atmo_wind", "Vento", _a_wind), ("x_atmo_space", "Texture Spaziale", _a_space),
    ("x_atmo_rain", "Pioggia digitale", _a_rain), ("x_atmo_shimmer", "Shimmer", _a_shimmer), ("x_atmo_metal", "Drone Metallico", _a_metal),
    ("x_atmo_ocean", "Onde", _a_ocean),
]:
    _atmo_sound(sid, name, fn, *root)


# ============================================================================ VOCI
def _vox_sound(sid, name, fn, gated=False, root=60):
    sound(sid, name, INSTR, "Voci", root=root, gated=gated, release=0.2, colour=VOX_COL)(lambda: polish(fn(), "vox"))


def _word(parts, f0_note, seed, shout=False):
    """parts: (consonante o None, vocali, durata in secondi, semitoni)."""
    out = np.zeros(secs(sum(p[2] for p in parts) + 0.2))
    at = 0.0
    for k, (cons, vowels, dur, semi) in enumerate(parts):
        f0 = mtof(f0_note + semi)
        if cons:
            c = ex._consonant(cons, seed + k, f0)
            ex._place(out, c * (1.5 if shout else 1.0), at)
            at += len(c) / SR * (0.6 if cons in "tkpdb" else 0.8)
        if vowels:
            v = vox(list(vowels), dur, f0, seed + 40 + k, min(0.12, dur * 0.5))
            if shout:
                v = drive(v * 2.5, 2.0) + hp(noise(dur, seed + 70 + k), 2500) * 0.08 * adsr(dur, 0.01, 0.1, 0.6)
            ex._place(out, v, at)
            at += dur
    return out


_vox_sound("x_vox_ah_chop", "Vox Ah (chop)", lambda: vox(["a"], 0.22, mtof(60), 1501))
_vox_sound("x_vox_ee", "Vox Ii", lambda: vox(["i"], 0.35, mtof(60), 1502))
_vox_sound("x_vox_hey_shout", "Vox Hey! (grido)", lambda: _word([(None, "ei", 0.32, 3)], 60, 1503, shout=True), root=63)
_vox_sound("x_vox_go", "Vox Go!", lambda: _word([("k", "o", 0.35, 0)], 57, 1504, shout=True), root=57)
_vox_sound("x_vox_yo", "Vox Yo", lambda: _word([(None, "io", 0.3, 0)], 55, 1505), root=55)
_vox_sound("x_vox_drop", "Vox Drop", lambda: _word([("d", "o", 0.28, 0), ("p", "", 0.05, 0)], 52, 1506, shout=True), root=52)
_vox_sound("x_vox_breath", "Respiro", lambda: formants(hp(noise(0.6, 1507), 400), *ex.VOWELS["a"]) * adsr(0.6, 0.2, 0.2, 0.7, rel_start=0.35, rel=0.2) * 3)
def _whisper():
    out = np.zeros(secs(0.7))
    ex._place(out, ex._consonant("s", 1508, 200.0) * 0.6, 0.0)
    w = formants(hp(noise(0.4, 1509), 600), *ex.VOWELS["i"]) * adsr(0.4, 0.05, 0.1, 0.8, rel_start=0.3, rel=0.08) * 3
    ex._place(out, w, 0.07)
    return out


_vox_sound("x_vox_whisper", "Sussurro", _whisper)


@sound("x_vox_robot_yeah", "Robot Yeah", INSTR, "Voci", root=60, colour=VOX_COL)
def _():
    x = _word([(None, "ia", 0.45, 0)], 60, 1510)
    t = tt(len(x) / SR)
    x = x * np.sign(np.sin(2 * np.pi * 110 * t)) * 0.7 + x * 0.3   # modulazione ad anello: voce robotica
    return polish(np.round(x * 12) / 12, "vox")


@sound("x_vox_stab", "Vox Stab (accordo)", INSTR, "Voci", root=60, colour=VOX_COL)
def _():
    d = 0.45
    x = sum(vox(["a"], d, mtof(60 + s), 1511 + s) for s in (0, 3, 7)) / 3
    return polish(drive(x * 2.0, 1.5) * np.exp(-tt(d) / 0.2), "vox")


_vox_sound("x_vox_choir_ooh", "Coro Ooh", lambda: reverb(chorus(sum(vox(["u"], 3.0, mtof(60) * (1 + dt), 1520 + k) for k, dt in enumerate([-0.005, 0.0, 0.006])), 8, 0.4, 0.5, seed=1523), 0.3, 2.5, 0.7, seed=1524), gated=True)
_vox_sound("x_vox_oh_long", "Vox Ooh lunga", lambda: chorus(vox(["o"], 2.0, mtof(60), 1525), 6, 0.5, 0.3, seed=1526), gated=True)


# ============================================================================ DNB / NEUROFUNK
_reg([("x_kick_dnb_punch", "Kick DnB Punch", dict(d=0.30, f0=280, f1=56, ptau=0.012, btau=0.09, clk=1.0, dist=2.2, seed=1601, tok=0.4))],
     "kick", "Kick", DRUM_COL["kick"], kick_x)
_reg([
    ("x_snare_dnb_crack", "Snare DnB Crack", dict(d=0.35, tone=200, drop=0.4, ttau=0.05, ntau=0.10, n_lo=1800, n_hi=11000, crack=1.4, dist=2.5, seed=1602, room=0.08)),
    ("x_snare_neuro",     "Snare Neuro",     dict(d=0.40, tone=185, drop=0.5, ttau=0.06, ntau=0.12, n_lo=1300, n_hi=9000, crack=1.0, dist=4.0, seed=1603, room=0.1, clap_layer=0.3)),
], "snare", "Rullanti", DRUM_COL["snare"], snare_x)
_reg([("x_hat_dnb", "Hi-hat DnB", dict(d=0.08, tau=0.018, pitch=1.15, hpf=8500, noise_amt=0.6, dist=1.3, seed=1604))],
     "hat", "Hi-hat chiusi", DRUM_COL["hat"], hat_x)


@sound("x_bass_neuro_growl", "Basso Neuro Growl", INSTR, "Bassi", root=36, gated=True, release=0.06, colour=BASS_COL)
def _():
    d = 1.6; t = tt(d); f = mtof(36)
    x = saw(f * 0.997, d, seed=1611) + saw(f * 1.004, d, seed=1612)
    x = svf(x, 400 * 2 ** (2.5 * (0.5 + 0.5 * np.sin(2 * np.pi * 4 * t))), 3.0)
    x = drive(x * 2.0, 4.0)
    x = formants(x, [500, 1000, 2600], [200, 300, 500], [1.0, 0.8, 0.4]) * 1.4 + lp(x, 200) * 0.7
    return polish(bass_note(lambda d: x, d), "bass")


@sound("x_bass_neuro_reese", "Basso Neuro Reese", INSTR, "Bassi", root=36, gated=True, release=0.08, colour=BASS_COL)
def _():
    d = 1.6; t = tt(d); f = mtof(36)
    x = sum(saw(f * (1 + det), d, seed=1613 + k) for k, det in enumerate([-0.012, -0.004, 0.005, 0.011]))
    x = lp(drive(x, 2.5), 1800) + np.sin(phase_of(np.full(len(t), f))) * 0.6
    return polish(bass_note(lambda d: x, d), "bass")


@sound("x_bass_neuro_stab", "Basso Neuro (colpo)", INSTR, "Bassi", root=36, colour=BASS_COL)
def _():
    d = 0.4; t = tt(d); f = mtof(36)
    x = fm(f, d, 1.5, 6.0 * np.exp(-t / 0.08))
    x = drive(x * 2.0, 3.5) * np.exp(-t / 0.15)
    return polish(x, "bass")


# ============================================================================ HARDSTYLE
def _hard_kick(d, f0, f1, ptau, dist, tail_note, seed, reverse_tail=False):
    t = tt(d)
    f = f1 + (f0 - f1) * np.exp(-t / ptau)
    body = np.sin(phase_of(f)) * np.exp(-t / (d * 0.45))
    x = drive(body * 3.0 + ex.click(d, seed, 1500, 8000, 0.002, 1.0), dist)
    x = lp(x, 9000) + peak(x, 2500, 1.0, 4.0) * 0.2
    if reverse_tail:   # "reverse bass": la coda del kick torna indietro fino al prossimo colpo
        tail = drive(np.sin(phase_of(np.full(len(t), mtof(tail_note)))) * 2.0, 3.0) * (t / d) ** 2
        x = x * np.exp(-t / 0.12) + lp(tail, 3000) * 0.8
    return x


@sound("x_kick_hardstyle", "Kick Hardstyle", DRUMS, "Kick", colour=DRUM_COL["kick"])
def _():
    return polish(_hard_kick(0.55, 400, 52, 0.035, 6.0, 33, 1701), "kick")


@sound("x_kick_rawstyle", "Kick Rawstyle", DRUMS, "Kick", colour=DRUM_COL["kick"])
def _():
    return polish(_hard_kick(0.60, 500, 55, 0.05, 9.0, 34, 1702), "kick")


@sound("x_kick_reverse_bass", "Kick Reverse Bass", DRUMS, "Kick", colour=DRUM_COL["kick"])
def _():
    return polish(_hard_kick(0.45, 380, 52, 0.03, 5.0, 33, 1703, reverse_tail=True), "kick")


_reg([("x_clap_hardstyle", "Clap Hardstyle", dict(d=0.55, bursts=4, spacing=0.012, tail=0.14, lo=900, hi=2800, seed=1704, room=0.3))],
     "clap", "Clap e schiocchi", DRUM_COL["clap"], clap_x)


@sound("x_lead_hardstyle", "Lead Hardstyle Screech", INSTR, "Synth e stab", root=60, gated=True, release=0.06, colour=SYNTH_COL)
def _():
    d = 1.2; t = tt(d)
    x = detuned_saw(mtof(60), d, 5, 0.015, 1705)
    x = svf(x, 2500 + 2000 * np.sin(2 * np.pi * 6 * t), 4.0, "bp")
    x = drive(x * 3.0, 4.0) * adsr(d, 0.003, 0.1, 0.9)
    return polish(reverb(x, 0.15, 1.5, 0.4), "synth")


# ============================================================================ TRAP
def _808(d, glide, dist, seed):
    t = tt(d)
    f = mtof(36) * (1 + glide * np.exp(-t / 0.04))
    x = np.sin(phase_of(f)) + ex.click(d, seed, 1500, 6000, 0.002, 0.3)
    return drive(x * dist, dist) * np.exp(-t / (d * 0.5))


@sound("x_bass_808_long", "808 Lungo", INSTR, "Bassi", root=36, gated=True, release=0.12, colour=BASS_COL)
def _():
    return polish(_808(2.5, 0.6, 1.3, 1801), "bass")


@sound("x_bass_808_dist", "808 Distorto", INSTR, "Bassi", root=36, gated=True, release=0.1, colour=BASS_COL)
def _():
    return polish(_808(2.0, 0.8, 3.5, 1802), "bass")


@sound("x_bass_808_short", "808 Corto", INSTR, "Bassi", root=36, colour=BASS_COL)
def _():
    return polish(_808(0.6, 0.5, 1.8, 1803), "bass")


_reg([("x_snare_trap", "Snare Trap", dict(d=0.35, tone=230, drop=0.3, ttau=0.04, ntau=0.12, n_lo=1500, n_hi=10000, crack=0.9, dist=1.5, seed=1804, room=0.12))],
     "snare", "Rullanti", DRUM_COL["snare"], snare_x)
_reg([("x_clap_trap", "Clap Trap", dict(d=0.45, bursts=4, spacing=0.011, tail=0.12, lo=1000, hi=3000, seed=1805, room=0.15))],
     "clap", "Clap e schiocchi", DRUM_COL["clap"], clap_x)
_reg([("x_hat_trap", "Hi-hat Trap", dict(d=0.07, tau=0.014, pitch=1.1, hpf=8000, noise_amt=0.5, dist=1.2, seed=1806))],
     "hat", "Hi-hat chiusi", DRUM_COL["hat"], hat_x)
_reg([("x_ohat_trap", "Hi-hat aperto Trap", dict(d=0.40, tau=0.14, pitch=1.1, hpf=7500, noise_amt=0.5, dist=1.2, seed=1807))],
     "ohat", "Hi-hat aperti", DRUM_COL["ohat"], hat_x)


@sound("x_perc_trap_rim", "Rim Trap", DRUMS, "Percussioni", colour=DRUM_COL["perc"])
def _():
    d = 0.12; t = tt(d)
    x = (np.sin(2 * np.pi * 1700 * t) + 0.6 * np.sin(2 * np.pi * 820 * t)) * np.exp(-t / 0.015) + ex.click(d, 1808, 3000, 10000, 0.001, 0.8)
    return polish(x, "perc")


# ============================================================================ TECHNO
_reg([("x_kick_techno_hard", "Kick Techno Hard", dict(d=0.50, f0=200, f1=46, ptau=0.030, btau=0.20, clk=0.6, dist=5.0, seed=1901, sub=0.3, tail_tau=0.2))],
     "kick", "Kick", DRUM_COL["kick"], kick_x)


@sound("x_perc_industrial", "Perc Industriale", DRUMS, "Percussioni", colour=DRUM_COL["perc"])
def _():
    d = 0.35; t = tt(d)
    x = metallic(d, 300, 4000, 16, 0.08, 1902) + bp(noise(d, 1903), 800, 4000) * np.exp(-t / 0.03)
    return polish(drive(x * 2.0, 3.0), "perc")


@sound("x_perc_metal_hit", "Colpo di metallo", DRUMS, "Percussioni", colour=DRUM_COL["perc"])
def _():
    d = 0.8; t = tt(d)
    x = modal(420, d, [1.0, 2.76, 5.4, 8.93], [1.0, 0.6, 0.4, 0.25], [0.3, 0.2, 0.12, 0.08], seed=1904)
    return polish(x + ex.click(d, 1905, 2000, 9000, 0.001, 0.6), "perc")


@sound("x_stab_techno", "Stab Techno (accordo minore)", INSTR, "Synth e stab", root=60, colour=SYNTH_COL)
def _():
    d = 0.6; t = tt(d)
    x = chord(lambda f, d: saw(f, d, seed=1906) + 0.5 * square(f, d, seed=1907), 60, [0, 3, 7, 10], d)
    x = svf(x, 600 + 4000 * np.exp(-t / 0.05), 1.4) * np.exp(-t / 0.15)
    return polish(echo(x, 0.1875, 0.4, 0.3), "synth")


@sound("x_ride_techno", "Ride Techno", DRUMS, "Piatti", colour=DRUM_COL["cym"])
def _():
    d = 0.7; t = tt(d)
    x = metallic(d, 3500, 12000, 20, 0.25, 1908) * 0.7 + hp(noise(d, 1909), 7000) * np.exp(-t / 0.05) * 0.3
    return polish(x, "cym")


# ============================================================================ ELECTRO HOUSE / BIG ROOM
@sound("x_lead_bigroom", "Lead Big Room", INSTR, "Synth e stab", root=60, gated=True, release=0.15, colour=SYNTH_COL)
def _():
    d = 1.2; t = tt(d)
    x = chord(lambda f, d: detuned_saw(f, d, 7, 0.018, 1951), 60, [0, 12], d)
    x = lp(x, 6000) * adsr(d, 0.003, 0.15, 0.85)
    return polish(reverb(x, 0.3, 2.0, 0.6, seed=1952), "synth")


@sound("x_bass_electro_wob", "Basso Electro House (wobble)", INSTR, "Bassi", root=36, gated=True, release=0.05, colour=BASS_COL)
def _():
    d = 1.6; t = tt(d)
    x = detuned_saw(mtof(36), d, 3, 0.01, 1953)
    x = svf(x, 200 * 2 ** (4 * (0.5 + 0.5 * np.sin(2 * np.pi * 4.27 * t - 1.57))), 2.0)
    return polish(bass_note(lambda d: drive(x, 2.0), d), "bass")
