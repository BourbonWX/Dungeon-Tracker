/* SPDX-License-Identifier: GPL-3.0-only
 * RONDO: SOUND MORPH (src/morph.c, EDIT > MORPH, the TRACKER's MORPH command), through ui_test.c's harness.
 *   build/host/morph_test             (tests/run_rondo.sh)
 * 1. EDIT > MORPH on T4: KNOB 1 picks a preset of its engine, KNOB 2 the amount: 100 % plays the target's values, 50 %
 *    half way, 0 % the sound as it was; lists and switches never blend.
 * 2. an edit while it blends becomes the sound's (A): back at 0 % the edited value.
 * 3. a parameter the automation moves is left to it.
 * 4. saved: the project holds the sound (A), the amount and the target; loaded back it blends the same.
 * 5. the MORPH command: a step's M100 sets the amount from that step, before its notes; the PATTERN BANK's B with
 *    M100 on step 1 and A with M0: switching patterns switches the sound.
 * 6. a sound load of another engine: A is the new sound, a target past its presets goes.
 * 7. the page drawn, every palette. */
#define UI_TEST_NO_MAIN 1
#include "ui_test.c"

static int fails9;
static void ok9(const char *what, int ok) { fails9 += check(what, ok); }
static char what[240];

static uint32_t best_target(const track_t *t)          /* the preset that differs most from the sound */
{
    uint32_t k = trk_index(t), p, id, best = 1, most = 0;
    for (p = 1; p <= morph_npresets(t); p++) {
        uint32_t n = 0;
        morph_set(k, p, 0);
        morph_poll();
        for (id = 0; id < P_COUNT; id++)
            n += (uint32_t)morph_differs(t, id);
        if (n > most) { most = n; best = p; }
    }
    return best;
}
static int at(const track_t *t, uint32_t pct)          /* every blending value where pct % puts it */
{
    uint32_t k = trk_index(t), id;
    for (id = 0; id < P_COUNT; id++) {
        int32_t a = mo.a[k][id], b = mo.b[k][id], want;
        if (!morph_ok(t, id))
            continue;
        want = a + ((b - a) * (int32_t)pct + (b >= a ? 50 : -50)) / 100;
        if (t->p[id] != want)
            return 0;
    }
    return 1;
}
static void step1(track_t *t, uint8_t note)
{
    track_defaults_steps(t);
    t->p[P_SLEN] = 4;
    t->step[0].time = ST_NOTE;
    t->step[0].n = 1;
    t->step[0].note[0] = note;
    t->step[0].vel = 100;
}

int main(void)
{
    track_t *t = &trk[3];
    static int16_t orig[P_COUNT], kept[P_COUNT];
    uint32_t tg, id, cut = P_COUNT, sw = P_COUNT;

    /* 1. */
    ui_power_on();
    track_select(3);
    go_page(GR_MORPH);
    frames(50);
    ok9("EDIT > MORPH on T4", cur_page()->graph == GR_MORPH && morph_npresets(t) > 1u);
    memcpy(orig, t->p, sizeof orig);
    tg = best_target(t);
    morph_set(3, 0, 0);
    morph_poll();
    turn(EN_K1, (int32_t)tg);
    frames(50);
    for (id = 0; id < P_COUNT; id++) {
        if (morph_differs(t, id) && cut == P_COUNT) cut = id;
        if (!morph_ok(t, id) && id != P_LEVEL && mo.b[3][id] != t->p[id] && sw == P_COUNT) sw = id;
    }
    {
        uint32_t nd = 0;
        for (id = 0; id < P_COUNT; id++) nd += (uint32_t)morph_differs(t, id);
        snprintf(what, sizeof what, "KNOB 1: target P%u (%s), %u parameters differ", mo.tgt[3], morph_tname(t), nd);
    }
    ok9(what, mo.tgt[3] == tg && cut < P_COUNT && !memcmp(orig, t->p, sizeof orig));
    turn(EN_K2, 100);
    frames(50);
    ok9("  KNOB 2 to 100 %: the target's values", mo.amt[3] == 100u && at(t, 100) && t->p[cut] == mo.b[3][cut]);
    morph_set(3, tg, 50);
    frames(20);
    ok9("  50 %: half way", at(t, 50));
    {
        int same = 1;
        for (id = 0; id < P_COUNT; id++)
            if (!morph_ok(t, id) && t->p[id] != orig[id]) same = 0;
        ok9("  lists, switches and the mix never blend", same);
    }
    morph_set(3, tg, 0);
    frames(20);
    ok9("  0 %: the sound as it was", !memcmp(orig, t->p, sizeof orig) && !mo.on[3]);

    /* 2. an edit while it blends */
    morph_set(3, tg, 100);
    frames(20);
    t->p[cut] = (int16_t)(mo.b[3][cut] == track_desc(t, cut)->min ? track_desc(t, cut)->min + 1 : track_desc(t, cut)->min);
    {
        int16_t e = t->p[cut];
        frames(20);
        ok9("an edit at 100 %: kept (A's now)", mo.a[3][cut] == e && t->p[cut] == mo.b[3][cut]);
        morph_set(3, tg, 0);
        frames(20);
        ok9("  back at 0 %: the edited value", t->p[cut] == e);
        orig[cut] = e;
    }

    /* 3. automation */
    morph_set(3, tg, 0);
    frames(20);
    seq_start();
    song.playing = 1;
    motion_set_event(t, 0, cut, 20);
    t->step[0].time = ST_NOTE;
    events_block(CTL);
    morph_set(3, tg, 100);
    frames(20);
    ok9("a parameter the automation moves: left to it", ((motion_active[3][cut / 32u] >> (cut % 32u)) & 1u) &&
        t->p[cut] != mo.b[3][cut]);
    song.playing = 0;
    seq_stop();
    motion_clear(t);
    frames(20);

    /* 4. saved */
    morph_set(3, tg, 70);
    frames(20);
    memcpy(kept, t->p, sizeof kept);
    project_capture(&proj_scratch);
    ok9("saved: the sound (A), amount 70, the target", proj_scratch.mo_amt[3] == 70u && proj_scratch.mo_tgt[3] == tg &&
        proj_scratch.t[3].p[cut] == mo.a[3][cut]);
    {
        project_store_t st;
        project_t back;
        ok9("  packed and read back the same", proj_pack(&st, &proj_scratch) && proj_import(&back, &st, sizeof st) &&
            back.mo_amt[3] == 70u && back.mo_tgt[3] == tg && back.t[3].p[cut] == mo.a[3][cut]);
        morph_reset();
        frames(20);
        project_restore_runtime(&back);
        frames(50);
        ok9("  loaded: blending 70 % again, the same values", mo.amt[3] == 70u && mo.tgt[3] == tg &&
            !memcmp(kept, t->p, sizeof kept));
    }

    /* 5. the MORPH command, the PATTERN BANK */
    ui_power_on();
    track_select(3);
    morph_set(3, tg, 0);
    frames(20);
    memcpy(orig, t->p, sizeof orig);
    step1(t, 72);
    motion_set_lock(t, 0, CMD_MORPH, 0);

    {
        bank_key(1);                                    /* B: a copy, current */
        frames(20);
        motion_set_lock(t, 0, CMD_MORPH, 100);          /* B: M100 on its step 1 */
        bank_key(0);                                    /* back to A */
        frames(20);
    }
    seq_start();
    song.playing = 1;
    events_block(CTL);
    ok9("A (M0 on step 1) plays: the sound as it is", mo.amt[3] == 0u && t->p[cut] == orig[cut]);
    bank_key(1);
    frames(20);
    {
        uint32_t b;
        for (b = 0; b < 100000u && bank.cur != 1u; b++)
            events_block(CTL);
        ok9("  B queued, at the pattern's end: M100, the target sound from its first step",
            bank.cur == 1u && mo.amt[3] == 100u && t->p[cut] == mo.b[3][cut]);
        bank_key(0);
        for (b = 0; b < 100000u && bank.cur != 0u; b++)
            events_block(CTL);
        ok9("  back to A: M0, the sound as it was", mo.amt[3] == 0u && t->p[cut] == orig[cut]);
    }
    {
        char c8[8];
        uint8_t ids[64];
        uint32_t n = tk_fx_list(t, ids), i, in = 0;
        for (i = 0; i < n; i++) in |= ids[i] == (uint8_t)CMD_MORPH;
        tk_cmd_text(CMD_MORPH, 100, c8);
        ok9("  the TRACKER shows it M100, in KNOB 4's list", str_eq(c8, "M100") && in);
    }
    song.playing = 0;
    seq_stop();

    /* 6. another engine */
    morph_set(3, morph_npresets(t), 60);
    frames(20);
    set_engine_of(t, ENGI_DRUM);
    frames(30);
    snprintf(what, sizeof what, "another engine: the morph lets go (%u presets there, target %u)", morph_npresets(t), mo.tgt[3]);
    ok9(what, mo.eng[3] == t->eng_req && mo.tgt[3] <= morph_npresets(t));

    /* 7. drawn */
    {
        uint32_t p;
        ui_power_on();
        track_select(3);
        go_page(GR_MORPH);
        morph_set(3, tg, 40);
        frames(20);
        for (p = 0; p < NPALETTES; p++) {
            palette_set(p);
            ui.force = 1;
            frame();
        }
        ok9("MORPH drawn, every palette", 1);
    }

    printf("morph: %s\n", fails9 ? "FAILED" : "all checks ok");
    return fails9 != 0;
}
