#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-only
# Copyright (C) 2026 Dungeon Tracker (RONDO) contributors
"""Dungeon Tracker: the DUNGEON KIT (SAMPLE's SET 1, the index Felucca's retired TRANH kept as an alias): a drum kit
of dungeon sounds -- a door's boom, an anvil's clang, an iron cell door, a bone, chains, a torch's whoosh, a tolling
bell, a rattle of bones, a drip, a gong -- from Versilian's VCSL (CC0 1.0), the drip made here. Each through a small
stone room and a light tape treatment (make_lofi_kit.py's), cut to its length and written as a 16 kHz 16-bit WAV into
assets/samples-cc0/DUNGEON/, in the LOFI KIT's key order (gen_samples.py: DUNGEON_MAP, the General MIDI drum notes,
plus the crash's). The processed files are in the tree; this script remakes them from a VCSL checkout:

  GIT_LFS_SKIP_SMUDGE=1 git clone --depth 1 --filter=blob:none --no-checkout https://github.com/sgossner/vcsl VCSL
  (git -C VCSL checkout HEAD -- the files in KIT below)
  make_dungeon_kit.py VCSL

Everything is seeded: the files come out the same.
"""
import sys
from pathlib import Path

import numpy as np
from scipy import signal

sys.path.insert(0, str(Path(__file__).resolve().parent))
from make_lofi_kit import SR, biquad, load, shape, tape, write  # noqa: E402

DEST = Path(__file__).resolve().parents[1] / "assets" / "samples-cc0" / "DUNGEON"
I, M = "Idiophones/Struck Idiophones", "Membranophones/Struck Membranophones"
SRC = {
    "boom": M + "/Bass Drum 2/bassdrum_hit_ff.wav",
    "log": I + "/Slit Drum/LogDrumLo_MedM_v3_rr1_Sum.wav",
    "anvil": I + "/Anvil/Anvil_Hit2_v3_rr1_Mid.wav",
    "brake": I + "/Brake Drum/BrakeDrum1_Hammer_v3_rr1_Mid.wav",
    "sleigh": I + "/Sleigh Bells/Sleighbells_Hit_rr1_Mid.wav",
    "claves": I + "/Claves/Claves2_Hit_v3_rr1_Mid.wav",
    "ocean": "Membranophones/Other Membranophones/Ocean Drum/OceanDrum_Sus_1_Mid.wav",
    "bell": I + "/Tubular Bells 1/chimes_C3_f_rr1.wav",
    "ratchet": I + "/Ratchet/Ratchet1_Fast_rr1_Mid.wav",
    "gong": I + "/Gong 1/gong_f.wav",
}


def room(x, rng, wet=0.25, secs=0.18):
    """a small stone room: a few early reflections and a short dark tail"""
    n = int(secs * SR)
    ir = rng.standard_normal(n) * np.exp(-np.arange(n) / (0.045 * SR))
    ir = biquad(ir, "lp", 2800)
    for ms, g in ((9, 0.5), (17, 0.35), (29, 0.25)):
        ir[int(ms * SR / 1000)] += g
    y = signal.fftconvolve(x, ir)[:len(x)]
    y /= np.abs(y).max() + 1e-9
    return (1 - wet) * x / (np.abs(x).max() + 1e-9) + wet * y


def pad(x, secs):
    n = int(secs * SR)
    return np.concatenate([x, np.zeros(max(0, n - len(x)))])[:n]


def mix(*parts):
    n = max(len(p) for p, _ in parts)
    out = np.zeros(n)
    for p, g in parts:
        out[:len(p)] += g * p / (np.abs(p).max() + 1e-9)
    return out


def drip(rng):
    """a drop into a pool: a rising chirp, then a ring, and its echo off the far wall"""
    n = int(0.12 * SR)
    t = np.arange(n) / SR
    f = 700 + 1300 * (1 - np.exp(-t / 0.012))
    ph = 2 * np.pi * np.cumsum(f) / SR
    x = np.sin(ph) * np.exp(-t / 0.028) * (1 - np.exp(-t / 0.0015))
    e = int(0.055 * SR)
    x[e:] += 0.22 * x[:n - e]
    return biquad(x, "hp", 300)


def torch(v, rng):
    """a torch swung past: the ocean drum's beads as a whoosh, a little crackle"""
    o = load(v / SRC["ocean"])
    o = o[int(0.4 * SR):int(0.4 * SR) + int(0.26 * SR)]
    n = len(o)
    env = np.sin(np.linspace(0, np.pi, n)) ** 1.5
    x = biquad(o * env, "hp", 900)
    c = np.zeros(n)
    for i in rng.integers(0, n - 40, 9):
        c[i:i + 30] += rng.standard_normal(30) * np.exp(-np.arange(30) / 6) * rng.uniform(0.3, 1.0)
    return mix((x, 1.0), (biquad(c, "hp", 2000), 0.35))


KIT = [   # name, how (from the sources), length s, fade curve, tape (drive, top Hz, warmth dB), room wet
    ("DOOM", lambda v, r: mix((shape(load(v / SRC["boom"]), 0.5, 0.8, -3), 1.0),
                              (shape(load(v / SRC["log"]), 0.3, 1.0, -5), 0.6)), 0.30, 1.4, (3.0, 3200, 6), 0.20),
    ("CLANG", lambda v, r: shape(load(v / SRC["anvil"]), 0.4, 0.6, -2), 0.24, 1.3, (2.2, 6500, 2), 0.30),
    ("CELL", lambda v, r: mix((shape(load(v / SRC["brake"]), 0.35, 0.8, -3), 1.0),
                              (shape(load(v / SRC["sleigh"]), 0.2, 1.5, -6), 0.25)), 0.20, 1.5, (2.6, 5500, 2), 0.30),
    ("BONE", lambda v, r: shape(load(v / SRC["claves"]), 0.12, 1.5, -4), 0.07, 2.0, (2.0, 5000, 3), 0.15),
    ("CHAIN", lambda v, r: biquad(shape(load(v / SRC["sleigh"]), 0.2, 1.2, -5), "hp", 1500), 0.10, 1.8,
     (1.8, 7000, 0), 0.20),
    ("TORCH", lambda v, r: torch(v, r), 0.26, 1.0, (1.6, 7000, 0), 0.20),
    ("TOLL", lambda v, r: shape(load(v / SRC["bell"]), 0.6, 0.5, -3), 0.28, 1.2, (1.8, 5000, 3), 0.25),
    ("RATTLE", lambda v, r: biquad(shape(load(v / SRC["ratchet"]), 0.25, 0.6, -2), "pk", 900, 1.0, 5), 0.16, 1.3,
     (2.0, 6000, 1), 0.20),
    ("DRIP", lambda v, r: drip(r), 0.12, 1.0, (1.4, 7000, 0), 0.35),
    ("GONG", lambda v, r: shape(load(v / SRC["gong"]), 0.7, 0.4, -2), 0.45, 1.1, (2.0, 5500, 2), 0.20),
]


def main(vcsl):
    v = Path(vcsl)
    rng = np.random.default_rng(13)
    DEST.mkdir(parents=True, exist_ok=True)
    for old in DEST.glob("*.wav"):
        old.unlink()
    total = 0
    for k, (name, how, secs, curve, (drive, top, warm), wet) in enumerate(KIT):
        x = pad(how(v, rng), secs * 1.2)
        x = room(tape(x, rng, drive, top, warm, wow=0.0015), rng, wet)
        total += write(DEST / f"{k:02d} {name}.wav", shape(x, secs, curve))
    print(f"DUNGEON KIT: {len(KIT)} hits, {total / 16000:.2f} s at 16000 Hz -> {DEST}")


if __name__ == "__main__":
    main(sys.argv[1])
