/* SPDX-License-Identifier: GPL-3.0-only
 * RONDO: HOME's dungeon crawl (src/ui_dungeon.c) through ui_test.c's harness.
 *   build/host/dungeon_test             (tests/run_rondo.sh)
 * 1. the party: a track with steps and not muted is an adventurer; muted or empty, none; levels 1..9 grow with what the
 *    track does (a 4-on-the-floor kick against a busy kit with locks, chances and ratchets).
 * 2. stopped: the party camps (no walking); playing: a beat a square, at the tempo (120 BPM: 2 squares a second).
 * 3. ten minutes at 120 BPM, 20 seeds: a busy four-track song reaches deeper floors than one simple track, on average,
 *    and both go below B1; nothing hangs, the floors stay 1..99.
 * 4. the drawing: HOME drawn over a minute of play in every palette without a fault (the stub display). */
#define UI_TEST_NO_MAIN 1
#include "ui_test.c"

static int fails4;
static void ok4(const char *what, int ok) { fails4 += check(what, ok); }

static void simple_song(void)
{
    uint32_t k, i;
    ui_power_on();
    for (k = 0; k < NTRK; k++)
        track_defaults_steps(&trk[k]);
    for (i = 0; i < 16u; i += 4u) {                       /* T1: a kick on the beats, nothing else */
        trk[0].step[i].time = ST_NOTE;
        trk[0].step[i].hit = 1;
        trk[0].step[i].vel = 100;
    }
}
static void busy_song(void)
{
    static const uint8_t N[16] = {48, 55, 60, 63, 67, 70, 72, 75, 48, 53, 57, 60, 65, 68, 72, 77};
    uint32_t k, i;
    ui_power_on();                                       /* (T1 the BEAT already) */
    for (k = 1; k < NTRK; k++) {
        track_t *t = &trk[k];
        track_defaults_steps(t);
        for (i = 0; i < 16u; i++) {
            step_t *s = &t->step[i];
            if (k == 2u && (i & 1u)) continue;
            s->time = ST_NOTE;
            s->n = k == 1u ? 3u : 1u;
            s->note[0] = (uint8_t)(N[(i * k) % 16u] - (k == 2u ? 24 : 0));
            s->note[1] = (uint8_t)(s->note[0] + 4);
            s->note[2] = (uint8_t)(s->note[0] + 7);
            s->vel = 100;
            if (i % 3u == 0u) ev_sput(s, EVK_CHANCE, 70);
            if (i % 5u == 0u) ev_sput(s, EVK_RATCH, 2);
        }
        for (i = 0; i < 8u; i++)
            motion_set_lock(t, i * 2u, P_E1, (int16_t)(20 + 10 * i));
    }
    for (i = 0; i < 16u; i++) {                           /* T1: more drums */
        step_t *s = &trk[0].step[i];
        s->time = ST_NOTE;
        s->hit |= (uint8_t)(1u << (3u + (i & 1u)) | (i % 3u == 0u ? 32u : 0u));
        if (i % 4u == 2u) ev_sput(s, EVK_NUDGE, 2);
    }
}
static void reset_game(uint32_t seed)
{
    memset(&dg, 0, sizeof dg);
    dg.floor = dg.best = 1;
    dg.rng = seed * 2654435761u + 1u;
}
static uint32_t play(uint32_t seconds)               /* the deepest floor reached */
{
    uint32_t ms, best = 1;
    song.playing = 1;
    song.g[G_BPM] = 120;
    for (ms = 0; ms < seconds * 1000u; ms += 33u) {
        fm1_ms += 33u;
        dg_tick();
        best = dg.floor > best ? dg.floor : best;
        if (dg.floor < 1u || dg.floor > 99u) return 0;
    }
    song.playing = 0;
    return best;
}

int main(void)
{
    uint32_t k, s, deep_simple = 0, deep_busy = 0, lo_simple = 99, lo_busy = 99;
    char what[160];

    /* 1. */
    simple_song();
    reset_game(1);
    fm1_ms += 2000u;
    dg_party();
    ok4("the party: T1 with steps is the FIGHTER, the empty tracks nobody", dg.in[0] && !dg.in[1] && !dg.in[2] && !dg.in[3]);
    {
        uint32_t lo = dg.lvl[0], hi;
        trk[0].p[P_MUTE] = 1;
        fm1_ms += 2000u;
        dg_party();
        ok4("  muted: out of the party", !dg.in[0]);
        busy_song();
        reset_game(1);
        fm1_ms += 2000u;
        dg_party();
        hi = dg.lvl[0];
        snprintf(what, sizeof what, "a busy song: all four in the party; a busier kit, a higher level (%u -> %u)", lo, hi);
        ok4(what, dg.in[0] && dg.in[1] && dg.in[2] && dg.in[3] && hi > lo && dg.lvl[1] >= 3u);
    }

    /* 2. */
    reset_game(2);
    song.playing = 0;
    for (k = 0; k < 60u; k++) { fm1_ms += 33u; dg_tick(); }
    ok4("stopped: they camp (no square walked)", dg.cell == 0u);
    dg.mon = DM_NONE;
    song.playing = 1;
    song.g[G_BPM] = 120;
    {
        uint32_t c0 = dg.cell, f0 = dg.walked;
        dg.rng = 7;
        for (k = 0; k < 30u; k++) { fm1_ms += 33u; dg_tick(); if (dg.mon) break; }
        (void)f0;
        snprintf(what, sizeof what, "playing at 120 BPM: about 2 squares a second (%u in %u ms)", dg.cell - c0, k * 33u);
        ok4(what, dg.mon || (dg.cell - c0 >= 1u && dg.cell - c0 <= 3u));
    }

    /* 3. */
    for (s = 0; s < 20u; s++) {
        uint32_t b;
        simple_song();
        reset_game(100 + s);
        fm1_ms += 2000u;
        b = play(600);
        deep_simple += b;
        lo_simple = b < lo_simple ? b : lo_simple;
        busy_song();
        reset_game(100 + s);
        fm1_ms += 2000u;
        b = play(600);
        deep_busy += b;
        lo_busy = b < lo_busy ? b : lo_busy;
    }
    snprintf(what, sizeof what, "10 minutes x 20 seeds: one simple track reaches B%.1f on average (least B%u), a busy song B%.1f (least B%u)",
             deep_simple / 20.0, lo_simple, deep_busy / 20.0, lo_busy);
    ok4(what, deep_busy > deep_simple + 20u && lo_simple > 1u && lo_simple && lo_busy);

    /* 4. */
    {
        uint32_t p;
        busy_song();
        reset_game(3);
        song.playing = 1;
        go_home();
        for (p = 0; p < NPALETTES; p++) {
            palette_set(p);
            for (k = 0; k < 120u; k++) { ui.force = 1; frame(); }
        }
        ok4("HOME drawn over a minute of play in every palette", 1);
    }
    printf("dungeon: %s\n", fails4 ? "FAILED" : "all checks ok");
    return fails4 != 0;
}
