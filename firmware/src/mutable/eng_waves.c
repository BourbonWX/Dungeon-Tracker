/* SPDX-License-Identifier: GPL-3.0-or-later
 * WAVES (QYFM 0.4): oscillator models of Ambika / Shruthi-1 (Emilie Gillet, Mutable Instruments,
 * github.com/pichenettes/ambika, GPL-3.0: voicecard/oscillator.cc and sub_oscillator.h, the waves of resources.cc in
 * waves_tables.h), written again for a Felucca engine (felucca_engine.h) at 44.1 kHz with 32-bit phases.
 *
 * Taken: the 16 wavetables (RenderInterpolatedWavetable: PRM scans the table's steps, two waves cross-faded),
 * WAVEQUENCE (RenderWavequence: PRM picks one of the 80 waves), 8-BIT LAND (Render8BitLand: the phase's top byte
 * XOR / AND the parameter), DIRTY PWM (RenderDirtyPwm: a pulse with no band-limiting), QUAD SAW PAD
 * (RenderQuadSawPad: four saws spread by PRM) and the sub-oscillator (square, triangle, pulse an octave or two
 * below). Ambika's other models are QYFM's already (CZ: PHASE, FM: FMTONE / FM6, VOWEL: VOICE, saw / square:
 * ANALOG). Its filter was analog: here ANALOG's (dsp.c tsvf, the part's FILTER TYPE). The waves are 8-bit and not
 * band-limited, as on the Shruthi; GRIT 0 interpolates them smoothly, GRIT up holds and quantizes the output to
 * 8 bits (a lower sample rate). */
#include "waves_tables.h"

static const char *const N_WV_SHAPE[] = {"WT 1", "WT 2", "WT 3", "WT 4", "WT 5", "WT 6", "WT 7", "WT 8", "WT 9",
                                         "WT 10", "WT 11", "WT 12", "WT 13", "WT 14", "WT 15", "WT 16", "WAVEQ",
                                         "8BIT", "DPWM", "QSAW"};
enum { WV_WT16 = 15, WV_WAVEQ, WV_8BIT, WV_DPWM, WV_QSAW, WV_N };
static const char *const N_WV_SUB[] = {"SQ", "TRI", "PLS", "SQ 2", "TRI 2", "PLS 2"};
#define WV_NWAVES (sizeof MI_WAV_RES_WAVES / 129u)

static void wv_note_on(track_t *t, voice_t *v)
{
    (void)t;
    v->s[0] = v->s[1] = 0;                              /* the filter */
    v->s[2] = v->s[3] = 0;                              /* GRIT: the held sample, its count */
    v->ph[1] = 0;                                       /* the sub */
    v->s[4] = (int32_t)0x40000000u;                     /* QUAD SAW's three more saws, spread out */
    v->s[5] = (int32_t)0x80000000u;
    v->s[6] = (int32_t)0xC0000000u;
    v->s[7] = 0;                                        /* (DT 1.1: the DC blocker) */
}

/* a wave's sample at phase ph (Q16 of 128 samples: the top 7 bits the index), 8-bit unsigned << 8 (Q8 between) */
static inline int32_t wv_wave(const uint8_t *w, uint32_t ph)
{
    uint32_t i = ph >> 25, f = (ph >> 17) & 255u;
    return (int32_t)(w[i] << 8) + (int32_t)(((int32_t)w[i + 1u] - (int32_t)w[i]) * (int32_t)f);
}

static void wv_render(track_t *t, voice_t *v, int32_t *out, uint32_t n, const vmod_t *m)
{
    const int16_t *p = t->p;
    uint32_t shape = (uint32_t)clamp(p[P_E0], 0, WV_N - 1), subt = (uint32_t)clamp(p[P_E6], 0, 5), i;
    int32_t prm = clamp(p[P_E1] + ((p[P_E2] * m->envq15) >> 15) + ((m->shape - (64 << 8)) >> 8), 0, 127);
    int32_t sub = p[P_E3] * 258, grit = p[P_E7], cut = (p[P_E4] << 8) + m->cutoff;
    uint32_t ph = v->ph[0], sph = v->ph[1], inc = m->inc, sinc = inc >> (subt >= 3u ? 2 : 1);
    uint32_t q1 = (uint32_t)v->s[4], q2 = (uint32_t)v->s[5], q3 = (uint32_t)v->s[6], qi1 = 0, qi2 = 0, qi3 = 0;
    uint32_t hold = 1u + (uint32_t)grit / 12u, pw = subt % 3u == 0u ? 0x80000000u : 0x40000000u;
    int32_t ic1 = v->s[0], ic2 = v->s[1], held = v->s[2], hn = v->s[3], g1 = 256, g2 = 0, dc = v->s[7];
    const uint8_t *w1 = MI_WAV_RES_WAVES, *w2 = MI_WAV_RES_WAVES;
    uint32_t ft = (uint32_t)p[P_FTYPE];
    int32_t fk = ANALOG_K(p[P_E5]);
    tsvf_t flt;
    tsvf_coef(&flt, cut, p[P_E5]);
    if (shape <= WV_WT16) {                             /* the wavetable: its steps, PRM between two of them */
        const uint8_t *def = MI_WAV_RES_WAVETABLES + 18u * shape;
        uint32_t ptr = (uint32_t)(prm << 1) * def[0], a = def[1u + (ptr >> 8)], b = def[2u + (ptr >> 8)];
        w1 = MI_WAV_RES_WAVES + 129u * (a % WV_NWAVES);
        w2 = MI_WAV_RES_WAVES + 129u * (b % WV_NWAVES);
        g2 = (int32_t)(ptr & 255u);
        g1 = 256 - g2;
    } else if (shape == WV_WAVEQ) {                     /* one of the 80 waves */
        w1 = MI_WAV_RES_WAVES + 129u * ((uint32_t)prm * WV_NWAVES / 128u);
    } else if (shape == WV_QSAW) {                      /* the three more saws: PRM spreads them (1/8192 of the note) */
        uint32_t spread = (uint32_t)(((uint64_t)inc * (uint32_t)prm) >> 13) + 65536u;
        qi1 = inc + spread;
        qi2 = qi1 + spread;
        qi3 = qi2 + spread;
    }
    for (i = 0; i < n; i++) {
        int32_t s;
        switch (shape) {
        case WV_WAVEQ:
            s = wv_wave(w1, ph) - 32768;
            break;
        case WV_8BIT: {
            uint32_t x = (uint32_t)prm;
            s = (int32_t)(((((ph >> 24) ^ (x << 1)) & ~x) + (x >> 1)) & 255u) * 256 - 32768;
            break;
        }
        case WV_DPWM:                                   /* (centred: a pulse of width w has a mean of 1 - 2w; DT 1.1) */
            s = ((ph >> 24) < 127u + (uint32_t)prm ? -32767 : 32767) + (prm * 2 - 2) * 32767 / 256;
            break;
        case WV_QSAW:
            s = (int32_t)((ph >> 18) + (q1 >> 18) + (q2 >> 18) + (q3 >> 18)) - 32768;   /* (four 14-bit saws) */
            q1 += qi1;
            q2 += qi2;
            q3 += qi3;
            break;
        default:
            s = ((wv_wave(w1, ph) * g1 + wv_wave(w2, ph) * g2) >> 8) - 32768;
            break;
        }
        ph += inc;
        if (sub) {                                      /* the sub: an octave (or two) below */
            int32_t b = subt % 3u == 1u ? (int32_t)((sph & 0x80000000u ? ~sph : sph) >> 15) - 32768
                                        : sph < pw ? -32767 : 32767;
            s += (int32_t)(((int64_t)(b - s) * sub) >> 15);   /* (b - s spans 2 x Q15: mulq15 clips it; DT 1.1) */
            sph += sinc;
        }
        if (grit) {                                     /* GRIT: held and 8-bit, as at a lower rate */
            if (--hn <= 0) {
                held = s & ~255;
                hn = (int32_t)hold;
            }
            s = held;
        }
        {   /* ANALOG's filter and soft knee */
            int32_t y = ft ? analog_tap(&flt, fk, s >> 1, &ic1, &ic2, ft) : tsvf_lp(&flt, s >> 1, &ic1, &ic2);
            int32_t a = y < 0 ? -y : y;
            if (a > 16000) {
                a = 16000 + (softclip((a - 16000) * 2) >> 1);
                y = y < 0 ? -a : a;
            }
            s = y << 1;
        }
        dc += ((s << 8) - dc) >> 11;                    /* (DT 1.1: the waves' offset out, and the resonant filter's */
        s -= dc >> 8;                                   /* knee's, ~3 Hz, as FMTONE) */
        out[i] += voice_amp(s, m, i) << 1;
    }
    v->ph[0] = ph;
    v->ph[1] = sph;
    v->s[0] = ic1;
    v->s[1] = ic2;
    v->s[2] = held;
    v->s[3] = hn;
    v->s[4] = (int32_t)q1;
    v->s[5] = (int32_t)q2;
    v->s[6] = (int32_t)q3;
    v->s[7] = dc;
}

static const preset_t WV_PRESETS[] = {
    /* name, {SHAPE, PRM, PENV, SUB, CUT, RES, SUBT, GRIT}, {A D S R}, fenv, mono, FX(dist, chorus, delay, reverb), the
     * PHRASES pattern it suggests. The first eight as QYFM 0.4's; Dungeon Tracker adds ten (TORCHLIGHT ..), voiced
     * from tests/waves_test.c --scan: WT 1 12 15 open with PRM, WT 13 peaks in the middle (a vowel), WT 7 closes to
     * nearly a sine, WT 5 9 14 stay soft */
    {"WT PAD", {2, 40, 20, 0, 85, 20, 0, 0}, {70, 90, 110, 90}, 10, 0, FX(0, 50, 25, 70), PAT(5)},
    {"WT SWEEP", {5, 10, 50, 0, 100, 30, 0, 0}, {20, 95, 80, 60}, 20, 0, FX(0, 30, 30, 50), PAT(4)},
    {"WT KEYS", {9, 64, -30, 0, 95, 10, 0, 0}, {0, 80, 40, 50}, 30, 0, FX(0, 20, 20, 40), PAT(6)},
    {"WAVEQ BASS", {WV_WAVEQ, 30, 0, 64, 60, 50, 0, 0}, {0, 60, 70, 30}, 40, 1, FX(10, 0, 0, 10), PAT(2)},
    {"8BIT LEAD", {WV_8BIT, 50, 0, 0, 110, 10, 0, 30}, {0, 70, 100, 40}, 0, 1, FX(0, 0, 40, 30), PAT(4)},
    {"DIRTY BASS", {WV_DPWM, 40, 0, 80, 55, 60, 3, 0}, {0, 55, 60, 25}, 50, 1, FX(20, 0, 0, 10), PAT(8)},
    {"QUAD PAD", {WV_QSAW, 50, 0, 0, 75, 15, 0, 0}, {60, 90, 110, 85}, 10, 0, FX(0, 60, 20, 70), PAT(5)},
    {"GRIT KEYS", {12, 80, 0, 40, 90, 20, 1, 60}, {0, 75, 50, 45}, 20, 0, FX(0, 0, 30, 30), PAT(6)},
    {"TORCHLIGHT", {4, 50, 10, 0, 80, 15, 0, 6}, {60, 90, 110, 95}, 8, 0, FX(0, 60, 25, 80), PAT(5)},      /* warm, a flicker of grit */
    {"CRYSTAL CAVE", {11, 40, 40, 0, 110, 10, 0, 0}, {0, 88, 0, 80}, 20, 0, FX(0, 20, 45, 75), PAT(7)},     /* a glass bell */
    {"COIN GET", {WV_8BIT, 90, 30, 0, 120, 0, 0, 20}, {0, 56, 0, 38}, 0, 0, FX(0, 0, 25, 20), PAT(7)},      /* a blip */
    {"ACID WAVE", {WV_DPWM, 30, 20, 0, 45, 105, 0, 0}, {0, 52, 40, 20}, 55, 1, FX(20, 0, 15, 10), PAT(1)},  /* squelch */
    {"BOSS FIGHT", {WV_QSAW, 60, 0, 40, 95, 25, 0, 0}, {0, 70, 100, 30}, 15, 1, FX(30, 0, 40, 30), PAT(4)}, /* the big lead */
    {"GHOST CHOIR", {12, 64, 0, 0, 90, 20, 0, 0}, {75, 90, 110, 100}, 0, 0, FX(0, 70, 30, 90), PAT(5)},     /* a vowel */
    {"DEEP DELVE", {6, 127, 0, 90, 60, 0, 1, 0}, {0, 60, 110, 20}, 0, 1, FX(0, 0, 0, 10), PAT(8)},          /* sub bass */
    {"LASER ZAP", {11, 0, 63, 0, 127, 30, 0, 40}, {0, 62, 0, 40}, 0, 0, FX(0, 0, 35, 20), PAT(12)},         /* a sweep up */
    {"ARP RUNES", {14, 40, 45, 0, 100, 20, 0, 0}, {0, 56, 30, 30}, 25, 0, FX(0, 10, 45, 25), PAT(13)},      /* plucked */
    {"MANA POOL", {0, 30, 40, 0, 85, 15, 0, 0}, {90, 100, 100, 100}, 10, 0, FX(0, 50, 30, 90), PAT(5)},     /* it opens slowly */
    /* Dungeon Tracker 1.0: more sounds (appended: a song's preset numbers stay) */
    {"WT BASS", {1, 30, 0, 80, 60, 30, 0, 0}, {0, 60, 80, 20}, 40, 1, FX(10, 0, 0, 0), PAT(2)},
    {"8BIT BASS", {WV_8BIT, 30, 0, 60, 90, 10, 0, 40}, {0, 60, 90, 16}, 0, 1, FX(0, 0, 0, 0), PAT(2)},
    {"QSAW BASS", {WV_QSAW, 40, 0, 60, 50, 40, 0, 0}, {0, 58, 70, 18}, 50, 1, FX(20, 0, 0, 0), PAT(8)},
    {"RUBBER", {WV_WAVEQ, 50, -20, 40, 50, 70, 0, 0}, {0, 54, 50, 16}, 60, 1, FX(0, 0, 0, 0), PAT(1)},
    {"SUB WAVE", {6, 64, 0, 127, 50, 0, 1, 0}, {0, 50, 110, 14}, 0, 1, FX(0, 0, 0, 0), PAT(8)},
    {"GRIT BASS", {WV_DPWM, 60, 0, 70, 70, 40, 3, 70}, {0, 58, 80, 16}, 30, 1, FX(20, 0, 0, 0), PAT(2)},
    {"ACID PULSE", {WV_DPWM, 20, 30, 0, 40, 115, 0, 0}, {0, 48, 30, 16}, 63, 1, FX(30, 0, 20, 10), PAT(1)},
    {"DUSK PAD", {3, 50, 10, 0, 70, 15, 0, 0}, {80, 95, 110, 95}, 10, 0, FX(0, 60, 25, 80), PAT(5)},
    {"VOWEL PAD", {12, 50, 20, 0, 95, 10, 0, 0}, {70, 90, 110, 90}, 0, 0, FX(0, 60, 20, 80), PAT(5)},
    {"GLASS WT", {11, 70, -20, 0, 110, 10, 0, 0}, {50, 95, 100, 90}, 0, 0, FX(0, 50, 30, 80), PAT(5)},
    {"DRONE", {WV_QSAW, 64, 0, 40, 60, 30, 1, 0}, {90, 100, 120, 100}, 0, 0, FX(0, 40, 30, 90), PAT(5)},
    {"CRYPT PAD", {14, 30, 30, 20, 65, 25, 1, 0}, {85, 100, 110, 100}, 10, 0, FX(0, 50, 40, 100), PAT(5)},
    {"SHIMMER WT", {1, 90, 40, 0, 110, 10, 0, 0}, {60, 95, 100, 100}, 0, 0, FX(0, 70, 50, 100), PAT(5)},
    {"WT PIANO", {9, 50, -20, 0, 100, 10, 0, 0}, {0, 86, 30, 50}, 30, 0, FX(0, 20, 20, 40), PAT(6)},
    {"LOFI KEYS", {4, 40, -10, 0, 80, 10, 0, 30}, {0, 84, 40, 50}, 20, 0, FX(0, 30, 20, 40), PAT(6)},
    {"TOY PIANO", {10, 80, 0, 0, 115, 0, 0, 20}, {0, 74, 0, 50}, 0, 0, FX(0, 10, 30, 40), PAT(7)},
    {"WT PLUCK", {1, 40, 30, 0, 90, 30, 0, 0}, {0, 60, 0, 40}, 50, 0, FX(0, 10, 40, 30), PAT(3)},
    {"8BIT PLUCK", {WV_8BIT, 70, 0, 0, 110, 0, 0, 40}, {0, 56, 0, 36}, 0, 0, FX(0, 0, 40, 20), PAT(13)},
    {"HARP RUNE", {13, 60, 20, 0, 100, 20, 0, 0}, {0, 76, 0, 60}, 30, 0, FX(0, 20, 45, 60), PAT(3)},
    {"WT LEAD", {15, 70, 20, 0, 105, 20, 0, 0}, {0, 70, 110, 40}, 10, 1, FX(0, 10, 40, 30), PAT(4)},
    {"QSAW LEAD", {WV_QSAW, 70, 0, 30, 100, 25, 0, 0}, {0, 70, 110, 36}, 10, 1, FX(20, 0, 40, 30), PAT(4)},
    {"PULSE HERO", {WV_DPWM, 24, 0, 0, 96, 20, 0, 0}, {0, 70, 110, 36}, 0, 1, FX(0, 10, 40, 30), PAT(4)},
    {"CHIP SOLO", {WV_8BIT, 40, 0, 0, 120, 0, 0, 20}, {0, 70, 110, 30}, 0, 1, FX(0, 0, 40, 20), PAT(4)},
    {"VOWEL LEAD", {12, 70, 30, 0, 100, 20, 0, 0}, {10, 70, 110, 40}, 0, 0, FX(0, 20, 40, 40), PAT(4)},
    {"DRAGON", {WV_QSAW, 90, 0, 60, 110, 30, 3, 60}, {0, 70, 110, 30}, 20, 1, FX(40, 0, 30, 30), PAT(4)},
    {"PORTAL", {11, 0, 63, 0, 110, 40, 0, 0}, {60, 96, 100, 90}, 0, 0, FX(0, 40, 60, 100), PAT(7)},
    {"TRAPDOOR", {WV_WAVEQ, 0, -63, 80, 70, 60, 4, 80}, {0, 80, 0, 60}, 0, 0, FX(20, 0, 30, 50), PAT(7)},
    {"ROBOT TALK", {12, 64, -40, 0, 90, 40, 0, 100}, {0, 60, 60, 30}, 0, 0, FX(0, 0, 30, 20), PAT(13)},
};

static const engine_t ENG_WAVES = {
    .name = "WAVES",
    .page_title = {"WAVE", "FLTR"},
    .edit = {
        {"SHAPE", F_ENUM, 0, WV_N - 1, 0, N_WV_SHAPE, 0},
        {"PRM", F_PCT, 0, 127, 32, 0, 0},
        {"PENV", F_BIPCT, -64, 63, 0, 0, 0},
        {"SUB", F_PCT, 0, 127, 0, 0, 0},
        {"CUT", F_CUTOFF, 0, 127, 100, 0, 0},
        {"RES", F_PCT, 0, 127, 20, 0, 0},
        {"SUBT", F_ENUM, 0, 5, 0, N_WV_SUB, 0},
        {"GRIT", F_PCT, 0, 127, 0, 0, 0},
    },
    .presets = WV_PRESETS,
    .npresets = NELEM(WV_PRESETS),
    .note_on = wv_note_on,
    .render = wv_render,
    .knob = {P_E1, P_E4, P_E5, P_E2},
    .keep = 0x03,                                       /* the filter */
};
