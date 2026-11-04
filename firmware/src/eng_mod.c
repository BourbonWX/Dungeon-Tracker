/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright (C) 2026 Dungeon Tracker (RONDO) contributors */
/* The AMIGA'D family (Dungeon Tracker 1.5): MOD (engine 21) and MOD KIT (19), the sound of the trackers it is named
 * after. 8-bit samples (tools/gen_mod.py, mod_tables.h: all synthesized, an attack and a loop) played as the Amiga's
 * Paula played them: at a whole-number period of its clock (so high notes land a little off, as they did), the
 * nearest sample and no interpolation (the images ring above the note: the crunch), 8 bits.
 *
 * MOD: SMPL the instrument (SQUARE .. NOISE), OFS where it starts (the trackers' 9xx), ARP a chord arpeggiated at the
 * trackers' 50 Hz tick (0xy), VIB a vibrato (4xy), LED the low-pass the Amiga's power LED switched (127 open; the
 * filter envelope and the matrix's CUT move it), CRSH fewer bits, RATE a lower output rate, DTUN a second channel a
 * little off (two channels on one note, the trackers' chorus). The host's ADSR is the amplitude.
 *
 * MOD KIT: GEN KIT's four voices and interface (eng_cycle.c: the GM map, the lanes, PITCH DECAY COLOR SWEEP SHAPE
 * LEVEL, TUNE DECAY COLOR OPEN CLICK VELO DRV VARI), each voice a hit of eight (SHAPE >> 4): PITCH its period, DECAY
 * how fast its loop (the tail) fades, COLOR the LED filter, SWEEP a slide down (2xx). The DUNGEON KIT and the LOFI KIT
 * (SAMPLE's until 1.5) are two of its kits. Included by engines.c after eng_cycle.c and WAVES (the filter) */
#ifndef DT_MOD_C
#define DT_MOD_C 1
#include "mod_tables.h"

#define MOD_FAMILY "AMIGA'D"                              /* the family's name (engines.c eng_family): one place */
#define MOD_PAULA 3546895u                                /* the PAL clock */
#define MOD_TICK 480u                                     /* the trackers' tick: 50 Hz at 24 kHz */
static const char *const N_MOD_SMPL[] = {"SQUARE", "PULSE", "SAW", "TRI", "SINE", "BASS", "SYNC", "ORGAN", "EPIANO",
                                         "PLUCK", "STRINGS", "CHOIR", "HOOVER", "STAB", "BELL", "NOISE", 0};
static const char *const N_MOD_ARP[] = {"OFF", "MAJ", "MIN", "SUS", "5TH", "OCT", "7TH", "MAJ7", "DIM", 0};
static const uint8_t MOD_ARP[9][2] = {{0, 0}, {4, 7}, {3, 7}, {5, 7}, {7, 12}, {12, 0}, {4, 10}, {4, 11}, {3, 6}};
static const uint16_t MOD_SEMI[25] = {   /* 2^(k/12), Q12 */
    4096, 4340, 4598, 4871, 5161, 5468, 5793, 6137, 6502, 6889, 7298, 7732, 8192,
    8679, 9195, 9742, 10321, 10935, 11585, 12274, 13004, 13777, 14596, 15464, 16384};

/* the sample increment (Q16 of a sample a 24 kHz sample) of sample s at the note of increment inc (C4 at its root),
 * times ratio (Q12): through a whole-number period, as Paula's */
static uint32_t mod_inc(const mod_smp_t *s, uint32_t inc, uint32_t ratio)
{
    uint32_t q = inc / (pitch_inc(960u) >> 12), per, rq;   /* the note over C4, Q12 (no 64-bit division: the */
    uint32_t rate = (uint32_t)(((uint64_t)(MOD_PAULA / s->period) * ((uint64_t)q * ratio >> 12)) >> 12);   /* chip has none) */
    per = rate ? MOD_PAULA / rate : 65535u;
    per = per < 28u ? 28u : per > 65535u ? 65535u : per;
    rq = MOD_PAULA / per;                                 /* (the rate the whole period gives) */
    return (rq * 2796u) >> 10;                            /* Hz -> Q16 of a sample at 24 kHz (65536 / 24000 = 2796 / 1024) */
}
/* the sample at position pos (Q16), with its loop: -> the 8-bit value << 8; *pos wrapped into the loop, *end at the
 * end of a one-shot */
static inline int32_t mod_read(const mod_smp_t *s, uint32_t *pos, int *end)
{
    uint32_t i = *pos >> 16;
    if (i >= s->len) {
        if (s->loop >= s->len) {
            *end = 1;
            return 0;
        }
        i = s->loop + (i - s->loop) % (uint32_t)(s->len - s->loop);
        *pos = (i << 16) | (*pos & 0xffffu);
    }
    return (int32_t)MOD_DATA[s->off + i] << 8;
}

/* ================================================================================================== MOD === */
/* state: ph[0] the position (Q16), ph[1] the second channel's, ph[2] the samples since the note (the tick);
 * s[0] s[1] the LED filter, s[2] the held sample, s[3] its count (RATE), s[4] the vibrato's phase */
static void mod_note_on(track_t *t, voice_t *v)
{
    const mod_smp_t *s = &MOD_SMP[(uint32_t)clamp(t->p[P_E0], 0, MOD_NINST - 1)];
    uint32_t st = s->loop < s->len && s->loop ? s->loop : s->len;   /* (OFS: within the attack, or the one-shot) */
    v->ph[0] = (uint32_t)(clamp(t->p[P_E1], 0, 127) * (int32_t)st / 128) << 16;
    v->ph[1] = v->ph[0];
    v->ph[2] = 0;
    v->s[0] = v->s[1] = v->s[2] = v->s[3] = v->s[4] = 0;
}

static void mod_render(track_t *t, voice_t *v, int32_t *out, uint32_t n, const vmod_t *m)
{
    const int16_t *p = t->p;
    const mod_smp_t *s = &MOD_SMP[(uint32_t)clamp(p[P_E0], 0, MOD_NINST - 1)];
    uint32_t arp = (uint32_t)clamp(p[P_E2], 0, 8), tick = v->ph[2], step, ratio, inc, inc2, i, ft = (uint32_t)p[P_FTYPE];
    uint32_t pos = v->ph[0], pos2 = v->ph[1], crush = (uint32_t)clamp(p[P_E5], 0, 127) * 5u / 127u;
    int32_t vib = clamp(p[P_E3], 0, 127), dt = clamp(p[P_E7], 0, 127), rate = clamp(p[P_E6], 0, 127);
    int32_t ic1 = v->s[0], ic2 = v->s[1], held = v->s[2], hn = v->s[3], vph = v->s[4], hold = 1 + rate / 10;
    int32_t cut = (clamp(p[P_E4], 0, 127) << 8) + m->cutoff, fk = ANALOG_K(0);
    int end = 0, end2 = 0;
    tsvf_t flt;
    if (n > CTL)
        n = CTL;
    step = (tick / MOD_TICK) % 3u;                        /* ARP: the note, then x, then y, a tick each */
    ratio = arp && step ? MOD_SEMI[MOD_ARP[arp][step - 1u]] : 4096u;
    if (vib) {                                            /* VIB: up to ~a semitone either way, 6 Hz */
        int32_t sv = sine_i((uint32_t)vph), d = (sv * vib) >> 15;   /* (d: -vib .. vib) */
        ratio = (uint32_t)((int64_t)ratio * (4096 + d * 2) >> 12);
        vph = (int32_t)((uint32_t)vph + n * 1073741u);    /* (6 Hz of 24 kHz, x 2^32) */
    }
    inc = mod_inc(s, m->inc, ratio);
    inc2 = dt ? mod_inc(s, m->inc, ratio + (ratio >> 11) * (uint32_t)dt / 8u) : 0u;   /* (DTUN: up to ~40 cents) */
    tsvf_coef(&flt, cut, 0);
    for (i = 0; i < n; i++) {
        int32_t x = mod_read(s, &pos, &end);
        pos += inc;
        if (dt) {
            int32_t x2 = mod_read(s, &pos2, &end2);
            pos2 += inc2;
            x = (x + x2) >> 1;
        }
        if (crush)                                        /* CRSH: 8 bits down to 3 */
            x &= ~((1 << (8 + crush)) - 1);
        if (rate) {                                       /* RATE: the output held, a lower rate */
            if (--hn <= 0) {
                held = x;
                hn = hold;
            }
            x = held;
        }
        if (p[P_E4] < 127 || m->cutoff < 0)              /* LED (open at 127 unless an envelope closes it) */
            x = (ft ? analog_tap(&flt, fk, x >> 1, &ic1, &ic2, ft) : tsvf_lp(&flt, x >> 1, &ic1, &ic2)) << 1;
        out[i] += voice_amp(clamp(x, -32767, 32767), m, i) << 1;
    }
    v->ph[0] = end ? (uint32_t)s->len << 16 : pos;
    v->ph[1] = end2 ? (uint32_t)s->len << 16 : pos2;
    v->ph[2] = tick + n < 0x40000000u ? tick + n : tick;
    v->s[0] = ic1;
    v->s[1] = ic2;
    v->s[2] = held;
    v->s[3] = hn;
    v->s[4] = vph;
}

/* {SMPL, OFS, ARP, VIB, LED, CRSH, RATE, DTUN} */
static const preset_t MOD_PRESETS[] = {
    {"TRACKER BASS", {5, 0, 0, 0, 90, 0, 0, 0}, {0, 60, 100, 16}, 20, 1, FX(10, 0, 0, 0), PAT(2)},
    {"ACID MOD", {2, 0, 0, 0, 40, 0, 0, 0}, {0, 54, 40, 14}, 60, 1, FX(30, 0, 20, 0), PAT(1)},
    {"SUB SINE", {4, 0, 0, 0, 127, 0, 0, 0}, {0, 50, 110, 14}, 0, 1, FX(0, 0, 0, 0), PAT(8)},
    {"SYNC BASS", {6, 0, 0, 0, 80, 1, 0, 20}, {0, 56, 80, 14}, 30, 1, FX(20, 0, 0, 0), PAT(2)},
    {"MOD PIANO", {8, 0, 0, 0, 110, 0, 0, 10}, {0, 86, 30, 50}, 0, 0, FX(0, 20, 20, 40), PAT(6)},
    {"MOD ORGAN", {7, 0, 0, 20, 115, 0, 0, 30}, {0, 0, 127, 16}, 0, 0, FX(0, 50, 0, 30), PAT(6)},
    {"RAVE STAB", {13, 0, 0, 0, 120, 0, 0, 0}, {0, 70, 0, 40}, 0, 0, FX(10, 0, 30, 40), PAT(6)},
    {"CHORD KEYS", {0, 0, 1, 0, 100, 0, 0, 0}, {0, 70, 60, 30}, 0, 0, FX(0, 20, 30, 30), PAT(6)},
    {"MOD PLUCK", {9, 0, 0, 0, 110, 0, 0, 0}, {0, 70, 0, 40}, 0, 0, FX(0, 10, 40, 40), PAT(3)},
    {"MOD BELL", {14, 0, 0, 0, 127, 0, 0, 0}, {0, 88, 0, 60}, 0, 0, FX(0, 20, 40, 60), PAT(7)},
    {"CHIP ARP", {1, 0, 2, 0, 110, 0, 0, 0}, {0, 60, 80, 20}, 0, 0, FX(0, 0, 40, 20), PAT(13)},
    {"PLUCK ARP", {9, 0, 4, 0, 100, 0, 0, 0}, {0, 70, 0, 40}, 0, 0, FX(0, 10, 45, 40), PAT(13)},
    {"SQUARE LEAD", {0, 0, 0, 30, 110, 0, 0, 0}, {0, 70, 110, 30}, 0, 1, FX(0, 10, 40, 30), PAT(4)},
    {"PULSE LEAD", {1, 0, 0, 40, 105, 0, 0, 20}, {0, 70, 110, 30}, 0, 1, FX(0, 20, 40, 30), PAT(4)},
    {"SAW LEAD", {2, 0, 0, 30, 100, 0, 0, 40}, {0, 70, 110, 34}, 10, 1, FX(10, 20, 40, 30), PAT(4)},
    {"HOOVER", {12, 0, 0, 0, 110, 0, 0, 0}, {10, 70, 110, 40}, 0, 1, FX(30, 30, 40, 40), PAT(4)},
    {"SYNC LEAD", {6, 0, 0, 30, 110, 0, 0, 10}, {0, 70, 110, 30}, 0, 1, FX(20, 0, 40, 30), PAT(4)},
    {"TRI LEAD", {3, 0, 0, 40, 127, 0, 0, 0}, {0, 70, 110, 30}, 0, 1, FX(0, 20, 40, 40), PAT(4)},
    {"CHIP SOLO", {0, 0, 5, 20, 115, 1, 0, 0}, {0, 70, 110, 30}, 0, 1, FX(0, 0, 40, 20), PAT(4)},
    {"MOD STRINGS", {10, 0, 0, 0, 100, 0, 0, 20}, {60, 90, 110, 85}, 0, 0, FX(0, 60, 20, 80), PAT(5)},
    {"MOD CHOIR", {11, 0, 0, 10, 110, 0, 0, 30}, {70, 90, 110, 90}, 0, 0, FX(0, 60, 20, 90), PAT(5)},
    {"WARM PAD", {10, 0, 0, 0, 70, 0, 0, 40}, {80, 95, 110, 95}, 10, 0, FX(0, 50, 30, 90), PAT(5)},
    {"HOOVER PAD", {12, 0, 0, 10, 80, 0, 0, 30}, {80, 96, 110, 95}, 10, 0, FX(0, 50, 30, 90), PAT(5)},
    {"NOISE SWEEP", {15, 0, 0, 0, 40, 0, 0, 0}, {40, 90, 80, 80}, 63, 0, FX(0, 0, 40, 60), PAT(7)},
};

static const engine_t ENG_MOD = {
    .name = "MOD",
    .page_title = {"SMPL", "FX"},
    .edit = {
        {"SMPL", F_ENUM, 0, MOD_NINST - 1, 0, N_MOD_SMPL, 0},
        {"OFS", F_PCT, 0, 127, 0, 0, 0},
        {"ARP", F_ENUM, 0, 8, 0, N_MOD_ARP, 0},
        {"VIB", F_PCT, 0, 127, 0, 0, 0},
        {"LED", F_PCT, 0, 127, 127, 0, 0},
        {"CRSH", F_PCT, 0, 127, 0, 0, 0},
        {"RATE", F_PCT, 0, 127, 0, 0, 0},
        {"DTUN", F_PCT, 0, 127, 0, 0, 0},
    },
    .presets = MOD_PRESETS,
    .npresets = NELEM(MOD_PRESETS),
    .note_on = mod_note_on,
    .render = mod_render,
    .knob = {P_E4, P_E2, P_E0, P_E3},
    .keep = 0x03,                                         /* the filter */
};

/* ============================================================================================== MOD KIT === */
static const char *const N_MK_SHP[CK_N][9] = {
    {"909", "808", "TIGHT", "GABBER", "DUSTY", "DOOM", "ELECTRO", "SOFT", 0},
    {"909", "808", "LOFI", "CRACK", "CLAP", "CLANG", "BREAK", "SNAP", 0},
    {"CLOSED", "DUSTY", "OPEN", "RIDE", "CRASH", "CHAIN", "SHAKER", "TAMB", 0},
    {"TOM", "CONGA", "COWBELL", "BONE", "RIM", "TOLL", "ZAP", "BLOCK", 0},
};
/* a voice: GEN KIT's (ck_note_on: the lane, the envelopes, the hit's offsets); ph[0] the position (Q16), s[3] s[5]
 * the LED filter */
static void mk_note_on(track_t *t, voice_t *v)
{
    ck_note_on(t, v);
    v->ph[0] = 0;
    v->s[3] = v->s[5] = 0;
}

static void mk_render(track_t *t, voice_t *v, int32_t *out, uint32_t n, const vmod_t *m)
{
    const int16_t *p = t->p;
    uint32_t lane = (uint32_t)v->s[0] & 3u, i, pi = (uint32_t)(t - trk) % NPART, ph2 = v->ph[2], pos = v->ph[0];
    int32_t shp = clamp(p[CK_SHAPE(lane)], 0, 127), col = clamp(p[CK_COLOR(lane)] + p[P_E2] + mk_x[3] * 2 / 5, 0, 127);
    const mod_smp_t *s = &MOD_SMP[MOD_NINST + lane * 8u + (uint32_t)(shp >> 4)];
    int32_t a0 = v->s[1], pe0 = v->s[2], ns = v->s[4], ic1 = v->s[3], ic2 = v->s[5];
    int32_t didx = p[CK_DECAY(lane)] * 120 / 127 + 12 + p[P_E1] * 24 / 64 + v->s[7];
    int32_t a1 = cyc_mul30(a0, cyc_kb(didx)), pe1, s16, sw16, lv = clamp(p[P_LN0 + lane], 0, 127), da;
    int32_t click = clamp(p[P_E4], 0, 127), drv = clamp(p[P_E6], 0, 127), g = 0, mk = 0, y[CTL];
    int32_t mute = (drum_mute[pi] >> lane) & 1u, end = 0;
    uint32_t inc;
    tsvf_t flt;
    vmod_t ml;
    if (n > CTL)
        n = CTL;
    /* PITCH: the hit's period moved (1/16 semitone: PITCH, TUNE, the GM note's, VARI's), SWEEP a slide down */
    sw16 = p[CK_SWEEP(lane)] * (lane == CK_KICK ? 768 : 384) / 127;
    pe1 = cyc_mul30(pe0, cyc_kb(48 + (p[CK_DECAY(lane)] - 64) / 6));
    s16 = (p[CK_PITCH(lane)] - 64) * 6 + p[P_E0] * 3 + v->s[6] + (int32_t)(((int64_t)sw16 * pe0) >> 30);
    inc = mod_inc(s, pitch_inc((uint32_t)clamp(960 + s16, 0, 2047)), 4096u);
    if (lv < 127) {
        int32_t gl = lv * lv * 2 + (lv * lv >> 6);
        ml = *m;
        ml.amp0 = (m->amp0 >> 4) * gl >> 11;
        ml.amp1 = (m->amp1 >> 4) * gl >> 11;
        m = &ml;
    }
    da = (a1 - a0) / (int32_t)n;
    tsvf_coef(&flt, ((lane == CK_HAT ? 92 : 72) + col * (lane == CK_HAT ? 35 : 55) / 127) << 8, 0);   /* COLOR: the LED
                                                           * filter, ~1 kHz (a hat's ~3 kHz) .. open at the top */
    for (i = 0; i < n; i++) {
        int32_t x = mod_read(s, &pos, &end), a = (a0 + da * (int32_t)i) >> 15;
        pos += inc;
        if (col < 120)
            x = lane == CK_HAT ? tsvf_lp(&flt, x >> 1, &ic1, &ic2) * 3        /* (a filtered hat's lost top made up) */
                               : tsvf_lp(&flt, x >> 1, &ic1, &ic2) << 1;
        y[i] = (clamp(x, -32767, 32767) * a) >> 15;
    }
    if (click && ph2 < CK_CLICK_LEN) {                    /* CLICK: a falling burst of noise at the hit */
        for (i = 0; i < n && ph2 + i < CK_CLICK_LEN; i++) {
            int32_t r = (int32_t)(CK_CLICK_LEN - ph2 - i), w = (int32_t)noise32(&ns) >> 16;
            y[i] += ((w * click) >> 8) * r / (int32_t)CK_CLICK_LEN * r / (int32_t)CK_CLICK_LEN;
        }
    }
    v->ph[2] = ph2 + n < 65536u ? ph2 + n : 65536u;
    v->ph[0] = pos;
    v->s[1] = end ? 0 : a1;                               /* (a one-shot's end: the voice is done, ck_amp) */
    v->s[2] = pe1;
    v->s[3] = ic1;
    v->s[4] = ns;
    v->s[5] = ic2;
    if (mute)
        return;
    if (drv) {
        g = 4096 + drv * 3 * 4096 / 127;
        mk = (int32_t)((19661u << 15) / (uint32_t)softclip((19661 * g) >> 12));
    }
    for (i = 0; i < n; i++) {
        int32_t x = y[i];
        if (g)
            x = (softclip((x * g) >> 12) * mk) >> 15;
        out[i] += voice_amp(soft_knee(x, 30000), m, i);
    }
}

/* the kits: PITCH DECAY COLOR SWEEP SHAPE LEVEL a voice (SHAPE: the hit, x 16) */
#define MK_S(k) ((k) * 16 + 8)
static const ck_kit_t MK_KITS[] = {
    {{{64, 84, 127, 0, MK_S(0), 127}, {64, 66, 127, 0, MK_S(0), 112}, {64, 40, 127, 0, MK_S(0), 92}, {64, 70, 127, 10, MK_S(0), 100}}},   /* TRACKER */
    {{{60, 96, 90, 10, MK_S(5), 127}, {60, 80, 100, 0, MK_S(5), 108}, {58, 60, 100, 0, MK_S(5), 90}, {64, 110, 127, 0, MK_S(5), 104}}},  /* DUNGEON KIT */
    {{{62, 70, 70, 0, MK_S(4), 124}, {62, 60, 70, 0, MK_S(2), 112}, {62, 36, 80, 0, MK_S(1), 84}, {60, 60, 90, 0, MK_S(0), 96}}},       /* LOFI KIT (its tom pitched, as the old one) */
    {{{64, 90, 127, 0, MK_S(1), 127}, {64, 64, 127, 0, MK_S(4), 110}, {66, 40, 127, 0, MK_S(0), 92}, {64, 70, 127, 0, MK_S(2), 98}}},   /* ACID HOUSE */
    {{{66, 70, 127, 0, MK_S(2), 124}, {68, 62, 127, 0, MK_S(6), 116}, {66, 36, 127, 0, MK_S(6), 90}, {70, 50, 127, 0, MK_S(4), 96}}},   /* JUNGLE */
    {{{64, 80, 127, 0, MK_S(3), 124}, {64, 60, 127, 0, MK_S(7), 112}, {64, 40, 127, 0, MK_S(0), 94}, {64, 50, 127, 30, MK_S(6), 100}}}, /* HARDCORE */
    {{{60, 90, 127, 20, MK_S(6), 127}, {66, 56, 127, 0, MK_S(7), 110}, {66, 36, 127, 0, MK_S(0), 92}, {64, 60, 127, 0, MK_S(3), 100}}}, /* ELECTRO */
    {{{64, 60, 110, 0, MK_S(2), 124}, {66, 50, 110, 0, MK_S(3), 108}, {68, 30, 120, 0, MK_S(7), 88}, {64, 50, 120, 0, MK_S(6), 100}}},  /* CHIPTUNE */
};
static const preset_t MK_PRESETS[] = {
    {"TRACKER", {0, 0, 0, 64, 10, 80, 0, 8}, {0, 100, 127, 100}, 0, 0, FX(0, 0, 0, 16), PAT(12)},
    {"DUNGEON KIT", {-2, 4, -4, 80, 0, 85, 10, 16}, {0, 100, 127, 100}, 0, 0, FX(0, 0, 20, 50), PAT(12)},
    {"LOFI KIT", {0, 0, -10, 50, 20, 90, 30, 18}, {0, 100, 127, 100}, 0, 0, FX(30, 0, 0, 20), PAT(12)},
    {"ACID HOUSE", {0, 0, 0, 70, 20, 80, 10, 6}, {0, 100, 127, 100}, 0, 0, FX(10, 0, 20, 20), PAT(12)},
    {"JUNGLE", {4, -4, 0, 50, 10, 85, 20, 20}, {0, 100, 127, 100}, 0, 0, FX(20, 0, 10, 30), PAT(12)},
    {"HARDCORE", {2, 0, 0, 60, 30, 70, 60, 10}, {0, 100, 127, 100}, 0, 0, FX(40, 0, 10, 20), PAT(12)},
    {"ELECTRO", {0, 0, 0, 60, 10, 85, 0, 6}, {0, 100, 127, 100}, 0, 0, FX(0, 10, 20, 16), PAT(12)},
    {"CHIPTUNE", {4, -6, 0, 50, 0, 60, 0, 4}, {0, 100, 127, 100}, 0, 0, FX(0, 0, 20, 10), PAT(12)},
};
typedef char mk_kits_fit[NELEM(MK_KITS) == NELEM(MK_PRESETS) ? 1 : -1];
static const ck_kit_t *mk_kit(uint32_t k) { return &MK_KITS[k % NELEM(MK_KITS)]; }

/* the values engine e's factory preset k holds outside E1..E8: a kit's voices (GEN KIT's, MOD KIT's: PITCH .. SHAPE,
 * the lanes' levels), GENESIS' operators; into p (P_COUNT values) */
static void eng_own_values(uint32_t e, uint32_t k, int16_t *p)
{
    uint32_t l, f;
    if (ENG_KIT(e)) {
        const ck_kit_t *K = e == ENGI_MKIT ? mk_kit(k) : &CK_KITS[k % NELEM(CK_KITS)];
        for (l = 0; l < CK_N; l++) {
            for (f = 0; f < 5u; f++)
                p[P_FM1_ATK + 5u * l + f] = K->v[l][f];
            p[P_LN0 + l] = K->v[l][5];
        }
        for (l = CK_N; l < NLANE; l++)
            p[P_LN0 + l] = 127;
    } else if (e == ENGI_GENESIS) {
        for (l = 0; l < 4u; l++)
            for (f = 0; f < 5u; f++)
                p[P_FM1_ATK + 5u * l + f] = GEN_OPS[k % NELEM(GEN_OPS)][l][f];
    }
}

static const param_desc_t MK_SHAPE_DESC[CK_N] = {
    {"BD HIT", F_INT, 0, 127, 8, N_MK_SHP[0], 0}, {"SD HIT", F_INT, 0, 127, 8, N_MK_SHP[1], 0},
    {"HH HIT", F_INT, 0, 127, 8, N_MK_SHP[2], 0}, {"PC HIT", F_INT, 0, 127, 8, N_MK_SHP[3], 0},
};
/* the descriptor of id on a kit track of engine e (GEN KIT's or MOD KIT's), 0 = not one of its own */
static const param_desc_t *kit_param_desc(uint32_t e, uint32_t id)
{
    if (e == ENGI_MKIT && id >= P_FM1_ATK && id <= P_FM4_LEVEL && (id - P_FM1_ATK) % 5u == 4u)
        return &MK_SHAPE_DESC[(id - P_FM1_ATK) / 5u];
    return ck_param_desc(id);
}

static const engine_t ENG_MKIT = {
    .name = "MOD KIT",
    .page_title = {"KIT", "FEEL"},
    .edit = {
        {"TUNE", F_BIPCT, -64, 63, 0, 0, 0},
        {"DECAY", F_BIPCT, -64, 63, 0, 0, 0},
        {"COLOR", F_BIPCT, -64, 63, 0, 0, 0},
        {"OPEN", F_PCT, 0, 127, 64, 0, 0},
        {"CLICK", F_PCT, 0, 127, 10, 0, 0},
        {"VELO", F_PCT, 0, 127, 80, 0, 0},
        {"DRV", F_PCT, 0, 127, 0, 0, 0},
        {"VARI", F_PCT, 0, 127, 8, 0, 0},
    },
    .presets = MK_PRESETS,
    .npresets = NELEM(MK_PRESETS),
    .note_on = mk_note_on,
    .render = mk_render,
    .amp = ck_amp,
    .knob = {P_E1, P_E2, P_E0, P_E4},
    .poly = CK_N,
    .sampled = 1,
    .oneshot = 1,
    .keys = ck_keys,
};
#endif
