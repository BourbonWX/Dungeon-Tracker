/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright (C) 2026 Dungeon Tracker (RONDO) contributors */
/* Dungeon Tracker 1.0: the power-on fanfare under the splash (main.c), about three seconds: a chip pulse lead (G C E G,
 * E G, A B, then C held with vibrato), WAVES' wavetable pad on the chords (C, F G, C), a triangle bass and a noise
 * crash on the last chord. Its own little synth, outside the tracks (they keep their sounds, nothing is saved): fx.c
 * mix_block adds it to the dry mix and a little to the reverb, so the MASTER knob sets its level as it does the music's.
 * jingle_start() starts it; any key or button cuts it short (jingle_stop: a quick fade); jingle_on() while it sounds.
 * The ISR renders it; the main loop only starts and stops it. */

#define JG_STEP (FS * 85u / 1000u)                       /* a sixteenth, 85 ms (176 BPM) */
#define JG_STEPS 32u                                     /* then the tails ring out */
#define JG_NV 6u                                         /* 0 lead, 1..3 pad, 4 bass, 5 noise */
#define JG_WAVE 30u                                      /* the pad's wave among WAVES' 80 (a warm, slightly hollow one) */

typedef struct { uint8_t step, voice, note, len; } jg_ev_t;
static const jg_ev_t JG_SCORE[] = {
    /* the lead */
    {0, 0, 67, 1}, {1, 0, 72, 1}, {2, 0, 76, 1}, {3, 0, 79, 3}, {6, 0, 76, 1}, {7, 0, 79, 3},
    {10, 0, 81, 2}, {12, 0, 83, 2}, {14, 0, 84, 18},
    /* the pad: C, then F and G, then C again, open */
    {0, 1, 60, 10}, {0, 2, 64, 10}, {0, 3, 67, 10}, {10, 1, 65, 2}, {10, 2, 69, 2}, {10, 3, 72, 2},
    {12, 1, 67, 2}, {12, 2, 71, 2}, {12, 3, 74, 2}, {14, 1, 64, 18}, {14, 2, 67, 18}, {14, 3, 72, 18},
    /* the bass */
    {0, 4, 36, 3}, {3, 4, 48, 3}, {6, 4, 36, 1}, {7, 4, 43, 3}, {10, 4, 41, 2}, {12, 4, 43, 2}, {14, 4, 36, 18},
    /* the noise: a tick to start, the crash at the end */
    {0, 5, 1, 1}, {14, 5, 2, 18},
};

static struct {
    volatile uint8_t on, stop;                           /* (stop: the main loop asks, the ISR fades) */
    uint32_t t;                                          /* samples since the start */
    uint16_t ev;                                         /* the next score event */
    uint32_t ph[JG_NV], inc[JG_NV], off[JG_NV];          /* phase, increment, the sample its note ends */
    int32_t env[JG_NV];                                  /* Q15 */
    uint8_t note[JG_NV], gate[JG_NV];
    uint32_t ns, vib;                                    /* the noise's register, the lead's vibrato phase */
    int32_t fade;                                        /* Q15: 32767, or falling once stopped */
} jg;

static void jingle_start(void)
{
    memset(&jg, 0, sizeof jg);
    jg.ns = 0x1234567u;
    jg.fade = 32767;
    jg.on = 1;
}
static void jingle_stop(void) { if (jg.on) jg.stop = 1; }
static int jingle_on(void) { return jg.on; }

/* n samples into the dry mix (l, r) and the reverb send */
static void jingle_render(int32_t *l, int32_t *r, int32_t *rev, uint32_t n)
{
    static const int32_t ATK[JG_NV] = {6000, 900, 900, 900, 8000, 32767};   /* Q15 a sample: how fast each rises */
    static const int32_t LVL[JG_NV] = {13500, 4800, 4800, 4800, 12000, 4500}; /* each voice's peak in the mix */
    uint32_t i, v;
    if (!jg.on)
        return;
    for (i = 0; i < n; i++, jg.t++) {
        int32_t sl = 0, sr = 0;
        while (jg.ev < NELEM(JG_SCORE) && JG_SCORE[jg.ev].step * JG_STEP <= jg.t) {   /* the notes due now */
            const jg_ev_t *e = &JG_SCORE[jg.ev++];
            v = e->voice;
            jg.note[v] = e->note;
            jg.inc[v] = v == 5u ? 0u : pitch_inc((uint32_t)e->note * 16u);
            jg.off[v] = (e->step + e->len) * JG_STEP - JG_STEP / 8u;
            jg.gate[v] = 1;
            if (v == 5u || v == 4u)
                jg.env[v] = 0;
        }
        for (v = 0; v < JG_NV; v++) {
            int32_t s, e;
            uint32_t ph;
            if (jg.gate[v] && jg.t >= jg.off[v])
                jg.gate[v] = 0;
            e = jg.env[v];
            if (jg.gate[v]) {
                int32_t top = v == 5u ? (jg.note[5] == 2u ? 32767 : 20000) : 32767;
                if (v == 5u && e >= top - 1)
                    jg.gate[v] = 0;                      /* (the noise: a hit, then only its decay) */
                e += (top - e) * ATK[v] >> 15 > 0 ? (top - e) * ATK[v] >> 15 : 1;
                if (e > top) e = top;
            } else {
                int32_t rel = v == 5u ? (jg.note[5] == 2u ? 14 : 9) : v == 0u ? 11 : v == 4u ? 12 : 13;   /* (e / 2^rel a
                                                          * sample: the crash ~0.4 s, the tick ~12 ms, the pad ~0.2 s) */
                e -= (e >> rel) + (e > 0);
                if (e < 0) e = 0;
            }
            jg.env[v] = e;
            if (!e) continue;
            ph = jg.ph[v];
            switch (v) {
            case 0: {                                    /* the lead: a pulse, 25 % widening to ~32 %, vibrato on C */
                uint32_t inc = jg.inc[0], pw = 0x40000000u + (jg.t < 120000u ? jg.t : 120000u) * 2700u;
                if (jg.note[0] == 84u && jg.t > 15u * JG_STEP) {
                    jg.vib += 537000u;                   /* (~5.5 Hz at 44.1 kHz, about 12 cents) */
                    inc += (uint32_t)(((int64_t)(int32_t)inc * (sine_i(jg.vib) >> 4)) >> 18);
                }
                ph += inc;
                s = (ph < pw ? 16384 : -16384) - ((int32_t)(pw >> 17) - 16384);   /* (its mean taken out) */
                break;
            }
            case 1: case 2: case 3: {                    /* the pad: WAVES' wave, interpolated, each voice a touch apart */
                const uint8_t *w = MI_WAV_RES_WAVES + 129u * JG_WAVE;
                uint32_t k = (ph += jg.inc[v] + (uint32_t)((int32_t)v - 2) * 6000u) >> 25, f = (ph >> 17) & 255u;
                s = (int32_t)w[k] * (int32_t)(256u - f) + (int32_t)w[k + 1u] * (int32_t)f - 32768;
                break;
            }
            case 4:                                      /* the bass: a triangle */
                ph += jg.inc[4];
                s = (int32_t)(ph >> 16) - 32768;
                s = (s < 0 ? -s : s) * 2 - 32768;
                break;
            default:                                     /* the noise */
                jg.ns ^= jg.ns << 13; jg.ns ^= jg.ns >> 17; jg.ns ^= jg.ns << 5;
                s = (int32_t)(jg.ns >> 16) - 32768;
                break;
            }
            jg.ph[v] = ph;
            s = (int32_t)(((int64_t)s * e >> 15) * LVL[v] >> 15);
            sl += v == 2u || v == 0u ? s * 5 / 4 : s * 3 / 4;   /* (a little width: the pad's and lead's places) */
            sr += v == 2u || v == 0u ? s * 3 / 4 : s * 5 / 4;
        }
        if (jg.stop) {
            jg.fade -= (jg.fade >> 9) + 1;
            if (jg.fade < 64) { jg.on = 0; jg.stop = 0; return; }
        }
        sl = (int32_t)((int64_t)sl * jg.fade >> 15);
        sr = (int32_t)((int64_t)sr * jg.fade >> 15);
        l[i] += sl;
        r[i] += sr;
        rev[i] += (sl + sr) >> 2;
    }
    if (jg.t > (JG_STEPS + 12u) * JG_STEP)                   /* (every tail has rung out by then) */
        jg.on = 0;
}
