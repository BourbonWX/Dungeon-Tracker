/* SPDX-License-Identifier: GPL-3.0-only
 * RONDO: HOME's dungeon crawl (src/ui_dungeon.c) through ui_test.c's harness.
 *   build/host/dungeon_test             (tests/run_rondo.sh)
 * 1. the party: a track with steps and not muted is an adventurer; muted or empty, none; levels 1..9 grow with what the
 *    track does (a 4-on-the-floor kick against a busy kit with locks, chances and ratchets).
 * 2. stopped: the party camps (no walking); playing: a beat a square, at the tempo (120 BPM: 2 squares a second).
 * 3. ten minutes at 120 BPM, 20 seeds: a busy four-track song reaches deeper floors than one simple track, on average,
 *    and both go below B1; nothing hangs, the floors stay 1..99.
 * 4. the drawing: HOME drawn over a minute of play in every palette without a fault (the stub display).
 * 5. health and falls: the most health grows with the track's level, and a track made busier gains it at once; an
 *    adventurer at 0 falls and its track is cut (dg_cut) while DUNGEON plays, not in PEACEFUL nor stopped; muting and
 *    unmuting does not raise it; the cleric raises, the stairs raise; the whole party fallen: a bar (DG_WIPE beats) of
 *    silence, then the retreat a floor up, healed; stopping raises everyone.
 * 6. the sound: a cut track's audio ramps to silence and comes back when raised (perform.c's black-key mute).
 * 7. ten minutes x 20 seeds: the simple loop is cut now and then, the busy song (almost) never.
 * 8. the game off (power-on): the corridor walks a square a beat, the party and levels shown, but no monster in ten
 *    minutes, no floor, nothing cut; MENU > GAME > DUNGEON (a tab of its own, OFF at power-on) switches it: ON a run
 *    from B1, OFF the run over and a cut lifted at once.
 * 9. the crossroads: ten minutes at 120 BPM of a 16-step loop (4 beats a phrase), the game off: a crossroads every other
 *    phrase, reached as that phrase ends (within a beat of the next one starting), straight on / left / right about a
 *    third each, the turns half a beat and the compass following; the game on, a monster ahead: straight on. */
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
    dg_game = 1;                                         /* (sections 1..7: the game on) */
}
static uint32_t cut_ms[4], play_ms;
static uint32_t play(uint32_t seconds)               /* the deepest floor reached */
{
    uint32_t ms, best = 1, k;
    song.playing = 1;
    song.g[G_BPM] = 120;
    for (ms = 0; ms < seconds * 1000u; ms += 33u) {
        fm1_ms += 33u;
        dg_tick();
        play_ms += 33u;
        for (k = 0; k < 4u; k++)
            cut_ms[k] += ((dg_cut >> k) & 1u) * 33u;
        best = dg.floor > best ? dg.floor : best;
        if (dg.floor < 1u || dg.floor > 99u) return 0;
    }
    song.playing = 0;
    return best;
}

static void fell(uint32_t k)                          /* a fight until adventurer k falls (the others kept at full) */
{
    uint32_t i, j, f = dg.floor;
    dg.floor = 60;                                       /* (blows the cleric's healing cannot keep up with) */
    for (i = 0; i < 400u && !dg_fallen(k); i++) {
        for (j = 0; j < 4u; j++)
            if (j != k && dg.in[j] && !dg_fallen(j))
                dg.hp[j] = dg_hpmax(j);
        dg.hp[k] = 1;
        dg.mon = DM_GOBLIN;
        dg.mon_dist = 1;
        dg.mon_max = dg.mon_hp = 9999;
        dg_fight();
    }
    dg.floor = (uint8_t)f;
    dg.mon = DM_NONE;
    dg.mon_dist = 0;
}
static void ticks(uint32_t ms)
{
    while (ms >= 33u) {
        fm1_ms += 33u;
        dg_tick();
        ms -= 33u;
    }
}
static uint64_t energy(uint32_t track, uint32_t nblocks)   /* the track's sound in the mix over nblocks */
{
    int32_t o[2u * CTL];
    uint64_t e = 0;
    uint32_t b, i;
    (void)track;
    for (b = 0; b < nblocks; b++) {
        events_block(CTL);
        memset(o, 0, sizeof o);
        mix_block(o, CTL);
        for (i = 0; i < 2u * CTL; i++)
            e += (uint64_t)(o[i] < 0 ? -o[i] : o[i]);
    }
    return e;
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
    ok4("the party: T1 with steps is the ROGUE, the empty tracks nobody", dg.in[0] && !dg.in[1] && !dg.in[2] && !dg.in[3]);
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
    {   /* 7. (the simple and the busy runs alternate: measured again apart) */
        double simple_cut, busy_cut = 0;
        memset(cut_ms, 0, sizeof cut_ms);
        play_ms = 0;
        for (s = 0; s < 20u; s++) { simple_song(); reset_game(300 + s); ui_home_view = HV_SCOPE; fm1_ms += 2000u; play(600); }
        simple_cut = 100.0 * cut_ms[0] / play_ms;
        memset(cut_ms, 0, sizeof cut_ms);
        play_ms = 0;
        for (s = 0; s < 20u; s++) { busy_song(); reset_game(300 + s); ui_home_view = HV_SCOPE; fm1_ms += 2000u; play(600); }
        for (k = 0; k < 4u; k++)
            busy_cut += 100.0 * cut_ms[k] / play_ms / 4.0;
        snprintf(what, sizeof what, "falls cut the simple loop %.1f %% of the time, a busy song's tracks %.2f %%", simple_cut, busy_cut);
        ok4(what, simple_cut > 2.0 && simple_cut < 15.0 && busy_cut < 1.0);
    }

    /* 5. health and falls */
    {
        int16_t lo, hi;
        simple_song();
        reset_game(9);
        fm1_ms += 2000u;
        dg_party();
        lo = dg_hpmax(0);
        busy_song();
        fm1_ms += 2000u;
        dg_party();
        hi = dg_hpmax(0);
        snprintf(what, sizeof what, "hidden health: the simple kick's most %d, the busy kit's %d; the busy kit gained it at once (%d)",
                 lo, hi, dg.hp[0]);
        ok4(what, hi > 2 * lo && dg.hp[0] == hi);
    }
    busy_song();
    reset_game(10);
    ui_home_view = HV_SCOPE;
    song.playing = 1;
    song.g[G_BPM] = 120;
    ticks(100);
    fell(2);
    ticks(66);
    ok4("T3's fighter falls: T3 cut while the game plays, the others not", dg_fallen(2) && dg_cut == 4u && dg.msg &&
        str_eq(dg.msg, "FIGHTER FALLS: T3 CUT"));
    ui_home_view = HV_TRACKS;
    ticks(66);
    ok4("  HOME showing TRACKS: still cut (the game runs whatever is shown)", dg_cut == 4u);
    ui_home_view = HV_SCOPE;
    trk[2].p[P_MUTE] = 1;
    ticks(1100);
    trk[2].p[P_MUTE] = 0;
    fm1_ms += 1100u;
    dg_party();
    ticks(66);
    ok4("  muted and unmuted: still fallen, still cut", dg_fallen(2) && (dg_cut & 4u));
    {
        uint32_t i;
        dg.walked = 0;
        for (i = 0; i < DG_FLOOR - 1u && dg_fallen(2); i++)
            dg_walk();
        snprintf(what, sizeof what, "the cleric raises the fighter (%u beats walked), at half health; T3 back", i);
        ok4(what, !dg_fallen(2) && dg.hp[2] == dg_hpmax(2) / 2 && dg.walked < DG_FLOOR);
        ticks(66);
        ok4("  its cut lifted", dg_cut == 0u);
    }
    trk[1].p[P_MUTE] = 1;                                /* no cleric: only the stairs */
    fm1_ms += 1100u;
    dg_party();
    fell(0);
    {
        uint32_t i, f = dg.floor;
        dg.walked = 0;
        for (i = 0; i < DG_FLOOR - 1u; i++)
            dg_walk();
        ok4("no cleric: the rogue stays down the whole floor", dg_fallen(0));
        dg_walk();
        ok4("  the stairs: up again on the next floor", !dg_fallen(0) && dg.floor == f + 1u);
    }
    trk[1].p[P_MUTE] = 0;
    simple_song();                                       /* the whole party: the rogue alone */
    reset_game(11);
    ui_home_view = HV_SCOPE;
    song.playing = 1;
    ticks(100);
    dg.floor = 6;
    fell(0);
    ticks(66);
    ok4("the whole party falls: the song cut, a bar of silence", dg.wipe && dg_cut == 1u);
    ticks(2100);                                         /* (4 beats at 120 BPM: 2 s) */
    ok4("  then they retreat a floor up, healed, the song back", !dg.wipe && !dg.dead && dg_cut == 0u && dg.floor == 5u &&
        dg.hp[0] == dg_hpmax(0));
    fell(0);
    ticks(66);
    song.playing = 0;
    ticks(66);
    ok4("stopped: a camp, the fallen raised, nothing cut", !dg.dead && !dg.wipe && dg_cut == 0u);

    /* 6. the sound */
    {
        uint64_t on, off, back;
        busy_song();
        for (k = 1; k < NTRK; k++)
            trk[k].p[P_MUTE] = 1;                        /* only T1's drums */
        reset_game(12);
        ui_home_view = HV_SCOPE;
        song.master_q12 = 4096;
        seq_start();
        song.playing = 1;
        ticks(1100);
        on = energy(0, 690);                             /* half a second */
        fell(0);
        ticks(33);
        energy(0, 1380);                                 /* (the ramp, the reverb's and delay's tails: a second) */
        off = energy(0, 690);
        song.playing = 0;
        ticks(33);
        song.playing = 1;
        ticks(33);
        energy(0, 50);
        back = energy(0, 690);
        snprintf(what, sizeof what, "the sound: T1 cut goes silent (%.0f -> %.0f), raised it plays again (%.0f)",
                 (double)on, (double)off, (double)back);
        ok4(what, on > 0u && off * 1000u < on && back * 4u > on);
        song.playing = 0;
        for (k = 1; k < NTRK; k++)
            trk[k].p[P_MUTE] = 0;
    }

    /* 8. the game off */
    ui_power_on();
    ok4("power-on: the game off", !dg_game && !menu_get(MI_GAME));
    {
        uint32_t i, ok = 1, c0;
        busy_song();
        dg_game_set(0);
        song.playing = 1;
        song.g[G_BPM] = 120;
        c0 = dg.cell;
        for (i = 0; i < 600000u / 33u; i++) {
            fm1_ms += 33u;
            dg_tick();
            ok &= !dg.mon && !dg.dead && !dg_cut && dg.floor == 1u && !dg.kills;
        }
        snprintf(what, sizeof what, "off, ten minutes playing: %u squares walked, the party in (levels %u %u %u %u), no monster, nothing cut",
                 dg.cell - c0, dg.lvl[0], dg.lvl[1], dg.lvl[2], dg.lvl[3]);
        ok4(what, ok && dg.cell - c0 >= 1150u && dg.cell - c0 <= 1250u && dg.in[0] && dg.in[3] && dg.lvl[1] >= 3u);
    }
    ok4("MENU: GAME, a tab of its own after SYSTEM, its one row DUNGEON (OFF / ON, a value)",
        MI_TAB[MI_GAME] == MTAB_GAME && MTAB_GAME == MTAB_COUNT - 1u && str_eq(MTAB_NAME[MTAB_GAME], "GAME") &&
        mtab_rows(MTAB_GAME) == 1u && str_eq(MI_NAME[MI_GAME], "DUNGEON") && menu_valued(MI_GAME) &&
        !MI_ACTION(MI_GAME) && menu_n(MI_GAME) == 2u && str_eq(menu_vname(MI_GAME, 0), "OFF") &&
        str_eq(menu_vname(MI_GAME, 1), "ON") && MI_ACTION(MI_ABOUT) && !menu_valued(MI_ABOUT));
    dg.floor = 9;
    menu_put(MI_GAME, 1);
    ok4("  ON: a run from B1, everyone at full", dg_game && menu_get(MI_GAME) && dg.floor == 1u && !dg.dead &&
        dg.hp[1] == dg_hpmax(1));
    ticks(100);
    fell(1);
    ticks(66);
    ok4("  a fall cuts as before", (dg_cut & 2u) != 0u);
    menu_put(MI_GAME, 0);
    ok4("  OFF: the run over, the cut lifted at once", !dg_game && !dg_cut && !dg.dead && !dg.mon);
    ticks(66);
    ok4("  and stays lifted", !dg_cut);
    {   /* the menu by the panel: HOLD HOME, ALGORITHM back one tab from DISPLAY (wraps) to GAME, a knob right */
        song.playing = 0;
        go_home();
        frames(100);
        hold(B_HOME);
        frames(100);
        ui.menu_sel = 0;                                 /* (DISPLAY's first row) */
        turn(EN_ALGO, -1);
        frames(50);
        ok4("the panel: HOME held opens the MENU, ALGORITHM back from DISPLAY: GAME > DUNGEON", ui.menu &&
            ui.menu_sel == MI_GAME);
        turn(EN_K1, 1);
        frames(50);
        ok4("  KNOB 1 right: ON", dg_game == 1u);
        turn(EN_K1, -1);
        frames(50);
        ok4("  left: OFF", dg_game == 0u);
        hold(B_HOME);
        frames(100);
    }

    /* 9. the crossroads */
    {
        uint32_t ms, el = 0, prev = 0, wraps = 0, dec = 0, near = 1, n0, i, lefts, rights;
        uint32_t wrap_t[400], dec_t[400];
        simple_song();
        reset_game(21);
        dg_game = 0;
        dg.heading = 0;
        song.playing = 1;
        song.g[G_BPM] = 120;
        trk[0].seq_idx = 15;
        n0 = dg.way[0] + dg.way[1] + dg.way[2];
        for (ms = 0; ms < 600000u; ms += 33u) {
            uint32_t idx, nd;
            fm1_ms += 33u;
            el += 33u;
            idx = (el / 125u) % 16u;                      /* (the sequencer's step: 1/16 at 120 BPM) */
            trk[0].seq_idx = (uint16_t)idx;
            if (idx < prev && wraps < 400u)
                wrap_t[wraps++] = el;
            prev = idx;
            dg_tick();
            nd = dg.way[0] + dg.way[1] + dg.way[2] - n0;
            if (nd > dec && dec < 400u)
                dec_t[dec++] = el;
        }
        for (i = 0; i < dec; i++) {                      /* each decision: within a beat of a phrase's start */
            uint32_t j, best = 0xFFFFFFFFu;
            for (j = 0; j < wraps; j++) {
                uint32_t d = dec_t[i] > wrap_t[j] ? dec_t[i] - wrap_t[j] : wrap_t[j] - dec_t[i];
                best = d < best ? d : best;
            }
            near &= best <= 520u;
        }
        lefts = dg.way[1];
        rights = dg.way[2];
        snprintf(what, sizeof what, "crossroads, game off: %u phrases, %u crossroads (straight %u, left %u, right %u)", wraps, dec,
                 dg.way[0], lefts, rights);
        ok4(what, wraps >= 295u && wraps <= 302u && dec + 2u >= wraps / 2u && dec <= wraps / 2u + 1u &&
            dg.way[0] * 5u > dec && lefts * 5u > dec && rights * 5u > dec &&
            dg.way[0] * 2u < dec && lefts * 2u < dec && rights * 2u < dec);
        ok4("  each reached as its phrase ends (within a beat of the next one)", near && dec > 0u);
        {   /* (a turn still under way at the end is not in the heading yet) */
            uint32_t want = (rights + 4u * 400u - lefts - (dg.turn > 0 ? 1u : 0u) + (dg.turn < 0 ? 1u : 0u)) & 3u;
            snprintf(what, sizeof what, "  the compass follows the turns: %u right - %u left = facing %c", rights, lefts,
                     DG_HEAD[dg.heading]);
            ok4(what, dg.heading == want);
        }
        {   /* a turn: half a beat at 120 BPM */
            uint32_t h0 = dg.heading, t;
            dg.turn = 1;
            dg.tph = 0;
            for (t = 0; t < 30u && dg.turn; t++) {
                fm1_ms += 33u;
                dg_tick();
            }
            snprintf(what, sizeof what, "  a turn right: %u ms, then facing %c", t * 33u, DG_HEAD[dg.heading]);
            ok4(what, !dg.turn && t * 33u >= 200u && t * 33u <= 300u && dg.heading == ((h0 + 1u) & 3u));
        }
        {   /* drawn mid-turn, a crossroads ahead, every palette */
            uint32_t p2;
            go_home();
            for (p2 = 0; p2 < NPALETTES; p2++) {
                palette_set(p2);
                dg.turn = (int8_t)(p2 & 1u ? 1 : -1);
                dg.tph = 9000u * (p2 + 1u);
                dg.junc = dg.cell + 2u;
                ui.force = 1;
                frame();
            }
            dg.turn = 0;
            dg.junc = 0;
            ok4("  HOME drawn mid-turn and with a crossroads ahead, every palette", 1);
        }
        reset_game(22);                                   /* the game on, a goblin coming: straight on */
        dg.mon = DM_GOBLIN;
        dg.mon_dist = 3;
        dg.junc = dg.cell + 1u;
        n0 = dg.way[0];
        dg_walk();
        ok4("  the game on, a monster ahead: straight on to it", dg.way[0] == n0 + 1u && !dg.turn && !dg.junc);
        dg.mon = DM_NONE;
        song.playing = 0;
    }

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
