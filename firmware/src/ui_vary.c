/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright (C) 2026 RONDO contributors. Built on Felucca (C) 2026 Leo Kuroshita (@kurogedelic), Hügelton Instruments */
/* RONDO's SEQ > VARY: sequence dice. DICE's way (EDIT > DICE) on the selected track's pattern: OCT+ rolls a
 * variation of it, the dice not held rolling again, so a variation you like can be kept part by part. Six dice, each
 * one kind of change; every die decides step by step from its own seed (vary_h: the seed and the step), so a die held
 * makes the same choices on the same steps whatever the others roll:
 *   HITS    hits added on empty steps (a melodic track: the note before them; a kit: one of the drums it plays, hats
 *           likeliest) and a few off-beat ones dropped
 *   NOTES   a melodic track: notes moved one to three semitones or an octave, onto the track's scale (SCL); a kit:
 *           drums swapped for their kin (closed / open hat, snare / clap, the toms rim bell round)
 *   SHIFT   off-beat hits moved a step early or late, onto an empty step (syncopation)
 *   RATCH   ratchets x2 / x3 (more in the pattern's last quarter: fills)
 *   ACC     accents on and off, velocities up and down
 *   PROB    a chance (50 .. 90 %) or a 1:2 / 2:2 on off-beat steps
 * WILD (KNOB 3) is how much each die does (default 50 %). Every roll starts from the ORIGINAL (the pattern as it was at
 * the first roll: vary.base), never from the last roll, and writes the result into the pattern (one undo, SAVE held).
 * The pattern changed by other means since (the tracker, a tool, a load): the next roll takes it as the new original.
 * KNOB 4 HIST / OCT-: the last eight rolls, newest first, then ORIG (the original back). The automation and the commands of the
 * steps stay where they are. Not while a SONG plays (its patterns are the slots'). */

#define VARY_N 6u
#define VARY_HIST 8u                                      /* the rolls kept (ORIG past them, always) */
static const char *const VARY_NAME[VARY_N] = {"HITS", "NOTES", "SHIFT", "RATCH", "ACC", "PROB"};
typedef struct {
    uint32_t seed[VARY_N];
    uint8_t wild, orig;                                   /* orig: the original itself */
} vary_roll_t;
static struct {
    uint32_t seed[NTRK][VARY_N];                          /* the dice now */
    uint8_t hold[NTRK], cur[NTRK], wild[NTRK];            /* bit d held; the cursor; WILD (0 = 50 %) */
    uint8_t ok[NTRK], hn[NTRK], hpos[NTRK];               /* an original kept; rolls kept; shown (hn: ORIG) */
    uint8_t cnt[NTRK][VARY_N];                            /* what each die did in the pattern shown */
    uint32_t sig[NTRK];                                   /* the pattern as VARY last wrote it (another: a new original) */
    vary_roll_t hist[NTRK][VARY_HIST];
    uint32_t rng;
    uint8_t anim;
} vary;
static step_t vary_base[NTRK][NSTEP] __attribute__((section(".pool")));   /* the originals */

static uint32_t vary_wild(uint32_t k) { return vary.wild[k] ? vary.wild[k] - 1u : 50u; }
static uint32_t vary_pat_sig(const track_t *t)
{
    const uint8_t *b = (const uint8_t *)t->step;
    uint32_t h = 2166136261u, i;
    for (i = 0; i < sizeof t->step; i++)
        h = (h ^ b[i]) * 16777619u;
    return h ^ (uint32_t)t->p[P_SLEN] * 2654435761u;
}
static uint32_t vary_h(uint32_t seed, uint32_t step, uint32_t salt)   /* a die's choice on a step: 32 bits */
{
    uint32_t x = seed ^ (step + 1u) * 0x9E3779B9u ^ salt * 0x85EBCA6Bu;
    x ^= x >> 16;
    x *= 0x7FEB352Du;
    x ^= x >> 15;
    x *= 0x846CA68Bu;
    x ^= x >> 16;
    return x;
}
static int vary_pct(uint32_t h, uint32_t pct10) { return h % 1000u < pct10; }   /* (tenths of a percent) */
static int vary_full(const step_t *s) { return s->time == ST_NOTE && (s->n || s->hit); }
static int vary_free(const step_t *s) { return !vary_full(s) && s->time != ST_TIE; }

/* the original with the dice applied, into out (NSTEP steps); counts what each die did */
static void vary_make(const track_t *t, const vary_roll_t *r, step_t *out, uint8_t *cnt)
{
    uint32_t k = trk_index(t) % NTRK, len = (uint32_t)clamp(t->p[P_SLEN], 1, NSTEP), i, w = r->wild, kit = drum_track(t);
    uint32_t used = 0, lastn = 60, d;
    memcpy(out, vary_base[k], sizeof vary_base[k]);
    for (d = 0; d < VARY_N; d++)
        cnt[d] = 0;
    if (r->orig)
        return;
    for (i = 0; i < len; i++)
        used |= vary_base[k][i].hit;
    for (i = 0; i < len; i++)                             /* HITS */
    {
        step_t *s = &out[i];
        uint32_t h = vary_h(r->seed[0], i, 1);
        if (vary_full(s)) {
            if (s->n)
                lastn = s->note[0];
            if (i % 4u && (i + 1u >= len || out[i + 1u].time != ST_TIE) && vary_pct(h >> 10, w * 2u)) {
                step_clear(s);
                cnt[0]++;
            }
        } else if (vary_free(s) && vary_pct(h, w * 5u)) {
            if (kit) {
                uint32_t pool[NLANE], np = 0, l;
                for (l = 0; l < NLANE; l++)
                    if ((used >> l) & 1u) {
                        pool[np++] = l;
                        if (l == 3u)
                            pool[np++] = l;               /* (a closed hat twice as likely) */
                    }
                if (!np)
                    continue;
                step_clear(s);
                s->time = ST_NOTE;
                s->hit = (uint8_t)(1u << pool[(h >> 12) % np]);
            } else {
                step_clear(s);
                s->time = ST_NOTE;
                s->n = 1;
                s->note[0] = (uint8_t)lastn;
                s->vel = 90;
            }
            cnt[0]++;
        }
    }
    for (i = 0; i < len; i++) {                           /* NOTES */
        step_t *s = &out[i];
        uint32_t h = vary_h(r->seed[1], i, 2), j;
        if (!vary_full(s) || !vary_pct(h, w * 5u))
            continue;
        if (kit) {
            static const uint8_t KIN[NLANE] = {0, 2, 1, 4, 3, 6, 7, 5};   /* kick stays; SD<>CP, CH<>OH, TM>RS>CB>TM */
            uint8_t hit = 0, acc = 0;
            uint32_t l;
            for (l = 0; l < NLANE; l++)
                if ((s->hit >> l) & 1u) {
                    uint32_t to = (h >> (8u + l)) & 1u ? KIN[l] : l;
                    hit |= (uint8_t)(1u << to);
                    acc |= (uint8_t)(((s->acc >> l) & 1u) << to);
                }
            if (hit != s->hit) {
                s->hit = hit;
                s->acc = acc;
                cnt[1]++;
            }
        } else if (s->n) {
            static const int8_t STEP[8] = {1, -1, 2, -2, 3, -3, 12, -12};
            int32_t dlt = STEP[(h >> 10) & 7u];
            for (j = 0; j < s->n && j < 4u; j++)
                s->note[j] = (uint8_t)clamp(scale_snap(t, (int32_t)s->note[j] + dlt), 24, 108);
            cnt[1]++;
        }
    }
    for (i = 1; i + 1u < len; i++) {                      /* SHIFT */
        uint32_t h = vary_h(r->seed[2], i, 3), to;
        step_t tmp;
        if (!(i % 4u) || !vary_full(&out[i]) || out[i + 1u].time == ST_TIE || !vary_pct(h, w * 3u))
            continue;
        to = (h >> 12) & 1u ? i + 1u : i - 1u;
        if (to % 4u == 0u && (h >> 13) & 1u)
            continue;                                     /* (onto a beat: rarely) */
        if (!vary_free(&out[to]))
            continue;
        tmp = out[to];
        out[to] = out[i];
        out[i] = tmp;
        cnt[2]++;
        i++;                                              /* (not moved twice) */
    }
    for (i = 0; i < len; i++) {                           /* RATCH */
        step_t *s = &out[i];
        uint32_t h = vary_h(r->seed[3], i, 4), p = w * (i * 4u >= len * 3u ? 6u : 2u);
        if (!vary_full(s) || !vary_pct(h, p))
            continue;
        step_set_ratchet(s, (h >> 12) % 3u == 0u ? 3u : 2u);
        cnt[3]++;
    }
    for (i = 0; i < len; i++) {                           /* ACCENT */
        step_t *s = &out[i];
        uint32_t h = vary_h(r->seed[4], i, 5);
        if (!vary_full(s) || !vary_pct(h, w * 4u))
            continue;
        if ((h >> 10) & 1u) {
            if (kit)
                s->acc ^= s->hit;
            else
                s->flags ^= SF_ACCENT;
        } else {
            int32_t v = s->vel ? s->vel : 96;
            s->vel = (uint8_t)clamp(v + ((h >> 11) & 1u ? 1 : -1) * (int32_t)(10u + (h >> 12) % 25u), 30, 127);
        }
        cnt[4]++;
    }
    for (i = 0; i < len; i++) {                           /* CHANCE */
        step_t *s = &out[i];
        uint32_t h = vary_h(r->seed[5], i, 6);
        if (!(i % 4u) || !vary_full(s) || !vary_pct(h, w * 3u))
            continue;
        if ((h >> 10) % 4u == 0u)
            step_set_chance_v(s, 101u + ((h >> 12) & 1u));  /* 1:2 or 2:2 */
        else
            step_set_chance(s, 50u + (h >> 12) % 41u);
        cnt[5]++;
    }
}

/* the roll r into the track's pattern (one undo) */
static void vary_write(track_t *t, const vary_roll_t *r)
{
    static step_t out[NSTEP];
    uint32_t k = trk_index(t) % NTRK;
    vary_make(t, r, out, vary.cnt[k]);
    step_undo_take(t, 0);
    memcpy(t->step, out, sizeof t->step);
    motion_undo_done(t);
    vary.sig[k] = vary_pat_sig(t);
}
static void vary_push(uint32_t k, const vary_roll_t *r)    /* newest first */
{
    uint32_t i;
    for (i = VARY_HIST - 1u; i > 0u; i--)
        vary.hist[k][i] = vary.hist[k][i - 1u];
    vary.hist[k][0] = *r;
    if (vary.hn[k] < VARY_HIST)
        vary.hn[k]++;
    vary.hpos[k] = 0;
}
static uint32_t vary_rand(void)
{
    uint32_t s = vary.rng ? vary.rng : 0x6A09E667u ^ fm1_ms * 2654435761u;
    s ^= s << 13;
    s ^= s >> 17;
    s ^= s << 5;
    vary.rng = s;
    return s;
}
static int vary_page_ok(void)
{
    if (chain_busy()) {
        ui_message("STOP TO EDIT");
        return 0;
    }
    return 1;
}

/* OCT+: the dice not held roll; the result written (the original: kept from the first roll, or anew) */
static void vary_roll(void)
{
    track_t *t = TSEL;
    uint32_t k = song.sel % NTRK, d;
    vary_roll_t r;
    if (!vary_page_ok())
        return;
    if ((vary.hold[k] & ((1u << VARY_N) - 1u)) == (1u << VARY_N) - 1u) {
        ui_message("ALL DICE HELD");
        return;
    }
    if (seq_is_empty(t)) {
        ui_message("NOTHING TO VARY");
        return;
    }
    if (!vary.ok[k] || vary.sig[k] != vary_pat_sig(t)) {  /* a new original: the pattern as it is */
        memcpy(vary_base[k], t->step, sizeof vary_base[k]);
        vary.ok[k] = 1;
        vary.hn[k] = 0;
    }
    for (d = 0; d < VARY_N; d++)
        if (!((vary.hold[k] >> d) & 1u) || !vary.seed[k][d])
            vary.seed[k][d] = vary_rand() | 1u;
    memcpy(r.seed, vary.seed[k], sizeof r.seed);
    r.wild = (uint8_t)vary_wild(k);
    r.orig = 0;
    vary_write(t, &r);
    vary_push(k, &r);
    vary.anim = 8;
    ui_message("VARIED");
}

/* KNOB 4 / OCT-: HIST, the roll before (1), the one before that (2) .. down to ORIG (past the oldest); 0 the newest */
static void vary_hist(int32_t s)
{
    track_t *t = TSEL;
    uint32_t k = song.sel % NTRK;
    int32_t p;
    vary_roll_t o;
    if (!vary.ok[k] || !vary.hn[k]) {
        ui_message("NO ROLLS YET");
        return;
    }
    if (vary.sig[k] != vary_pat_sig(t)) {                 /* (changed since: that is the original now) */
        vary.ok[k] = 0;
        ui_message("PATTERN CHANGED");
        return;
    }
    if (!vary_page_ok())
        return;
    p = clamp((int32_t)vary.hpos[k] + s, 0, (int32_t)vary.hn[k]);
    if ((uint32_t)p == vary.hpos[k])
        return;
    vary.hpos[k] = (uint8_t)p;
    if ((uint32_t)p == vary.hn[k]) {                      /* ORIG */
        memset(&o, 0, sizeof o);
        o.orig = 1;
        vary_write(t, &o);
        ui_message("ORIGINAL");
        return;
    }
    vary_write(t, &vary.hist[k][p]);
    memcpy(vary.seed[k], vary.hist[k][p].seed, sizeof vary.seed[k]);   /* (the dice show that roll's: HOLD keeps them) */
    ui_message("EARLIER ROLL");
}

static void vary_edit(uint32_t slot, int32_t s)
{
    uint32_t k = song.sel % NTRK;
    if (slot == 0u)
        vary.cur[k] = (uint8_t)clamp((int32_t)(vary.cur[k] % VARY_N) + (s > 0 ? 1 : -1), 0, (int32_t)VARY_N - 1);
    else if (slot == 1u)
        vary.hold[k] = (uint8_t)(s > 0 ? vary.hold[k] | 1u << (vary.cur[k] % VARY_N) : vary.hold[k] & ~(1u << (vary.cur[k] % VARY_N)));
    else if (slot == 2u)
        vary.wild[k] = (uint8_t)(clamp((int32_t)vary_wild(k) + accel(EN_K3, s, 100), 0, 100) + 1);
    else
        vary_hist(s);
}
/* the pattern shown is a roll of VARY's (not the original, not changed since) */
static int vary_shown(uint32_t k) { return vary.ok[k] && vary.sig[k] == vary_pat_sig(&trk[k]) && vary.hpos[k] < vary.hn[k]; }
