/* SPDX-License-Identifier: GPL-3.0-only */
/* RONDO: SOUND MORPH (EDIT > MORPH). Each track blends its sound (A: what it is, every edit and load included) toward
 * a TARGET (B: a preset of the same engine, as apply_preset_to would load it) by AMOUNT 0..100 %. Only what glides
 * blends (morph_ok: a sound parameter motion could automate, not a list or a switch, not the mix): the engine, its
 * lists and switches, an FM6 patch's operators and a kit's choice stay A's.
 *   the tracker's MORPH command (CMD_MORPH, a step record) sets AMOUNT from its step on: a pattern of the bank whose
 *     first step says MORPH 100 plays its track as the target, so the chorus can play the bright lead and the verse
 *     the soft one with four sounds
 *   A is kept as it is edited: a value that is not the one the morph wrote was changed by hand or by a load, and
 *     becomes A's (morph_poll); a parameter the pattern's automation is moving is left to it
 *   saved: AMOUNT and TARGET in the project (project.c PROJ_MORPH_OFF), the sound as A (morph_base)
 * The ISR applies a MORPH command at once (before its step's notes); the main loop the knobs, the loads, the edits */
#if RONDO_LAYOUT
#define MO_MAX 100
static struct {
    uint8_t amt[NTRK];                       /* AMOUNT 0..100 (the knob, the MORPH command) */
    uint8_t tgt[NTRK];                       /* TARGET: the engine's preset + 1, 0 none */
    uint8_t eng[NTRK];                       /* the engine b was made for (another: made again) */
    uint8_t on[NTRK];                        /* a[] / w[] hold this track's values (amount above 0 once) */
    uint8_t shown[NTRK];                     /* the amount w[] was written for */
    volatile uint8_t req;                    /* bit k: track k to write (the main loop, or the ISR's command) */
    int16_t a[NTRK][P_COUNT];                /* A: the sound */
    int16_t b[NTRK][P_COUNT];                /* B: the target's values */
    int16_t w[NTRK][P_COUNT];                /* what the morph last wrote */
} mo __attribute__((section(".pool")));
static track_t mo_tmp __attribute__((section(".pool")));   /* (the target made on a copy: main loop only) */

static const param_desc_t *track_desc(const track_t *t, uint32_t id);
static int param_kept(uint32_t i);
static uint32_t preset_orig(const engine_t *e, uint32_t k);
/* id of track t blends: a sound value that glides */
static int morph_ok(const track_t *t, uint32_t id)
{
    const param_desc_t *d;
    if (id >= P_COUNT || !motion_param(id) || id == P_LEVEL || id == P_PAN || id == P_MUTE)
        return 0;
    d = track_desc(t, id);
    return !d->names && d->fmt != F_ENUM && d->fmt != F_ONOFF && d->fmt != F_OCT && d->fmt != F_STEPS &&
           d->fmt != F_NOTE && d->max > d->min;
}
/* the presets track t's engine has (TARGET's range: 0 none, 1..n) */
static uint32_t morph_npresets(const track_t *t)
{
    const engine_t *e = ENGINES[t->eng_req % NENGINES];
#if !FELUCCA_FM4
    if (t->eng_req % NENGINES == ENGI_DIGITAL)
        return 0;
#endif
    return e->npresets;
}
/* the TARGET's name ("" none) */
static const char *morph_tname(const track_t *t)
{
    uint32_t k = trk_index(t), n = morph_npresets(t);
    if (!mo.tgt[k] || mo.tgt[k] > n)
        return "";
    return ENGINES[t->eng_req % NENGINES]->presets[preset_orig(ENGINES[t->eng_req % NENGINES], mo.tgt[k] - 1u)].name;
}
/* B: the preset's values as apply_preset_to would set them, the rest A's (main loop) */
static void morph_make(uint32_t k)
{
    const track_t *t = &trk[k];
    const engine_t *e = ENGINES[t->eng_req % NENGINES];
    uint32_t i, pi;
    mo.eng[k] = t->eng_req;
    memcpy(mo.b[k], mo.on[k] ? mo.a[k] : t->p, sizeof mo.b[k]);
    if (!mo.tgt[k] || mo.tgt[k] > morph_npresets(t))
        return;
    pi = preset_orig(e, (mo.tgt[k] - 1u) % e->npresets);
    for (i = 0; i < P_E0; i++)
        if (!param_kept(i))
            mo.b[k][i] = TP[i].def;
    for (i = 0; i < 8u; i++)
        mo.b[k][P_E0 + i] = (int16_t)e->presets[pi].e[i];
    mo.b[k][P_ATK] = e->presets[pi].env[0];
    mo.b[k][P_DEC] = e->presets[pi].env[1];
    mo.b[k][P_SUS] = e->presets[pi].env[2];
    mo.b[k][P_REL] = e->presets[pi].env[3];
    mo.b[k][P_ED_FLT] = e->presets[pi].fenv;
    {
        static const uint8_t FX_DEF[4] = {0, 24, 28, 36};
        for (i = 0; i < 4u; i++)
            mo.b[k][P_DIST + i] = (int16_t)(e->presets[pi].fx[i] ? e->presets[pi].fx[i] - 1 : FX_DEF[i]);
    }
    if (t->eng_req % NENGINES == ENGI_CKIT) {          /* a kit's voices (eng_cycle.c ck_kit_load, on a copy) */
        memcpy(mo_tmp.p, mo.b[k], sizeof mo_tmp.p);
        ck_kit_load(&mo_tmp, pi);
        memcpy(mo.b[k], mo_tmp.p, sizeof mo.b[k]);
    }
}
/* track k's values for its amount now -> p (the ISR's command, or the main loop with the interrupts off). Amount 0:
 * A back, and the morph lets go of the track */
static __attribute__((noinline)) void morph_apply(uint32_t k)
{
    track_t *t = &trk[k];
    uint32_t id, amt = mo.amt[k] <= MO_MAX ? mo.amt[k] : MO_MAX;
    if (!mo.on[k]) {
        if (!amt)
            return;
        memcpy(mo.a[k], t->p, sizeof mo.a[k]);
        memcpy(mo.w[k], t->p, sizeof mo.w[k]);
        mo.on[k] = 1;
    }
    for (id = 0; id < P_COUNT; id++) {
        int32_t v;
        if (!morph_ok(t, id) || ((motion_active[k][id / 32u] >> (id % 32u)) & 1u))
            continue;
        if (t->p[id] != mo.w[k][id])                    /* (changed since: an edit, A's now) */
            mo.a[k][id] = t->p[id];
        v = mo.a[k][id] + ((int32_t)(mo.b[k][id] - mo.a[k][id]) * (int32_t)amt + (mo.b[k][id] >= mo.a[k][id] ? 50 : -50)) / MO_MAX;
        t->p[id] = mo.w[k][id] = (int16_t)v;
    }
    mo.shown[k] = (uint8_t)amt;
    if (!amt)
        mo.on[k] = 0;
}
/* ISR: step `step` of track t starts: its MORPH command sets the amount (at once, before its notes) */
static int ph_cmd(const track_t *t, uint32_t row, uint32_t id, int32_t *v);
static void morph_step(track_t *t, uint32_t step)
{
    int32_t v;
    uint32_t k = trk_index(t);
    if (!ph_cmd(t, step, CMD_MORPH, &v))
        return;
    v = clamp(v, 0, MO_MAX);
    if ((uint32_t)v == mo.amt[k] && mo.shown[k] == (uint8_t)v)
        return;
    mo.amt[k] = (uint8_t)v;
    if (mo.eng[k] != t->eng_req)
        mo.req |= (uint8_t)(1u << k);                   /* (B for another engine: the main loop makes it first) */
    else
        morph_apply(k);
}
/* main loop, every pass: another engine (B made again, A the new sound), the knob, edits while it blends */
static void morph_poll(void)
{
    uint32_t k;
    for (k = 0; k < NTRK; k++) {
        track_t *t = &trk[k];
        uint32_t f;
        if (mo.eng[k] != t->eng_req) {
            if (mo.on[k]) {                             /* (a sound load of another engine: A is that sound) */
                f = motion_guard();
                mo.on[k] = 0;
                motion_unguard(f);
            }
            if (mo.tgt[k] > morph_npresets(t))
                mo.tgt[k] = 0;
            morph_make(k);
            mo.req |= (uint8_t)(1u << k);
        }
        if (!mo.on[k] && !mo.amt[k] && !((mo.req >> k) & 1u))
            continue;
        f = motion_guard();
        mo.req &= (uint8_t)~(1u << k);
        morph_apply(k);                                 /* (the edits since become A's) */
        motion_unguard(f);
    }
}
/* the knobs (EDIT > MORPH): TARGET (another preset: B made again), AMOUNT */
static void morph_set(uint32_t k, uint32_t tgt, uint32_t amt)
{
    k %= NTRK;
    if (tgt != mo.tgt[k]) {
        mo.tgt[k] = (uint8_t)tgt;
        morph_make(k);
    }
    mo.amt[k] = (uint8_t)(amt <= MO_MAX ? amt : MO_MAX);
    mo.req |= (uint8_t)(1u << k);
}
/* the sound as a project saves it: A's value where the morph is blending */
static int16_t morph_base(const track_t *t, uint32_t id)
{
    uint32_t k = trk_index(t);
    if (mo.on[k] && id < P_COUNT && morph_ok(t, id) && t->p[id] == mo.w[k][id] &&
        !((motion_active[k][id / 32u] >> (id % 32u)) & 1u))
        return mo.a[k][id];
    return motion_base_value(t, id);
}
/* B's value of id and how far A..B the sound is (the page) */
static int morph_differs(const track_t *t, uint32_t id) { uint32_t k = trk_index(t); return morph_ok(t, id) && mo.b[k][id] != (mo.on[k] ? mo.a[k][id] : t->p[id]); }
/* power-on, a project loaded (its amounts and targets; 0 0: none) */
static void morph_load(const uint8_t *amt, const uint8_t *tgt)
{
    uint32_t k;
    for (k = 0; k < NTRK; k++) {
        mo.on[k] = 0;
        mo.amt[k] = (uint8_t)(amt[k] <= MO_MAX ? amt[k] : 0u);
        mo.tgt[k] = tgt[k];
        mo.eng[k] = 0xFFu;                              /* (morph_poll makes B, then writes the amount) */
        mo.shown[k] = 0;
    }
}
static void morph_reset(void)
{
    static const uint8_t z[NTRK] = {0, 0, 0, 0};
    memset(&mo, 0, sizeof mo);
    morph_load(z, z);
}
#endif
