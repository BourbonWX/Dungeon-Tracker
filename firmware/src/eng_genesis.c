/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright (C) 2026 Dungeon Tracker contributors */
/* Dungeon Tracker 1.2: GENESIS, the FM family's voice, after the Sega Mega Drive / Genesis's Yamaha YM2612 (OPN2):
 * four operators in the chip's eight algorithms, feedback on operator 1, its frequency multiples (x0.5, x1 .. x15),
 * a detune between the operators, an envelope and a level per operator, and the chip's 9-bit DAC with the step at
 * its zero crossing (GRIT). Its structure and its colour in the FM-1's budget, not the chip's circuit (no cycle
 * accuracy, no SSG-EG, no LFO of its own: the track's LFO and matrix do that).
 *   E1 ALG    1..8: OPN2's (1 a chain of four .. 4 two stacks .. 8 four carriers)
 *   E2 FB     0..7: operator 1's feedback (OPN2's pi/16 .. 4 pi)
 *   E3..E6    MUL1..MUL4: x0.5 (0), x1..x15
 *   E7 DTUNE  the operators drawn apart (OPN2's DT, up to ~6 cents)
 *   E8 GRIT   0 clean (the sine interpolated); else the chip's plain 10-bit sine, its DAC's 9 bits and crossover step,
 *             then fewer bits (from 96)
 *   P_FMk_ATK DEC SUS REL LVL (k = 1..4): operator k's attack, decay to its sustain level, release, and level (a
 *             carrier's loudness, a modulator's brightness)
 * The operators' envelopes are the voice's amplitude (engine_t.ownenv, done: the voice ends when its carriers are
 * silent); a preset loads its operators too (GEN_OPS, gen_ops_load). Velocity: a carrier quieter, a modulator darker.
 * The state of the part's voices is in its engine memory (engines.c eng_mem): four phases and attenuations each */
#ifndef DT_GENESIS_C
#define DT_GENESIS_C 1

#define GEN_SIL (16 << 8)                /* attenuation: 1/256 of 6.02 dB; 16 octaves (96 dB): silent */
typedef struct {
    uint32_t ph[4];
    int32_t a[4];                        /* the envelopes' attenuation */
    uint8_t st[4];                       /* 0 off, 1 attack, 2 decay, 3 sustain, 4 release */
    uint8_t live, down;
    int32_t fb1, fb2;                    /* operator 1's last two outputs (Q15) */
    int32_t dc;                          /* the DC blocker (Q15 << 8) */
} gen_note_t;
ENG_MEM_FITS("GENESIS", sizeof(gen_note_t) * NVOICE);

static const uint16_t GEN_EXP[256] = {    /* 32767 x 2^(-i / 256) */
    32767, 32678, 32590, 32502, 32414, 32326, 32239, 32152, 32065, 31978, 31892, 31805, 31719, 31634, 31548, 31463,
    31378, 31293, 31208, 31124, 31040, 30956, 30872, 30789, 30705, 30622, 30540, 30457, 30375, 30293, 30211, 30129,
    30047, 29966, 29885, 29804, 29724, 29643, 29563, 29483, 29404, 29324, 29245, 29166, 29087, 29008, 28930, 28852,
    28774, 28696, 28618, 28541, 28464, 28387, 28310, 28233, 28157, 28081, 28005, 27929, 27854, 27778, 27703, 27628,
    27554, 27479, 27405, 27331, 27257, 27183, 27110, 27036, 26963, 26890, 26818, 26745, 26673, 26601, 26529, 26457,
    26385, 26314, 26243, 26172, 26101, 26031, 25960, 25890, 25820, 25750, 25681, 25611, 25542, 25473, 25404, 25335,
    25267, 25198, 25130, 25062, 24995, 24927, 24860, 24792, 24725, 24659, 24592, 24525, 24459, 24393, 24327, 24261,
    24196, 24130, 24065, 24000, 23935, 23870, 23806, 23741, 23677, 23613, 23549, 23486, 23422, 23359, 23296, 23233,
    23170, 23107, 23045, 22982, 22920, 22858, 22796, 22735, 22673, 22612, 22551, 22490, 22429, 22368, 22308, 22248,
    22187, 22127, 22068, 22008, 21948, 21889, 21830, 21771, 21712, 21653, 21595, 21536, 21478, 21420, 21362, 21304,
    21247, 21189, 21132, 21075, 21018, 20961, 20904, 20848, 20791, 20735, 20679, 20623, 20568, 20512, 20456, 20401,
    20346, 20291, 20236, 20181, 20127, 20072, 20018, 19964, 19910, 19856, 19802, 19749, 19696, 19642, 19589, 19536,
    19483, 19431, 19378, 19326, 19274, 19221, 19169, 19118, 19066, 19014, 18963, 18912, 18861, 18810, 18759, 18708,
    18657, 18607, 18557, 18506, 18456, 18406, 18357, 18307, 18258, 18208, 18159, 18110, 18061, 18012, 17963, 17915,
    17866, 17818, 17770, 17722, 17674, 17626, 17578, 17531, 17483, 17436, 17389, 17342, 17295, 17248, 17202, 17155,
    17109, 17063, 17016, 16970, 16925, 16879, 16833, 16788, 16742, 16697, 16652, 16607, 16562, 16517, 16472, 16428
};
static inline int32_t gen_gain(int32_t a)   /* attenuation -> Q15 */
{
    return a >= GEN_SIL ? 0 : a <= 0 ? 32767 : (int32_t)(GEN_EXP[a & 255] >> (a >> 8));
}
/* the algorithms: each operator's modulators (bit j: operator j + 1), the carriers */
static const uint8_t GEN_MOD[8][4] = {
    {0, 1, 2, 4},                        /* 1: 1 > 2 > 3 > 4 */
    {0, 0, 3, 4},                        /* 2: (1 + 2) > 3 > 4 */
    {0, 0, 2, 5},                        /* 3: (1 + (2 > 3)) > 4 */
    {0, 1, 0, 6},                        /* 4: ((1 > 2) + 3) > 4 */
    {0, 1, 0, 4},                        /* 5: (1 > 2) + (3 > 4) */
    {0, 1, 1, 1},                        /* 6: 1 > (2, 3, 4) */
    {0, 1, 0, 0},                        /* 7: (1 > 2) + 3 + 4 */
    {0, 0, 0, 0},                        /* 8: 1 + 2 + 3 + 4 */
};
static const uint8_t GEN_CAR[8] = {8, 8, 8, 8, 10, 14, 14, 15};
static const int16_t GEN_NORM[5] = {0, 32767, 23170, 18919, 16384};   /* the carriers' sum: 1 / sqrt(n) */

static gen_note_t *gen_note_of(track_t *t, voice_t *v)
{
    uint32_t i;
    if (t < &trk[0] || t >= &trk[NPART])
        return 0;
    i = (uint32_t)(v - t->v);
    return i < NVOICE ? (gen_note_t *)ENG_MEM(t - trk) + i : 0;
}
static int32_t gen_op(const track_t *t, uint32_t k, uint32_t f) { return clamp(t->p[P_FM1_ATK + 5u * k + f], 0, 127); }

static void gen_note_on(track_t *t, voice_t *v)
{
    gen_note_t *n = gen_note_of(t, v);
    uint32_t k, fresh;
    if (!n)
        return;
    fresh = (!v->env && !v->env_out) || !n->live;
    for (k = 0; k < 4u; k++) {
        if (fresh) {
            n->ph[k] = 0;
            n->a[k] = GEN_SIL;
        }
        n->st[k] = 1;
    }
    if (fresh) {
        n->fb1 = n->fb2 = 0;
        n->dc = 0;
    }
    n->live = 1;
    n->down = 1;
}

/* the envelope of operator k, one block on (attack: an exponential approach, as the chip's; decay and release: dB
 * linear, the time to fall 96 dB) */
static void gen_env(const track_t *t, gen_note_t *n, uint32_t k)
{
    int32_t a = n->a[k], sl = (127 - gen_op(t, k, 2)) * 33;   /* (SUS 0: past silent, the operator ends) */
    switch (n->st[k]) {
    case 1: {
        int32_t x = gen_op(t, k, 0);
        if (!x) { a = 0; } else {
            int32_t kq = 2377600 / (int32_t)TIME_MS_X10[x];
            a -= (int32_t)(((int64_t)a * (kq > 65536 ? 65536 : kq)) >> 16) + 4;
        }
        if (a <= 0) { a = 0; n->st[k] = 2; }
        break;
    }
    case 2:
        a += 29724 / (int32_t)TIME_MS_X10[gen_op(t, k, 1)] + 1;
        if (a >= sl) { a = sl < GEN_SIL ? sl : GEN_SIL; n->st[k] = sl < GEN_SIL ? 3u : 0u; }
        break;
    case 3:
        a = sl;
        break;
    case 4:
        a += 29724 / (int32_t)TIME_MS_X10[gen_op(t, k, 3)] + 1;
        if (a >= GEN_SIL) { a = GEN_SIL; n->st[k] = 0; }
        break;
    default:
        a = GEN_SIL;
        break;
    }
    n->a[k] = a;
}

static int gen_done(track_t *t, voice_t *v)
{
    gen_note_t *n = gen_note_of(t, v);
    uint32_t k, c;
    if (!n || !n->live)
        return 1;
    c = GEN_CAR[(uint32_t)t->p[P_E0] & 7u];
    for (k = 0; k < 4u; k++)
        if (((c >> k) & 1u) && n->st[k] && gen_op(t, k, 4))   /* (a carrier at LVL 0 is silent: it holds nothing) */
            return 0;
    n->live = 0;
    return 1;
}

static void gen_render(track_t *t, voice_t *v, int32_t *out, uint32_t len, const vmod_t *m)
{
    static const int8_t DTK[4] = {0, 1, -1, 2};
    gen_note_t *n = gen_note_of(t, v);
    const int16_t *p = t->p;
    uint32_t alg, car, k, i, inc[4], nc = 0, fbs, grit;
    int32_t g0[4], dg[4], tl[4], vel, lv, q = 0, lad = 0, kamp;
    if (!n || !n->live)
        return;
    if (!v->gate && n->down) {                            /* the key let go: every operator releases */
        n->down = 0;
        for (k = 0; k < 4u; k++)
            if (n->st[k])
                n->st[k] = 4;
    }
    alg = (uint32_t)p[P_E0] & 7u;
    car = GEN_CAR[alg];
    fbs = (uint32_t)clamp(p[P_E1], 0, 7);
    vel = 127 - (int32_t)(v->mvel ? v->mvel : v->vel);
    for (k = 0; k < 4u; k++) {
        uint32_t mul = (uint32_t)clamp(p[P_E2 + k], 0, 15), b = m->inc;
        int32_t d = clamp(p[P_E6], 0, 127) * DTK[k];
        b = mul ? b * mul : b >> 1;
        inc[k] = d ? b + (uint32_t)((int64_t)(b >> 8) * d / 286) : b;
        tl[k] = gen_op(t, k, 4) ? (127 - gen_op(t, k, 4)) * 24 + vel * (((car >> k) & 1u) ? 5 : 2) : GEN_SIL;   /* (LVL 0:
                                                           * off) */
        if ((car >> k) & 1u)
            nc++;
        g0[k] = gen_gain(n->a[k] + tl[k]);
        gen_env(t, n, k);
        dg[k] = (gen_gain(n->a[k] + tl[k]) - g0[k]) / (int32_t)(len ? len : 1u);
    }
    lv = clamp(m->cutoff >> 8, -64, 63) * 8;              /* (the matrix's CUT: the modulators brighter / darker) */
    if (lv)
        for (k = 0; k < 4u; k++)
            if (!((car >> k) & 1u)) {
                g0[k] = clamp(g0[k] + (g0[k] * lv >> 9), 0, 32767);
                dg[k] = dg[k] + (dg[k] * lv >> 9);
            }
    grit = (uint32_t)clamp(p[P_E7], 0, 127);
    if (grit) {                                           /* the DAC: 9 bits (fewer from 96), its step at zero */
        uint32_t bits = grit < 96u ? 9u : 9u - (grit - 96u) / 8u;
        q = 32768 >> (bits - 1u);
        lad = q * (int32_t)(grit < 64u ? grit : 64u) / 32;
    }
    kamp = p[P_VOICE] == V_UNISON ? VOICE_FS * 4 / 7 : VOICE_FS * 7 / 5;   /* (at WAVES' level, one voice) */
    for (i = 0; i < len; i++) {
        int32_t o[4], s = 0, x;
        uint32_t fm = fbs ? (uint32_t)(n->fb1 + n->fb2) << (10u + fbs) : 0u;
        o[0] = ((grit ? SINE[(n->ph[0] + fm) >> 22] : sine_i(n->ph[0] + fm)) * (g0[0] + dg[0] * (int32_t)i)) >> 15;
        n->fb2 = n->fb1;
        n->fb1 = o[0];
        for (k = 1; k < 4u; k++) {
            uint32_t mm = GEN_MOD[alg][k], j;
            int32_t md = 0;
            for (j = 0; j < k; j++)
                if ((mm >> j) & 1u)
                    md += o[j];
            uint32_t pk = n->ph[k] + ((uint32_t)md << 18);
            o[k] = ((grit ? SINE[pk >> 22] : sine_i(pk)) * (g0[k] + dg[k] * (int32_t)i)) >> 15;   /* (GRIT 0 clean: the
                                                           * sine interpolated; else the chip's plain 10-bit table) */
        }
        for (k = 0; k < 4u; k++) {
            if ((car >> k) & 1u)
                s += o[k];
            n->ph[k] += inc[k];
        }
        s = (s * GEN_NORM[nc]) >> 15;
        n->dc += ((s << 8) - n->dc) >> 10;                /* (a ~7 Hz high-pass: feedback leaves a little DC) */
        x = s - (n->dc >> 8);
        if (q) {                                          /* (the DAC last, as the chip's) */
            x = x >= 0 ? (x / q) * q + lad : -((-x / q) * q) - lad;
            x = clamp(x, -32767, 32767);
        }
        out[i] += mulq15(mulq15(x, amp_at(m, i)), kamp);
    }
}

/* the labels of the operators' values on a GENESIS track (params.c track_desc): ATK1 .. LVL4 */
static const param_desc_t GEN_DESC[4][5] = {
#define GEN_D(k) {{"ATK" #k, F_TIME, 0, 127, 0, 0, 0}, {"DEC" #k, F_TIME, 0, 127, 0, 0, 0}, \
                  {"SUS" #k, F_PCT, 0, 127, 127, 0, 0}, {"REL" #k, F_TIME, 0, 127, 0, 0, 0}, {"LVL" #k, F_PCT, 0, 127, 127, 0, 0}}
    GEN_D(1), GEN_D(2), GEN_D(3), GEN_D(4)
#undef GEN_D
};
static const param_desc_t *gen_param_desc(uint32_t id)
{
    return id >= P_FM1_ATK && id <= P_FM4_LEVEL ? &GEN_DESC[(id - P_FM1_ATK) / 5u][(id - P_FM1_ATK) % 5u] : 0;
}

/* {ALG, FB, MUL1, MUL2, MUL3, MUL4, DTUNE, GRIT} (ALG 0..7: shown 1..8); each preset's operators in GEN_OPS */
static const preset_t GEN_PRESETS[] = {
    {"GEN BASS", {0, 4, 1, 1, 1, 1, 0, 12}, {0, 0, 127, 20}, 0, 1, FX(10, 0, 0, 0), PAT(2)},
    {"SLAP BASS", {3, 6, 1, 3, 1, 1, 0, 12}, {0, 0, 127, 20}, 0, 1, FX(0, 0, 0, 4), PAT(1)},
    {"SUB THUMP", {4, 0, 1, 1, 1, 0, 0, 0}, {0, 0, 127, 20}, 0, 1, FX(0, 0, 0, 0), PAT(8)},
    {"ROCK BASS", {0, 6, 1, 1, 2, 1, 0, 32}, {0, 0, 127, 20}, 0, 1, FX(20, 0, 0, 0), PAT(2)},
    {"GEN PIANO", {4, 2, 1, 1, 14, 1, 20, 0}, {0, 0, 127, 20}, 0, 0, FX(0, 30, 20, 40), PAT(6)},
    {"CLAV", {4, 3, 1, 3, 1, 1, 0, 0}, {0, 0, 127, 20}, 0, 0, FX(10, 0, 10, 20), PAT(6)},
    {"ORGAN", {7, 0, 0, 1, 2, 3, 30, 0}, {0, 0, 127, 20}, 0, 0, FX(0, 50, 0, 30), PAT(6)},
    {"MUSIC BOX", {4, 0, 3, 1, 7, 2, 0, 0}, {0, 0, 127, 20}, 0, 0, FX(0, 20, 40, 60), PAT(7)},
    {"HARP", {4, 0, 1, 1, 2, 1, 10, 0}, {0, 0, 127, 20}, 0, 0, FX(0, 20, 30, 60), PAT(3)},
    {"MARIMBA", {4, 0, 4, 1, 10, 1, 0, 0}, {0, 0, 127, 20}, 0, 0, FX(0, 0, 10, 40), PAT(3)},
    {"SITAR", {2, 5, 1, 3, 1, 1, 10, 8}, {0, 0, 127, 20}, 0, 0, FX(0, 20, 30, 40), PAT(3)},
    {"GUITAR", {0, 6, 1, 1, 1, 1, 12, 48}, {0, 0, 127, 20}, 0, 1, FX(40, 0, 20, 20), PAT(4)},
    {"SQUARE LEAD", {4, 0, 2, 1, 2, 1, 60, 8}, {0, 0, 127, 20}, 0, 1, FX(0, 30, 40, 30), PAT(4)},
    {"SAW LEAD", {0, 5, 1, 1, 1, 1, 30, 16}, {0, 0, 127, 20}, 0, 1, FX(10, 20, 40, 30), PAT(4)},
    {"HERO LEAD", {5, 4, 1, 1, 1, 2, 40, 8}, {0, 0, 127, 20}, 0, 0, FX(10, 30, 50, 40), PAT(4)},
    {"SYNC SCREAM", {0, 5, 3, 2, 1, 1, 0, 40}, {0, 0, 127, 20}, 0, 1, FX(30, 0, 40, 20), PAT(4)},
    {"FLUTE", {6, 0, 1, 1, 2, 1, 20, 0}, {0, 0, 127, 20}, 0, 1, FX(0, 20, 40, 50), PAT(4)},
    {"HARMONICA", {0, 3, 1, 1, 1, 1, 10, 8}, {0, 0, 127, 20}, 0, 1, FX(10, 10, 30, 30), PAT(4)},
    {"BRASS", {5, 3, 1, 1, 1, 1, 50, 0}, {0, 0, 127, 20}, 0, 0, FX(0, 20, 20, 40), PAT(4)},
    {"STRINGS", {5, 3, 1, 1, 1, 1, 90, 0}, {0, 0, 127, 20}, 0, 0, FX(0, 60, 20, 60), PAT(5)},
    {"CHOIR", {4, 0, 1, 1, 3, 1, 70, 0}, {0, 0, 127, 20}, 0, 0, FX(0, 60, 20, 80), PAT(5)},
    {"WARM PAD", {4, 2, 1, 1, 1, 2, 80, 0}, {0, 0, 127, 20}, 0, 0, FX(0, 50, 20, 70), PAT(5)},
    {"BELL PAD", {4, 0, 3, 1, 1, 1, 40, 0}, {0, 0, 127, 20}, 0, 0, FX(0, 40, 40, 70), PAT(7)},
    {"LASER", {0, 6, 15, 9, 3, 1, 0, 24}, {0, 0, 127, 20}, 0, 0, FX(0, 0, 40, 30), PAT(7)},
};
/* each preset's operators: {ATK, DEC, SUS, REL, LVL} for operators 1..4 */
static const uint8_t GEN_OPS[][4][5] = {
    {{0, 70, 60, 40, 96}, {0, 80, 40, 50, 100}, {0, 60, 50, 40, 90}, {0, 76, 100, 36, 127}},      /* GEN BASS */
    {{0, 66, 0, 40, 100}, {0, 62, 0, 40, 96}, {0, 74, 40, 40, 92}, {0, 72, 90, 36, 127}},         /* SLAP BASS */
    {{0, 72, 0, 40, 80}, {0, 70, 90, 30, 120}, {0, 56, 0, 40, 70}, {0, 74, 110, 30, 127}},        /* SUB THUMP */
    {{0, 64, 80, 40, 100}, {0, 60, 70, 40, 96}, {0, 58, 60, 40, 94}, {0, 80, 100, 40, 127}},      /* ROCK BASS */
    {{0, 96, 0, 70, 88}, {0, 108, 30, 80, 120}, {0, 78, 0, 60, 84}, {0, 112, 20, 82, 120}},         /* GEN PIANO */
    {{0, 66, 0, 40, 104}, {0, 82, 20, 40, 127}, {0, 66, 0, 40, 96}, {0, 80, 20, 40, 127}},        /* CLAV */
    {{0, 0, 127, 24, 110}, {0, 0, 127, 24, 127}, {0, 0, 127, 24, 100}, {0, 0, 127, 24, 90}},      /* ORGAN */
    {{0, 92, 0, 90, 80}, {0, 110, 0, 94, 120}, {0, 86, 0, 90, 76}, {0, 106, 0, 92, 116}},          /* MUSIC BOX */
    {{0, 90, 0, 80, 92}, {0, 110, 0, 94, 124}, {0, 86, 0, 80, 86}, {0, 108, 0, 94, 120}},          /* HARP */
    {{0, 62, 0, 50, 90}, {0, 90, 0, 70, 127}, {0, 56, 0, 50, 70}, {0, 86, 0, 66, 110}},          /* MARIMBA */
    {{0, 104, 40, 80, 96}, {0, 106, 30, 80, 100}, {0, 84, 50, 70, 92}, {0, 110, 40, 86, 127}},       /* SITAR */
    {{0, 90, 90, 60, 100}, {0, 90, 90, 60, 96}, {0, 90, 100, 60, 96}, {0, 96, 110, 60, 127}},      /* GUITAR */
    {{0, 0, 127, 40, 92}, {0, 0, 127, 40, 120}, {0, 0, 127, 40, 90}, {0, 0, 127, 40, 120}},       /* SQUARE LEAD */
    {{0, 0, 127, 40, 100}, {0, 0, 127, 40, 98}, {0, 0, 127, 40, 96}, {0, 0, 127, 40, 127}},       /* SAW LEAD */
    {{20, 70, 110, 40, 100}, {0, 0, 127, 40, 120}, {0, 0, 127, 40, 112}, {0, 0, 127, 40, 104}},  /* HERO LEAD */
    {{0, 0, 127, 36, 100}, {0, 0, 127, 36, 96}, {0, 0, 127, 36, 96}, {0, 0, 127, 36, 127}},      /* SYNC SCREAM */
    {{60, 70, 100, 50, 70}, {40, 0, 127, 50, 127}, {50, 60, 60, 50, 50}, {40, 0, 127, 50, 64}},  /* FLUTE */
    {{30, 70, 100, 40, 94}, {20, 70, 100, 40, 94}, {20, 70, 100, 40, 92}, {20, 0, 127, 40, 127}}, /* HARMONICA */
    {{56, 84, 90, 50, 102}, {30, 0, 127, 50, 124}, {30, 0, 127, 50, 118}, {30, 0, 127, 50, 112}}, /* BRASS */
    {{76, 90, 100, 84, 92}, {70, 0, 127, 84, 120}, {70, 0, 127, 84, 116}, {70, 0, 127, 84, 112}}, /* STRINGS */
    {{70, 80, 90, 84, 80}, {70, 0, 127, 86, 124}, {70, 80, 90, 84, 72}, {70, 0, 127, 86, 118}},  /* CHOIR */
    {{80, 90, 100, 88, 84}, {74, 0, 127, 88, 124}, {80, 90, 100, 88, 80}, {74, 0, 127, 88, 116}}, /* WARM PAD */
    {{0, 108, 40, 96, 84}, {60, 0, 127, 88, 120}, {0, 112, 30, 96, 80}, {60, 0, 127, 88, 116}},    /* BELL PAD */
    {{0, 82, 0, 50, 110}, {0, 80, 0, 50, 104}, {0, 78, 0, 50, 100}, {0, 86, 0, 54, 127}},        /* LASER */
};
typedef char gen_ops_fit[NELEM(GEN_OPS) == NELEM(GEN_PRESETS) ? 1 : -1];
static void gen_ops_load(track_t *t, uint32_t k)
{
    uint32_t o, f;
    for (o = 0; o < 4u; o++)
        for (f = 0; f < 5u; f++)
            t->p[P_FM1_ATK + 5u * o + f] = GEN_OPS[k % NELEM(GEN_OPS)][o][f];
}

static const char *const N_GEN_ALG[] = {"1", "2", "3", "4", "5", "6", "7", "8"};
static const char *const N_GEN_MUL[] = {"0.5", "1", "2", "3", "4", "5", "6", "7", "8", "9", "10", "11", "12", "13", "14", "15"};
static const engine_t ENG_GENESIS = {
    .name = "GENESIS",
    .page_title = {"FM", "OPS"},
    .edit = {
        {"ALG", F_ENUM, 0, 7, 4, N_GEN_ALG, 0},
        {"FB", F_INT, 0, 7, 0, 0, 0},
        {"MUL1", F_ENUM, 0, 15, 1, N_GEN_MUL, 0},
        {"MUL2", F_ENUM, 0, 15, 1, N_GEN_MUL, 0},
        {"MUL3", F_ENUM, 0, 15, 1, N_GEN_MUL, 0},
        {"MUL4", F_ENUM, 0, 15, 1, N_GEN_MUL, 0},
        {"DTUNE", F_PCT, 0, 127, 0, 0, 0},
        {"GRIT", F_PCT, 0, 127, 0, 0, 0},
    },
    .presets = GEN_PRESETS,
    .npresets = NELEM(GEN_PRESETS),
    .note_on = gen_note_on,
    .render = gen_render,
    .knob = {P_E1, P_FM1_LEVEL, P_E6, P_E7},
    .ownenv = 1,
    .done = gen_done,
    .mem = sizeof(gen_note_t) * NVOICE,
};
#endif
