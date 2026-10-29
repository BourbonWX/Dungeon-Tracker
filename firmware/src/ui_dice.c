/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright (C) 2026 RONDO contributors. Built on Felucca (C) 2026 Leo Kuroshita (@kurogedelic), Hügelton Instruments */
/* RONDO's EDIT > DICE: sound design by rolling dice, on FMTONE, FMKIT and (RE-1) CHIP tracks. Each die is one part of the sound;
 * OCT+ rolls every die that is not held, so a sound you like can be kept part by part while the rest is rolled
 * again (an offshoot of it). The dice only land on values that sound musical: each draws from ranges and weighted
 * choices set for the engine and the track's role (T2 chords / pads, T3 bass, T4 lead; others as T2).
 *   FMTONE  RATIO COLOR CNTR MSUS FDBK SWEEP STACK SUB (its eight values) and ENV (the amplitude envelope, one of a
 *           role's shapes)
 *   CHIP    MODE WAVE DUTY CRSH SWEEP VIB ARP TONE (its eight values) and ENV. DUTY follows the WAVE rolled (a
 *           wave RAM table, or a pulse width), CRSH the MODE (STEP: a stepped decay, DCY); a chord or bass track
 *           never rolls NOISE or a chord ARP
 *   FMKIT   BD SD HH PC (each voice's PITCH DECAY COLOR SWEEP SHAPE together), MIX (the four levels) and FEEL (OPEN
 *           CLICK DRV VARI)
 * KNOB 1 DIE (the cursor), KNOB 2 HOLD, KNOB 3 WILD (100 %: a fresh roll; lower: each die moves only part of the way
 * from where it is towards its new roll, a mutation), KNOB 4 HIST (the last eight sounds before a roll, newest first:
 * turning loads one). OCT+ ROLL, OCT- back one in HIST. The keys play as on any page, to hear each roll. */

#define DICE_MAX 9u
#define DICE_VALS 32u                                     /* the values a roll can change, per engine (dice_ids) */
#define DICE_HIST 8u
static struct {
    uint16_t hold[NTRK];                                  /* bit d: die d held */
    uint8_t cur[NTRK], wild[NTRK], hpos[NTRK], hn[NTRK];  /* the cursor, WILD (0 = 100 %), HIST shown, HIST kept */
    uint8_t eng[NTRK];                                    /* the engine the history is of (+ 1) */
    int16_t hist[NTRK][DICE_HIST][DICE_VALS];
    uint32_t rng;
    uint8_t anim;                                         /* frames of the roll still shown tumbling */
} dice;

static int dice_engine_ok(const track_t *t)
{
    uint32_t e = t->eng_req % NENGINES;
    return e == ENGI_CTONE || e == ENGI_CKIT || e == ENGI_LOFI;
}
static int dice_kit(const track_t *t) { return t->eng_req % NENGINES == ENGI_CKIT; }
static int dice_lofi(const track_t *t) { return t->eng_req % NENGINES == ENGI_LOFI; }
static uint32_t dice_count(const track_t *t) { return dice_kit(t) ? 6u : 9u; }
static const char *const DICE_TONE_N[9] = {"RATIO", "COLOR", "CNTR", "MSUS", "FDBK", "SWEEP", "STACK", "SUB", "ENV"};
static const char *const DICE_LOFI_N[9] = {"MODE", "WAVE", "DUTY", "CRSH", "SWEEP", "VIB", "ARP", "TONE", "ENV"};
static const char *const DICE_KIT_N[6] = {"BD", "SD", "HH", "PC", "MIX", "FEEL"};
static const char *dice_name(const track_t *t, uint32_t d)
{
    return dice_kit(t) ? DICE_KIT_N[d % 6u] : dice_lofi(t) ? DICE_LOFI_N[d % 9u] : DICE_TONE_N[d % 9u];
}
/* a die that is a choice, not a distance (WILD: the chance it changes, not how far) */
static int dice_discrete(const track_t *t, uint32_t d)
{
    if (dice_kit(t))
        return 0;
    if (dice_lofi(t))
        return d <= 3u || d == 6u || d == 8u;             /* CHIP WAVE DUTY CRSH ARP ENV: lists, tables, decays */
    return d == 0u || d == 8u;                            /* RATIO, ENV */
}
static uint32_t dice_wild(uint32_t k) { return dice.wild[k] ? dice.wild[k] - 1u : 100u; }

/* the parameters of die d (ids, at most 5) */
static uint32_t dice_die_ids(const track_t *t, uint32_t d, uint8_t *ids)
{
    uint32_t k;
    if (!dice_kit(t)) {
        if (d < 8u) {
            ids[0] = (uint8_t)(P_E0 + d);
            return 1;
        }
        ids[0] = P_ATK, ids[1] = P_DEC, ids[2] = P_SUS, ids[3] = P_REL;
        return 4;
    }
    if (d < 4u) {
        for (k = 0; k < 5u; k++)
            ids[k] = (uint8_t)(P_FM1_ATK + 5u * d + k);
        return 5;
    }
    if (d == 4u) {
        for (k = 0; k < 4u; k++)
            ids[k] = (uint8_t)(P_LN0 + k);
        return 4;
    }
    ids[0] = P_E3, ids[1] = P_E4, ids[2] = P_E6, ids[3] = P_E7;   /* FEEL: OPEN CLICK DRV VARI */
    return 4;
}
/* every value the dice can change, in a fixed order (the history's) */
static uint32_t dice_ids(const track_t *t, uint8_t *ids)
{
    uint32_t d, n = 0;
    for (d = 0; d < dice_count(t); d++)
        n += dice_die_ids(t, d, ids + n);
    return n;
}

static uint32_t dice_rand(void)
{
    uint32_t s = dice.rng ? dice.rng : 0x9E3779B9u ^ fm1_ms;
    s ^= s << 13;
    s ^= s >> 17;
    s ^= s << 5;
    dice.rng = s;
    return s;
}
static int32_t dice_in(int32_t lo, int32_t hi) { return lo + (int32_t)(dice_rand() % (uint32_t)(hi - lo + 1)); }
static int dice_chance(uint32_t pct) { return dice_rand() % 100u < pct; }
/* a weighted pick: w[i] the weight of choice i (n of them) */
static uint32_t dice_pick(const uint8_t *w, uint32_t n)
{
    uint32_t s = 0, i, r;
    for (i = 0; i < n; i++)
        s += w[i];
    r = dice_rand() % (s ? s : 1u);
    for (i = 0; i < n; i++) {
        if (r < w[i])
            return i;
        r -= w[i];
    }
    return 0;
}

/* the track's role: 1 chords / pads, 2 bass, 3 lead (the RONDO layout; other tracks as chords) */
static uint32_t dice_role(const track_t *t)
{
    uint32_t k = trk_index(t) % NTRK;
#if RONDO_LAYOUT
    k = trk_cls(k);                                     /* (Dungeon Tracker: its class: FIGHTER bass, MAGE lead) */
    return k == CLS_FIGHTER ? 2u : k == CLS_MAGE ? 3u : 1u;
#else
    return k == 2u || k == 3u ? k : 1u;
#endif
}

/* the common ENV (ATK DEC SUS REL): one of the role's shapes, a little varied */
static void dice_env(const uint8_t (*E)[4], int16_t *v)
{
    const uint8_t *e = E[dice_rand() % 5u];
    uint32_t k;
    for (k = 0; k < 4u; k++)
        v[k] = (int16_t)clamp((int32_t)e[k] + (e[k] ? dice_in(-6, 6) : 0), 0, 127);
}

/* CHIP's dice (eng_lofi.c, Felucca's LOFI). The roll goes die by die, in order, so DUTY and CRSH read the MODE and WAVE
 * just rolled */
static void dice_fresh_lofi(const track_t *t, uint32_t d, int16_t *v, uint32_t role)
{
    uint32_t r = role - 1u, chip = (uint32_t)t->p[P_E0], wave = (uint32_t)t->p[P_E1];
    switch (d) {
    case 0: {                                             /* MODE: 4BIT 4B/2 8BIT 1BIT STEP (1BIT: never, a pulse at 1 bit is lopsided) */
        static const uint8_t W[3][5] = {{3, 1, 3, 0, 5}, {3, 1, 2, 0, 6}, {3, 1, 3, 0, 6}};
        v[0] = (int16_t)dice_pick(W[r], 5);
        break;
    }
    case 1: {                                             /* WAVE: PLS TRI SAW NOIS WRAM (noise: never) */
        static const uint8_t W[3][5] = {{5, 1, 1, 0, 5}, {3, 6, 1, 0, 4}, {7, 1, 2, 0, 4}};
        v[0] = (int16_t)dice_pick(W[r], 5);
        break;
    }
    case 2:                                               /* DUTY: the wave RAM table, or the pulse width */
        if (wave == 4u) {
            static const uint8_t W[3][16] = {
                /* SQ50 SQ25 SQ12 SINE TRI SAW SAW2 BAS1 BAS2 ORGN HOLW VOXA VOXO VOXE BUZZ STAIR */
                {1, 1, 0, 2, 1, 1, 2, 0, 0, 4, 2, 3, 3, 2, 0, 1},   /* chords / pads */
                {2, 1, 0, 1, 3, 2, 1, 5, 5, 1, 2, 0, 1, 0, 0, 1},   /* bass */
                {3, 3, 0, 1, 0, 2, 2, 0, 0, 2, 2, 2, 1, 2, 0, 1},   /* lead (SQ12, BUZZ: too thin and harsh) */
            };
            v[0] = (int16_t)(dice_pick(W[r], 16) * 8u + 4u);
        } else if (wave == 0u) {                          /* STEP: 12.5 25 50 75 %; else 12.5 25 37.5 50 % (12.5: never,
                                                           * its peak is past full scale) */
            static const uint8_t S[3][4] = {{0, 3, 4, 1}, {0, 3, 3, 1}, {0, 4, 3, 1}};
            static const uint8_t P[3][4] = {{0, 3, 2, 4}, {0, 3, 2, 3}, {0, 4, 2, 3}};
            v[0] = (int16_t)(dice_pick(chip == 4u ? S[r] : P[r], 4) * 32u + 16u);
        } else
            v[0] = (int16_t)dice_in(0, 127);              /* (TRI SAW: unused) */
        break;
    case 3:                                               /* CRSH; STEP: the decay DCY (b * 4: 0 CONST, D0..D15, L1..) */
        if (chip == 4u) {
            static const int8_t D[3][2] = {{7, 15}, {7, 16}, {5, 14}};
            uint32_t c = dice_rand() % 100u;
            int32_t b = c < (r == 2u ? 55u : 40u) ? 0 : c < 92u ? dice_in(D[r][0], D[r][1]) : dice_in(19, 26);
            v[0] = (int16_t)(b ? b * 4 + 1 : 0);
        } else {
            uint32_t c = dice_rand() % 100u;
            v[0] = (int16_t)(c < 60u ? dice_in(0, 8) : c < 92u ? dice_in(8, 40) : dice_in(40, 64));
        }
        break;
    case 4: {                                             /* SWEEP (8: 1/16 st a tick, ~86 st a second): a held note would
                                                           * fall to nothing, so only a lead's short STEP decay zaps */
        int32_t b = (int32_t)t->p[P_E3] >> 2;
        v[0] = (int16_t)(r == 2u && chip == 4u && b >= 1 && b <= 8 && dice_chance(35) ? dice_in(8, 24) : 0);
        break;
    }
    case 5: {                                             /* VIB: a lead often, a bass hardly */
        static const uint8_t C[3] = {70, 90, 40};
        static const int8_t S[3][2] = {{6, 20}, {4, 10}, {10, 30}};
        v[0] = (int16_t)(dice_chance(C[r]) ? 0 : dice_in(S[r][0], S[r][1]));
        break;
    }
    case 6: {                                             /* ARP: OFF OCT MAJ MIN (a chord one: a lead only, seldom) */
        static const uint8_t W[3][4] = {{7, 2, 0, 0}, {8, 2, 0, 0}, {8, 2, 1, 1}};
        v[0] = (int16_t)dice_pick(W[r], 4);
        break;
    }
    case 7: {                                             /* TONE */
        static const int8_t C[3][2] = {{70, 120}, {70, 127}, {90, 127}};
        v[0] = (int16_t)dice_in(C[r][0], C[r][1]);
        break;
    }
    default: {                                            /* ENV (STEP: the ADSR only gates, REL the tail) */
        static const uint8_t E[3][5][4] = {
            {{0, 70, 100, 40}, {0, 90, 110, 60}, {20, 90, 100, 70}, {0, 60, 80, 30}, {40, 100, 110, 80}},
            {{0, 50, 110, 12}, {0, 40, 127, 8}, {0, 60, 70, 15}, {0, 45, 90, 10}, {0, 55, 50, 12}},
            {{0, 70, 110, 30}, {0, 60, 127, 40}, {0, 50, 90, 25}, {6, 80, 110, 45}, {0, 40, 70, 20}},
        };
        dice_env(E[r], v);
        break;
    }
    }
}

/* a fresh roll of die d into v[] (its values, in dice_die_ids' order) */
static void dice_fresh(const track_t *t, uint32_t d, int16_t *v)
{
    uint32_t role = dice_role(t);
    if (dice_kit(t)) {
        static const int8_t R[4][5][2] = {                /* PITCH DECAY COLOR SWEEP SHAPE: lowest, highest */
            {{48, 76}, {60, 110}, {0, 60}, {40, 110}, {0, 90}},
            {{50, 80}, {40, 80}, {20, 90}, {10, 60}, {40, 110}},
            {{50, 90}, {20, 50}, {50, 127}, {0, 0}, {0, 127}},
            {{40, 96}, {35, 90}, {20, 110}, {0, 60}, {0, 127}},
        };
        static const int8_t L[4][2] = {{110, 127}, {90, 120}, {70, 105}, {80, 115}};
        uint32_t k;
        if (d < 4u) {
            for (k = 0; k < 5u; k++)
                v[k] = (int16_t)dice_in(R[d][k][0], R[d][k][1]);
        } else if (d == 4u) {
            for (k = 0; k < 4u; k++)
                v[k] = (int16_t)dice_in(L[k][0], L[k][1]);
        } else {
            v[0] = (int16_t)dice_in(30, 110);             /* OPEN */
            v[1] = (int16_t)dice_in(0, 90);               /* CLICK */
            v[2] = (int16_t)(dice_chance(60) ? 0 : dice_in(10, 70));   /* DRV */
            v[3] = (int16_t)dice_in(0, 40);               /* VARI */
        }
        return;
    }
    if (dice_lofi(t)) {
        dice_fresh_lofi(t, d, v, role);
        return;
    }
    switch (d) {
    case 0: {                                             /* RATIO: CYC_RATIO's 16, weighted by role */
        static const uint8_t W[3][16] = {
            /* .5  1  1.5 2 2.5 3 3.5 4  5  6  7  8 1.41 2.76 3.14 5.19 */
            {3, 8, 2, 6, 1, 4, 1, 2, 1, 1, 0, 0, 1, 2, 2, 1},   /* chords / pads */
            {5, 10, 2, 5, 0, 2, 0, 1, 0, 0, 0, 0, 1, 0, 0, 0},  /* bass */
            {2, 6, 2, 6, 1, 4, 1, 3, 1, 1, 1, 0, 1, 1, 1, 0},   /* lead */
        };
        v[0] = (int16_t)dice_pick(W[role - 1u], 16);
        break;
    }
    case 1: {                                             /* COLOR */
        static const int8_t C[3][2] = {{20, 80}, {40, 100}, {50, 110}};
        v[0] = (int16_t)dice_in(C[role - 1u][0], C[role - 1u][1]);
        break;
    }
    case 2: {                                             /* CNTR */
        static const int8_t C[3][2] = {{60, 127}, {20, 80}, {50, 120}};
        v[0] = (int16_t)dice_in(C[role - 1u][0], C[role - 1u][1]);
        break;
    }
    case 3: {                                             /* MSUS */
        static const int8_t C[3][2] = {{20, 90}, {0, 60}, {30, 100}};
        v[0] = (int16_t)dice_in(C[role - 1u][0], C[role - 1u][1]);
        break;
    }
    case 4:                                               /* FDBK: mostly low, a lead more often high */
        v[0] = (int16_t)(dice_chance(role == 3u ? 50 : 70) ? dice_in(0, 30) : dice_in(30, 90));
        break;
    case 5:                                               /* SWEEP: mostly none, else a small blip either way */
        v[0] = (int16_t)(dice_chance(70) ? 0 : (dice_chance(50) ? 1 : -1) * dice_in(2, 12));
        break;
    case 6:                                               /* STACK */
        v[0] = (int16_t)(role == 1u ? (dice_chance(60) ? dice_in(30, 90) : 0) :
                         role == 2u ? (dice_chance(10) ? dice_in(10, 30) : 0) : (dice_chance(40) ? dice_in(15, 50) : 0));
        break;
    case 7:                                               /* SUB */
        v[0] = (int16_t)(role == 2u ? (dice_chance(70) ? dice_in(40, 110) : 0) :
                         role == 1u ? (dice_chance(25) ? dice_in(20, 60) : 0) : (dice_chance(15) ? dice_in(20, 50) : 0));
        break;
    default: {                                            /* ENV: one of the role's shapes, a little varied */
        static const uint8_t E[3][5][4] = {
            {{40, 90, 110, 80}, {60, 100, 100, 90}, {30, 80, 90, 70}, {70, 110, 120, 100}, {0, 84, 52, 46}},
            {{0, 50, 90, 12}, {0, 40, 110, 10}, {0, 60, 40, 15}, {2, 55, 70, 20}, {0, 45, 20, 10}},
            {{0, 70, 100, 30}, {4, 60, 110, 40}, {0, 50, 80, 25}, {10, 80, 100, 45}, {0, 40, 60, 20}},
        };
        dice_env(E[role - 1u], v);
        break;
    }
    }
}

/* the brightest COLOR each RATIO keeps sounding musical at (measured: rondo dice_test.c's brightness, a held C4 at full
 * MSUS, about twice the brightest factory sound): a high ratio with a high index is a harsh buzz */
static const uint8_t DICE_CMAX[16] = {127, 110, 105, 95, 82, 77, 72, 68, 62, 57, 53, 50, 108, 80, 75, 61};
/* after a roll: a COLOR over its RATIO's brightest. The rolled one gives way: COLOR rolled, it comes down; RATIO rolled
 * (COLOR held), RATIO is rolled again among the ones that take that COLOR */
static void dice_tame(track_t *t, uint32_t rolled)
{
    uint32_t r = (uint32_t)t->p[P_E0] & 15u, i;
    if (t->p[P_E1] <= DICE_CMAX[r])
        return;
    if ((rolled >> 1) & 1u) {
        t->p[P_E1] = DICE_CMAX[r];
        return;
    }
    if (rolled & 1u) {
        uint8_t w[16];
        for (i = 0; i < 16u; i++)
            w[i] = (uint8_t)(DICE_CMAX[i] >= t->p[P_E1] ? (i == 1u ? 4u : i == 3u || i == 0u ? 2u : 1u) : 0u);
        t->p[P_E0] = (int16_t)dice_pick(w, 16);
    }
}

static void dice_snapshot(const track_t *t, int16_t *out)
{
    uint8_t ids[DICE_VALS];
    uint32_t n = dice_ids(t, ids), i;
    for (i = 0; i < n; i++)
        out[i] = t->p[ids[i]];
}
static void dice_restore(track_t *t, const int16_t *in)
{
    uint8_t ids[DICE_VALS];
    uint32_t n = dice_ids(t, ids), i;
    fm1_irq_off();
    for (i = 0; i < n; i++)
        t->p[ids[i]] = in[i];
    fm1_irq_on();
}
/* the sound as it is into the history (newest first, eight kept); a history of another engine starts over */
static void dice_push(track_t *t)
{
    uint32_t k = trk_index(t) % NTRK, i;
    if (dice.eng[k] != t->eng_req % NENGINES + 1u) {
        dice.eng[k] = (uint8_t)(t->eng_req % NENGINES + 1u);
        dice.hn[k] = 0;
    }
    for (i = DICE_HIST - 1u; i > 0u; i--)
        memcpy(dice.hist[k][i], dice.hist[k][i - 1u], sizeof dice.hist[k][i]);
    dice_snapshot(t, dice.hist[k][0]);
    if (dice.hn[k] < DICE_HIST)
        dice.hn[k]++;
    dice.hpos[k] = 0;
}

/* OCT+: every die not held rolls (WILD: how far towards its new roll). One undo (SAVE held) */
static void dice_roll(void)
{
    track_t *t = TSEL;
    uint32_t k = song.sel % NTRK, d, j, w = dice_wild(k), n = dice_count(t);
    if (!dice_engine_ok(t))
        return;
    if ((dice.hold[k] & ((1u << n) - 1u)) == (1u << n) - 1u) {
        ui_message("ALL DICE HELD");
        return;
    }
    if (!dice.rng)
        dice.rng = 0x9E3779B9u ^ (fm1_ms * 2654435761u);
    dice_push(t);
    load_begin(t, UNDO_SOUND);
    fm1_irq_off();
    for (d = 0; d < n; d++) {
        uint8_t ids[5];
        int16_t v[5];
        uint32_t m = dice_die_ids(t, d, ids);
        if ((dice.hold[k] >> d) & 1u)
            continue;
        dice_fresh(t, d, v);
        for (j = 0; j < m; j++) {
            int32_t cur = t->p[ids[j]];
            if (dice_discrete(t, d))
                t->p[ids[j]] = (int16_t)(dice_rand() % 100u < w ? v[j] : cur);
            else
                t->p[ids[j]] = (int16_t)(cur + (v[j] - cur) * (int32_t)w / 100);
        }
    }
    if (t->eng_req % NENGINES == ENGI_CTONE && w) {       /* (what the roll changed: a harsh pair gives way) */
        uint32_t rolled = ~(uint32_t)dice.hold[k];
        dice_tame(t, rolled);
        if (t->p[P_E1] > 95 && t->p[P_E4] > 40 && ((rolled >> 4) & 1u))   /* (a bright index, much feedback) */
            t->p[P_E4] = 40;
    }
    fm1_irq_on();
    t->user = 0;
    load_end(t);
    dice.anim = 8;
    ui_message("ROLLED");
}

/* KNOB 4 / OCT-: HIST, the sound before the last roll (1), the one before that (2) .. ; 0: back to the newest */
static void dice_hist(int32_t s)
{
    track_t *t = TSEL;
    uint32_t k = song.sel % NTRK;
    int32_t p;
    if (dice.eng[k] != t->eng_req % NENGINES + 1u || !dice.hn[k]) {
        ui_message("NO ROLLS YET");
        return;
    }
    if (!dice.hpos[k]) {                                  /* leaving the newest: keep it, as HIST 0 (once) */
        int16_t now[DICE_VALS];
        dice_snapshot(t, now);
        if (memcmp(now, dice.hist[k][0], sizeof now))
            dice_push(t);
        dice.hpos[k] = 0;
    }
    p = clamp((int32_t)dice.hpos[k] + s, 0, (int32_t)dice.hn[k] - 1);
    if ((uint32_t)p == dice.hpos[k])
        return;
    dice.hpos[k] = (uint8_t)p;
    load_begin(t, UNDO_SOUND);
    dice_restore(t, dice.hist[k][p]);
    load_end(t);
}

static void dice_edit(uint32_t slot, int32_t s)
{
    uint32_t k = song.sel % NTRK, n = dice_count(TSEL);
    if (!dice_engine_ok(TSEL))
        return;
    if (slot == 0u)
        dice.cur[k] = (uint8_t)clamp((int32_t)(dice.cur[k] % n) + (s > 0 ? 1 : -1), 0, (int32_t)n - 1);
    else if (slot == 1u)
        dice.hold[k] = (uint16_t)(s > 0 ? dice.hold[k] | 1u << (dice.cur[k] % n) : dice.hold[k] & ~(1u << (dice.cur[k] % n)));
    else if (slot == 2u)
        dice.wild[k] = (uint8_t)(clamp((int32_t)dice_wild(k) + accel(EN_K3, s, 100), 0, 100) + 1);
    else
        dice_hist(s);
}

/* a die's value as text (b holds 12): one value its formatted value; ENV its kind; a kit's die its first value */
static void dice_text(const track_t *t, uint32_t d, char *b)
{
    uint8_t ids[5];
    uint32_t m = dice_die_ids(t, d, ids);
    const char *unit;
    b[0] = 0;
    if (m == 1u) {
        param_format(track_desc(t, ids[0]), t->p[ids[0]], b, &unit);
        return;
    }
    if (!dice_kit(t)) {                                   /* ENV */
        const int16_t *p = t->p;
        str_cpy(b, p[P_ATK] > 20 ? "SWELL" : p[P_SUS] < 50 ? "PLUCK" : p[P_REL] > 60 ? "LONG" : "HOLD", 12);
        return;
    }
    fmt_int(b, t->p[ids[0]]);                             /* (a kit's die: its first value, PITCH / KICK / OPEN) */
}

/* the face a die shows (1..6): its first value's place in its range */
static uint32_t dice_face(const track_t *t, uint32_t d)
{
    uint8_t ids[5];
    const param_desc_t *dd;
    int32_t span;
    (void)dice_die_ids(t, d, ids);
    dd = track_desc(t, ids[0]);
    span = dd->max - dd->min;
    return span > 0 ? 1u + (uint32_t)((t->p[ids[0]] - dd->min) * 6 / (span + 1)) : 1u;
}
