/* SPDX-License-Identifier: GPL-3.0-only
 * RONDO: SEQ > VARY, sequence dice (src/ui_vary.c), through ui_test.c's harness.
 *   build/host/vary_test              (tests/run_rondo.sh)
 * 1. SEQ's taps: TRACKER, AUTOMATION, VARY; OCT+ rolls a variation into the pattern; SAVE held undoes it.
 * 2. a die held makes the same choices: every die held but ACCENT, five rolls differ only in accents and velocities.
 * 3. WILD 0: the original; WILD 100 changes more than WILD 20.
 * 4. every roll from the original: twenty rolls never drift further; HIST down to ORIG gives the original back
 *    exactly; HIST 1 the roll before.
 * 5. the pattern changed by hand: the next roll takes it as the original.
 * 6. a melodic track's notes stay on its scale (C major); a kit's downbeats stay; the steps' locks stay on their steps.
 * 7. the page drawn after rolls, every palette, without a fault. */
#define UI_TEST_NO_MAIN 1
#include "ui_test.c"

static int fails7;
static void ok7(const char *what, int ok) { fails7 += check(what, ok); }
static char what[200];

static void melody(track_t *t)
{
    static const uint8_t N[16] = {60, 0, 64, 0, 67, 0, 72, 67, 65, 0, 64, 0, 62, 0, 60, 0};
    uint32_t i;
    track_defaults_steps(t);
    t->p[P_SLEN] = 16;
    for (i = 0; i < 16u; i++) {
        step_t *s = &t->step[i];
        step_clear(s);
        if (!N[i])
            continue;
        s->time = ST_NOTE;
        s->n = 1;
        s->note[0] = N[i];
        s->vel = 96;
    }
}
static void kit(track_t *t)
{
    uint32_t i;
    track_defaults_steps(t);
    t->p[P_SLEN] = 16;
    for (i = 0; i < 16u; i++) {
        step_t *s = &t->step[i];
        step_clear(s);
        s->time = ST_NOTE;
        s->hit = (uint8_t)((i % 4u == 0u ? 1u : 0u) | (i % 8u == 4u ? 2u : 0u) | (i % 2u == 0u ? 8u : 0u));
        if (!s->hit)
            s->time = ST_REST;
    }
}
static uint32_t diff_steps(const step_t *a, const step_t *b, uint32_t n)
{
    uint32_t i, d = 0;
    for (i = 0; i < n; i++)
        d += memcmp(&a[i], &b[i], sizeof a[i]) != 0;
    return d;
}
static void open_vary(uint32_t trk_i)
{
    track_select(trk_i);
    go_page(GR_VARY);
}

int main(void)
{
    static step_t orig[NSTEP], prev[NSTEP];
    track_t *t = &trk[1];
    uint32_t i, k;

    /* 1. */
    ui_power_on();
    melody(t);
    track_select(1);
    go_home();
    press(B_SEQ);
    frames(400);
    {
        int a, b, c;
        a = str_eq(cur_page()->title, "TRACKER");
        press(B_SEQ);
        frames(400);                                       /* (taps 0.3 s apart: no double tap, no lock) */
        b = str_eq(cur_page()->title, "AUTOMATION");
        press(B_SEQ);
        frames(400);
        c = str_eq(cur_page()->title, "VARY");
        ok7("SEQ's taps: TRACKER, AUTOMATION, VARY", a && b && c);
    }
    memcpy(orig, t->step, sizeof orig);
    press(B_OCTUP);
    frames(50);
    snprintf(what, sizeof what, "OCT+ rolls: %u of 16 steps varied", diff_steps(t->step, orig, 16));
    ok7(what, diff_steps(t->step, orig, 16) > 0u && vary.ok[1] && vary_shown(1));
    hold(B_SAVE);
    frames(50);
    ok7("  SAVE held: the original back (one undo)", !diff_steps(t->step, orig, NSTEP));

    /* 2. a die held */
    ui_power_on();
    melody(t);
    open_vary(1);
    vary.wild[1] = 81;                                         /* 80 % */
    vary.hold[1] = (uint8_t)(0x3Fu & ~(1u << 4));              /* all but ACCENT */
    vary_roll();
    {
        int only = 1;
        for (k = 0; k < 5u; k++) {
            memcpy(prev, t->step, sizeof prev);
            vary_roll();
            for (i = 0; i < 16u; i++) {
                step_t a = prev[i], b = t->step[i];
                a.flags &= (uint8_t)~SF_ACCENT;
                b.flags &= (uint8_t)~SF_ACCENT;
                a.vel = b.vel = 0;
                a.acc = b.acc = 0;
                only &= !memcmp(&a, &b, sizeof a);
            }
        }
        ok7("every die held but ACCENT: five rolls differ only in accents and velocities", only);
    }

    /* 3. WILD */
    {
        uint32_t lo = 0, hi = 0;
        ui_power_on();
        melody(t);
        open_vary(1);
        memcpy(orig, t->step, sizeof orig);
        vary.wild[1] = 1;                                      /* 0 % */
        vary_roll();
        ok7("WILD 0: the original", !diff_steps(t->step, orig, NSTEP));
        for (k = 0; k < 20u; k++) {
            vary.wild[1] = 21;
            vary_roll();
            lo += diff_steps(t->step, orig, 16);
            vary.wild[1] = 101;
            vary_roll();
            hi += diff_steps(t->step, orig, 16);
        }
        snprintf(what, sizeof what, "WILD 100 changes more than WILD 20 (%u vs %u steps, 20 rolls each)", hi, lo);
        ok7(what, hi > lo && lo > 0u);
    }

    /* 4. from the original */
    ui_power_on();
    melody(t);
    open_vary(1);
    memcpy(orig, t->step, sizeof orig);
    {
        uint32_t most = 0;
        vary.wild[1] = 61;
        for (k = 0; k < 20u; k++) {
            memcpy(prev, t->step, sizeof prev);
            vary_roll();
            most = diff_steps(t->step, orig, 16) > most ? diff_steps(t->step, orig, 16) : most;
        }
        snprintf(what, sizeof what, "twenty rolls, each from the original: at most %u of 16 steps from it", most);
        ok7(what, most <= 14u);
        vary_hist(1);
        ok7("  HIST 1: the roll before", !diff_steps(t->step, prev, NSTEP));
        vary_hist(20);
        ok7("  HIST down to ORIG (past the eight rolls kept): the original exactly", !diff_steps(t->step, orig, NSTEP) &&
            vary.hpos[1] == vary.hn[1] && vary.hn[1] == VARY_HIST);
        vary_hist(-20);
    }

    /* 5. changed by hand */
    t->step[1].time = ST_NOTE;
    t->step[1].n = 1;
    t->step[1].note[0] = 70;
    memcpy(orig, t->step, sizeof orig);
    vary_roll();
    vary_hist(20);
    ok7("the pattern changed by hand: the next roll's original is it", !diff_steps(t->step, orig, NSTEP));

    /* 6. the scale, a kit, the locks */
    {
        int inscale = 1, downs = 1, locks;
        int16_t v;
        ui_power_on();
        melody(t);
        t->p[P_ROOT] = 0;
        for (i = 0; i <= (uint32_t)TP[P_SCALE].max && !str_eq(TP[P_SCALE].names[i], "MAJ") && !str_eq(TP[P_SCALE].names[i], "MAJOR"); i++)
            ;
        t->p[P_SCALE] = (int16_t)(i <= (uint32_t)TP[P_SCALE].max ? i : 1u);
        motion_set_lock(t, 4, P_E1, 30);
        open_vary(1);
        vary.wild[1] = 101;
        for (k = 0; k < 20u; k++) {
            vary_roll();
            for (i = 0; i < 16u; i++)
                if (t->step[i].time == ST_NOTE && t->step[i].n)
                    inscale &= scale_snap(t, t->step[i].note[0]) == t->step[i].note[0];
        }
        locks = motion_lock_get(t, 4, P_E1, &v) && v == 30;
        snprintf(what, sizeof what, "melodic, WILD 100, 20 rolls: every note on C %s; the lock on step 4 stays",
                 TP[P_SCALE].names[t->p[P_SCALE]]);
        ok7(what, inscale && locks);
        kit(&trk[0]);
        open_vary(0);
        vary.wild[0] = 101;
        for (k = 0; k < 20u; k++) {
            vary_roll();
            for (i = 0; i < 16u; i += 4u)
                downs &= (trk[0].step[i].hit & 1u) && trk[0].step[i].time == ST_NOTE;
        }
        ok7("a kit, WILD 100, 20 rolls: the kick on every downbeat stays", downs);
    }

    /* 7. drawn */
    {
        uint32_t p;
        open_vary(1);
        vary_roll();
        for (p = 0; p < NPALETTES; p++) {
            palette_set(p);
            ui.force = 1;
            frame();
        }
        ok7("VARY drawn after rolls, every palette", 1);
    }

    printf("vary: %s\n", fails7 ? "FAILED" : "all checks ok");
    return fails7 != 0;
}
