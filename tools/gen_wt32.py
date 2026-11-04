#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-only
"""Dungeon Tracker 1.5: the wavetables of WAVE 32 (firmware/src/eng_wt32.c), written to firmware/src/wt32_tables.h.

Eight banks of eight frames (single cycles), the engine morphing between neighbouring frames. Every frame is band-
limited and kept at seven levels (a mipmap): 256 samples with harmonics 1..120, 128 with 1..60, then 128 / 30,
64 / 15, 32 / 7, 16 / 3, 8 / 1 (from the third on, four samples or more a harmonic: the linear interpolation's
images fold back quietly, under -35 dB), each followed by a copy of its first sample (the interpolation's guard),
16-bit. The engine reads the fullest level whose top harmonic stays under ~11.5 kHz at the note. A frame's levels share one
gain (its 256-sample peak at -0.8 dBFS), so a note keeps its level from one size to the next.
    python3 tools/gen_wt32.py"""
import math
import pathlib
import numpy as np

SIZES = [256, 128, 128, 64, 32, 16, 8]
HARM = [120, 60, 30, 15, 7, 3, 1]
NF = 8
OVER = 4096                      # the time-domain shapes are drawn at this resolution, then band-limited
BANKS = ["CLASSIC", "PWM", "FORMANT", "ORGAN", "GLASS", "SYNC", "DIGITAL", "GROWL"]


def spectrum_of_wave(w):
    """complex harmonics 1..120 of a time-domain cycle"""
    X = np.fft.rfft(np.asarray(w, dtype=float)) / (len(w) / 2)
    return X[1:121]


def spectrum_additive(amps, phases=None):
    """harmonics from amplitudes (sine phase unless given)"""
    a = np.zeros(120, dtype=complex)
    for k, v in enumerate(amps[:120]):
        ph = (phases[k] if phases is not None else 0.0)
        a[k] = v * np.exp(1j * (ph - math.pi / 2))      # a sine: -j
    return a


def saw(x): return 1 - 2 * x                              # (x in [0, 1)); falling saw, sine-phase harmonics


def bank_classic():
    x = np.arange(OVER) / OVER
    out = []
    out.append(spectrum_additive([1]))
    out.append(spectrum_additive([(8 / math.pi ** 2) * ((-1) ** ((k - 1) // 2)) / k ** 2 if k % 2 else 0 for k in range(1, 121)]))
    out.append(spectrum_additive([(2 / math.pi) / k * math.exp(-k / 6) for k in range(1, 121)]))   # a soft saw
    out.append(spectrum_of_wave(saw(x)))
    for w in (0.5, 0.3, 0.15, 0.05):
        out.append(spectrum_of_wave(np.where(x < w, 1.0, -1.0)))
    return out


def bank_pwm():
    x = np.arange(OVER) / OVER
    return [spectrum_of_wave(np.where(x < w, 1.0, -1.0)) for w in (0.5, 0.43, 0.36, 0.29, 0.22, 0.15, 0.09, 0.04)]


def bank_formant():
    f0 = 130.0
    V = [(300, 870, 2240), (400, 800, 2600), (730, 1090, 2440), (530, 1840, 2480), (270, 2290, 3010),
         (600, 1500, 2500), (250, 1000, 2000), (850, 1600, 2800)]           # U O A E I, a schwa, a nasal, a bright AH
    out = []
    for f1, f2, f3 in V:
        a = []
        for k in range(1, 121):
            f = k * f0
            r = 0
            for fc, g, bw in ((f1, 1.0, 90), (f2, 0.6, 120), (f3, 0.3, 160)):
                r += g / (1 + ((f - fc) / bw) ** 2)
            a.append(r / k ** 0.5)
        out.append(spectrum_additive(a))
    return out


def bank_organ():
    DB = [[1], [1, 0.5], [1, 0.6, 0.3], [1, 0.7, 0.5, 0.4], [1, 0.8, 0.6, 0.5, 0, 0.4],
          [1, 0.8, 0.7, 0.6, 0, 0.5, 0, 0.4], [0.8, 1, 0.8, 0.7, 0.4, 0.6, 0.3, 0.6, 0, 0.3],
          [0.6, 0.9, 1, 0.8, 0.6, 0.8, 0.5, 0.8, 0.4, 0.5, 0.3, 0.5, 0, 0, 0, 0.4]]
    return [spectrum_additive(d) for d in DB]


def bank_glass():
    out = []
    for i in range(NF):
        c = 3 + i * 5                                    # a bright cluster moving up
        a = [0.0] * 120
        a[0] = 1.0
        for k in range(1, 121):
            a[k - 1] += 0.8 * math.exp(-((k - c) / (1.2 + i * 0.4)) ** 2)
        out.append(spectrum_additive(a))
    return out


def bank_sync():
    x = np.arange(OVER) / OVER
    out = []
    for i in range(NF):
        r = 1 + i * 0.55
        w = saw((x * r) % 1.0) * (1 - x) ** 0.5           # (a hard-synced saw, softened at the reset)
        out.append(spectrum_of_wave(w))
    return out


def bank_digital():
    rng = np.random.default_rng(1985)
    out = []
    for i in range(NF):
        a = []
        for k in range(1, 121):
            base = 1 / k ** (1.2 - i * 0.1)
            m = (k % (i + 2) == 0) or (k & (1 << (i % 4)))
            a.append(base * (0.3 + 0.7 * m) * (0.6 + 0.4 * rng.random()))
        out.append(spectrum_additive(a))
    return out


def bank_growl():
    out = []
    for i in range(NF):
        d = 0.03 + i * 0.055                             # a comb's notches moving through a saw
        a = [abs(math.cos(math.pi * k * d)) / k for k in range(1, 121)]
        out.append(spectrum_additive(a))
    return out


def frame_sizes(X):
    """the levels of a frame from its harmonics; one gain for all"""
    waves = []
    for n, h in zip(SIZES, HARM):
        spec = np.zeros(n // 2 + 1, dtype=complex)
        spec[1:h + 1] = X[:h]
        waves.append(np.fft.irfft(spec * (n / 2), n))
    g = 0.91 / max(1e-9, np.max(np.abs(waves[0])))
    return [np.clip(np.round(w * g * 32767), -32767, 32767).astype(int) for w in waves]


def main():
    gens = [bank_classic, bank_pwm, bank_formant, bank_organ, bank_glass, bank_sync, bank_digital, bank_growl]
    rows = []
    for b, gen in enumerate(gens):
        frames = gen()
        assert len(frames) == NF, BANKS[b]
        for f in frames:
            data = []
            for w in frame_sizes(f):
                data += list(w) + [int(w[0])]
            rows.append(data)
    per = sum(s + 1 for s in SIZES)
    out = ["/* SPDX-License-Identifier: GPL-3.0-only",
           " * Copyright (C) 2026 Dungeon Tracker (RONDO) contributors */",
           "/* generated by tools/gen_wt32.py: WAVE 32's banks (eng_wt32.c) */",
           f"#define WT32_NB {len(gens)}",
           f"#define WT32_NF {NF}",
           f"#define WT32_FRAME {per}             /* a frame's seven levels, each with its guard sample */",
           "#define WT32_NL 7",
           "static const uint16_t WT32_OFS[WT32_NL] = {" + ", ".join(str(sum(s + 1 for s in SIZES[:i])) for i in range(7)) + "};",
           "static const uint8_t WT32_BITS[WT32_NL] = {" + ", ".join(str(int(math.log2(s))) for s in SIZES) + "};",
           "static const uint8_t WT32_HARM[WT32_NL] = {" + ", ".join(str(h) for h in HARM) + "};",
           f"static const int16_t WT32[WT32_NB * WT32_NF][WT32_FRAME] = {{"]
    for b in range(len(gens)):
        out.append(f"    /* {BANKS[b]} */")
        for f in range(NF):
            d = rows[b * NF + f]
            out.append("    {" + ",".join(str(v) for v in d) + "},")
    out.append("};")
    dest = pathlib.Path(__file__).resolve().parent.parent / "firmware" / "src" / "wt32_tables.h"
    dest.write_text("\n".join(out) + "\n")
    print(f"WAVE 32: {len(gens)} banks x {NF} frames x {per} samples -> {dest} ({len(gens) * NF * per * 2} B)")


if __name__ == "__main__":
    main()
