/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright (C) 2026 RONDO contributors. Built on Felucca (C) 2026 Leo Kuroshita (@kurogedelic), Hügelton Instruments */
/* RONDO's two engines: CYCLE KIT (engine 14) and CYCLE TONE (engine 15). Both are small operator-FM voices with
 * macro controls in the spirit of a "machine" groovebox: a few knobs per sound (PITCH, DECAY, COLOR, SWEEP, SHAPE)
 * instead of a full operator editor. All fixed point, as Felucca's other engines (engines/src/dsp.c).
 *
 * ------------------------------------------------------------------------------------------------ CYCLE KIT ---
 * Four drum voices, one voice each, playing at once: KICK, SNARE, HAT, PERC. Every voice has its own six values,
 * stored in the track's parameters that only Felucca's retired DIGITAL engine used (P_FMk_*, k = the voice), and
 * shown on EDIT's pages KICK, SNARE, HAT, PERC (PITCH DECAY COLOR SWEEP), SHAPE (one per voice) and LANES (the
 * voice levels, P_LN0..P_LN3):
 *   PITCH  P_FMk_ATK    +-24 semitones around the voice's designed pitch (64)
 *   DECAY  P_FMk_DEC    4 ms .. 4 s (the time it takes to fall 60 dB)
 *   COLOR  P_FMk_SUS    the FM index (KICK, PERC: with its own faster envelope), the snare's body and noise
 *                       brightness, the hat's metal
 *   SWEEP  P_FMk_REL    the pitch drop after the hit (KICK up to 4 octaves, SNARE and PERC up to 2, HAT none)
 *   SHAPE  P_FMk_LEVEL  KICK: drive into a soft clip (sine -> square-ish), SNARE: body <-> noise, HAT: the metal's
 *                       inharmonic ratio (8 steps), PERC: the operators' ratio (8 steps)
 *   LEVEL  P_LNk        square law, 100 % = full
 * EDIT 1 / EDIT 2 (P_E0..P_E7) act on the whole kit: TUNE (+-12 semitones), DECAY (x 1/4 .. x 4), COLOR (+-64),
 * OPEN (the open hat's length over the closed one), CLICK (a short transient on KICK SNARE PERC), VELO (velocity
 * sensitivity, 0 = every hit full), DRV (a soft clip on every hit) and VARI (a little random pitch and decay per hit).
 * All of them, the lanes' values included, can be parameter-locked per step (SEQ: hold a step, turn a knob).
 *
 * Notes: the General MIDI drum map, as Felucca's DRUM: 35 36 KICK; 38 40 SNARE; 42 44 closed HAT, 46 open HAT,
 * cymbals 49 51 52 55 57 59 a long open HAT; toms, rim, clap, cowbell, congas, claves, .. PERC at their own pitch.
 * So the step grid's eight rows (BD SD CP CH OH TM RS CB) all play, on the four voices, and a QY70 or any GM drum
 * part plays it as it is. A voice plays one hit at a time: a new hit on it starts over (the closed hat chokes the
 * open one by itself).
 *
 * ----------------------------------------------------------------------------------------------- CYCLE TONE ---
 * Two operators and a sub: a modulator (RATIO of the note, with FEEDBACK) into a carrier, the carrier optionally
 * doubled a few cents up (STACK) and a sine an octave down (SUB). COLOR is the FM index, CNTR how fast the index
 * falls after the note starts and MSUS where it rests (the timbre's own envelope, Model-machine style), SWEEP a
 * pitch blip at the start (up or, below 0, from under). The host's ADSR is the amplitude, so pads, chords, basses and
 * leads all come from the one engine; ENV DEST SHP / LFO DEST SHP and the matrix's SHAPE move COLOR. */
#include "cycle_tables.h"


static inline int32_t cyc_kb(int32_t i) { return CYC_KB[clamp(i, 0, CYC_NKB - 1)]; }
static inline int32_t cyc_mul30(int32_t a, int32_t k) { return (int32_t)(((int64_t)a * k) >> 30); }
/* an index amount 0..127 as the phase offset of a Q15 modulator sample (x 32768 = the offset in 2^32 of a turn):
 * square law, up to 2.9 radians */
static inline int32_t cyc_idx(int32_t c)
{
    c = clamp(c, 0, 127);
    return (c * c * 15) >> 2;
}
/* a Q15 sine of phase ph, phase-modulated by a Q15 sample md at index k (cyc_idx) */
static inline int32_t cyc_pm(uint32_t ph, int32_t md, int32_t k) { return sine_i(ph + (uint32_t)(md * k)); }

/* the operators' ratios, Q8 */
static const uint16_t CYC_RATIO[16] = {128, 256, 384, 512, 640, 768, 896, 1024, 1280, 1536, 1792, 2048,
                                       361, 707, 804, 1329};
static const char *const N_CYC_RATIO[] = {"0.5", "1", "1.5", "2", "2.5", "3", "3.5", "4", "5", "6", "7", "8",
                                          "1.41", "2.76", "3.14", "5.19", 0};

/* ===================================================================================================== KIT === */
enum { CK_KICK, CK_SNARE, CK_HAT, CK_PERC, CK_N };
#define CK_SILENT (1 << 16)                       /* the amplitude envelope's end: -84 dB of Q30 */
#define CK_CLICK_LEN 160u                         /* the CLICK transient, samples (3.6 ms) */
static const uint8_t CK_BASE[CK_N] = {31, 52, 79, 62};   /* the designed pitches: G1 49 Hz, E3, G5, D4 */
static const uint16_t CK_HAT_RATIO[8] = {361, 443, 617, 707, 804, 947, 1178, 1329};   /* 1.41 .. 5.19 */
static const uint16_t CK_PERC_RATIO[8] = {256, 512, 768, 384, 707, 896, 128, 1178};   /* 1 2 3 1.5 2.76 3.5 .5 4.6 */
static const char *const N_CK_LANE[CK_N] = {"KICK", "SNARE", "HAT", "PERC"};
static const char *const N_CK_ABBR[CK_N] = {"BD", "SD", "HH", "PC"};

/* the parameters of voice l */
#define CK_PITCH(l) (P_FM1_ATK + 5u * (l))
#define CK_DECAY(l) (P_FM1_DEC + 5u * (l))
#define CK_COLOR(l) (P_FM1_SUS + 5u * (l))
#define CK_SWEEP(l) (P_FM1_REL + 5u * (l))
#define CK_SHAPE(l) (P_FM1_LEVEL + 5u * (l))

/* General MIDI notes 35..81 -> {voice, semitones from its designed pitch, decay index offset (12 = x2; 120 an open
 * hat, 121 a cymbal: OPEN's length)} */
static const int8_t CK_GM[47][3] = {
    {CK_KICK, -2, 0}, {CK_KICK, 0, 0}, {CK_PERC, 10, -24}, {CK_SNARE, 0, 0}, {CK_PERC, 5, -6},        /* 35 36 37 38 39 */
    {CK_SNARE, 2, -6}, {CK_PERC, -14, 6}, {CK_HAT, 0, 0}, {CK_PERC, -11, 6}, {CK_HAT, -2, -6},         /* 40 .. 44 */
    {CK_PERC, -7, 6}, {CK_HAT, 0, 120}, {CK_PERC, -4, 6}, {CK_PERC, -2, 6}, {CK_HAT, -5, 121},         /* 45 .. 49 */
    {CK_PERC, 1, 6}, {CK_HAT, 3, 121}, {CK_HAT, -7, 121}, {CK_PERC, 17, 0}, {CK_HAT, 5, -12},          /* 50 .. 54 */
    {CK_HAT, -3, 121}, {CK_PERC, 7, -6}, {CK_HAT, -6, 121}, {CK_PERC, -9, 18}, {CK_HAT, 2, 121},       /* 55 .. 59 */
    {CK_PERC, 9, -12}, {CK_PERC, 4, -12}, {CK_PERC, 5, -18}, {CK_PERC, 2, -6}, {CK_PERC, -3, -6},      /* 60 .. 64 */
    {CK_PERC, 0, 0}, {CK_PERC, -5, 0}, {CK_PERC, 14, -12}, {CK_PERC, 11, -12}, {CK_HAT, 7, -12},       /* 65 .. 69 */
    {CK_HAT, 9, -18}, {CK_PERC, 19, 6}, {CK_PERC, 17, 12}, {CK_PERC, 12, -24}, {CK_PERC, 8, -24},      /* 70 .. 74 */
    {CK_PERC, 15, -24}, {CK_PERC, 13, -18}, {CK_PERC, 10, -18}, {CK_PERC, 6, -6}, {CK_PERC, 3, -6},    /* 75 .. 79 */
    {CK_PERC, 21, -18}, {CK_PERC, 21, 18},                                                             /* 80 81 */
};
/* the GM row of a note (other notes as their place in the octave from 36) */
static const int8_t *ck_gm(uint32_t note)
{
    uint32_t n = note >= 35u && note <= 81u ? note : 36u + (note + 120u - 36u) % 12u;
    return CK_GM[n - 35u];
}
static uint32_t ck_lane_of(uint32_t note) { return (uint32_t)ck_gm(note)[0]; }

/* a voice's state: s[0] its voice (lane), s[1] the amplitude envelope (Q30), s[2] the pitch envelope (Q30),
 * s[3] the index envelope (Q30; HAT: the feedback's last sample), s[4] the noise, s[5] a filter, s[6] the hit's
 * pitch offset (1/16 semitone), s[7] its decay offset (index steps); ph[2] the samples since the hit (HAT: its
 * second operator) */
static voice_t *ck_reuse(track_t *t, uint32_t note)
{
    uint32_t lane = ck_lane_of(note), i;
    for (i = 0; i < NVOICE; i++)
        if (t->v[i].active && (uint32_t)t->v[i].s[0] == lane)
            return &t->v[i];
    return 0;
}

/* the velocity gain the VELO knob leaves of velocity vel, over the host's own (amp1 = this x vel), Q15 */
static int32_t ck_velo(const int16_t *p, uint32_t vel)
{
    int32_t v = vel ? (int32_t)vel : 1, target = 127 - (clamp(p[P_E5], 0, 127) * (127 - v)) / 127;
    return target * 32767 / v;
}

static void ck_note_on(track_t *t, voice_t *v)
{
    const int8_t *g = ck_gm(v->note);
    const int16_t *p = t->p;
    uint32_t lane = (uint32_t)g[0], r = (v->age + 1u) * 2654435761u ^ (uint32_t)v->note * 40503u;
    int32_t dofs = g[2], vari = clamp(p[P_E7], 0, 127);
    if (dofs >= 120)                                      /* an open hat (120) or a cymbal (121): OPEN's length */
        dofs = 12 + clamp(p[P_E3], 0, 127) * 48 / 127 + (dofs == 121 ? 12 : 0);   /* x2 .. x32 */
    v->s[0] = (int32_t)lane;
    if (lane == CK_KICK && t == &trk[0])
        duck_kick();                                      /* (the INSERT's DUCK on the other tracks: core.h) */
    v->s[1] = 1 << 30;
    v->s[2] = 1 << 30;
    v->s[3] = lane == CK_HAT ? 0 : 1 << 30;
    v->s[4] = (int32_t)(r | 1u);
    v->s[5] = 0;
    v->s[6] = g[1] * 16 + (vari ? (int32_t)((r >> 8) % (uint32_t)(2 * vari + 1)) - vari : 0) / 8;
    v->s[7] = dofs + (vari ? (int32_t)((r >> 20) % (uint32_t)(vari / 8 + 1)) - vari / 16 : 0);
    v->ph[0] = 0;                                         /* every hit from a zero crossing (CLICK is the snap) */
    v->ph[1] = 0;
    v->ph[2] = 0;
    v->env = 1 << 24;                                     /* the ADSR held at full: a hit, not a note */
    v->env_out = mulq15(ck_velo(p, v->vel), v->vel * 258);   /* (no ramp up from 0) */
}

/* the voice's amplitude is the hit (s[1], in the render); the ADSR only keeps it alive. Ends it when it has rung
 * out, or when another voice took its lane */
static int32_t ck_amp(track_t *t, voice_t *v, int32_t adsr)
{
    (void)adsr;
    if (!v->active)
        return 0;
    if (v->s[1] < CK_SILENT) {
        v->active = v->gate = 0;
        v->stage = 0;
        v->env = 0;
        return 0;
    }
    v->env = 1 << 24;
    return ck_velo(t->p, v->vel);
}

static void ck_render(track_t *t, voice_t *v, int32_t *out, uint32_t n, const vmod_t *m)
{
    const int16_t *p = t->p;
    uint32_t lane = (uint32_t)v->s[0] & 3u, i, pi = (uint32_t)(t - trk) % NPART;
    int32_t a0 = v->s[1], pe0 = v->s[2], me0 = v->s[3], ns = v->s[4], flt = v->s[5];
    uint32_t ph0 = v->ph[0], ph1 = v->ph[1], ph2 = v->ph[2];
    int32_t col = clamp(p[CK_COLOR(lane)] + p[P_E2] + mk_x[3] * 2 / 5, 0, 127), shp = clamp(p[CK_SHAPE(lane)], 0, 127);   /* (RIDE) */
    int32_t didx = p[CK_DECAY(lane)] * 120 / 127 + 12 + p[P_E1] * 24 / 64 + v->s[7];
    int32_t a1 = cyc_mul30(a0, cyc_kb(didx)), pe1, me1, p16, sw16, lv = clamp(p[P_LN0 + lane], 0, 127);
    int32_t click = lane == CK_HAT ? 0 : clamp(p[P_E4], 0, 127), drv = clamp(p[P_E6], 0, 127), g = 0, mk = 0;
    int32_t y[CTL], da, k0, k1, mute = (drum_mute[pi] >> lane) & 1u;
    uint32_t inc;
    vmod_t ml;
    if (n > CTL)
        n = CTL;
    /* pitch: the voice's, the kit's TUNE, the hit's, then SWEEP's drop on top */
    sw16 = lane == CK_KICK ? p[CK_SWEEP(lane)] * 768 / 127 : lane == CK_HAT ? 0 : p[CK_SWEEP(lane)] * 384 / 127;
    pe1 = cyc_mul30(pe0, cyc_kb((lane == CK_KICK ? 48 : lane == CK_SNARE ? 48 : 54) + (p[CK_DECAY(lane)] - 64) / 6));
    p16 = (int32_t)CK_BASE[lane] * 16 + (p[CK_PITCH(lane)] - 64) * 6 + p[P_E0] * 3 + v->s[6];
    inc = pitch_inc((uint32_t)clamp(p16 + (int32_t)(((int64_t)sw16 * pe0) >> 30), 0, 2047));
    /* the index envelope (not HAT's: its s[3] is the feedback) */
    me1 = lane == CK_HAT ? me0 : cyc_mul30(me0, cyc_kb(didx - (lane == CK_PERC ? 12 : 30)));
    if (lv < 127) {                                       /* the voice's LEVEL on the block's amplitude ramp */
        int32_t gl = lv * lv * 2 + (lv * lv >> 6);
        ml = *m;
        ml.amp0 = (m->amp0 >> 4) * gl >> 11;
        ml.amp1 = (m->amp1 >> 4) * gl >> 11;
        m = &ml;
    }
    da = (a1 - a0) / (int32_t)n;
    switch (lane) {
    case CK_KICK: {                                       /* sine, FM at 1:1 with a fast index envelope, SHAPE drive */
        int32_t ki = cyc_idx(col), sg = 4096 + shp * 229, nk = 0;
        k0 = (int32_t)(((int64_t)ki * me0) >> 30);
        k1 = (int32_t)(((int64_t)ki * me1) >> 30);
        if (shp)
            nk = (int32_t)((32767u << 15) / (uint32_t)softclip((32767 * sg) >> 12));
        for (i = 0; i < n; i++) {
            int32_t md = sine_i(ph1), s = cyc_pm(ph0, md, k0 + (k1 - k0) * (int32_t)i / (int32_t)n);
            int32_t a = (a0 + da * (int32_t)i) >> 15;
            ph0 += inc;
            ph1 += inc;
            if (shp)
                s = (softclip((s * sg) >> 12) * nk) >> 15;
            y[i] = (s * a) >> 15;
        }
        break;
    }
    case CK_SNARE: {                                      /* an FM body (1:1.41) and noise, COLOR its low-pass */
        int32_t ki = cyc_idx(col * 3 / 4), nm = shp * 258, kq = 4000 + col * 220;
        uint32_t minc = (inc >> 8) * 361u;
        k0 = (int32_t)(((int64_t)ki * me0) >> 30);
        k1 = (int32_t)(((int64_t)ki * me1) >> 30);
        for (i = 0; i < n; i++) {
            int32_t md = sine_i(ph1), body = cyc_pm(ph0, md, k0 + (k1 - k0) * (int32_t)i / (int32_t)n);
            int32_t a = (a0 + da * (int32_t)i) >> 15, ab = (a * a) >> 15, w = (int32_t)noise32(&ns) >> 16, hp;
            ph0 += inc;
            ph1 += minc;
            flt += ((w - flt) * kq) >> 15;
            hp = flt + (flt >> 1);                        /* (the low-pass' level back up) */
            y[i] = (((body * ab) >> 15) * (32767 - nm) + ((hp * a) >> 15) * nm) >> 15;
        }
        break;
    }
    case CK_HAT: {                                        /* two operators at an inharmonic ratio, feedback, noise, HP */
        int32_t ki = cyc_idx(64 + col / 2), fb = me0, kq = 3000 + col * 150;   /* COLOR: the high-pass up */
        uint32_t minc = (inc >> 8) * CK_HAT_RATIO[shp >> 4], m2inc = (inc >> 8) * 1543u;   /* (x 6.03) */
        for (i = 0; i < n; i++) {
            int32_t md = cyc_pm(ph1, fb, 9000), md2 = sine_i(ph2), w = (int32_t)noise32(&ns) >> 17, s, hp;
            int32_t a = (a0 + da * (int32_t)i) >> 15;
            fb = md;
            s = cyc_pm(ph0, (md + md2) >> 1, ki) + w;
            ph0 += inc;
            ph1 += minc;
            ph2 += m2inc;
            flt += (((s - flt) >> 1) * kq) >> 14;
            hp = s - flt;
            y[i] = (clamp(hp, -32767, 32767) * a) >> 15;
        }
        me1 = fb;
        break;
    }
    default: {                                            /* PERC: two operators at SHAPE's ratio, index envelope */
        int32_t ki = cyc_idx(col);
        uint32_t minc = (inc >> 8) * CK_PERC_RATIO[shp >> 4];
        k0 = (int32_t)(((int64_t)ki * me0) >> 30);
        k1 = (int32_t)(((int64_t)ki * me1) >> 30);
        for (i = 0; i < n; i++) {
            int32_t md = sine_i(ph1), s = cyc_pm(ph0, md, k0 + (k1 - k0) * (int32_t)i / (int32_t)n);
            int32_t a = (a0 + da * (int32_t)i) >> 15;
            ph0 += inc;
            ph1 += minc;
            y[i] = (s * a) >> 15;
        }
        break;
    }
    }
    if (click && ph2 < CK_CLICK_LEN && lane != CK_HAT) {  /* CLICK: a falling burst of noise at the hit */
        for (i = 0; i < n && ph2 + i < CK_CLICK_LEN; i++) {
            int32_t r = (int32_t)(CK_CLICK_LEN - ph2 - i), w = (int32_t)noise32(&ns) >> 16;
            y[i] += ((w * click) >> 7) * r / (int32_t)CK_CLICK_LEN * r / (int32_t)CK_CLICK_LEN;
        }
    }
    if (lane != CK_HAT)
        ph2 = ph2 + n < 65536u ? ph2 + n : 65536u;   /* (the click counter, held at its end) */
    v->s[1] = a1;
    v->s[2] = pe1;
    v->s[3] = me1;
    v->s[4] = ns;
    v->s[5] = flt;
    v->ph[0] = ph0;
    v->ph[1] = ph1;
    v->ph[2] = ph2;
    if (mute)                                             /* a muted voice rings on unheard (the EDIT layer's keys) */
        return;
    if (drv) {                                            /* DRV: x1..x4 into the soft clip, the level kept */
        g = 4096 + drv * 3 * 4096 / 127;
        mk = (int32_t)((19661u << 15) / (uint32_t)softclip((19661 * g) >> 12));
    }
    for (i = 0; i < n; i++) {
        int32_t s = y[i];
        if (g)
            s = (softclip((s * g) >> 12) * mk) >> 15;
        out[i] += voice_amp(soft_knee(s, 30000), m, i);
    }
}

/* the keys play the GM map, the first C (key 7) the kick, as DRUM's */
static int32_t ck_keys(const track_t *t, uint32_t k)
{
    (void)t;
    return clamp(29 + 12 * song.octave + (int32_t)k, 0, 127);
}

/* the kits: the engine's values in the preset; each voice's six in CK_KITS (ck_kit_load) */
typedef struct { int8_t v[CK_N][6]; } ck_kit_t;          /* PITCH DECAY COLOR SWEEP SHAPE LEVEL */
static const ck_kit_t CK_KITS[] = {
    {{{64, 82, 38, 72, 24, 127}, {64, 58, 52, 30, 78, 112}, {64, 36, 80, 0, 40, 92}, {64, 54, 58, 22, 20, 100}}},  /* CYCLE */
    {{{58, 100, 12, 80, 0, 127}, {66, 56, 30, 34, 70, 108}, {68, 30, 64, 0, 16, 88}, {60, 62, 34, 26, 96, 100}}},  /* 808ISH */
    {{{66, 74, 46, 76, 60, 127}, {62, 60, 60, 36, 90, 115}, {62, 40, 96, 0, 56, 96}, {70, 50, 70, 16, 64, 96}}},   /* 909ISH */
    {{{60, 88, 70, 92, 96, 120}, {56, 64, 80, 52, 50, 112}, {56, 34, 110, 0, 90, 90}, {52, 70, 96, 40, 32, 104}}}, /* GRIT */
    {{{72, 66, 90, 48, 30, 118}, {76, 50, 98, 20, 40, 100}, {80, 44, 127, 0, 127, 88}, {84, 74, 110, 8, 72, 108}}}, /* METAL */
    {{{56, 96, 20, 60, 10, 127}, {60, 70, 24, 24, 100, 96}, {60, 26, 56, 0, 8, 80}, {56, 80, 40, 30, 48, 110}}},    /* DUB */
    {{{60, 90, 30, 70, 30, 127}, {60, 62, 40, 30, 95, 114}, {62, 32, 60, 0, 30, 84}, {72, 40, 50, 10, 20, 96}}},  /* BOOM BAP */
    {{{62, 84, 50, 78, 50, 127}, {66, 52, 76, 20, 110, 104}, {70, 28, 110, 0, 72, 92}, {64, 60, 80, 20, 64, 98}}},  /* TECHNO KIT */
    {{{56, 104, 16, 90, 0, 127}, {70, 48, 46, 40, 60, 110}, {72, 30, 90, 0, 40, 90}, {66, 54, 40, 40, 80, 100}}},  /* ELECTRO KIT */
    {{{58, 80, 24, 60, 40, 124}, {58, 58, 30, 24, 85, 108}, {58, 34, 40, 0, 20, 80}, {60, 52, 30, 20, 40, 96}}},  /* DUSTY KIT */
    {{{54, 96, 24, 56, 10, 127}, {56, 70, 30, 46, 40, 106}, {60, 40, 70, 0, 30, 80}, {50, 80, 40, 50, 48, 118}}},  /* TRIBAL KIT */
    {{{66, 60, 40, 70, 20, 127}, {74, 36, 70, 10, 110, 96}, {76, 20, 100, 0, 100, 88}, {80, 34, 90, 8, 112, 104}}},  /* MINIMAL KIT */
    {{{60, 86, 90, 96, 110, 124}, {54, 66, 100, 60, 70, 116}, {54, 40, 127, 0, 120, 94}, {48, 74, 110, 46, 24, 108}}},  /* FORGE KIT */
    {{{70, 64, 60, 100, 40, 124}, {80, 44, 90, 60, 100, 108}, {84, 24, 120, 0, 64, 86}, {88, 40, 100, 64, 90, 104}}},  /* CHIP KIT 2 */
    {{{54, 100, 20, 64, 20, 127}, {52, 74, 28, 40, 80, 110}, {56, 40, 50, 0, 50, 82}, {46, 90, 30, 60, 56, 112}}},  /* CAVE KIT */
    {{{64, 78, 36, 70, 30, 127}, {64, 56, 60, 34, 96, 110}, {66, 44, 96, 0, 48, 96}, {72, 50, 60, 30, 88, 100}}},  /* DISCO KIT */
};
/* {TUNE, DECAY, COLOR (the three -64..63, 0 = the voices as set), OPEN, CLICK, VELO, DRV, VARI}; every kit suggests
 * the BEAT pattern */
static const preset_t CK_PRESETS[] = {
    {"CYCLE KIT", {0, 0, 0, 64, 40, 80, 0, 12}, {0, 100, 127, 100}, 0, 0, FX(0, 0, 0, 16), PAT(12)},
    {"808ISH", {0, 0, 0, 80, 16, 80, 0, 6}, {0, 100, 127, 100}, 0, 0, FX(0, 0, 0, 20), PAT(12)},
    {"909ISH", {0, 0, 0, 56, 72, 80, 24, 8}, {0, 100, 127, 100}, 0, 0, FX(0, 0, 0, 14), PAT(12)},
    {"GRIT KIT", {-4, -4, 0, 64, 90, 70, 80, 20}, {0, 100, 127, 100}, 0, 0, FX(20, 0, 0, 10), PAT(12)},
    {"METAL KIT", {0, 0, 0, 90, 30, 90, 10, 30}, {0, 100, 127, 100}, 0, 0, FX(0, 20, 10, 30), PAT(12)},
    {"DUB KIT", {-2, 6, -4, 100, 10, 90, 16, 16}, {0, 100, 127, 100}, 0, 0, FX(0, 0, 40, 40), PAT(12)},
    /* Dungeon Tracker 1.0: more sounds (appended: a song's preset numbers stay) */
    {"BOOM BAP", {0, 0, -6, 50, 30, 90, 10, 10}, {0, 100, 127, 100}, 0, 0, FX(10, 0, 0, 22), PAT(12)},
    {"TECHNO KIT", {0, -4, 4, 60, 60, 70, 20, 8}, {0, 100, 127, 100}, 0, 0, FX(10, 0, 10, 18), PAT(12)},
    {"ELECTRO KIT", {0, 0, 0, 70, 20, 85, 0, 6}, {0, 100, 127, 100}, 0, 0, FX(0, 10, 20, 16), PAT(12)},
    {"DUSTY KIT", {-6, -2, -10, 40, 20, 95, 30, 24}, {0, 100, 127, 100}, 0, 0, FX(20, 0, 0, 24), PAT(12)},
    {"TRIBAL KIT", {0, 4, 0, 50, 30, 85, 0, 30}, {0, 100, 127, 100}, 0, 0, FX(0, 0, 20, 40), PAT(12)},
    {"MINIMAL KIT", {0, -10, 0, 40, 80, 60, 0, 4}, {0, 100, 127, 100}, 0, 0, FX(0, 0, 30, 14), PAT(12)},
    {"FORGE KIT", {-6, 0, 8, 70, 100, 70, 70, 18}, {0, 100, 127, 100}, 0, 0, FX(40, 0, 10, 20), PAT(12)},
    {"CHIP KIT 2", {4, -6, 6, 50, 20, 60, 20, 6}, {0, 100, 127, 100}, 0, 0, FX(0, 0, 20, 10), PAT(12)},
    {"CAVE KIT", {-4, 6, -6, 70, 20, 80, 10, 16}, {0, 100, 127, 100}, 0, 0, FX(0, 10, 30, 70), PAT(12)},
    {"DISCO KIT", {0, 0, 0, 90, 40, 80, 0, 10}, {0, 100, 127, 100}, 0, 0, FX(0, 20, 10, 26), PAT(12)},
};
typedef char ck_kits_fit[NELEM(CK_KITS) == NELEM(CK_PRESETS) ? 1 : -1];

/* kit k's voices into the track (a preset load, a clean sound, power-on) */
static void ck_kit_load(track_t *t, uint32_t k)
{
    const ck_kit_t *K = &CK_KITS[k % NELEM(CK_KITS)];
    uint32_t l;
    for (l = 0; l < CK_N; l++) {
        t->p[CK_PITCH(l)] = K->v[l][0];
        t->p[CK_DECAY(l)] = K->v[l][1];
        t->p[CK_COLOR(l)] = K->v[l][2];
        t->p[CK_SWEEP(l)] = K->v[l][3];
        t->p[CK_SHAPE(l)] = K->v[l][4];
        t->p[P_LN0 + l] = K->v[l][5];
    }
    for (l = CK_N; l < NLANE; l++)
        t->p[P_LN0 + l] = 127;
}

/* the labels of a KIT track's voice parameters (params.c track_desc): the page names the voice */
static const param_desc_t CK_DESC[6] = {
    {"PITCH", F_INT, 0, 127, 64, 0, 0}, {"DECAY", F_PCT, 0, 127, 64, 0, 0}, {"COLOR", F_PCT, 0, 127, 64, 0, 0},
    {"SWEEP", F_PCT, 0, 127, 0, 0, 0}, {"SHAPE", F_PCT, 0, 127, 0, 0, 0}, {"LEVEL", F_PCT, 0, 127, 127, 0, 0},
};
static const char *const N_CK_SHP_HAT[] = {"1.41", "1.73", "2.41", "2.76", "3.14", "3.70", "4.60", "5.19", 0};
static const char *const N_CK_SHP_PERC[] = {"1", "2", "3", "1.5", "2.76", "3.5", "0.5", "4.6", 0};
static const param_desc_t CK_SHAPE_DESC[CK_N] = {
    {"BD DRV", F_PCT, 0, 127, 0, 0, 0}, {"SD NOIS", F_PCT, 0, 127, 0, 0, 0},
    {"HH RAT", F_INT, 0, 127, 0, N_CK_SHP_HAT, 0}, {"PC RAT", F_INT, 0, 127, 0, N_CK_SHP_PERC, 0},
};
static const param_desc_t CK_LEVEL_DESC[CK_N] = {
    {"KICK", F_PCT, 0, 127, 127, 0, 0}, {"SNARE", F_PCT, 0, 127, 127, 0, 0},
    {"HAT", F_PCT, 0, 127, 127, 0, 0}, {"PERC", F_PCT, 0, 127, 127, 0, 0},
};
/* the descriptor of id on a KIT track, 0 = not one of its own */
static const param_desc_t *ck_param_desc(uint32_t id)
{
    if (id >= P_FM1_ATK && id <= P_FM4_LEVEL) {
        uint32_t k = (id - P_FM1_ATK) % 5u;
        return k == 4u ? &CK_SHAPE_DESC[(id - P_FM1_ATK) / 5u] : &CK_DESC[k];
    }
    if (id >= P_LN0 && id < P_LN0 + CK_N)
        return &CK_LEVEL_DESC[id - P_LN0];
    return 0;
}

static const engine_t ENG_CKIT = {
    .name = "FMKIT",
    .page_title = {"KIT", "FEEL"},
    .edit = {
        {"TUNE", F_BIPCT, -64, 63, 0, 0, 0},
        {"DECAY", F_BIPCT, -64, 63, 0, 0, 0},
        {"COLOR", F_BIPCT, -64, 63, 0, 0, 0},
        {"OPEN", F_PCT, 0, 127, 64, 0, 0},
        {"CLICK", F_PCT, 0, 127, 40, 0, 0},
        {"VELO", F_PCT, 0, 127, 80, 0, 0},
        {"DRV", F_PCT, 0, 127, 0, 0, 0},
        {"VARI", F_PCT, 0, 127, 12, 0, 0},
    },
    .presets = CK_PRESETS,
    .npresets = NELEM(CK_PRESETS),
    .note_on = ck_note_on,
    .render = ck_render,
    .amp = ck_amp,
    .knob = {P_E1, P_E2, P_E0, P_E4},
    .poly = CK_N,
    .sampled = 1,                /* (no phases kept over a retrigger: every hit starts the same) */
    .oneshot = 1,
    .keys = ck_keys,
};

/* ==================================================================================================== TONE === */
/* state: s[0] the index envelope (Q30), s[1] the pitch envelope (Q30), s[2] the modulator's last sample (feedback),
 * s[3] the DC blocker's (a 3 Hz one-pole, Q15 << 8: full FEEDBACK leaves a little DC) */
static void ct_note_on(track_t *t, voice_t *v)
{
    (void)t;
    v->s[0] = 1 << 30;
    v->s[1] = 1 << 30;
    v->s[2] = 0;
    v->s[3] = 0;
}

static void ct_render(track_t *t, voice_t *v, int32_t *out, uint32_t n, const vmod_t *m)
{
    const int16_t *p = t->p;
    int32_t me0 = v->s[0], pe0 = v->s[1], fbs = v->s[2], dc = v->s[3], ms = clamp(p[P_E3], 0, 127) << 23;
    int32_t me1 = ms + cyc_mul30(me0 - ms, cyc_kb(p[P_E2] * 143 / 127)), pe1 = cyc_mul30(pe0, cyc_kb(60));
    int32_t col = clamp(p[P_E1] + ((m->shape - (64 << 8)) >> 8) + (m->cutoff >> 8) + (v->vel - 96) / 4 + mk_x[3] * 2 / 5, 0, 127);
    int32_t ki = cyc_idx(col), k0 = (int32_t)(((int64_t)ki * (me0 >> 15)) >> 15), k1 = (int32_t)(((int64_t)ki * (me1 >> 15)) >> 15);
    int32_t e4 = clamp(p[P_E4] + mk_x[3] / 4, 0, 127);    /* (the FX layer's RIDE: a little more FDBK) */
    int32_t fb = e4 * e4 * 5 >> 2, stk = p[P_E6] * 258, sub = p[P_E7] * 258, sw = p[P_E5];
    uint32_t inc = m->inc, minc, inc2, i, ph0 = v->ph[0], ph1 = v->ph[1], ph2 = v->ph[2];
    if (sw && pe0 > (1 << 16)) {                          /* SWEEP: the pitch starts away from the note */
        int32_t d16 = sw * 384 / 64;                      /* +-24 semitones */
        inc = cents_inc(clamp(m->pitch16 + (int32_t)(((int64_t)d16 * pe0) >> 30), 0, 2047), 0, m->fine);
    }
    minc = (inc >> 8) * CYC_RATIO[(uint32_t)p[P_E0] & 15u];
    inc2 = inc + (inc >> 12) * (uint32_t)(p[P_E6] * 72 / 127);   /* STACK: up to ~30 cents up */
    if (n > CTL)
        n = CTL;
    for (i = 0; i < n; i++) {
        int32_t k = k0 + (k1 - k0) * (int32_t)i / (int32_t)n, md = cyc_pm(ph1, fbs, fb), s = cyc_pm(ph0, md, k);
        fbs = md;
        ph0 += inc;
        ph1 += minc;
        if (stk) {                                        /* the second carrier (its phase in ph[2]'s high use: */
            int32_t s2 = cyc_pm(ph2, md, k);              /* SUB is then off) */
            ph2 += inc2;
            s = s - ((((s - s2) >> 1) * stk) >> 15);
        } else if (sub) {
            int32_t so = sine_i(ph2);
            ph2 += inc >> 1;
            s = ((s * (32767 - (sub >> 1))) >> 15) + ((so * sub) >> 16);
        }
        dc += ((s << 8) - dc) >> 11;
        out[i] += voice_amp(s - (dc >> 8), m, i);
    }
    v->s[0] = me1;
    v->s[1] = pe1;
    v->s[2] = fbs;
    v->s[3] = dc;
    v->ph[0] = ph0;
    v->ph[1] = ph1;
    v->ph[2] = ph2;
}

/* {RATIO, COLOR, CNTR, MSUS, FDBK, SWEEP, STACK, SUB} */
static const preset_t CT_PRESETS[] = {
    {"FM PAD", {1, 44, 104, 56, 12, 0, 64, 0}, {52, 90, 112, 78}, 0, 0, FX(0, 40, 30, 60), PAT(5)},
    {"CHORD EP", {1, 74, 66, 12, 0, 0, 24, 0}, {0, 84, 52, 46}, 0, 0, FX(0, 30, 20, 36), PAT(6)},
    {"STAB", {3, 84, 44, 0, 30, 4, 0, 0}, {0, 56, 38, 26}, 0, 0, FX(0, 0, 40, 30), PAT(6)},
    {"GLASS PAD", {6, 62, 92, 24, 0, 0, 48, 0}, {24, 100, 76, 84}, 0, 0, FX(0, 30, 40, 70), PAT(5)},
    {"ORGAN", {3, 36, 127, 127, 0, 0, 0, 56}, {0, 0, 127, 16}, 0, 0, FX(0, 50, 0, 30), PAT(6)},
    {"FM BASS", {1, 76, 46, 26, 40, 0, 0, 64}, {0, 60, 92, 14}, 0, 1, FX(10, 0, 0, 0), PAT(2)},
    {"SUB BASS", {1, 22, 34, 0, 0, 0, 0, 110}, {0, 40, 112, 12}, 0, 1, FX(0, 0, 0, 0), PAT(8)},
    {"PLUCK BASS", {3, 92, 32, 0, 20, 6, 0, 44}, {0, 56, 30, 12}, 0, 1, FX(0, 0, 10, 0), PAT(1)},
    {"FM LEAD", {1, 86, 92, 72, 56, 0, 24, 0}, {0, 70, 100, 32}, 0, 0, FX(0, 20, 50, 30), PAT(4)},
    {"SYNC LEAD", {3, 96, 100, 90, 72, 0, 0, 0}, {0, 60, 110, 30}, 0, 0, FX(16, 0, 46, 24), PAT(3)},
    {"BRASS", {1, 72, 70, 60, 20, -8, 32, 0}, {14, 70, 100, 36}, 0, 0, FX(0, 20, 20, 40), PAT(4)},
    {"BELL", {13, 70, 84, 0, 0, 0, 20, 0}, {0, 96, 0, 80}, 0, 0, FX(0, 20, 40, 50), PAT(7)},
    /* Dungeon Tracker 1.0: more sounds (appended: a song's preset numbers stay) */
    {"REESE", {1, 60, 80, 60, 24, 0, 100, 40}, {0, 60, 110, 20}, 0, 1, FX(20, 10, 0, 0), PAT(8)},
    {"DUB BASS", {1, 30, 40, 10, 0, 0, 0, 90}, {0, 50, 100, 16}, 0, 1, FX(0, 0, 20, 10), PAT(2)},
    {"SLAP BASS", {5, 96, 26, 0, 10, 8, 0, 30}, {0, 52, 40, 12}, 0, 1, FX(0, 0, 0, 4), PAT(1)},
    {"GROWL FM", {0, 90, 70, 70, 80, 0, 0, 40}, {0, 60, 100, 18}, 0, 1, FX(30, 0, 0, 0), PAT(8)},
    {"ROLLING", {3, 70, 30, 0, 30, 4, 0, 60}, {0, 48, 30, 10}, 0, 1, FX(10, 0, 10, 0), PAT(2)},
    {"WOOD BASS", {1, 50, 20, 0, 0, 2, 0, 50}, {0, 64, 20, 24}, 0, 1, FX(0, 0, 0, 10), PAT(2)},
    {"ZAP BASS", {1, 84, 20, 0, 40, 30, 0, 40}, {0, 50, 50, 12}, 0, 1, FX(10, 0, 0, 0), PAT(1)},
    {"OCTAVE BASS", {0, 64, 50, 30, 20, 0, 40, 70}, {0, 60, 90, 16}, 0, 1, FX(10, 0, 0, 0), PAT(2)},
    {"WARM PAD", {1, 36, 110, 40, 8, 0, 90, 20}, {70, 90, 110, 90}, 0, 0, FX(0, 50, 20, 70), PAT(5)},
    {"CHOIR FM", {3, 50, 110, 60, 0, 0, 80, 0}, {60, 90, 110, 90}, 0, 0, FX(0, 60, 20, 80), PAT(5)},
    {"DARK PAD", {0, 40, 100, 30, 20, 0, 70, 30}, {80, 96, 110, 96}, 0, 0, FX(0, 40, 30, 80), PAT(5)},
    {"AIR PAD", {7, 30, 110, 40, 0, 0, 100, 0}, {90, 96, 100, 100}, 0, 0, FX(0, 60, 40, 90), PAT(5)},
    {"FM STRINGS", {1, 60, 90, 50, 30, -2, 110, 0}, {50, 90, 110, 80}, 0, 0, FX(0, 60, 20, 60), PAT(5)},
    {"DX PIANO", {3, 80, 60, 10, 0, 0, 10, 0}, {0, 88, 30, 50}, 0, 0, FX(0, 10, 20, 40), PAT(6)},
    {"VIBES", {7, 56, 80, 0, 0, 0, 30, 0}, {0, 92, 0, 70}, 0, 0, FX(0, 40, 30, 50), PAT(7)},
    {"CLAV", {5, 100, 30, 0, 40, 0, 0, 0}, {0, 60, 30, 20}, 0, 0, FX(10, 0, 10, 20), PAT(6)},
    {"ORGAN 2", {1, 50, 127, 100, 30, 0, 40, 80}, {0, 0, 127, 16}, 0, 0, FX(0, 60, 0, 40), PAT(6)},
    {"MUSIC BOX", {11, 60, 70, 0, 0, 0, 0, 0}, {0, 90, 0, 60}, 0, 0, FX(0, 20, 40, 60), PAT(7)},
    {"FM HARP", {3, 64, 50, 0, 0, 0, 20, 0}, {0, 80, 0, 60}, 0, 0, FX(0, 20, 30, 60), PAT(3)},
    {"KOTO", {5, 86, 40, 0, 20, 4, 0, 0}, {0, 74, 0, 40}, 0, 0, FX(0, 0, 30, 40), PAT(3)},
    {"DROP PLUCK", {13, 70, 36, 0, 0, 20, 0, 0}, {0, 64, 0, 40}, 0, 0, FX(0, 10, 40, 40), PAT(13)},
    {"FM MARIMBA", {7, 60, 30, 0, 0, 0, 0, 0}, {0, 80, 0, 50}, 0, 0, FX(0, 0, 10, 40), PAT(3)},
    {"FM SAW LEAD", {1, 100, 100, 90, 64, 0, 0, 0}, {0, 70, 110, 30}, 0, 1, FX(10, 40, 40, 30), PAT(4)},
    {"HOLLOW FM", {3, 70, 100, 90, 20, 0, 0, 30}, {0, 70, 110, 34}, 0, 0, FX(0, 20, 40, 30), PAT(4)},
    {"SOLO HORN", {1, 64, 70, 70, 10, -6, 20, 0}, {20, 70, 100, 40}, 0, 0, FX(0, 20, 30, 40), PAT(4)},
    {"WHISTLE", {1, 10, 100, 10, 0, 0, 0, 0}, {30, 80, 110, 40}, 0, 1, FX(0, 20, 40, 50), PAT(4)},
    {"SCREAMER", {3, 110, 110, 100, 72, 0, 60, 0}, {0, 70, 110, 30}, 0, 1, FX(40, 0, 40, 20), PAT(4)},
    {"GLIDE LEAD", {3, 90, 100, 80, 60, -10, 30, 0}, {0, 70, 110, 30}, 0, 1, FX(10, 10, 50, 30), PAT(4)},
    {"HERO LEAD", {1, 80, 90, 80, 20, 0, 30, 0}, {10, 70, 110, 40}, 0, 0, FX(10, 30, 50, 40), PAT(4)},
    {"SIREN", {1, 80, 100, 80, 50, 63, 0, 0}, {40, 90, 100, 60}, 0, 0, FX(0, 20, 50, 60), PAT(7)},
};

static const engine_t ENG_CTONE = {
    .name = "FMTONE",
    .page_title = {"TONE", "BODY"},
    .edit = {
        {"RATIO", F_ENUM, 0, 15, 1, N_CYC_RATIO, 0},
        {"COLOR", F_PCT, 0, 127, 64, 0, 0},
        {"CNTR", F_PCT, 0, 127, 64, 0, 0},
        {"MSUS", F_PCT, 0, 127, 32, 0, 0},
        {"FDBK", F_PCT, 0, 127, 0, 0, 0},
        {"SWEEP", F_BIPCT, -64, 63, 0, 0, 0},
        {"STACK", F_PCT, 0, 127, 0, 0, 0},
        {"SUB", F_PCT, 0, 127, 0, 0, 0},
    },
    .presets = CT_PRESETS,
    .npresets = NELEM(CT_PRESETS),
    .note_on = ct_note_on,
    .render = ct_render,
    .knob = {P_E1, P_E2, P_E0, P_REL},
};
