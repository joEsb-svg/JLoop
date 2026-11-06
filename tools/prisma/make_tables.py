"""
Converte wavetable libere (CC0) in Resources/Prisma/PrismaTables.bin per Prisma.

Fonti (tutte CC0 / pubblico dominio):
 - Kimura Taro Free Wavetables  https://www.kimurataro.com/free-wavetables.html
 - WaveEdit Online               https://waveeditonline.com
 (prese da https://github.com/atsushieno/open-vital-resources, formato .vitaltable)

Uso: python3 make_tables.py <cartella open-vital-resources> <file .bin di uscita>
Formato: "PRSM", int32 numero tabelle, poi per ogni tabella 32 byte di nome (UTF-8)
e 32 forme x 1024 campioni int16 (band-limited: armonici sopra 512 tolti).
"""
import base64, json, struct, sys
import numpy as np

TABLES = [
    ("Hardcore FM",  "KimuraTaroFreeWavetables/Wavetables/Hardcore FM/Hardcore FM3.vitaltable"),
    ("Phase Dist",   "KimuraTaroFreeWavetables/Wavetables/Phase Distortion/PD3.vitaltable"),
    ("Chebyshev",    "KimuraTaroFreeWavetables/Wavetables/Chebyshev polynomials/Chebyshev poly2.vitaltable"),
    ("Saw Sweep",    "KimuraTaroFreeWavetables/Wavetables/Filter Sweep/SweepSaw.vitaltable"),
    ("OP Digitale",  "KimuraTaroFreeWavetables/Wavetables/TE OP-1/OP1 Digital02.vitaltable"),
    ("CZ Reso",      "KimuraTaroFreeWavetables/Wavetables/CZ Reso/CZ Reso2.vitaltable"),
    ("PPG",          "KimuraTaroFreeWavetables/Wavetables/PPG Wave/PPG03.vitaltable"),
    ("Vocoder",      "KimuraTaroFreeWavetables/Wavetables/Vocorder/DirtyHarry.vitaltable"),
    ("Formant K",    "KimuraTaroFreeWavetables/Wavetables/Formant/Formant1.vitaltable"),
    ("Vocali GMa",   "KimuraTaroFreeWavetables/Wavetables/GMa Vowel/GMa Vowel.vitaltable"),
    ("Juno PWM",     "KimuraTaroFreeWavetables/Wavetables/Alpha Juno SawPWM/AJ-PWM Saw2.vitaltable"),
    ("Corde",        "KimuraTaroFreeWavetables/Wavetables/Karplus Strong/StringCp1.vitaltable"),
    ("Aah Woh Ye",   "WaveEditOnline/Wavetables/AAHWOHYE.vitaltable"),
    ("Parlante",     "WaveEditOnline/Wavetables/TALKING.vitaltable"),
    ("Reso Saw",     "WaveEditOnline/Wavetables/RESO_SAW.vitaltable"),
    ("Supersaw W",   "WaveEditOnline/Wavetables/SUPERSAW.vitaltable"),
    ("Basso W",      "WaveEditOnline/Wavetables/BASS_BY_.vitaltable"),
]
FRAMES, SIZE = 32, 1024

def load(path):
    d = json.load(open(path))
    c = d["groups"][0]["components"][0]
    a = np.frombuffer(base64.b64decode(c["audio_file"]), dtype=np.int16).astype(np.float64) / 32768.0
    win = int(c.get("window_size", 2048))
    n = len(a) // win
    frames = a[: n * win].reshape(n, win)
    # le tabelle prese da una voce hanno pause (forme mute): si tolgono, se no il morph passa dal silenzio
    rms = np.sqrt((frames ** 2).mean(axis=1))
    return frames[rms > 0.2 * rms.max()]

def resample(frame):
    spec = np.fft.rfft(frame)
    out = np.zeros(SIZE // 2 + 1, dtype=complex)
    keep = min(len(spec), SIZE // 2 + 1)
    out[:keep] = spec[:keep] * (SIZE / len(frame))
    out[0] = 0.0                                  # via la continua
    out[SIZE // 2] = 0.0
    return np.fft.irfft(out, SIZE)

def main():
    root, dest = sys.argv[1], sys.argv[2]
    blob = bytearray(b"PRSM") + struct.pack("<i", len(TABLES))
    for name, rel in TABLES:
        frames = load(root + "/" + rel)
        idx = np.round(np.linspace(0, len(frames) - 1, FRAMES)).astype(int)
        t = np.array([resample(frames[i]) for i in idx])
        if name == "Vocoder":   # presa da una voce: ogni forma allo stesso volume (se no salta su e giu' col morph)
            t /= np.maximum(1e-9, np.sqrt((t ** 2).mean(axis=1, keepdims=True)))
        # stesso volume percepito per tutte (RMS ~0.45), senza superare il picco
        t *= min(0.45 / max(1e-9, np.sqrt((t ** 2).mean())), 0.98 / max(1e-9, np.abs(t).max()))
        q = np.clip(np.round(t * 32767), -32767, 32767).astype("<i2")
        blob += name.encode("utf-8")[:31].ljust(32, b"\0") + q.tobytes()
        print(f"{name:12s} {len(frames)} forme da {frames.shape[1]} -> {FRAMES}x{SIZE}  rms {np.sqrt((t**2).mean()):.3f}")
    open(dest, "wb").write(blob)
    print(len(blob), "byte")

main()
