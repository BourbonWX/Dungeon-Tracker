/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright (C) 2026 Dungeon Tracker (RONDO) contributors */
/* WAVE 32 (engine 20, Dungeon Tracker 1.5): WAVES' hi-fi sibling. Where WAVES plays Ambika's 8-bit waves as the
 * Shruthi did, WAVE 32 morphs through eight banks of eight band-limited 16-bit frames (tools/gen_wt32.py,
 * wt32_tables.h): CLASSIC (sine .. saw .. thin pulse), PWM, FORMANT (vowels), ORGAN (drawbars), GLASS (a bright cluster
 * moving up), SYNC (a hard-synced saw), DIGITAL (PPG-style spectra), GROWL (a comb through a saw).
 *   BANK  the bank; POS the place in it (between two frames, cross-faded); PENV the envelope on POS (the matrix's
 *   SHAPE moves it too); DTUN a second oscillator a few cents up; CUT RES ANALOG's filter (the part's FILTER TYPE);
 *   SUB a sine an octave down; WARP bends the phase the frames are read at (brighter, a little grit).
 * Each frame is kept at seven levels (a mipmap): the note reads the fullest whose top harmonic stays under ~11.5 kHz,
 * so no note aliases (WARP aside; the interpolation's images under -35 dB). Included by engines.c after WAVES (its filter's helpers) */
#ifndef DT_WT32_C
#define DT_WT32_C 1
#include "wt32_tables.h"

static const char *const N_WT32_BANK[] = {"CLASSIC", "PWM", "FORMANT", "ORGAN", "GLASS", "SYNC", "DIGITAL", "GROWL", 0};

/* the level a note of increment inc reads (0 = 256 samples, 120 harmonics .. 6 = 8, 1) */
static uint32_t wt32_level(uint32_t inc)
{
    uint32_t l = 0;
    while (l < WT32_NL - 1u && (uint64_t)inc * WT32_HARM[l] > 2058000000ull)   /* (11.5 kHz of 24 kHz, x 2^32) */
        l++;
    return l;
}
/* frame f's sample at phase ph, size l (interpolated) */
static inline int32_t wt32_read(const int16_t *w, uint32_t bits, uint32_t ph)
{
    uint32_t i = ph >> (32u - bits), fr = (ph >> (17u - bits)) & 0x7fffu;
    int32_t a = w[i], b = w[i + 1u];
    return a + (((b - a) * (int32_t)fr) >> 15);
}

static void wt32_note_on(track_t *t, voice_t *v)
{
    (void)t;
    v->s[0] = v->s[1] = 0;                                /* the filter */
    v->s[7] = 0;                                          /* the DC blocker */
    v->ph[1] = v->ph[0] + (1u << 25);                     /* (the second oscillator an 1/128 turn apart) */
    v->ph[2] = 0;                                         /* the sub */
}

static void wt32_render(track_t *t, voice_t *v, int32_t *out, uint32_t n, const vmod_t *m)
{
    const int16_t *p = t->p;
    uint32_t bank = (uint32_t)clamp(p[P_E0], 0, WT32_NB - 1), i, lvl, bits, fa, fb;
    int32_t pos = clamp(p[P_E1] + ((p[P_E2] * m->envq15) >> 15) + ((m->shape - (64 << 8)) >> 8), 0, 127);
    int32_t fp = pos * (WT32_NF - 1) * 256 / 127, g2 = fp & 255, g1 = 256 - g2;
    int32_t dt = clamp(p[P_E3], 0, 127), sub = p[P_E6] * 258, warp = clamp(p[P_E7], 0, 127);
    int32_t cut = (p[P_E4] << 8) + m->cutoff, ic1 = v->s[0], ic2 = v->s[1], dc = v->s[7], wk = warp * warp * 2;
    uint32_t ph = v->ph[0], ph2 = v->ph[1], sph = v->ph[2], inc = m->inc;
    uint32_t inc2 = inc + (inc >> 12) * (uint32_t)(dt * 72 / 127), ft = (uint32_t)p[P_FTYPE];
    const int16_t *w1, *w2;
    int32_t fk = ANALOG_K(p[P_E5]);
    tsvf_t flt;
    tsvf_coef(&flt, cut, p[P_E5]);
    fa = (uint32_t)(fp >> 8);
    fb = fa + 1u < WT32_NF ? fa + 1u : fa;
    lvl = wt32_level(dt ? inc2 : inc);
    bits = WT32_BITS[lvl];
    w1 = WT32[bank * WT32_NF + fa] + WT32_OFS[lvl];
    w2 = WT32[bank * WT32_NF + fb] + WT32_OFS[lvl];
    for (i = 0; i < n; i++) {
        uint32_t r = warp ? ph + (uint32_t)(sine_i(ph) * wk) : ph;
        int32_t s = (wt32_read(w1, bits, r) * g1 + wt32_read(w2, bits, r) * g2) >> 8;
        ph += inc;
        if (dt) {                                         /* DTUN: a second oscillator, the two at half */
            uint32_t r2 = warp ? ph2 + (uint32_t)(sine_i(ph2) * wk) : ph2;
            int32_t s2 = (wt32_read(w1, bits, r2) * g1 + wt32_read(w2, bits, r2) * g2) >> 8;
            s = (s + s2) >> 1;
            ph2 += inc2;
        }
        if (sub) {                                        /* SUB: a sine an octave down */
            s += (int32_t)(((int64_t)(sine_i(sph) - s) * sub) >> 15);
            sph += inc >> 1;
        }
        {   /* ANALOG's filter and soft knee, as WAVES' */
            int32_t y = ft ? analog_tap(&flt, fk, s >> 1, &ic1, &ic2, ft) : tsvf_lp(&flt, s >> 1, &ic1, &ic2);
            int32_t a = y < 0 ? -y : y;
            if (a > 16000) {
                a = 16000 + (softclip((a - 16000) * 2) >> 1);
                y = y < 0 ? -a : a;
            }
            s = y << 1;
        }
        dc += ((s << 8) - dc) >> 11;
        s -= dc >> 8;
        out[i] += voice_amp(s, m, i) << 1;
    }
    v->ph[0] = ph;
    v->ph[1] = ph2;
    v->ph[2] = sph;
    v->s[0] = ic1;
    v->s[1] = ic2;
    v->s[7] = dc;
}

/* {BANK, POS, PENV, DTUN, CUT, RES, SUB, WARP} */
static const preset_t WT32_PRESETS[] = {
    {"HIFI BASS", {0, 50, -10, 0, 70, 30, 90, 0}, {0, 60, 90, 18}, 40, 1, FX(10, 0, 0, 0), PAT(2)},
    {"GROWL BASS", {7, 30, 30, 20, 80, 40, 60, 10}, {0, 60, 90, 16}, 30, 1, FX(30, 0, 0, 0), PAT(8)},
    {"FORMANT BASS", {2, 40, 20, 0, 75, 20, 80, 0}, {0, 58, 80, 16}, 20, 1, FX(10, 0, 0, 0), PAT(2)},
    {"SYNC BASS", {5, 20, 50, 0, 85, 30, 50, 0}, {0, 56, 60, 14}, 30, 1, FX(20, 0, 0, 0), PAT(1)},
    {"WT EPIANO", {0, 8, 30, 10, 95, 10, 0, 20}, {0, 84, 30, 50}, 20, 0, FX(0, 30, 20, 40), PAT(6)},
    {"DRAWBARS", {3, 70, 0, 30, 110, 0, 40, 0}, {0, 0, 127, 16}, 0, 0, FX(0, 60, 0, 40), PAT(6)},
    {"GLASS KEYS", {4, 50, -30, 20, 110, 10, 0, 0}, {0, 86, 20, 60}, 0, 0, FX(0, 30, 30, 60), PAT(6)},
    {"DIGI KEYS", {6, 40, -20, 0, 100, 20, 0, 10}, {0, 80, 30, 50}, 20, 0, FX(0, 20, 20, 40), PAT(6)},
    {"PPG PLUCK", {6, 20, 60, 0, 100, 30, 0, 0}, {0, 60, 0, 40}, 40, 0, FX(0, 10, 40, 40), PAT(3)},
    {"VOWEL PLUCK", {2, 10, 50, 0, 100, 20, 0, 0}, {0, 62, 0, 44}, 20, 0, FX(0, 20, 40, 50), PAT(13)},
    {"GLASS HARP", {4, 70, -40, 10, 115, 0, 0, 0}, {0, 76, 0, 60}, 0, 0, FX(0, 20, 45, 70), PAT(3)},
    {"SYNC PLUCK", {5, 60, -50, 0, 100, 20, 0, 0}, {0, 58, 0, 36}, 30, 0, FX(0, 0, 40, 30), PAT(13)},
    {"HIFI LEAD", {0, 80, 0, 30, 105, 20, 0, 0}, {0, 70, 110, 36}, 10, 1, FX(0, 20, 40, 30), PAT(4)},
    {"SYNC LEAD", {5, 30, 40, 20, 110, 20, 0, 0}, {0, 70, 110, 34}, 10, 1, FX(20, 0, 40, 30), PAT(4)},
    {"PWM LEAD", {1, 40, 30, 30, 105, 15, 0, 0}, {0, 70, 110, 36}, 0, 1, FX(0, 30, 40, 30), PAT(4)},
    {"VOX LEAD", {2, 50, 30, 20, 105, 20, 0, 0}, {10, 70, 110, 40}, 0, 0, FX(0, 20, 40, 40), PAT(4)},
    {"WARP LEAD", {0, 64, 0, 20, 110, 20, 0, 60}, {0, 70, 110, 30}, 10, 1, FX(20, 0, 40, 30), PAT(4)},
    {"DIGI SOLO", {6, 70, 20, 0, 110, 10, 0, 20}, {0, 70, 110, 30}, 0, 1, FX(0, 10, 40, 30), PAT(4)},
    {"ORGAN LEAD", {3, 100, 0, 10, 115, 0, 0, 30}, {0, 0, 127, 20}, 0, 1, FX(10, 40, 30, 30), PAT(4)},
    {"FORMANT PAD", {2, 30, 30, 50, 95, 15, 0, 0}, {80, 95, 110, 95}, 0, 0, FX(0, 60, 25, 90), PAT(5)},
    {"GLASS PAD", {4, 20, 40, 60, 110, 10, 0, 0}, {70, 95, 100, 95}, 0, 0, FX(0, 60, 40, 90), PAT(5)},
    {"PWM STRINGS", {1, 10, 30, 70, 90, 10, 0, 0}, {60, 90, 110, 85}, 10, 0, FX(0, 70, 20, 80), PAT(5)},
    {"DIGI PAD", {6, 50, 30, 50, 90, 15, 0, 10}, {80, 96, 110, 100}, 0, 0, FX(0, 50, 30, 90), PAT(5)},
    {"DIMENSION", {7, 0, 63, 30, 110, 40, 0, 40}, {60, 96, 100, 90}, 0, 0, FX(0, 40, 60, 100), PAT(7)},
};

static const engine_t ENG_WT32 = {
    .name = "WAVE 32",
    .page_title = {"WAVE", "FLTR"},
    .edit = {
        {"BANK", F_ENUM, 0, WT32_NB - 1, 0, N_WT32_BANK, 0},
        {"POS", F_PCT, 0, 127, 32, 0, 0},
        {"PENV", F_BIPCT, -64, 63, 0, 0, 0},
        {"DTUN", F_PCT, 0, 127, 0, 0, 0},
        {"CUT", F_CUTOFF, 0, 127, 100, 0, 0},
        {"RES", F_PCT, 0, 127, 20, 0, 0},
        {"SUB", F_PCT, 0, 127, 0, 0, 0},
        {"WARP", F_PCT, 0, 127, 0, 0, 0},
    },
    .presets = WT32_PRESETS,
    .npresets = NELEM(WT32_PRESETS),
    .note_on = wt32_note_on,
    .render = wt32_render,
    .knob = {P_E1, P_E4, P_E5, P_E2},
    .keep = 0x03,                                         /* the filter */
};
#endif
