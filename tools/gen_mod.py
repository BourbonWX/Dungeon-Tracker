#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-only
"""Dungeon Tracker 1.5: the samples of the AMIGA'D family (firmware/src/eng_mod.c), written to firmware/src/mod_tables.h.

Everything here is synthesized (no recordings): 8-bit signed samples in the tracker style, an attack and a loop.
A tone's loop holds whole cycles (a single cycle, or several detuned voices that all close on the loop), so it
sustains for ever and the engine's envelope ends it; a drum's loop is its tail (the kick's last cycle, the snare's
noise, the hat's shimmer), faded by the kit's DECAY, the way tracker musicians made a short sample ring.

Each sample: its data, its loop start (== its length: a one-shot), and the Amiga period that plays it at its root
(an instrument: C4, MIDI 60; a drum: as designed, PITCH 64). The periods are the Paula chip's (PAL clock 3546895 Hz).
    python3 tools/gen_mod.py"""
import math
import pathlib
import numpy as np

PAULA = 3546895
R = 16574                        # ProTracker's C-3 rate (period 214): most samples are made at it
C4 = 261.6256
rng = np.random.default_rng(1987)

INST = ["SQUARE", "PULSE", "SAW", "TRI", "SINE", "BASS", "SYNC", "ORGAN", "EPIANO", "PLUCK", "STRINGS", "CHOIR",
        "HOOVER", "STAB", "BELL", "NOISE"]
HIT = [["909", "808", "TIGHT", "GABBER", "DUSTY", "DOOM", "ELECTRO", "SOFT"],          # KICK
       ["909", "808", "LOFI", "CRACK", "CLAP", "CLANG", "BREAK", "SNAP"],              # SNARE
       ["CLOSED", "DUSTY", "OPEN", "RIDE", "CRASH", "CHAIN", "SHAKER", "TAMB"],       # HAT
       ["TOM", "CONGA", "COWBELL", "BONE", "RIM", "TOLL", "ZAP", "BLOCK"]]            # PERC


def period_for(rate_needed):
    return int(max(28, min(65535, round(PAULA / rate_needed))))


def saw_cycle(n, harm=None):
    t = np.arange(n) / n
    if harm is None:
        return 1.0 - 2.0 * t
    return sum(np.sin(2 * np.pi * k * t) / k for k in range(1, harm + 1)) * (2 / np.pi)


def lp1(x, a):
    y = np.zeros_like(x)
    s = 0.0
    for i, v in enumerate(x):
        s += a * (v - s)
        y[i] = s
    return y


def hp1(x, a):
    return x - lp1(x, a)


def norm(x, peak=0.94):
    x = np.asarray(x, dtype=float)
    x = x - x.mean()
    m = np.max(np.abs(x)) or 1.0
    return x * (peak / m)


class Smp:
    def __init__(self, data, loop, period):
        self.data, self.loop, self.period = np.asarray(data, dtype=float), loop, period


def tone(attack, cycle_len, loop, f0_in_sample, rate=R):
    """an instrument: attack then loop; f0_in_sample its fundamental at `rate` -> the period of C4"""
    data = np.concatenate([attack, loop]) if len(attack) else loop
    p = period_for(rate * C4 / f0_in_sample)
    return Smp(norm(data), len(attack), p)


def ensemble(cyc, k, detunes, lpa=None):
    """voices of a cycle shape each closing on a loop of cyc * k samples: voice j makes k + d_j cycles"""
    n = cyc * k
    t = np.arange(n) / n
    out = np.zeros(n)
    for d in detunes:
        ph = (t * (k + d)) % 1.0
        out += 1.0 - 2.0 * ph
    if lpa:
        out = lp1(np.concatenate([out, out]), lpa)[n:]   # (the filter run over the loop twice: it closes)
    return out


def make_inst():
    out = []
    N = 64
    f0 = R / N
    sq = np.where(np.arange(32) < 16, 1.0, -1.0)
    out.append(tone([], 32, sq, R / 32))
    pu = np.where(np.arange(32) < 8, 1.0, -1.0)
    out.append(tone([], 32, pu, R / 32))
    out.append(tone([], N, saw_cycle(N), f0))
    t = np.arange(32) / 32
    out.append(tone([], 32, 1 - 4 * np.abs(t - 0.5), R / 32))
    out.append(tone([], N, np.sin(2 * np.pi * np.arange(N) / N), f0))
    # BASS: a saw through a falling low-pass for 0.07 s, then its last cycle
    n = 1152
    sw = np.tile(saw_cycle(N), n // N + 1)[:n]
    a = np.linspace(0.9, 0.18, n)
    y = np.zeros(n)
    s = 0.0
    for i in range(n):
        s += a[i] * (sw[i] - s)
        y[i] = s
    out.append(tone(y[:-N], N, y[-N:], f0))
    # SYNC: a hard-synced saw (slave 2.7x), one cycle
    tt = np.arange(N) / N
    out.append(tone([], N, 1 - 2 * ((tt * 2.7) % 1.0), f0))
    # ORGAN: drawbars 16' 8' 4' 2 2/3'
    org = sum(g * np.sin(2 * np.pi * h * tt) for h, g in ((1, 1.0), (2, 0.8), (3, 0.5), (4, 0.6), (6, 0.3), (8, 0.25)))
    out.append(tone([], N, org, f0))
    # EPIANO: FM 1:1 with a falling index, then a mellow loop
    n = 1536
    ti = np.arange(n)
    idx = 2.6 * np.exp(-ti / 380.0) + 0.3
    ep = np.sin(2 * np.pi * ti / N + idx * np.sin(2 * np.pi * ti / N))
    out.append(tone(ep[:-N], N, ep[-N:], f0))
    # PLUCK: Karplus-Strong, period 64, 0.09 s, then its last cycle
    n = 1536
    buf = rng.uniform(-1, 1, N)
    pl = np.zeros(n)
    for i in range(n):
        v = buf[i % N]
        pl[i] = v
        buf[i % N] = 0.5 * (v + buf[(i + 1) % N]) * 0.996
    out.append(tone(pl[:-N], N, pl[-N:], f0))
    # STRINGS: three saws closing on 64 x 32 samples (detune 1/32 of the note), softened
    out.append(tone([], N, ensemble(N, 32, (0, 1, -1), 0.35), f0))
    # CHOIR: an "aah" on harmonics, three voices
    k = 32
    nn = N * k
    tt2 = np.arange(nn) / nn
    ch = np.zeros(nn)
    for d in (0, 1, -1):
        for h in range(1, 24):
            f = h * C4
            g = sum(G / (1 + ((f - F) / B) ** 2) for F, G, B in ((730, 1.0, 110), (1090, 0.5, 130), (2440, 0.25, 180)))
            ch += g * np.sin(2 * np.pi * h * (k + d) * tt2 + h * d)
    out.append(tone([], N, ch, f0))
    # HOOVER: PWM-ish detuned saws, an octave down too
    hv = ensemble(N, 16, (0, 1, -1, 2)) + 0.6 * ensemble(N * 2, 8, (0, 1))[: N * 16]
    out.append(tone([], N, hv, f0))
    # STAB: a minor-7 chord of saws, decaying, one shot (at half the rate)
    rs = R / 2
    n = int(rs * 0.42)
    ti = np.arange(n) / rs
    st = sum(1 - 2 * ((ti * C4 * r) % 1.0) for r in (1, 1.189, 1.498, 1.782))
    st = lp1(st * np.exp(-ti / 0.15), 0.5)
    out.append(Smp(norm(st), n, period_for(rs)))
    # BELL: inharmonic, one shot at half the rate
    n = int(rs * 0.55)
    ti = np.arange(n) / rs
    bl = sum(g * np.sin(2 * np.pi * C4 * r * ti) * np.exp(-ti / d) for r, g, d in
             ((1, 1.0, 0.35), (2.76, 0.5, 0.2), (5.4, 0.3, 0.1), (8.93, 0.15, 0.06)))
    out.append(Smp(norm(bl), n, period_for(rs)))
    # NOISE: a loop of white noise (wind, fx)
    out.append(Smp(norm(rng.uniform(-1, 1, 1024)), 0, period_for(R)))
    return out


def drum_sine(rate, f_hi, f_lo, tau, n_att, drive=0.0, click=0.0, square=False):
    """a pitched drum: a sine swept from f_hi down to f_lo, then whole cycles at f_lo as its loop"""
    cyc = max(8, int(round(rate / f_lo)))
    f_lo = rate / cyc
    ph = 0.0
    y = []
    for i in range(n_att):
        f = f_lo + (f_hi - f_lo) * math.exp(-i / (tau * rate))
        ph += f / rate
        y.append(math.sin(2 * math.pi * ph))
    while (ph % 1.0) > 1.0 / cyc:          # (run on to the cycle's start: the loop joins)
        ph += f_lo / rate
        y.append(math.sin(2 * math.pi * ph))
    y = np.array(y)
    env = np.exp(-np.arange(len(y)) / (n_att * 1.4))
    y = y * (0.35 + 0.65 * env)
    loop = np.sin(2 * np.pi * np.arange(cyc) / cyc) * 0.35
    if square:
        y, loop = np.sign(y) * np.abs(y) ** 0.3, np.sign(loop) * np.abs(loop) ** 0.3 * 0.35 / 0.35
    if drive:
        y, loop = np.tanh(y * drive) / np.tanh(drive), np.tanh(loop * drive) / np.tanh(drive)
    if click:
        y[:24] += click * rng.uniform(-1, 1, 24) * np.linspace(1, 0, 24)
    return np.concatenate([y, loop]), len(y)


def noise_hit(rate, n_att, n_loop, hp, lp, tone_f=0.0, tone_amt=0.0, tone_tau=0.04, bursts=1):
    n = n_att + n_loop
    w = rng.uniform(-1, 1, n)
    w = lp1(hp1(w, hp), lp)
    env = np.ones(n)
    env[:n_att] = np.exp(-np.arange(n_att) / (n_att * 0.6)) * 0.6 + 0.4
    if bursts > 1:                             # a clap: bursts before the body
        g = int(rate * 0.009)
        for b in range(bursts):
            env[b * g:(b + 1) * g] *= np.linspace(1.4, 0.5, g)
    env[n_att:] = 0.4
    y = w * env
    if tone_f:
        ti = np.arange(n_att) / rate
        y[:n_att] += tone_amt * np.sin(2 * np.pi * tone_f * ti) * np.exp(-ti / tone_tau)
    return y, n_att


def metal(rate, n, freqs, amp=1.0):
    """partials whose cycles close on n samples (a loop of them joins)"""
    t = np.arange(n)
    out = np.zeros(n)
    for f in freqs:
        k = max(1, int(round(f * n / rate)))
        out += np.sign(np.sin(2 * np.pi * k * t / n))
    return out * amp / max(1, len(freqs))


def make_hits():
    hits = []
    R1, R2 = 11084, 8287                    # (periods 320, 428)
    # KICKS
    k = [drum_sine(R1, 160, 55, 0.025, 700, click=0.3),
         drum_sine(R2, 110, 45, 0.04, 900),
         drum_sine(R1, 220, 70, 0.012, 450, click=0.5),
         drum_sine(R1, 260, 60, 0.02, 800, drive=5.0, click=0.4),
         drum_sine(R2, 130, 52, 0.03, 650, drive=1.5),
         drum_sine(R2, 90, 38, 0.06, 1200, drive=2.0),
         drum_sine(R1, 180, 50, 0.03, 700, square=True),
         drum_sine(R2, 100, 50, 0.03, 600)]
    for i, (d, l) in enumerate(k):
        if i == 4:                                   # DUSTY: crushed, darker
            d = lp1(np.round(d * 12) / 12, 0.5)
        if i == 5:                                   # DOOM: a rumble under the thud
            d = d + np.concatenate([lp1(rng.uniform(-1, 1, l), 0.05) * 0.5, np.zeros(len(d) - l)])
        hits.append(Smp(norm(d), l, period_for([R1, R2, R1, R1, R2, R2, R1, R2][i])))
    # SNARES
    s = [noise_hit(R, 900, 1024, 0.25, 0.75, 190, 0.8, 0.03),
         noise_hit(R1, 700, 1024, 0.2, 0.6, 175, 1.2, 0.05),
         noise_hit(R1, 800, 1024, 0.15, 0.35, 180, 0.7, 0.04),
         noise_hit(R, 500, 512, 0.45, 0.9, 420, 0.9, 0.012),
         noise_hit(R, 1200, 1024, 0.3, 0.7, bursts=3),
         None,
         noise_hit(R1, 1000, 1024, 0.18, 0.8, 200, 0.6, 0.05),
         noise_hit(R, 600, 512, 0.4, 0.95, 250, 0.7, 0.02)]
    for i, x in enumerate(s):
        if x is None:                                # CLANG: an anvil, partials that loop
            n = 1024
            m = metal(R, n, (460, 1180, 1710, 2630, 3390))
            att = np.concatenate([m, m])[:1400] * np.exp(-np.arange(1400) / 500.0) * 1.5
            hits.append(Smp(norm(np.concatenate([att, m * 0.5])), 1400, period_for(R)))
            continue
        d, l = x
        if i == 2:
            d = np.round(d * 10) / 10
        hits.append(Smp(norm(d), l, period_for([R, R1, R1, R, R, R, R1, R][i])))
    # HATS (at R; the loop a shimmer of metal and noise)
    def hat(n_att, hp, lp, partials=(3150, 4240, 5300, 6530, 7710), noise=0.6, loop=1024):
        m = metal(R, loop, partials)
        w = lp1(hp1(rng.uniform(-1, 1, n_att + loop), hp), lp)
        body = np.concatenate([m, m, m, m])[: n_att + loop] * (1 - noise) + w * noise
        env = np.concatenate([np.exp(-np.arange(n_att) / (n_att * 0.5)) * 0.7 + 0.3, np.full(loop, 0.3)])
        return Smp(norm(hp1(body * env, 0.5)), n_att, period_for(R))
    hits += [hat(300, 0.7, 0.95), hat(400, 0.5, 0.6, noise=0.8), hat(500, 0.65, 0.95),
             hat(700, 0.6, 0.9, (2210, 3330, 4170, 5680), noise=0.3), hat(1400, 0.5, 0.9, noise=0.85),
             hat(600, 0.55, 0.85, (1830, 2970, 3610, 4890, 6210), noise=0.2), hat(500, 0.8, 0.7, noise=1.0),
             hat(500, 0.6, 0.9, (5110, 6990, 7930), noise=0.4)]
    # PERCS
    t1, l1 = drum_sine(R1, 260, 110, 0.03, 500)
    t2, l2 = drum_sine(R1, 520, 260, 0.01, 350, click=0.4)
    n = 1024
    cb = metal(R, n, (540, 800))
    cba = np.concatenate([cb, cb])[:900] * np.exp(-np.arange(900) / 400.0) * 1.3
    ti = np.arange(500) / R
    bone = np.sin(2 * np.pi * 1600 * ti) * np.exp(-ti / 0.012)
    rim = np.concatenate([rng.uniform(-1, 1, 30) * np.linspace(1, 0, 30), np.zeros(320)]) + \
        np.sin(2 * np.pi * 820 * np.arange(350) / R) * np.exp(-np.arange(350) / 70.0)
    toll = metal(R2, 2048, (220, 440 * 1.19, 660 * 1.01, 880 * 1.33))
    tolla = np.concatenate([toll, toll])[:2000] * np.exp(-np.arange(2000) / 900.0) * 1.4
    zap, lz = drum_sine(R, 2400, 180, 0.01, 1000)
    tb = np.arange(700) / R
    block = (np.sin(2 * np.pi * 1180 * tb) + 0.5 * np.sin(2 * np.pi * 2760 * tb)) * np.exp(-tb / 0.02)
    hits += [Smp(norm(t1), l1, period_for(R1)), Smp(norm(t2), l2, period_for(R1)),
             Smp(norm(np.concatenate([cba, cb * 0.4])), 900, period_for(R)), Smp(norm(bone), 500, period_for(R)),
             Smp(norm(rim), 350, period_for(R)), Smp(norm(np.concatenate([tolla, toll * 0.4])), 2000, period_for(R2)),
             Smp(norm(zap[:lz]), lz, period_for(R)), Smp(norm(block), 700, period_for(R))]
    return hits


def main():
    inst, hits = make_inst(), make_hits()
    assert len(inst) == 16 and len(hits) == 32
    blob, rows = [], []
    for s in inst + hits:
        q = np.clip(np.round(s.data * 127), -127, 127).astype(int)
        rows.append((len(blob), len(q), min(s.loop, len(q)), s.period))
        blob += list(q)
    out = ["/* SPDX-License-Identifier: GPL-3.0-only",
           " * Copyright (C) 2026 Dungeon Tracker (RONDO) contributors */",
           "/* generated by tools/gen_mod.py: the AMIGA'D family's samples (eng_mod.c), 8-bit, all synthesized */",
           "typedef struct { uint32_t off; uint16_t len, loop, period; } mod_smp_t;   /* loop == len: a one-shot */",
           f"#define MOD_NINST {len(inst)}",
           f"#define MOD_NHIT {len(hits)}",
           f"static const int8_t MOD_DATA[{len(blob)}] = {{"]
    for i in range(0, len(blob), 32):
        out.append("    " + ",".join(str(v) for v in blob[i:i + 32]) + ",")
    out.append("};")
    out.append("static const mod_smp_t MOD_SMP[MOD_NINST + MOD_NHIT] = {   /* the instruments, then the kit's hits (lane x 8) */")
    names = INST + [f"{['KICK', 'SNARE', 'HAT', 'PERC'][i // 8]} {HIT[i // 8][i % 8]}" for i in range(32)]
    for (o, n, l, p), nm in zip(rows, names):
        out.append(f"    {{{o}, {n}, {l}, {p}}},   /* {nm} */")
    out.append("};")
    dest = pathlib.Path(__file__).resolve().parent.parent / "firmware" / "src" / "mod_tables.h"
    dest.write_text("\n".join(out) + "\n")
    print(f"AMIGA'D: {len(inst)} instruments, {len(hits)} hits, {len(blob)} B -> {dest}")


if __name__ == "__main__":
    main()
