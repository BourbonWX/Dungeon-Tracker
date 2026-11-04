/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright (C) 2026 Dungeon Tracker (RONDO) contributors. Built on Felucca (C) 2026 Leo Kuroshita (@kurogedelic), Hügelton Instruments */
/* Dungeon Tracker: SID (engine 17), a voice after the Commodore 64's sound chip, the MOS 6581 (written here from
 * its documented behaviour; no emulator's code):
 *   WAVE   the 6581's four waveforms -- TRI, SAW, PLS (pulse), NSE (noise: its 23-bit shift register, clocked by bit 19
 *          of the 24-bit accumulator) -- and the combined ones, the waves ANDed as the chip's outputs fight (ST, PT,
 *          PS, PST: thin, buzzy, the "growl" of many C64 basses), and DRM: a C64 drum, one burst of noise then the
 *          pulse falling in pitch (SWEEP: how far it falls)
 *   PW     the pulse width (12-bit on the chip; 50 % at 64), PWM its slow sweep (a triangle LFO, ~2.6 Hz, how wide)
 *   MOD    the second oscillator (voice 3's role on the chip): SYNC resets the voice's phase at its every cycle (the
 *          voice is TUNE semitones above it: the classic sync lead), RING multiplies TRI by it (the bells), S+R both;
 *          the modulator plays the note itself
 *   TUNE   0..48 semitones: with MOD, the voice above the modulator; without, a plain transpose
 *   SWEEP  the envelope moves TUNE (sync sweeps, zaps, drums), -64..63
 *   CUT RES the chip's 12 dB filter (the part's FILTER TYPE: LP / BP / HP / ..), driven a little before it, as the
 *          6581's is
 * The waveforms are computed at twice the rate and averaged (the chip runs at 1 MHz: at 44.1 kHz a naive saw would
 * alias hard), 12-bit as on the chip. */
static const char *const N_SID_WAVE[] = {"TRI", "SAW", "PLS", "NSE", "ST", "PT", "PS", "PST", "DRM"};
enum { SW_TRI, SW_SAW, SW_PLS, SW_NSE, SW_ST, SW_PT, SW_PS, SW_PST, SW_DRM, SW_N };
static const char *const N_SID_MOD[] = {"OFF", "SYNC", "RING", "S+R"};
#define SID_LFO_INC 260000u                            /* PWM: ~2.6 Hz at 44.1 kHz */
#define SID_DRM_NOISE 500                              /* DRM: the noise burst, samples (~11 ms: half a C64 frame) */

static void sid_note_on(track_t *t, voice_t *v)
{
    (void)t;
    v->ph[0] = v->ph[1] = 0;
    v->s[0] = v->s[1] = 0;                              /* the filter */
    v->s[2] = 0x7FFFF8;                                 /* the noise register, as the chip resets it */
    v->s[4] = 0;                                        /* the samples since the note (DRM) */
}

/* the 12-bit output of waveform w at accumulator a (24 bits in the top of 32), r: ring's MSB (bit 31 of the
 * modulator), pw: the 12-bit width, nz: the noise's 8 bits */
static inline uint32_t sid_wave(uint32_t w, uint32_t a, uint32_t r, uint32_t pw, uint32_t nz)
{
    uint32_t saw = a >> 20, msb = (a ^ r) >> 31;
    uint32_t tri = ((msb ? ~a : a) >> 19) & 0xFFEu;
    uint32_t pls = saw >= pw ? 0xFFFu : 0u;
    switch (w) {
    case SW_TRI: return tri;
    case SW_SAW: return saw;
    case SW_PLS: case SW_DRM: return pls;
    case SW_NSE: return nz << 4;
    case SW_ST: return tri & saw;
    case SW_PT: return pls & tri;
    case SW_PS: return pls & saw;
    default: return pls & tri & saw;
    }
}
static inline uint32_t sid_noise_bits(uint32_t r)       /* the chip's eight output taps of the register */
{
    return ((r >> 13) & 0x80u) | ((r >> 12) & 0x40u) | ((r >> 9) & 0x20u) | ((r >> 7) & 0x10u) |   /* bits 20 18 14 11 */
           ((r >> 6) & 0x08u) | ((r >> 3) & 0x04u) | ((r >> 1) & 0x02u) | (r & 0x01u);            /* 9 5 2 0 */
}

static void sid_render(track_t *t, voice_t *v, int32_t *out, uint32_t n, const vmod_t *m)
{
    const int16_t *p = t->p;
    uint32_t wave = (uint32_t)clamp(p[P_E0], 0, SW_N - 1), mod = (uint32_t)clamp(p[P_E3], 0, 3), i;
    int32_t age = v->s[4], sweep = p[P_E5], tune16, pwv;
    uint32_t a = v->ph[0], b = v->ph[1], lfo = v->ph[2], nreg = (uint32_t)v->s[2], inc, minc = m->inc, pw;
    int32_t ic1 = v->s[0], ic2 = v->s[1], dc = v->s[3];
    uint32_t ft = (uint32_t)p[P_FTYPE];
    int32_t fk = ANALOG_K(p[P_E7]), lt;
    tsvf_t flt;
    tsvf_coef(&flt, (p[P_E6] << 8) + m->cutoff, p[P_E7]);
    /* the voice's pitch: TUNE, moved by the envelope (SWEEP: +-48 semitones at full) */
    tune16 = p[P_E4] * 16 + ((sweep * 12 * m->envq15) >> 15);   /* (DRM: it falls from there as the envelope does) */
    inc = (uint32_t)(((uint64_t)minc * pow2_q16(clamp(tune16, -48 * 16, 60 * 16))) >> 16);
    /* PW: its knob, the part's SHAPE modulation, and the PWM triangle */
    lt = (int32_t)(lfo >> 16);                          /* 0..65535 */
    lt = (lt < 32768 ? lt : 65535 - lt) - 16384;        /* -16384..16383 */
    pwv = p[P_E1] * 32 + ((m->shape - (64 << 8)) >> 3) + ((lt * p[P_E2]) >> 11);
    pw = (uint32_t)clamp(pwv, 16, 4079);
    lfo += SID_LFO_INC * n;
    for (i = 0; i < n; i++) {
        uint32_t w = wave == SW_DRM && age < SID_DRM_NOISE ? (uint32_t)SW_NSE : wave, k, acc = 0;
        for (k = 0; k < 2u; k++) {                      /* twice a sample: (half steps) */
            uint32_t a0 = a, ri = mod >= 2u ? b : 0u;
            a += inc >> 1;
            b += minc >> 1;
            if ((mod & 1u) && b < (minc >> 1))           /* SYNC: the modulator wrapped: the voice starts again */
                a = 0;
            if ((!(a0 & 0x08000000u) && (a & 0x08000000u)) || (w == SW_NSE && wave == SW_DRM)) {
                /* bit 19 rose: the noise register steps (DRM's burst: every time, white, as a drum's first frame) */
                uint32_t fb = ((nreg >> 22) ^ (nreg >> 17)) & 1u;
                nreg = ((nreg << 1) | fb) & 0x7FFFFFu;
            }
            acc += sid_wave(w, a, ri, pw, sid_noise_bits(nreg));
        }
        {
            int32_t s = ((int32_t)acc - 4096) * 8;      /* two 12-bit samples: +-32768 */
            s = softclip(s + (s >> 2)) ;                /* the 6581's input stage, driven a little */
            {
                int32_t y = ft ? analog_tap(&flt, fk, s >> 1, &ic1, &ic2, ft) : tsvf_lp(&flt, s >> 1, &ic1, &ic2);
                int32_t q = y < 0 ? -y : y;
                if (q > 16000) {
                    q = 16000 + (softclip((q - 16000) * 2) >> 1);
                    y = y < 0 ? -q : q;
                }
                s = y << 1;
            }
            dc += ((s << 8) - dc) >> 11;                /* the combined waves' offset out (~3 Hz) */
            s -= dc >> 8;
            out[i] += voice_amp(s, m, i) << 1;
        }
        age += age < 0x7FFFFFFF;
    }
    v->ph[0] = a;
    v->ph[1] = b;
    v->ph[2] = lfo;
    v->s[0] = ic1;
    v->s[1] = ic2;
    v->s[2] = (int32_t)nreg;
    v->s[3] = dc;
    v->s[4] = age;
}

static const preset_t SID_PRESETS[] = {
    /* name, {WAVE, PW, PWM, MOD, TUNE, SWEEP, CUT, RES}, {A D S R}, fenv, mono, FX(dist, chorus, delay, reverb), the
     * PHRASES pattern it suggests */
    {"MONTY BASS", {SW_PLS, 40, 70, 0, 0, 0, 70, 50}, {0, 64, 70, 20}, 30, 1, FX(10, 0, 0, 5), PAT(2)},         /* PWM bass */
    {"GROWL BASS", {SW_PST, 50, 30, 0, 0, 0, 80, 40}, {0, 60, 80, 20}, 20, 1, FX(20, 0, 0, 5), PAT(8)},        /* combined */
    {"SAW BASS", {SW_SAW, 64, 0, 0, 0, 0, 55, 85}, {0, 58, 50, 20}, 50, 1, FX(10, 0, 0, 5), PAT(1)},           /* squelch */
    {"SYNC LEAD", {SW_SAW, 64, 0, 1, 19, 20, 110, 20}, {0, 80, 100, 40}, 0, 1, FX(0, 0, 40, 30), PAT(4)},       /* sweeping */
    {"PWM LEAD", {SW_PLS, 48, 60, 0, 0, 0, 105, 15}, {0, 78, 100, 45}, 10, 1, FX(0, 10, 45, 35), PAT(4)},
    {"TRI FLUTE", {SW_TRI, 64, 0, 0, 0, 0, 120, 0}, {25, 80, 110, 50}, 0, 1, FX(0, 20, 30, 50), PAT(6)},
    {"RING BELL", {SW_TRI, 64, 0, 2, 17, 0, 115, 10}, {0, 85, 0, 75}, 0, 0, FX(0, 20, 45, 70), PAT(7)},
    {"SID STRINGS", {SW_PLS, 56, 90, 0, 0, 0, 85, 25}, {70, 90, 110, 85}, 10, 0, FX(0, 60, 25, 75), PAT(5)},
    {"C64 ARP", {SW_PLS, 32, 40, 0, 0, 0, 110, 10}, {0, 66, 60, 25}, 0, 0, FX(0, 0, 35, 25), PAT(13)},          /* hold a chord, ARP on */
    {"FILTER SWEEP", {SW_SAW, 64, 0, 0, 0, 0, 30, 100}, {0, 90, 60, 40}, 63, 0, FX(0, 20, 30, 40), PAT(4)},
    {"SID KICK", {SW_DRM, 64, 0, 0, 0, 36, 95, 0}, {0, 58, 0, 40}, 0, 0, FX(0, 0, 0, 10), PAT(12)},
    {"SID SNARE", {SW_DRM, 40, 0, 0, 12, 20, 110, 0}, {0, 54, 0, 40}, 0, 0, FX(0, 0, 0, 25), PAT(12)},
    {"SID HAT", {SW_NSE, 64, 0, 0, 36, 0, 127, 0}, {0, 40, 0, 30}, 0, 0, FX(0, 0, 0, 15), PAT(12)},
    {"ZAP", {SW_PLS, 20, 0, 0, 36, -50, 120, 30}, {0, 60, 0, 40}, 0, 0, FX(0, 0, 40, 20), PAT(7)},
    {"POWER UP", {SW_TRI, 64, 0, 0, 0, 40, 120, 0}, {60, 70, 100, 40}, 0, 1, FX(0, 0, 45, 30), PAT(7)},
    {"NOISE WIND", {SW_NSE, 64, 0, 0, 0, 0, 60, 90}, {90, 90, 100, 95}, 30, 0, FX(0, 40, 30, 90), PAT(5)},
    {"TITLE SCREEN", {SW_PT, 50, 50, 0, 12, 0, 110, 20}, {0, 75, 90, 45}, 10, 1, FX(0, 20, 50, 40), PAT(4)},  /* the loader */
    {"SPELLBOOK", {SW_TRI, 64, 0, 3, 7, 25, 100, 30}, {50, 90, 90, 80}, 15, 0, FX(0, 50, 40, 80), PAT(5)},     /* sync + ring */
    /* Dungeon Tracker 1.0: more sounds (appended: a song's preset numbers stay) */
    {"SID TOM", {SW_DRM, 64, 0, 0, 0, 24, 100, 0}, {0, 62, 0, 40}, 0, 0, FX(0, 0, 0, 20), PAT(12)},
    {"SID CLAP", {SW_NSE, 64, 0, 0, 20, 0, 110, 30}, {0, 48, 0, 34}, 0, 0, FX(0, 0, 0, 25), PAT(12)},
    {"SID COWBELL", {SW_PLS, 64, 0, 2, 9, 0, 120, 0}, {0, 52, 0, 34}, 0, 0, FX(0, 0, 0, 15), PAT(12)},
    {"SID BOOM", {SW_DRM, 64, 0, 0, 0, 50, 80, 0}, {0, 66, 0, 44}, 0, 0, FX(0, 0, 0, 20), PAT(12)},
    {"SID PULSE", {SW_PLS, 30, 40, 0, 0, 0, 80, 40}, {0, 60, 80, 20}, 30, 1, FX(10, 0, 0, 5), PAT(2)},
    {"SID TRI BASS", {SW_TRI, 64, 0, 0, 0, 0, 120, 0}, {0, 60, 100, 16}, 0, 1, FX(0, 0, 0, 5), PAT(8)},
    {"SYNC BASS", {SW_SAW, 64, 0, 1, 12, 0, 70, 50}, {0, 58, 80, 20}, 40, 1, FX(10, 0, 0, 5), PAT(1)},
    {"BUZZ BASS", {SW_PT, 60, 20, 0, 0, 0, 70, 40}, {0, 58, 80, 18}, 20, 1, FX(10, 0, 0, 5), PAT(2)},
    {"FUNK BASS", {SW_PLS, 50, 0, 0, 0, 8, 60, 90}, {0, 52, 40, 16}, 60, 1, FX(0, 0, 0, 5), PAT(1)},
    {"DEEP PULSE", {SW_PLS, 32, 60, 0, 0, 0, 50, 20}, {0, 60, 92, 16}, 10, 1, FX(0, 0, 0, 5), PAT(8)},
    {"RING BASS", {SW_TRI, 64, 0, 2, 12, 0, 100, 20}, {0, 58, 90, 18}, 0, 1, FX(0, 0, 0, 5), PAT(2)},
    {"SID CHOIR", {SW_PT, 50, 80, 0, 0, 0, 65, 30}, {70, 90, 88, 85}, 10, 0, FX(0, 60, 25, 80), PAT(5)},
    {"SAW PAD", {SW_SAW, 64, 0, 0, 0, 0, 60, 40}, {70, 90, 110, 85}, 20, 0, FX(0, 60, 25, 75), PAT(5)},
    {"DREAM PAD", {SW_PLS, 36, 110, 0, 0, 0, 75, 20}, {80, 96, 88, 96}, 0, 0, FX(0, 70, 40, 90), PAT(5)},
    {"SID ORGAN", {SW_PLS, 40, 0, 0, 0, 0, 90, 10}, {0, 0, 90, 20}, 0, 0, FX(0, 40, 0, 30), PAT(6)},
    {"SID KEYS", {SW_PLS, 40, 20, 0, 0, 0, 100, 20}, {0, 80, 30, 40}, 30, 0, FX(0, 20, 20, 30), PAT(6)},
    {"SID PLUCK", {SW_PLS, 32, 0, 0, 0, 0, 100, 40}, {0, 60, 0, 36}, 50, 0, FX(0, 0, 35, 25), PAT(3)},
    {"HARPSI", {SW_PS, 40, 0, 0, 0, 0, 115, 10}, {0, 80, 0, 40}, 0, 0, FX(0, 10, 20, 30), PAT(3)},
    {"SID SOLO", {SW_PLS, 36, 30, 0, 0, 0, 95, 20}, {0, 70, 88, 40}, 0, 1, FX(0, 10, 45, 30), PAT(4)},
    {"SAW SOLO", {SW_SAW, 64, 0, 0, 0, 0, 100, 40}, {0, 70, 110, 40}, 20, 1, FX(0, 10, 40, 30), PAT(4)},
    {"RING LEAD", {SW_TRI, 64, 0, 2, 7, 0, 115, 10}, {0, 70, 110, 40}, 0, 1, FX(0, 10, 40, 30), PAT(4)},
    {"SYNC SCREAM", {SW_SAW, 64, 0, 1, 31, 30, 115, 30}, {0, 70, 110, 36}, 0, 1, FX(20, 0, 40, 30), PAT(4)},
    {"HOLLOW LEAD", {SW_PT, 50, 40, 0, 0, 0, 90, 20}, {10, 70, 92, 40}, 0, 1, FX(0, 20, 40, 35), PAT(4)},
    {"ARCADE LEAD", {SW_PLS, 24, 0, 0, 0, 0, 100, 10}, {0, 70, 92, 30}, 0, 1, FX(0, 0, 40, 20), PAT(4)},
    {"EXPLOSION", {SW_NSE, 64, 0, 0, 0, -40, 90, 40}, {0, 80, 0, 60}, 40, 0, FX(20, 0, 20, 50), PAT(7)},
    {"LASER", {SW_PLS, 30, 0, 0, 24, -63, 120, 20}, {0, 60, 0, 40}, 0, 0, FX(0, 0, 40, 20), PAT(7)},
    {"TELEPORT", {SW_TRI, 64, 0, 3, 19, 63, 110, 30}, {20, 80, 60, 60}, 0, 0, FX(0, 40, 50, 70), PAT(7)},
    {"ALARM", {SW_PLS, 40, 0, 0, 12, 40, 100, 0}, {0, 70, 88, 30}, 0, 0, FX(0, 0, 30, 20), PAT(7)},
};

static const engine_t ENG_SID = {
    .name = "SID",
    .page_title = {"OSC", "MOD"},
    .edit = {
        {"WAVE", F_ENUM, 0, SW_N - 1, SW_PLS, N_SID_WAVE, 0},
        {"PW", F_PCT, 0, 127, 64, 0, 0},
        {"PWM", F_PCT, 0, 127, 0, 0, 0},
        {"MOD", F_ENUM, 0, 3, 0, N_SID_MOD, 0},
        {"TUNE", F_SEMI, 0, 48, 0, 0, 0},
        {"SWEEP", F_BIPCT, -64, 63, 0, 0, 0},
        {"CUT", F_CUTOFF, 0, 127, 110, 0, 0},
        {"RES", F_PCT, 0, 127, 20, 0, 0},
    },
    .presets = SID_PRESETS,
    .npresets = NELEM(SID_PRESETS),
    .note_on = sid_note_on,
    .render = sid_render,
    .knob = {P_E1, P_E6, P_E7, P_E5},
    .keep = 0x03,                                       /* the filter */
};
