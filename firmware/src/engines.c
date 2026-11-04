/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright (C) 2026 Leo Kuroshita (@kurogedelic), Hügelton Instruments */
/* Engine table (order = PRESETS browsing order and the engine numbers of the editor protocol), the
 * factory patterns (SAVE > PHRASES) and the parts' sounds at power-on. */
#include "../engines/src/dsp.c"   /* Felucca-Engines (MIT): the shared DSP blocks */

/* The parts' engine memory (1.5): a part plays one engine at a time, so the engines with large per-part state
 * (engine_t.mem: PHYS's slots, GRAIN's grains and index, DRUM's lanes, FM6's notes, SLICE's reverse windows)
 * keep it in one region per part instead of an array each. The region holds the state of the engine that
 * last claimed it (eng_mem_owner); a switch to another such engine zeroes its engine_t.mem bytes, which is
 * the state each had at power-on, so a switch sounds as it did with the arrays (tests/union_test.c). State
 * that runs on across notes and switches stays outside (WHEEL's wheels, GRAIN's random numbers).
 * Sized by the largest, PHYS (3 x SYMP's slot); an engine that needs more than ENG_MEM_PART grows it for
 * every part (x NPART of the pool). Zero-initialised pool section: main.c fm1_cstart clears it. */
#define ENG_MEM_PART 12888u
static union {
    uint64_t align;
    uint8_t b[ENG_MEM_PART];
} eng_mem[NPART] __attribute__((section(".pool")));
static uint8_t eng_mem_owner[NPART];       /* engine index + 1 whose state is in the region, 0 = none (zeros) */
#define ENG_MEM(p) ((void *)eng_mem[p].b)  /* part p's region (p < NPART) */
#define ENG_MEM_FITS(name, size) _Static_assert((size) <= ENG_MEM_PART, name " state over ENG_MEM_PART")

#include "../engines/src/eng_analog.c"   /* Felucca-Engines (MIT): ANALOG LOFI TRIO WHEEL NOISE */
#include "eng_phase.c"
#include "../engines/src/eng_lofi.c"
#include "eng_sample.c"
#include "eng_formant.c"
#include "../engines/src/eng_trio.c"
#include "../engines/src/eng_wheel.c"
#include "eng_grain.c"
#include "eng_phys.c"           /* PHYS: DaisySP physical models (phys_dsp.c, MIT) */
#include "eng_drum.c"           /* DRUM: the 8-lane kit (drum_voice.c) */
#include "../engines/src/eng_noise.c"
#include "eng_fm6.c"            /* FM6: 6-operator FM, msfa ported (fm6_core.c, Apache-2.0) */
#include "fm4_convert.c"        /* DIGITAL's tables, and its sounds -> FM6 */
#if DT_RETIRE
/* Dungeon Tracker 1.1: a retired engine's slot (retire.c): silent, no presets, never offered; a sound of it loads as its
 * substitute (ENG_SUBST below). The engines' code is no longer referenced: the build leaves it out */
static const engine_t ENG_GONE = {
    .name = "-",
    .page_title = {"-", "-"},
    .edit = FM4_EDIT("-", "-", "-", "-", "-", "-", "-", F_INT, F_INT, 0, 0),
    .note_on = fm4_gone_note_on,
    .render = fm4_gone_render,
    .knob = {P_E4, P_E5, P_E6, P_REL},
};
#define ENG_KEPT(x) &ENG_GONE
#else
#define ENG_KEPT(x) &x
#endif
#if FELUCCA_FM4
#include "eng_digital.c"        /* DIGITAL: four-operator FM (retired; FELUCCA_FM4=1 builds it) */
#endif
#if FELUCCA_SLICE
#include "eng_slice.c"
#endif
#if RONDO_LAYOUT && !FELUCCA_SLICE  /* Dungeon Tracker: SLICE retired; 13 reserved as DIGITAL's 1 is (never offered:
                                     * eng_ok; a SLICE sound in a project loads as SAMPLE, project.c proj_slice) */
static const engine_t ENG_SLICE_GONE = {
    .name = "-",
    .page_title = {"-", "-"},
    .edit = FM4_EDIT("-", "-", "-", "-", "-", "-", "-", F_INT, F_INT, 0, 0),
    .note_on = fm4_gone_note_on,
    .render = fm4_gone_render,
    .knob = {P_E4, P_E5, P_E6, P_REL},
};
#endif
#include "eng_genesis.c"        /* 1.2: GENESIS (18): after the YM2612 (before GEN KIT, 14) */
#include "eng_cycle.c"          /* RONDO: CYCLE KIT (14) and CYCLE TONE (15) */
#include "mutable/eng_waves.c"  /* 1.1: WAVES (16): Ambika's oscillators (Mutable Instruments, GPL-3.0) */
#include "eng_wt32.c"           /* 1.5: WAVE 32 (20): after WAVES (its filter) */
#include "eng_mod.c"            /* 1.5: the AMIGA'D family, MOD KIT (19) and MOD (21) */
#include "eng_sid.c"            /* 1.3: SID (17): after the MOS 6581 */

/* the editor protocol, user presets and projects store these indices: append, never reorder */
static const engine_t *const ENGINES[NENGINES] = {
    &ENG_ANALOG,                 /* 0 (1.5: back, the subtractive family's) */
#if FELUCCA_FM4
    &ENG_DIGITAL,                /* 1 (ENGI_DIGITAL) */
#else
    &ENG_FM4_GONE,               /* 1: reserved (DIGITAL, retired: its sounds convert to FM6, fm4_convert.c) */
#endif
    ENG_KEPT(ENG_PHASE),                  /* 2 */
    ENG_KEPT(ENG_LOFI),                   /* 3 CHIP (1.5: retired, -> WAVES) */
    ENG_KEPT(ENG_SAMPLE),                 /* 4 (1.5: retired: its kits MOD KIT's, the rest GENESIS) */
    ENG_KEPT(ENG_FORMANT),                /* 5 VOICE (eng_formant.c: "voice" is a sounding note in voice.c) */
    ENG_KEPT(ENG_TRIO),                   /* 6 */
    ENG_KEPT(ENG_WHEEL),                  /* 7 */
    ENG_KEPT(ENG_GRAIN),                  /* 8 (1.5: retired, -> WAVE 32) */
    ENG_KEPT(ENG_PHYS),                   /* 9 (ENGI_PHYS; 1.5: retired, -> MOD, its drums MOD KIT) */
    ENG_KEPT(ENG_DRUM),                   /* 10 (ENGI_DRUM) */
    ENG_KEPT(ENG_NOISE),                  /* 11 */
    ENG_KEPT(ENG_FM6),                    /* 12 (ENGI_FM6) */
#if FELUCCA_SLICE
    &ENG_SLICE,                  /* 13 (FELUCCA_SLICE=0 builds without it) */
#elif RONDO_LAYOUT
    &ENG_SLICE_GONE,             /* 13: reserved (SLICE, retired in Dungeon Tracker) */
#endif
    &ENG_CKIT,                   /* 14 (ENGI_CKIT): RONDO */
    ENG_KEPT(ENG_CTONE),                  /* 15 (ENGI_CTONE): RONDO (1.5: retired, -> GENESIS) */
    &ENG_WAVES,                  /* 16 (ENGI_WAVES): Dungeon Tracker */
    &ENG_SID,                    /* 17 (ENGI_SID): Dungeon Tracker */
    &ENG_GENESIS,                /* 18 (ENGI_GENESIS): Dungeon Tracker 1.2 */
    &ENG_MKIT,                   /* 19 (ENGI_MKIT): Dungeon Tracker 1.5, MOD KIT */
    &ENG_WT32,                   /* 20 (ENGI_WT32): Dungeon Tracker 1.5 */
    &ENG_MOD,                    /* 21 (ENGI_MOD): Dungeon Tracker 1.5 */
};

/* a track's engine number as an index (the audio paths: a compare, cheaper than % NENGINES; a bad number: 0) */
static inline uint32_t eng_idx(uint32_t e) { return e < NENGINES ? e : 0u; }

/* part t's engine memory goes to its engine (t->engine; voice.c engine_block, once a block: on a switch, after
 * the old engine's fade, before any note on the new one): zeroed (its engine_t.mem bytes) when another
 * engine's state is in it. An engine without memory leaves it as it is: a switch back finds its state. */
static void eng_mem_claim(const track_t *t)
{
    uint32_t p = (uint32_t)(t - trk), e, k, n;
    uint32_t *w;
    if (p >= NPART)
        return;
    e = eng_idx(t->engine);
    n = ENGINES[e]->mem;
    if (!n || eng_mem_owner[p] == e + 1u)
        return;
    w = (uint32_t *)ENG_MEM(p);
    for (k = 0; k < (n + 3u) / 4u; k++)
        w[k] = 0;
    eng_mem_owner[p] = (uint8_t)(e + 1u);
}

/* the sample source a track's sound plays that has no data (SAMPLE's SET, GRAIN's SRC, SLICE's SRC: an empty or
 * invalid user slot, a set or PIANO this build lacks): its name (it plays a sine: eng_sample.c smp_sine), 0 = none;
 * *code a small number for it (1.., per engine and source) so the UI says it once (ui_input.c sample_notice) */
static uint8_t snd_said[NTRK];   /* per track: the code last said, 0 = none (a project load clears them: said again) */
static const char *snd_missing(const track_t *t, uint32_t *code)
{
    const engine_t *e = ENGINES[eng_idx(t->eng_req)];
    uint32_t si = (uint32_t)t->p[P_E0] % SMP_NALL;
#if FELUCCA_SLICE
    if (e == &ENG_SLICE) {
        si = slc_src_of(t->p);
        *code = 64u + si;
        return slc_get(si) ? 0 : N_SLC_SRC[si];
    }
#endif
    if (e != &ENG_SAMPLE && e != &ENG_GRAIN)
        return 0;
    *code = (e == &ENG_GRAIN ? 32u : 1u) + si % 32u;
    return smp_set_missing(si) ? SMP_ALL_NAMES[si] : 0;
}

/* the order the engines are shown in (PRESETS browsing and its ENG knob, the EDIT layer's keys, the editor's list):
 * engine indices, never DIGITAL's reserved 1 (with FELUCCA_FM4 it follows FM6). The indices stay as they are (the
 * stores and the protocol hold them); only this table orders them */
#if DT_RETIRE
static const uint8_t ENGINE_ORDER[NENG_SHOWN] = {18, 14, 16, 20, 21, 19, 0, 17};   /* 1.5: the four families, two each:
                                                                               * FM, WAVE, AMIGA'D, SUB */   /* 1.1: FMKIT FMTONE WAVES SID CHIP SAMPLE
                                                                               * PHYS GRAIN */
#else
static const uint8_t ENGINE_ORDER[NENG_SHOWN] = {
    14, 15,                      /* RONDO: FMKIT, FMTONE first */
    0,                           /* ANALOG */
    12,                          /* FM6 */
    16,                          /* 1.1: WAVES */
    17,                          /* 1.3: SID */
#if FELUCCA_FM4
    1,                           /* DIGITAL */
#endif
    2, 3, 4, 5, 6, 7, 9,         /* PHASE LOFI SAMPLE VOICE TRIO WHEEL PHYS */
    19, 20, 21,                  /* 1.5: MOD KIT, WAVE 32, MOD */
    10,                          /* DRUM (RONDO: SLICE, 13, not offered: core.h NENG_SHOWN) */
    8,                           /* GRAIN (1.3: after the 14 on the EDIT layer's keys, with NOISE: PRESETS only) */
#if !FELUCCA_FM4
    11,                          /* NOISE (1.1: last, after the 14 on the EDIT layer's keys: PRESETS only) */
#endif
};
#endif

/* the engines one can pick (engine 1 only with FELUCCA_FM4), in ENGINE_ORDER: eng_ok(e), the n-th of them
 * eng_vis(n), e's place among them eng_rank(e), the next / previous one eng_step(e, dir) (wraps) */
#if DT_RETIRE
#define ENGI_GRAIN 8u
static const uint8_t ENG_SUBST[NENGINES] = {   /* 1.1: a retired engine's substitute (retire.c), 0 = kept (1.5: the four
                                                * families') */
    [ENGI_DIGITAL] = ENGI_GENESIS,       /* DIGITAL (Felucca's retired; its sounds as FM6 until 1.1) -> GENESIS */
    [2] = ENGI_WAVES,                    /* PHASE */
    [3] = ENGI_WAVES,                    /* CHIP (1.5: the chip waves are WAVES') */
    [4] = ENGI_MKIT,                     /* SAMPLE (1.5: its DUNGEON and LOFI kits MOD KIT's; PIANO, FLUTE GENESIS': retire.c) */
    [5] = ENGI_WAVES,                    /* VOICE (the formants: WAVES' vowel tables) */
    [6] = ENGI_SID,                      /* TRIO (three oscillators, sync, ring: SID's) */
    [7] = ENGI_GENESIS,                  /* WHEEL (the organs) */
    [8] = ENGI_WT32,                     /* GRAIN (1.5: the textures, WAVE 32's pads) */
    [ENGI_PHYS] = ENGI_MOD,              /* PHYS (1.5: MOD; its drums MOD KIT's: retire.c) */
    [ENGI_DRUM] = ENGI_CKIT,             /* DRUM -> GEN KIT (the GM map, the grid: the same) */
    [11] = ENGI_SID,                     /* NOISE (winds: SID's noise) */
    [ENGI_FM6] = ENGI_GENESIS,           /* FM6 -> GENESIS */
    [ENGI_SLICE] = ENGI_MKIT,            /* SLICE (retired in 1.0: its breaks) */
    [ENGI_CTONE] = ENGI_GENESIS,         /* FMTONE (1.5) -> GENESIS */
};
static int eng_retired(uint32_t e) { return e < NENGINES && ENG_SUBST[e] != 0u; }
static uint32_t eng_subst(uint32_t e) { e = e < NENGINES ? e : 0u; return ENG_SUBST[e] ? ENG_SUBST[e] : e; }
#else
static int eng_retired(uint32_t e) { (void)e; return 0; }
static uint32_t eng_subst(uint32_t e) { return e; }
#endif
/* 1.5: the four families the eight engines come in, two each (ENGINE_ORDER's pairs): FM, WAVE, AMIGA'D, SUB */
static const char *eng_family(uint32_t e)
{
    switch (e) {
    case ENGI_GENESIS: case ENGI_CKIT: case ENGI_CTONE: case ENGI_FM6: case ENGI_DIGITAL: return "FM";
    case ENGI_WAVES: case ENGI_WT32: case 3u: case 8u: return "WAVE";
    case ENGI_MOD: case ENGI_MKIT: case ENGI_SAMPLE: case ENGI_PHYS: return MOD_FAMILY;
    default: return "SUB";
    }
}
static int eng_ok(uint32_t e) { return e < NENGINES && !eng_retired(e) && (FELUCCA_FM4 || e != ENGI_DIGITAL) && (FELUCCA_SLICE || !RONDO_LAYOUT || e != ENGI_SLICE); }
static uint32_t eng_vis(uint32_t n) { return ENGINE_ORDER[n % NENG_SHOWN]; }
static uint32_t eng_rank(uint32_t e)
{
    uint32_t n;
    if (!eng_ok(e))
        e = DT_RETIRE ? eng_subst(e) : ENGI_FM6;   /* (DIGITAL without FELUCCA_FM4: its sounds play as FM6; 1.1: a retired
                                                * engine's as its substitute) */
    for (n = 0; n < NENG_SHOWN && ENGINE_ORDER[n] != e; n++)
        ;
    return n < NENG_SHOWN ? n : 0u;
}
static uint32_t eng_step(uint32_t e, int32_t dir)
{
    return eng_vis((eng_rank(e % NENGINES) + (dir > 0 ? 1u : NENG_SHOWN - 1u)) % NENG_SHOWN);
}

/* factory sequence patterns: SAVE > PHRASES loads one into the selected track (ui.c pat_load); a preset
 * only suggests one with PAT(n), loading a sound never touches the steps. Absolute notes, loaded as they
 * are (DRUM plays them as GM drums, SLICE as slices; SCL TRANS and OCT transpose): 0 = rest;
 * flags 1 = accent, 2 = slide, 4 = tie (holds the previous note). Names: at most 8 characters */
#define T_ 4
static const struct {
    const char *name;
    uint8_t note[16], flags[16];
} PATTERNS[] = {
    {"ACID", {45, 45, 57, 45, 0, 48, 45, 55, 45, 0, 57, 52, 45, 48, 0, 50},          /* 1 */
     {1, 0, 2, 0, 0, 0, 1, 2, 0, 0, 1, 0, 0, 2, 0, 1}},
    {"OFFBEAT", {0, 36, 0, 36, 0, 36, 0, 48, 0, 36, 0, 36, 0, 39, 0, 43},          /* 2 bass */
     {0, 1, 0, 0, 0, 1, 0, 0, 0, 1, 0, 0, 0, 0, 0, 0}},
    {"MELODY", {60, 0, 67, 0, 72, 67, 0, 64, 62, 0, 69, 0, 74, 69, 0, 67},         /* 3 pluck */
     {1, 0, 0, 0, 1, 0, 0, 0, 1, 0, 0, 0, 1, 0, 0, 0}},
    {"LEAD", {72, 0, 0, 74, 0, 0, 76, 0, 79, 0, 76, 0, 74, 0, 0, 0},               /* 4 */
     {1, T_, 0, 2, T_, 0, 0, 0, 1, 0, 2, 0, 0, T_, T_, 0}},
    {"PAD", {60, 0, 0, 0, 0, 0, 0, 0, 57, 0, 0, 0, 55, 0, 0, 0},                   /* 5 long notes */
     {0, T_, T_, T_, T_, T_, T_, 0, 0, T_, T_, 0, 0, T_, T_, 0}},
    {"KEYS", {0, 0, 60, 0, 0, 63, 0, 0, 0, 0, 60, 0, 0, 65, 0, 63},                /* 6 offbeat stabs */
     {0, 0, 1, 0, 0, 0, 0, 0, 0, 0, 1, 0, 0, 0, 0, 0}},
    {"BELL", {72, 0, 0, 79, 0, 0, 84, 0, 0, 0, 76, 0, 0, 0, 0, 0},                 /* 7 sparse */
     {1, 0, 0, 0, 0, 0, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0}},
    {"SUB", {36, 0, 0, 0, 0, 0, 0, 36, 0, 0, 34, 0, 0, 0, 0, 0},                   /* 8 low, held */
     {1, T_, T_, T_, 0, 0, 0, 0, 0, 0, 0, T_, T_, T_, 0, 0}},
    /* SLICE (eng_slice.c): note = C4 + slice */
    {"CHOP", {60, 61, 62, 67, 64, 65, 60, 69, 68, 70, 62, 67, 72, 72, 74, 64},     /* 9 16 slices re-ordered */
     {1, 0, 0, 0, 1, 0, 0, 0, 0, 0, 0, 0, 1, 0, 0, 0}},
    {"STUTTER", {60, 60, 61, 61, 62, 0, 63, 63, 64, 65, 65, 0, 66, 66, 66, 67},    /* 10 8 slices, repeats */
     {1, 0, 0, 0, 1, 0, 0, 0, 0, 0, 0, 0, 1, 0, 0, 0}},
    {"SLICES", {60, 61, 62, 63, 64, 65, 66, 67, 68, 69, 70, 71, 72, 73, 74, 75},   /* 11 in order */
     {1, 0, 0, 0, 1, 0, 0, 0, 1, 0, 0, 0, 1, 0, 0, 0}},
    /* DRUM (General MIDI: 36 kick, 38 snare, 42 closed / 46 open hi-hat) */
    {"BEAT", {36, 42, 42, 42, 38, 42, 36, 42, 36, 42, 42, 36, 38, 42, 46, 42},     /* 12 */
     {1, 0, 0, 0, 1, 0, 0, 0, 1, 0, 0, 0, 1, 0, 0, 0}},
    /* 16ths up a C minor arpeggio, twice: the line the ARP presets (RAVE, ARP 8BIT, ARP LEAD) suggest
     * now that the arpeggiator is the track's and a preset no longer switches it on */
    {"ARP", {48, 51, 55, 60, 63, 67, 72, 75, 48, 51, 55, 60, 63, 67, 72, 75},       /* 13 */
     {1, 0, 0, 0, 0, 0, 0, 0, 1, 0, 0, 0, 0, 0, 0, 0}},
};
#undef T_
#define NPATTERNS (sizeof PATTERNS / sizeof PATTERNS[0])

/* the parts at power-on (engine, preset, PATTERNS[n - 1] in the sequencer, 0 = empty: all are): bass, pad, lead, drums */
/* RONDO: T1 drums (FMKIT, the BEAT pattern), T2 chords / pads (FMTONE FM PAD), T3 bass (FMTONE FM BASS, mono),
 * T4 lead (FMTONE FM LEAD); the sequencers empty but the drums' */
#if RONDO_LAYOUT
static const uint8_t TRK_DEF[NPART][3] = {{ENGI_CKIT, 0, 12}, {ENGI_GENESIS, 21, 0}, {ENGI_GENESIS, 0, 0}, {ENGI_GENESIS, 14, 0}};   /* (1.5:
                                                           * GENESIS WARM PAD, GEN BASS, HERO LEAD) */
#else
static const uint8_t TRK_DEF[NPART][3] = {{0, 4, 0}, {ENGI_FM6, 4, 0}, {3, 0, 0}, {ENGI_DRUM, 0, 0}};   /* Felucca's */
#endif
static uint32_t trk_def_engine(uint32_t i) { return TRK_DEF[i % NPART][0]; }
