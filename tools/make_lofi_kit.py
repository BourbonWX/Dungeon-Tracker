#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-only
# Copyright (C) 2026 Dungeon Tracker (RONDO) contributors
"""Dungeon Tracker: the LOFI KIT (SAMPLE's SET 3, in place of 1.2's SAX): real acoustic drums from Versilian's
VCSL (CC0 1.0) through a tape treatment -- pitched down a little, low-mid warmth, asymmetric tape saturation, wow and
flutter, the top end rolled off -- then cut to their lengths and written as 16 kHz 16-bit WAVs into
assets/samples-cc0/LOFI/, one per key from F3 (gen_samples.py: kind "lofi", KIT_BASE). The processed files are in
the tree; this script remakes them from a VCSL checkout:

  GIT_LFS_SKIP_SMUDGE=1 git clone --depth 1 --filter=blob:none --no-checkout https://github.com/sgossner/vcsl VCSL
  (git -C VCSL checkout HEAD -- the files in KIT and SHELLS below)
  make_lofi_kit.py VCSL

SHAKER is two takes of VCSL's large shaker layered, with a hollow bump at 850 Hz. SHELLS is a rattle of small wooden
shells, made from one VCSL woodblock click: eleven copies pitched up 4..13 semitones, knocking in a quick fading
flurry over a little of the small shaker, with hollow mids. Everything is seeded: the files come out the same.
"""
import struct
import sys
from pathlib import Path

import numpy as np
from scipy import signal
from scipy.io import wavfile

SR, OUT_SR = 44100, 16000
DEST = Path(__file__).resolve().parents[1] / "assets" / "samples-cc0" / "LOFI"
M, I = "Membranophones/Struck Membranophones", "Idiophones/Struck Idiophones"
# key order (F3 up): name, VCSL file, length s, fade curve, semitones, tape (drive, top Hz, warmth dB)
KIT = [
    ("KICK", M + "/Bass Drum 1/BDrumNew_hit_v7_rr1_Sum.wav", 0.34, 1.6, -1, (3.0, 4200, 6)),
    ("SNARE", M + "/Snare Drum, Modern 1/Snare2_HitSN_v7_rr1_Mid.wav", 0.26, 1.8, -1, (2.6, 6000, 4)),
    ("SNARE2", M + "/Snare Drum, Modern 2/Snare3M_HitSN_v5_rr1_Mid.wav", 0.20, 2.0, -2, (3.2, 5000, 4)),
    ("RIM", M + "/Snare Drum, Modern 2/Snare3M_Xstick_v2_rr1_Mid.wav", 0.09, 2.0, 0, (2.0, 6500, 4)),
    ("CL HAT", I + "/Hi-Hat Cymbal/HiHat_HitC_v3_rr1_Mid.wav", 0.08, 2.0, 0, (1.8, 7000, 0)),
    ("OP HAT", I + "/Hi-Hat Cymbal/HiHat_HitO_rr1_Mid.wav", 0.30, 1.4, 0, (1.8, 7000, 0)),
    ("TOM", M + "/Tom 2/Stick/TomL_HitS_v3_rr1_Mid.wav", 0.26, 1.5, -2, (2.4, 5000, 4)),
]
SHAKER = (I + "/Shaker, Large/LShaker_Shake1D_rr2_Mid.wav", I + "/Shaker, Large/LShaker_Hit_rr1_Mid.wav")
SHELLS = (I + "/Woodblock/wood_click_mp.wav", I + "/Shaker, Small/Mid_ShakerLowFaster_Down_rr1.wav")
SOURCES = [f for _, f, *_ in KIT] + list(SHAKER) + list(SHELLS)


def load(path):
    sr, x = wavfile.read(path)
    x = x.astype(float)
    x = x.mean(1) if x.ndim > 1 else x
    x = x / (np.abs(x).max() + 1e-9)
    if sr != SR:
        x = signal.resample_poly(x, SR, sr)
    on = int(np.argmax(np.abs(x) > 0.05))
    return x[max(0, on - 40):]


def biquad(x, kind, f, q=0.707, gain=0.0):
    if kind == "lp":
        b, a = signal.butter(2, f / (SR / 2), "low")
    elif kind == "hp":
        b, a = signal.butter(2, f / (SR / 2), "high")
    else:                                              # peaking
        A = 10 ** (gain / 40)
        w0 = 2 * np.pi * f / SR
        al = np.sin(w0) / (2 * q)
        b = [1 + al * A, -2 * np.cos(w0), 1 - al * A]
        a = [1 + al / A, -2 * np.cos(w0), 1 - al / A]
    return signal.lfilter(b, a, x)


def tape(x, rng, drive, top, warm, wow=0.0025):
    x = x / np.abs(x).max()
    x = biquad(x, "pk", 140, 0.8, warm)                 # low-mid warmth
    x = np.tanh(drive * x + 0.12) - np.tanh(0.12)       # saturation, a little asymmetric
    t = np.arange(len(x)) / SR                          # wow and flutter: a wandering read head
    d = wow * SR * (0.6 * np.sin(2 * np.pi * 0.7 * t + rng.random() * 6) + 0.4 * np.sin(2 * np.pi * 7.3 * t))
    x = np.interp(np.arange(len(x)) - d - d.max(), np.arange(len(x)), x)
    x = biquad(biquad(x, "lp", top), "lp", top * 1.15)  # the top end gone
    x = biquad(x, "hp", 35)
    return x / np.abs(x).max()


def shape(x, secs, curve=1.0, semi=0):
    if semi:
        x = signal.resample(x, int(len(x) * 2 ** (-semi / 12)))
    n = min(len(x), int(secs * SR))
    return x[:n] * np.linspace(1, 0, n) ** curve


def shaker(v):
    a, b = load(v / SHAKER[0]), load(v / SHAKER[1])
    n = int(0.24 * SR)
    x = np.zeros(n)
    x[:min(n, len(a))] += a[:n] * 0.9
    x[:min(n, len(b))] += b[:n] * 0.6
    x = biquad(x, "pk", 850, 1.2, 6)                    # a hollow mid body
    return x * np.linspace(1, 0, n) ** 1.3


def shells(v):
    c = load(v / SHELLS[0])[:int(0.03 * SR)]
    c = c * np.linspace(1, 0, len(c)) ** 2
    r = np.random.default_rng(11)
    n = int(0.20 * SR)
    x, t = np.zeros(n), 0.0
    for k in range(11):                                 # the shells knock together: a quick flurry, fading
        y = signal.resample(c, int(len(c) * 2 ** (-r.uniform(4, 13) / 12)))
        i = int(t * SR)
        m = min(len(y), n - i)
        x[i:i + m] += y[:m] * (0.95 ** k) * r.uniform(0.5, 1.0)
        t += r.uniform(0.004, 0.017)
    g = load(v / SHELLS[1])[:n]
    x[:len(g)] += g * 0.25
    x = biquad(biquad(x, "pk", 700, 1.0, 7), "pk", 1500, 1.4, 3)
    return x * np.linspace(1, 0, n) ** 1.1


def write(path, x):
    y = signal.resample_poly(x, 160, 441) * 0.95        # to 16 kHz (filtered: tape is smooth)
    wavfile.write(path, OUT_SR, (np.clip(y, -1, 1) * 32767).astype(np.int16))
    return len(y)


def main(vcsl):
    v = Path(vcsl)
    rng = np.random.default_rng(7)
    DEST.mkdir(parents=True, exist_ok=True)
    for old in DEST.glob("*.wav"):
        old.unlink()
    total, k = 0, 0
    for name, f, secs, curve, semi, (drive, top, warm) in KIT:
        x = shape(tape(shape(load(v / f), secs * 1.3, 0.5, semi), rng, drive, top, warm), secs, curve)
        total += write(DEST / f"{k:02d} {name}.wav", x)
        k += 1
    for name, fn, (drive, top, warm) in (("SHAKER", shaker, (2.0, 6000, 2)), ("SHELLS", shells, (1.8, 5500, 1))):
        total += write(DEST / f"{k:02d} {name}.wav", tape(fn(v), rng, drive, top, warm))
        k += 1
    print(f"LOFI KIT: {k} hits, {total / OUT_SR:.2f} s at {OUT_SR} Hz -> {DEST}")


if __name__ == "__main__":
    main(sys.argv[1])
