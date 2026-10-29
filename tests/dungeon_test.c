/* SPDX-License-Identifier: GPL-3.0-only
 * RONDO: HOME's dungeon crawl (src/ui_dungeon.c) through ui_test.c's harness.
 *   build/host/dungeon_test             (tests/run_rondo.sh)
 * 1. the party: a track with steps and not muted is an adventurer; muted or empty, none; everyone starts at L1, a
 *    simple loop or a busy song alike.
 * 2. stopped: the party camps (no walking); playing: a beat a square, at the tempo (120 BPM: 2 squares a second).
 * 3. ten minutes at 120 BPM, 20 seeds: a busy four-track song reaches deeper floors than one simple track, on average
 *    (a lone kick has the floor's ten and its boss to beat alone: it often stays on B1); nothing hangs, floors 1..99.
 * 4. the drawing: HOME drawn over a minute of play in every palette without a fault (the stub display).
 * 5. health and falls: the most health grows with the party's level, a level up gives it at once; an
 *    adventurer at 0 falls and its track is cut (dg_cut) while DUNGEON plays, not in PEACEFUL nor stopped; muting and
 *    unmuting does not raise it; the cleric raises, the stairs raise; the whole party fallen: a bar (DG_WIPE beats) of
 *    silence, then the retreat a floor up, healed; stopping raises everyone.
 * 6. the sound: a cut track's audio ramps to silence and comes back when raised (perform.c's black-key mute).
 * 7. ten minutes x 20 seeds: the simple loop is cut the most, the busy song now and then (Dungeon Tracker's moderate
 *    DIFFICULTY: the monsters match the party; 1.0: a lone kick on the beats dodges three monster turns in four).
 * 7b. twenty power-on songs (boot_song.c), ten minutes each at 120 BPM: push and pull, a few falls and a whole-party
 *    fall now and then, the first fall a few minutes in, kills all along.
 * 8. the game off (power-on): the corridor walks a square a beat, the party and levels shown, but no monster in ten
 *    minutes, no floor, nothing cut; MENU > GAME > DUNGEON (a tab of its own, OFF at power-on) switches it: ON a run
 *    from B1, OFF the run over and a cut lifted at once.
 * 9. the crossroads: ten minutes at 120 BPM of a 16-step loop (4 beats a phrase), the game off: a crossroads every other
 *    phrase, reached as that phrase ends (within a beat of the next one starting), straight on / left / right about a
 *    third each, the turns half a beat and the compass following; the game on, a monster ahead: straight on.
 * 10. 1.0, the fight in turns (dg_sixteenth): a round is five sixteenths, the monster's first, T1..T4 after, so its
 *    turns walk through the beat (sixteenths 0 5 10 15: each place once); a hero with nothing played charges and its
 *    next blow is stronger; a REST guards; a drumless sixteenth, the rogue dodges; a tie: the weapons follow through
 *    weaker, the cleric heals while held (more each turn), the mage stores power and bolts harder; a step that does not
 *    play (chance 0) is nothing to its hero; a party of two still takes five sixteenths a round; a smaller party, a
 *    smaller monster.
 * 11. 1.1, FLOOR FX: every floor down one track gets one effect (only that track's values change; HOME says which), the
 *    next floor lifts it and puts another; never the same track and effect twice running; stopped (the camp), a
 *    retreat or the game off lifts it and every value is back (the autosave's signature as before the run); a value
 *    turned meanwhile keeps the turn; an INSERT or SLICER in use is never taken, nor a value automation moves; over
 *    300 floors every effect comes and every adventurer gets some.
 * 15. the floor's boss: DG_BOSS_AT monsters slain on a floor (a pair counts two) and the ogre comes, nothing else;
 *    slain, everyone levels up (LEVEL UP: Ln, the health at once), the stairs DG_STAIRS squares on, the count from 0;
 *    at L9 no higher; a retreat keeps the level and counts the floor again.
 * 12. 1.1, MENU > LARGE: HOME's dungeon takes the whole screen (no cards); a knob turned brings the tall cards back,
 *    the dungeon again 2 s after the last turn; HOME > TRACKS, a layer and the other pages keep LARGE's cards. */
#define UI_TEST_NO_MAIN 1
#include "ui_test.c"
#include "../firmware/src/boot_song.c"           /* (7b: the power-on songs) */

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
/* 1.0: the fight's turns follow what the sequencer plays (seq.c dgq): 33 ms of it (its events, no sound), then a UI tick */
static uint32_t seq_left;
static void seq_ms33(void)
{
    uint32_t n = seq_left + FS * 33u / 1000u;
    for (; n >= CTL; n -= CTL)
        events_block(CTL);
    seq_left = n;
}
static void go(void) { song.g[G_BPM] = 120; seq_start(); song.playing = 1; }
static void halt(void) { song.playing = 0; seq_stop(); }
static void tick33(void)
{
    fm1_ms += 33u;
    if (song.playing)
        seq_ms33();
    dg_tick();
}
static uint32_t play(uint32_t seconds)               /* the deepest floor reached */
{
    uint32_t ms, best = 1, k;
    go();
    for (ms = 0; ms < seconds * 1000u; ms += 33u) {
        tick33();
        play_ms += 33u;
        for (k = 0; k < 4u; k++)
            cut_ms[k] += ((dg_cut >> k) & 1u) * 33u;
        best = dg.floor > best ? dg.floor : best;
        if (dg.floor < 1u || dg.floor > 99u) return 0;
    }
    halt();
    return best;
}

static void fell(uint32_t k)                          /* a fight until adventurer k falls (the others kept at full) */
{
    uint32_t i, j, f = dg.floor;
    dg.floor = 20;                                       /* (blows the cleric's healing cannot keep up with, one
                                                          * short of an L1 hero's full health) */
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
        ok4("  the rogue starts at L1", lo == 1u);
        trk[0].p[P_MUTE] = 1;
        fm1_ms += 2000u;
        dg_party();
        ok4("  muted: out of the party", !dg.in[0]);
        busy_song();
        reset_game(1);
        fm1_ms += 2000u;
        dg_party();
        hi = dg.lvl[0];
        snprintf(what, sizeof what, "a busy song: all four in the party, all at L1 too (%u -> %u)", lo, hi);
        ok4(what, dg.in[0] && dg.in[1] && dg.in[2] && dg.in[3] && hi == 1u && dg.lvl[1] == 1u && dg.lvl[3] == 1u);
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
    ok4(what, deep_busy > deep_simple + 60u && deep_simple > 20u && lo_simple && lo_busy > 2u);
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
        ok4(what, simple_cut > 0.3 && simple_cut < 25.0 && busy_cut > 0.05 && busy_cut < 8.0 && busy_cut < simple_cut);
    }
    {   /* 7b. the power-on songs: the push and pull */
        uint32_t falls = 0, wipes = 0, kills = 0, firsts[20], nf = 0, ms, k;
        for (s = 0; s < 20u; s++) {
            uint8_t dead0 = 0, wipe0 = 0;
            uint32_t first = 0;
            ui_power_on();
            boot_song(0xB007u + s * 7919u);
            boot_song_settle();
            reset_game(500 + s);
            ui_home_view = HV_SCOPE;
            fm1_ms += 2000u;
            go();
            for (ms = 0; ms < 600000u; ms += 33u) {
                tick33();
                for (k = 0; k < 4u; k++)
                    if (((dg.dead >> k) & 1u) && !((dead0 >> k) & 1u)) {
                        falls++;
                        if (!first) first = ms | 1u;
                    }
                wipes += dg.wipe && !wipe0;
                dead0 = dg.dead;
                wipe0 = dg.wipe;
            }
            halt();
            kills += dg.kills;
            if (first) firsts[nf++] = first;
        }
        {
            uint32_t i, j, med;
            for (i = 0; i < nf; i++)                         /* (sorted: the median) */
                for (j = i + 1u; j < nf; j++)
                    if (firsts[j] < firsts[i]) { uint32_t t = firsts[i]; firsts[i] = firsts[j]; firsts[j] = t; }
            med = nf ? firsts[nf / 2u] : 0u;
            snprintf(what, sizeof what, "20 power-on songs x 10 min: %.1f kills, %.1f falls, %.2f whole-party falls a run; "
                     "the first fall at %.1f min (median, %u of 20 fell)", kills / 20.0, falls / 20.0, wipes / 20.0,
                     med / 60000.0, nf);
            ok4(what, kills >= 40u * 20u && falls >= 3u * 20u && falls <= 15u * 20u && wipes <= 3u * 20u && nf >= 15u &&
                med >= 90000u && med <= 480000u);
        }
    }

    /* 5. health and falls */
    {
        int16_t lo, hi;
        busy_song();
        reset_game(9);
        fm1_ms += 2000u;
        dg_party();
        lo = dg_hpmax(0);
        dg.hp[0] = (int16_t)(lo - 3);
        dg.boss_up = 1;
        dg_killed(DM_OGRE);                              /* (the floor's boss slain) */
        hi = dg_hpmax(0);
        snprintf(what, sizeof what, "hidden health: the rogue's most %d at L1, %d at L2; the four gained at once (%d)",
                 lo, hi, dg.hp[0]);
        ok4(what, lo == 10 && hi == 14 && dg.hp[0] == lo + 1 && dg_hpmax(2) == 18);
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
    fm1_ms += 1100u;                                     /* (the party counted with T3 muted; no beat walked, so no
                                                          * cleric's raise in between) */
    dg_party();
    trk[2].p[P_MUTE] = 0;
    fm1_ms += 1100u;
    dg_party();
    ticks(66);
    ok4("  muted and unmuted: still fallen, still cut", dg_fallen(2) && (dg_cut & 4u));
    {
        uint32_t i;
        dg.walked = 0;
        for (i = 0; i < 23u && dg_fallen(2); i++)
            dg_walk();
        snprintf(what, sizeof what, "the cleric raises the fighter (%u beats walked), at half health; T3 back", i);
        ok4(what, !dg_fallen(2) && dg.hp[2] == dg_hpmax(2) / 2 && i < 23u);
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
        for (i = 0; i < 23u; i++)
            dg_walk();
        ok4("no cleric: the rogue stays down the whole floor", dg_fallen(0));
        dg.stairs = 1;                                   /* (the floor's boss slain: the stairs) */
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
        ok4(what, ok && dg.cell - c0 >= 1150u && dg.cell - c0 <= 1250u && dg.in[0] && dg.in[3] && dg.lvl[1] == 1u);
    }
    ok4("MENU: GAME, a tab of its own after SYSTEM, its first row DUNGEON (OFF / ON, a value; 1.0: then THE PARTY's five)",
        MI_TAB[MI_GAME] == MTAB_GAME && MTAB_GAME == MTAB_COUNT - 1u && str_eq(MTAB_NAME[MTAB_GAME], "GAME") &&
        mtab_rows(MTAB_GAME) == 6u && str_eq(MI_NAME[MI_GAME], "DUNGEON") && menu_valued(MI_GAME) &&
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

    /* 10. the fight in turns */
    {
        dgq_t b[NTRK];
        uint32_t i, mon_at[8], nm = 0;
        int32_t hp0;
        #define B_CLEAR() memset(b, 0, sizeof b)
        #define B_NOTE(k) (b[k].on = 1, b[k].w = 256)
        busy_song();
        reset_game(41);
        fm1_ms += 2000u;
        dg_party();
        dg.mon = DM_GOBLIN; dg.mon_dist = 1; dg.mon_max = dg.mon_hp = 30000;
        dg_engage();
        for (i = 0; i < 20u; i++) {                       /* every track a note every sixteenth */
            uint32_t atk0;
            B_CLEAR(); for (k = 0; k < 4u; k++) B_NOTE(k);
            for (k = 0; k < 4u; k++) dg.hp[k] = dg_hpmax(k);
            atk0 = dg.mon_atk = 0;
            dg_sixteenth(b);
            if (dg.mon_atk && nm < 8u) mon_at[nm++] = i;
            (void)atk0;
        }
        snprintf(what, sizeof what, "the turns: the monster on sixteenths %u %u %u %u (places in the beat %u %u %u %u)",
                 mon_at[0], mon_at[1], mon_at[2], mon_at[3], mon_at[0] % 4u, mon_at[1] % 4u, mon_at[2] % 4u, mon_at[3] % 4u);
        ok4(what, nm == 4u && mon_at[0] == 0u && mon_at[1] == 5u && mon_at[2] == 10u && mon_at[3] == 15u);

        {   /* charge: four turns of nothing, then a note, against a note every turn (the fighter, many rounds) */
            double plain = 0, charged = 0;
            uint32_t r;
            for (r = 0; r < 400u; r++) {
                int16_t h;
                dg.mon_hp = 30000; dg_engage(); dg.charge[DG_FTR] = 0;
                B_CLEAR(); B_NOTE(DG_FTR); dg.pw[DG_FTR].on = 1; dg.pw[DG_FTR].w = 256;
                h = dg.mon_hp; dg_hero(DG_FTR); plain += h - dg.mon_hp;
                dg.mon_hp = 30000; dg.charge[DG_FTR] = 4; dg.pw[DG_FTR].on = 1; dg.pw[DG_FTR].w = 256;
                h = dg.mon_hp; dg_hero(DG_FTR); charged += h - dg.mon_hp;
            }
            snprintf(what, sizeof what, "charge: four turns of nothing, then a note: %.1f against %.1f a plain one (x%.1f)",
                     charged / 400, plain / 400, charged / plain);
            ok4(what, charged > plain * 4.0 && charged < plain * 6.0);
            dg.charge[DG_FTR] = 0;
            memset(&dg.pw[DG_FTR], 0, sizeof dg.pw[0]);
            dg_hero(DG_FTR);
            ok4("  nothing played: it charges", dg.charge[DG_FTR] == 1u);
            dg.pw[DG_FTR].rest = 1;
            dg_hero(DG_FTR);
            ok4("  a REST: on guard, no charge", (dg.guard >> DG_FTR & 1u) && dg.charge[DG_FTR] == 1u);
        }
        {   /* the dodge */
            uint32_t r, hits_drum = 0, hits_none = 0;
            dg.floor = 30;
            for (r = 0; r < 600u; r++) {
                for (k = 0; k < 4u; k++) dg.hp[k] = dg_hpmax(k);
                dg.dead = 0; dg.wipe = 0; dg.mon = DM_GOBLIN; dg.mon_dist = 1; dg.guard = 0;
                dg.in[1] = dg.in[2] = dg.in[3] = 0;           /* (only the rogue to strike) */
                B_CLEAR(); B_NOTE(0);
                dg_monster(b);
                hits_drum += dg.hp[0] < dg_hpmax(0);
                dg.hp[0] = dg_hpmax(0);
                B_CLEAR();
                dg_monster(b);
                hits_none += dg.hp[0] < dg_hpmax(0);
            }
            snprintf(what, sizeof what, "the rogue: hit %u of 600 with a drum on the monster's sixteenth, %u without (dodged)",
                     hits_drum, hits_none);
            ok4(what, hits_drum > 250u && hits_drum < 470u && hits_none == 0u);
            dg.floor = 1;
            dg_party();
            fm1_ms += 2000u;
            dg_party();
        }
        {   /* ties: the weapons weaker, the cleric heals while held, the mage stores */
            uint32_t r;
            double note = 0, tie1 = 0, tie4 = 0;
            int16_t h;
            for (r = 0; r < 400u; r++) {
                dg.mon = DM_GOBLIN; dg.mon_dist = 1;
                dg.mon_hp = 30000; dg.charge[DG_RGE] = 0; dg.pw[DG_RGE].on = 1; dg.pw[DG_RGE].w = 256;
                h = dg.mon_hp; dg_hero(DG_RGE); note += h - dg.mon_hp;
                dg.mon_hp = 30000; dg.pw[DG_RGE].tie = 1;
                h = dg.mon_hp; dg_hero(DG_RGE); tie1 += h - dg.mon_hp;
                dg.mon_hp = 30000; dg.pw[DG_RGE].tie = 4;
                h = dg.mon_hp; dg_hero(DG_RGE); tie4 += h - dg.mon_hp;
            }
            snprintf(what, sizeof what, "a tie: the rogue follows through %.1f, then %.1f, a note %.1f", tie1 / 400, tie4 / 400, note / 400);
            ok4(what, tie1 < note * 0.7 && tie1 > note * 0.3 && tie4 < tie1 && tie4 > 0);
            for (k = 0; k < 4u; k++) dg.hp[k] = dg_hpmax(k);
            dg.hp[DG_FTR] = 1;
            dg.chan[DG_CLR] = 0;
            {
                int16_t a, c;
                dg.pw[DG_CLR].tie = 1; dg_hero(DG_CLR); a = dg.hp[DG_FTR];
                dg.pw[DG_CLR].tie = 2; dg_hero(DG_CLR); c = dg.hp[DG_FTR];
                snprintf(what, sizeof what, "  the cleric holds a chord: heals %d, then %d (more each turn held)", a - 1, c - a);
                ok4(what, a > 1 && c - a > a - 1);
            }
            {
                double plainb = 0, stored = 0;
                for (r = 0; r < 400u; r++) {
                    dg.mon_hp = 30000; dg.chan[DG_MAG] = 0; dg.pw[DG_MAG].on = 1; dg.pw[DG_MAG].w = 256;
                    h = dg.mon_hp; dg_hero(DG_MAG); plainb += h - dg.mon_hp;
                    dg.pw[DG_MAG].tie = 1; dg_hero(DG_MAG); dg.pw[DG_MAG].tie = 2; dg_hero(DG_MAG);
                    dg.mon_hp = 30000; dg.pw[DG_MAG].on = 1; dg.pw[DG_MAG].w = 256;
                    h = dg.mon_hp; dg_hero(DG_MAG); stored += h - dg.mon_hp;
                }
                snprintf(what, sizeof what, "  the mage holds two turns, then bolts: %.1f against %.1f", stored / 400, plainb / 400);
                ok4(what, stored > plainb * 1.7);
            }
        }
        {   /* the sequencer: what really played. A track whose steps never play (chance 0): its hero only charges */
            uint32_t ms, acted = 0, n0;
            busy_song();
            for (i = 0; i < 16u; i++) step_set_chance(&trk[2].step[i], 0);
            reset_game(43);
            fm1_ms += 2000u;
            dg_party();
            go();
            for (ms = 0; ms < 3000u; ms += 33u) {
                uint32_t a = dgq.step;
                tick33();
                for (n0 = a; n0 != dgq.step; n0++)
                    acted += dgq.b[n0 % DGQ_N][2].on;
            }
            halt();
            snprintf(what, sizeof what, "chance 0 on T3: none of its notes reach the fighter (%u), T2's do (%u)", acted,
                     (uint32_t)dgq.b[0][1].on + dgq.b[1][1].on + dgq.b[2][1].on + dgq.b[3][1].on + dgq.b[4][1].on + dgq.b[5][1].on + dgq.b[6][1].on + dgq.b[7][1].on);
            ok4(what, acted == 0u);
        }
        {   /* a party of two: still five sixteenths a round; a smaller monster */
            uint32_t at[3], na = 0;
            int32_t big, small;
            busy_song();
            reset_game(44);
            fm1_ms += 2000u;
            dg_party();
            dg.floor = 6; dg.walked = 3;
            dg.rng = 5; dg.mon = DM_NONE;
            while (!dg.mon) { dg.walked = 3; dg_walk(); }
            big = dg.mon_max;
            track_defaults_steps(&trk[1]);
            track_defaults_steps(&trk[3]);
            fm1_ms += 2000u;
            dg_party();
            dg.rng = 5; dg.mon = DM_NONE;
            while (!dg.mon) { dg.walked = 3; dg_walk(); }
            small = dg.mon_max;
            dg.mon_dist = 1; dg.mon_hp = 30000;
            dg_engage();
            for (i = 0; i < 15u; i++) {
                B_CLEAR(); B_NOTE(0); B_NOTE(2);
                for (k = 0; k < 4u; k++) dg.hp[k] = dg.in[k] ? dg_hpmax(k) : 0;
                dg.mon_atk = 0;
                dg_sixteenth(b);
                if (dg.mon_atk && na < 3u) at[na++] = i;
            }
            snprintf(what, sizeof what, "a party of two: the monster still every fifth sixteenth (%u %u %u); its health %d against %d",
                     at[0], at[1], at[2], small, big);
            ok4(what, na == 3u && at[1] == 5u && at[2] == 10u && small < big * 3 / 4);
        }
        hp0 = 0; (void)hp0;
        dg.mon = DM_NONE;
        #undef B_CLEAR
        #undef B_NOTE
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

    /* 11. 1.1: FLOOR FX */
    {
        static int16_t snap[NTRK][P_COUNT];
        uint32_t i, fl, same = 0, changed_ok = 1, msg_ok = 1, twice = 0, lift_ok = 1, seen_fx = 0, seen_trk = 0, pt = 0xFF, pf = 0xFF;
        uint32_t sig0;
        busy_song();
        reset_game(7);
        dgf.on = 0; dgf.last_trk = dgf.last_fx = 0xFF;
        dg_party();
        go();
        memcpy(snap, trk, 0);                            /* (the values before the run) */
        for (k = 0; k < NTRK; k++) memcpy(snap[k], trk[k].p, sizeof snap[k]);
        sig0 = autosave_sig();
        for (fl = 0; fl < 300u; fl++) {
            uint32_t nchg = 0, kchg = 0xFF;
            dg.stairs = 1;
            dg.mon = DM_NONE;
            dg_walk();
            for (k = 0; k < NTRK; k++)
                for (i = 0; i < P_COUNT; i++)
                    if (trk[k].p[i] != snap[k][i]) { nchg += kchg != k; kchg = k; }
            changed_ok &= dgf.on && nchg == 1u && kchg == dgf.trk;
            msg_ok &= dg.msg == dgf.msg && dg.msg_f > 0 && (str_len(dgf.msg) > 9u);
            twice += dgf.trk == pt && dgf.fx == pf;
            pt = dgf.trk; pf = dgf.fx;
            seen_fx |= 1u << dgf.fx;
            seen_trk |= 1u << dgf.trk;
        }
        printf("dungeon:   (FLOOR FX over 300 floors: effects 0x%X of 0x3FF, tracks 0x%X; last \"%s\")\n", seen_fx, seen_trk, dgf.msg);
        ok4("1.1 FLOOR FX: each floor down, one track's values change (one effect), HOME says which (STAIRS: Tn ..)",
            changed_ok && msg_ok);
        ok4("1.1 .. never the same track and effect twice running; over 300 floors all ten effects, all four adventurers",
            !twice && seen_fx == 0x3FFu && seen_trk == 0xFu);
        halt();
        fm1_ms += 33u;
        dg_tick();                                       /* stopped: the camp lifts it */
        for (k = 0; k < NTRK; k++) lift_ok &= !memcmp(snap[k], trk[k].p, sizeof snap[k]);
        ok4("1.1 .. stopped (the camp): lifted, every value as before the run (the autosave signature unchanged)",
            lift_ok && !dgf.on && autosave_sig() == sig0);
        /* a turn meanwhile stays; a retreat and the game off lift it */
        go();
        dg.stairs = 1; dg_walk();
        {
            uint32_t id = dgf.id[0], tk = dgf.trk;
            int16_t v = (int16_t)(dgf.set[0] == TP[id].max ? dgf.set[0] - 1 : dgf.set[0] + 1);
            trk[tk].p[id] = v;                            /* (the user turns it) */
            dg_retreat();
            ok4("1.1 .. a retreat lifts it; a value turned meanwhile keeps the turn", !dgf.on && trk[tk].p[id] == v);
            trk[tk].p[id] = snap[tk][id];
        }
        dg.stairs = 1; dg_walk();
        dg_game_set(0);
        lift_ok = !dgf.on;
        for (k = 0; k < NTRK; k++) lift_ok &= !memcmp(snap[k], trk[k].p, sizeof snap[k]);
        ok4("1.1 .. MENU > GAME > DUNGEON OFF lifts it", lift_ok);
        /* in use: an INSERT, a SLICER, automation on DLY: never taken */
        reset_game(9);
        dgf.last_trk = dgf.last_fx = 0xFF;
        for (k = 0; k < NTRK; k++) {
            trk[k].p[P_ITYPE] = 1;                       /* (SOFT) */
            trk[k].p[P_SLCR] = SL_STUT;
        }
        motion_active[1][P_DLY / 32u] |= 1u << (P_DLY % 32u);
        dg_party();
        same = 1;
        for (fl = 0; fl < 200u; fl++) {
            dg.stairs = 1; dg.mon = DM_NONE; dg_walk();
            same &= dgf.on && dgf.fx <= DGF_DRIVE && !(dgf.trk == 1u && dgf.fx == DGF_ECHO);
            for (k = 0; k < NTRK; k++) same &= trk[k].p[P_ITYPE] == 1 && trk[k].p[P_SLCR] == SL_STUT;
        }
        motion_active[1][P_DLY / 32u] &= ~(1u << (P_DLY % 32u));
        halt(); fm1_ms += 33u; dg_tick();
        ok4("1.1 .. an INSERT or SLICER in use is never taken, nor a value automation moves (only the sends then)", same);
        for (k = 0; k < NTRK; k++) { trk[k].p[P_ITYPE] = 0; trk[k].p[P_SLCR] = SL_OFF; }
        dg_game_set(0);
    }

    if (getenv("DG_DEMO")) {   /* (not a test: DG_DEMO=dir build/host/dungeon_test: FLOOR FX heard, dir/floor_fx_demo.wav) */
        char path[512];
        FILE *w;
        uint32_t n = 0, ms, blk = 0, nb, i;
        int32_t o[2 * CTL];
        snprintf(path, sizeof path, "%s/floor_fx_demo.wav", getenv("DG_DEMO"));
        w = fopen(path, "wb");
        if (w) {
            static const uint8_t HDR[44] = {'R','I','F','F',0,0,0,0,'W','A','V','E','f','m','t',' ',16,0,0,0,1,0,2,0,
                                            0x44,0xAC,0,0,0x10,0xB1,2,0,4,0,16,0,'d','a','t','a',0,0,0,0};
            uint32_t sz;
            fwrite(HDR, 1, 44, w);
            busy_song();
            set_engine_of(&trk[3], ENGI_WAVES); apply_preset_to(&trk[3], 12);   /* T4: WAVES BOSS FIGHT */
            trk[3].engine = trk[3].eng_req;
            reset_game(3);
            dgf.on = 0; dgf.last_trk = dgf.last_fx = 0xFF;
            song.master_q12 = 2900;                      /* (the MASTER knob most of the way up) */
            dg_party();
            go();
            for (ms = 0; ms < 50000u; ms += 33u) {
                if (ms % 8000u < 33u && ms > 1000u) {        /* a floor down every 4 bars (the demo) */
                    dg.stairs = 1;
                    dg.mon = DM_NONE; dg.mon_dist = 0;
                }
                for (k = 0; k < 4u; k++) if (dg.in[k]) dg.hp[k] = dg_hpmax(k);   /* (no falls: the effects heard) */
                nb = (uint32_t)((uint64_t)(ms + 33u) * FS / 1000u / CTL) - blk;
                for (i = 0; i < nb; i++, blk++) {
                    mix_block(o, CTL);
                    for (k = 0; k < CTL; k++) {
                        int16_t v[2] = {(int16_t)clamp(o[2 * k], -32767, 32767), (int16_t)clamp(o[2 * k + 1], -32767, 32767)};
                        fwrite(v, 2, 2, w);
                        n++;
                    }
                }
                fm1_ms += 33u;
                {
                    uint32_t f0 = dg.floor;
                    dg_tick();
                    if (dg.floor != f0) printf("dungeon demo: %.1f s B%u %s\n", ms / 1000.0, dg.floor, dg.msg ? dg.msg : "");
                }
            }
            halt();
            for (i = 0; i < 2u * FS / CTL; i++) {
                fm1_ms += 1; dg_tick();
                mix_block(o, CTL);
                for (k = 0; k < CTL; k++) {
                    int16_t v[2] = {(int16_t)clamp(o[2 * k], -32767, 32767), (int16_t)clamp(o[2 * k + 1], -32767, 32767)};
                    fwrite(v, 2, 2, w);
                    n++;
                }
            }
            sz = n * 4u;
            fseek(w, 40, SEEK_SET); fwrite(&sz, 4, 1, w);
            sz += 36u; fseek(w, 4, SEEK_SET); fwrite(&sz, 4, 1, w);
            fclose(w);
        }
    }

    /* 12. 1.1: LARGE, the dungeon whole */
    {
        int ok = 1;
        ui_power_on();
        ui_prefs |= PREF_LARGE;
        lg_peek = 0;
        go_home(); ui_home_view = HV_SCOPE; frame();
        ok &= dg_full_on();
        turn(EN_K2, 1);                                  /* a knob: the cards back */
        frame();
        ok &= !dg_full_on();
        frames(1500);
        ok &= !dg_full_on();                             /* (still within 2 s) */
        fm1_ms += 2100u; frame();
        ok &= dg_full_on();
        ok4("1.1 LARGE: HOME's dungeon whole; a knob turned shows the cards, the dungeon again 2 s after", ok);
        ui_home_view = HV_TRACKS; frame();
        ok = !dg_full_on();
        ui_home_view = HV_SCOPE; ui.layer = LAYER_EDIT; ok &= !dg_full_on(); ui.layer = 0;
        go_page(GR_ADSR); frame(); ok &= !dg_full_on();
        ui_prefs &= (uint8_t)~PREF_LARGE; go_home(); frame(); ok &= !dg_full_on();
        ok4("1.1 .. HOME > TRACKS, a layer, the other pages and LARGE off: as before", ok);
    }
    /* 13. 1.1: a monster on its way is never replaced (1.0 rolled a new one over it); now and then a pair */
    {
        uint32_t seed, i, kept = 1, enc = 0, pairs = 0, ogre_pair = 0, low_pair = 0, boss_ok = 1, msg_ok = 1;
        for (seed = 1; seed <= 3000u; seed++) {          /* encounters as played: it spawns 4 away, 3 beats to meet */
            uint32_t m0;
            reset_game(seed);
            dg_party();
            dg.floor = (uint8_t)(1u + seed % 12u);
            dg.walked = 3;
            for (i = 0; i < 200u && !dg.mon; i++) dg_walk();
            if (!dg.mon) continue;
            m0 = dg.mon;
            enc++;
            while (dg.mon_dist > 1u) { dg.mon_dist--; dg_walk(); }
            kept &= dg.mon == m0;
            if (dg.mon2) {
                pairs++;
                low_pair |= dg.floor < 2u;
                ogre_pair |= dg.mon == DM_OGRE;
                msg_ok &= dg.msg == dg.pair_msg && strstr(dg.pair_msg, " AND ") != NULL &&
                          dg.mon2_hp > 0 && dg.mon2_hp == dg.mon2_max && dg.mon_hp <= dg.mon_max;
            }
        }
        printf("dungeon:   (pairs: %u of %u encounters, %.1f %%)\n", pairs, enc, 100.0 * pairs / (enc ? enc : 1));
        ok4("1.1 a monster coming up the corridor stays the one that comes", kept && enc > 2500u);
        ok4("1.1 .. now and then a second joins it: rare (about 1 in 12), never on floor 1, never beside the ogre",
            pairs * 100u > enc * 4u && pairs * 100u < enc * 13u && !low_pair && !ogre_pair && msg_ok);
        /* the boss: due (the floor's tenth slain) with a skeleton on its way: the skeleton stays, the ogre after it */
        reset_game(7); dg_party();
        dg.floor = 5; dg.fkills = DG_BOSS_AT;
        dg.mon = DM_SKEL; dg.mon_dist = 3; dg.mon_max = dg.mon_hp = 9;
        dg_walk();
        boss_ok &= dg.mon == DM_SKEL && dg.boss_due && !dg.mon2;
        for (i = 0; i < 5u; i++) { dg_walk(); boss_ok &= dg.mon == DM_SKEL && !dg.mon2; }
        dg.mon = DM_NONE; dg.mon_dist = 0;                /* (slain) */
        dg_walk();
        boss_ok &= dg.mon == DM_OGRE && !dg.boss_due && dg.mon_dist == 4u;
        ok4("1.1 .. the boss waits for the monster on its way, then comes (no second beside it)", boss_ok);
        /* a pair fought: the cleave, the one slain, the other steps up; a wipe clears both */
        {
            int ok = 1;
            reset_game(11); dg_party();
            dg.floor = 4; dg.mon = DM_GOBLIN; dg.mon_dist = 1; dg.mon_max = dg.mon_hp = 10;
            dg.mon2 = DM_SLIME; dg.mon2_max = dg.mon2_hp = 6;
            dg_strike(DG_RGE, 4 << 8, 0);
            ok &= dg.mon_hp == 6 && dg.mon2_hp == 6;      /* (a rogue's blow: the front one only) */
            dg_strike(DG_FTR, 4 << 8, 0);
            ok &= dg.mon_hp == 2 && dg.mon2_hp == 4 && dg.mon2_hit == 6;   /* (the axe: half to the other) */
            dg_strike(DG_FTR, 8 << 8, 0);
            ok &= dg.mon2 == DM_NONE && dg.kills == 1u && dg.mon == DM_GOBLIN;   /* CLEAVED */
            ok4("1.1 .. the fighter's axe cleaves the one beside (half the blow); it can fall to it", ok);
            dg.mon2 = DM_SKEL; dg.mon2_max = dg.mon2_hp = 5;
            dg.mon_hp = 0; dg_slain();
            ok = dg.mon == DM_SKEL && dg.mon_hp == 5 && dg.mon_max == 5 && dg.mon2 == DM_NONE && dg.mon_dist == 1u;
            dg_slain();
            ok &= dg.mon == DM_NONE && dg.mon_dist == 0u && dg.kills == 3u;
            ok4("1.1 .. one slain, the other steps up and the fight goes on; both slain, the party walks on", ok);
            ok = 1;
            {
                uint32_t hits1 = 0, hits2 = 0, t;
                dgq_t b[4];
                memset(b, 0, sizeof b);
                for (t = 0; t < 4u; t++) b[t].on = 1;
                for (k = 0; k < 4u; k++) dg.lvl[k] = 4;     /* (a party some bosses in: blows big enough to share) */
                for (t = 0; t < 4000u; t++) {
                    int32_t before = 0;
                    for (k = 0; k < 4u; k++) { if (dg.in[k]) dg.hp[k] = 300; before += dg.in[k] ? 300 : 0; }
                    dg.dead = 0; dg.guard = 0;
                    dg.mon = DM_GOBLIN; dg.mon2 = (uint8_t)(t & 1u ? DM_SLIME : DM_NONE); dg.mon_dist = 1;
                    dg_monster(b);
                    for (k = 0; k < 4u; k++) if (dg.in[k]) before -= dg.hp[k];
                    if (t & 1u) hits2 += (uint32_t)before; else hits1 += (uint32_t)before;
                }
                printf("dungeon:   (damage a turn: one monster %.2f, a pair %.2f)\n", hits1 / 2000.0, hits2 / 2000.0);
                ok &= hits2 > hits1 * 105u / 100u && hits2 < hits1 * 150u / 100u;
            }
            ok4("1.1 .. a pair both strike, each lighter: a little more than one monster, not double", ok);
            for (k = 0; k < 4u; k++) if (dg.in[k]) dg.hp[k] = 1;
            dg.mon = DM_GOBLIN; dg.mon2 = DM_SLIME; dg.boss_due = 1;
            for (i = 0; i < 400u && dg_up(); i++) {
                dgq_t b[4];
                memset(b, 0, sizeof b);
                for (k = 0; k < 4u; k++) b[k].on = 1;
                dg_monster(b);
            }
            ok4("1.1 .. the party falls to a pair: both gone, nothing due", !dg_up() && !dg.mon && !dg.mon2 && !dg.boss_due);
        }
    }
    /* 14. THE PARTY (Dungeon Tracker, core.h): each track a class, its voices and its role; 12 points */
    {
        int ok;
        uint32_t k, n, i, e;
        step_t keep[NSTEP];
        ui_power_on();
        ok = trk_cls(0) == CLS_ROGUE && trk_cls(1) == CLS_CLERIC && trk_cls(2) == CLS_FIGHTER && trk_cls(3) == CLS_MAGE &&
             trk_cap(0) == 4u && trk_cap(1) == 4u && trk_cap(2) == 1u && trk_cap(3) == 3u;
        ok4("1.0 THE PARTY: by default T1 ROGUE 4, T2 CLERIC 4, T3 FIGHTER 1, T4 MAGE 3 (the layout as before)", ok);
        ok4(".. MENU > GAME: T1..T4 and PARTY after DUNGEON", MI_TAB[MI_CLS1] == MTAB_GAME && MI_TAB[MI_PARTY] == MTAB_GAME &&
            str_eq(MI_NAME[MI_CLS3], "T3") && str_eq(menu_vname(MI_PARTY, 0), "12/12") && menu_n(MI_CLS1) == 4u);
        memcpy(keep, trk[2].step, sizeof keep);
        menu_put(MI_CLS3, CLS_ROGUE);                   /* T3 FIGHTER -> ROGUE: 15 points */
        ok = trk_cls(2) == CLS_FIGHTER && party_waiting && str_eq(ui.msg, "PARTY 15/12: TOO BIG") &&
             menu_get(MI_CLS3) == CLS_ROGUE && str_eq(menu_vname(MI_PARTY, 0), "15/12 !");
        ok4(".. a class that puts the party over waits: nothing changes, PARTY 15/12 says so", ok);
        menu_put(MI_CLS2, CLS_FIGHTER);                 /* T2 CLERIC -> FIGHTER: 12 */
        ok = !party_waiting && trk_cls(1) == CLS_FIGHTER && trk_cls(2) == CLS_ROGUE && trk_cap(1) == 1u && trk_cap(2) == 4u &&
             str_eq(ui.msg, "PARTY SET") && cls_allows(CLS_ROGUE, track_cat(&trk[2])) && cls_allows(CLS_FIGHTER, track_cat(&trk[1])) &&
             trk[2].eng_req == ENGI_CKIT && !memcmp(trk[2].step, keep, sizeof keep);
        ok4(".. the change that brings it back takes both: T2 FIGHTER (a bass), T3 ROGUE (a kit), the steps kept", ok);
        menu_put(MI_CLS4, CLS_CLERIC);                  /* 4+1+4+4 = 13: waits */
        menu_close();
        printf("dungeon:   (closed: '%s', cls4 %u, waiting %u)\n", ui.msg, trk_cls(3), party_waiting);
        ok4(".. the MENU closed with the party over: as it was, and said", trk_cls(3) == CLS_MAGE && !party_waiting &&
            str_eq(ui.msg, "PARTY NOT CHANGED"));
        /* voices: two rogues, four voices each */
        go();
        for (k = 0; k < 4u; k++) trk_note_on(&trk[2], 36u + 2u * k, 100);
        events_block(CTL);
        for (k = 0, n = 0; k < NVOICE; k++) n += trk[2].v[k].active != 0;
        printf("dungeon:   (T3 voices %u, nvoice %u / %u)\n", n, trk_nvoice(&trk[2]), trk_nvoice(&trk[1]));
        ok4(".. a ROGUE on T3 plays four at once, the FIGHTER on T2 one (its class's voices)", trk_nvoice(&trk[2]) == 4u && trk_nvoice(&trk[1]) == 1u);
        for (k = 0; k < 4u; k++) trk_note_off(&trk[2], 36u + 2u * k);
        halt();
        /* PRESETS > LIST: CLASS first and by default; the escape hatch ALL */
        song.sel = 2;
        memset(&favorites, 0, sizeof favorites);         /* (a fresh unit's settings: LIST's default) */
        ok = list_mode() == LIST_CLASS && str_eq(list_name(list_mode()), "CLASS") && list_pos(LIST_CLASS) == 0u &&
             list_of(1) == 0u;
        {
            uint32_t total, kk, bad = 0;
            preset_pos(&total);
            for (i = 0; i < total; i++) { e = preset_at(i, &kk); bad += e < NENGINES && preset_cat(e, kk) != CAT_DRUM; }
            printf("dungeon:   (CLASS list on a rogue: %u sounds, %u not drums)\n", total, bad);
            ok &= total >= 15u && !bad;
            ok &= eng_step_listed(ENGI_CKIT, 1) != 0u;   /* (ANALOG: no drums) */
            for (e = ENGI_CKIT, i = 0; i < 12u; i++) { e = eng_step_listed(e, 1); ok &= e != 0u && e != 12u && e != 16u; }
        }
        ok4(".. PRESETS > LIST CLASS (the default): a ROGUE track sees drums only; ENG skips the engines with none", ok);
        list_set(0);
        {
            uint32_t total;
            preset_pos(&total);
            printf("dungeon:   (ALL: %u sounds)\n", total);
            ok4(".. LIST ALL, the way out: every sound again", total > 100u && str_eq(list_name(list_mode()), "ALL"));
        }
        list_set(LIST_CLASS);
        select_engine(0);                                /* ANALOG on a rogue: refused */
        ok4(".. on a ROGUE an engine with no drums is refused (the EDIT layer's keys), one with some gives its first",
            trk[2].eng_req == ENGI_CKIT && (select_engine(ENGI_SID), trk[2].eng_req == ENGI_SID && preset_cat(ENGI_SID, trk[2].preset) == CAT_DRUM));
        /* the dungeon: roles by class */
        reset_game(21);
        dg_party();
        ok = dg_hpmax(1) == 6 + 4 * dg.lvl[1] + 4 && dg_hpmax(2) == 6 + 4 * dg.lvl[2];   /* (the fighter's +4: T2's now) */
        ok4(".. the dungeon: the fighter's health with its class (T2), the rogues' on T1 and T3", ok);
        {   /* both rogues dodge when their own drum leaves the sixteenth empty */
            dgq_t b[4];
            uint32_t d0 = 0, t;
            memset(b, 0, sizeof b);
            b[0].on = 1; b[1].on = 1; b[3].on = 1;           /* (T3, a rogue, plays nothing here) */
            for (k = 0; k < 4u; k++) { dg.in[k] = 1; dg.lvl[k] = 3; }
            dg.dodged = 0;
            for (t = 0; t < 400u; t++) {
                for (k = 0; k < 4u; k++) if (dg.in[k]) dg.hp[k] = 300;
                dg.dead = 0; dg.mon = DM_GOBLIN; dg.mon2 = 0; dg.mon_dist = 1;
                dg_monster(b);
            }
            d0 = dg.dodged;
            printf("dungeon:   (dodged %u, in %u%u%u%u)\n", d0, dg.in[0], dg.in[1], dg.in[2], dg.in[3]);
            ok4(".. a ROGUE on T3 dodges on its own track's gaps", d0 > 40u);
        }
        /* saved with the song */
        project_save(0);
        menu_put(MI_CLS2, CLS_CLERIC);                  /* 4+4+4+3 = 15: waits */
        menu_put(MI_CLS3, CLS_FIGHTER);                 /* back to 12: the default party */
        ok = trk_cls(1) == CLS_CLERIC && trk_cls(2) == CLS_FIGHTER;
        project_load(0);
        ok &= trk_cls(1) == CLS_FIGHTER && trk_cls(2) == CLS_ROGUE;
        ok4(".. the party is saved with the song (a save, a change, the load: the saved party)", ok);
        new_song(1);
        ok4(".. NEW + DEFAULT SOUNDS: the default party", trk_cls(0) == 0u && trk_cls(1) == 1u && trk_cls(2) == 2u && trk_cls(3) == 3u);
        {   /* a song over the points (damaged): the default party */
            project_t *q = &proj_scratch;
            project_capture(q);
            q->clsx[0] = 0; q->clsx[1] = 1u ^ 0u; q->clsx[2] = 2u ^ 0u; q->clsx[3] = 3u ^ 0u;   /* (all ROGUE: 16) */
            q->sum = proj_sum(q);
            project_restore_runtime(q);
            ok4(".. a stored party over 12 points (damaged) loads as the default", trk_cls(0) == 0u && trk_cls(1) == 1u &&
                trk_cls(2) == 2u && trk_cls(3) == 3u);
        }
        list_set(LIST_CLASS);
    }
    /* 15. the floor's boss and the party's level */
    {
        uint32_t i, ok = 1, f0, n;
        busy_song();
        reset_game(31);
        fm1_ms += 2000u;
        dg_party();
        f0 = dg.floor;
        for (i = 0; i < DG_BOSS_AT - 1u; i++) { dg.mon = DM_GOBLIN; dg_slain(); }
        for (i = 0; i < 300u; i++) { dg_walk(); ok &= dg.mon != DM_OGRE; if (dg.mon) { dg.mon = DM_NONE; dg.mon2 = DM_NONE; dg.fkills = DG_BOSS_AT - 1u; } }
        ok4("the boss: nine slain on a floor, no ogre", ok && !dg.boss_up);
        dg.mon = DM_SKEL; dg.mon2 = DM_GOBLIN; dg.mon2_hp = 5; dg.mon_dist = 1;
        dg.fkills = DG_BOSS_AT - 2u;
        dg_slain(); dg_slain();                              /* (a pair: two) */
        ok4("  a pair slain counts two", dg.fkills == DG_BOSS_AT && !dg.mon);
        for (i = 0, n = 0; i < 40u && dg.mon != DM_OGRE; i++) { dg_walk(); n += dg.mon && dg.mon != DM_OGRE; }
        ok4("  the tenth: the ogre comes, nothing before it", dg.mon == DM_OGRE && dg.boss_up == 1u && !n);
        for (i = 0; i < 30u; i++) { dg_walk(); ok &= dg.mon == DM_OGRE && !dg.mon2; }
        ok4("  nothing beside it, nothing after it while it stands", ok);
        dg.mon_hp = 0;
        dg_slain();
        snprintf(what, sizeof what, "  slain: everyone L2 (%u %u %u %u), \"%s\"", dg.lvl[0], dg.lvl[1], dg.lvl[2], dg.lvl[3], dg.msg ? dg.msg : "");
        ok4(what, dg.plv == 2u && dg.lvl[0] == 2u && dg.lvl[1] == 2u && dg.lvl[2] == 2u && dg.lvl[3] == 2u &&
            dg.msg && str_eq(dg.msg, "LEVEL UP: L2") && dg.floor == f0);
        dg_walk();
        ok4("  the stairs two squares on (nothing comes meanwhile)", dg.floor == f0 && !dg.mon);
        dg_walk();
        ok4("  .. one floor down, its count from 0", dg.floor == f0 + 1u && !dg.fkills && !dg.boss_up && !dg.stairs);
        fm1_ms += 2000u;
        dg_party();
        ok4("  the level stays (the party's count a second later)", dg.lvl[0] == 2u && dg_hpmax(0) == 14);
        for (i = 0; i < 12u; i++) { dg.fkills = DG_BOSS_AT; dg.boss_up = 1; dg.mon = DM_OGRE; dg.mon_hp = 0; dg_slain(); dg.stairs = 1; dg_walk(); }
        ok4("  at most L9 (twelve bosses more: L9, OGRE SLAIN)", dg.plv == 9u && dg.lvl[3] == 9u && dg_hpmax(3) == 42 &&
            dg.floor == f0 + 13u);
        dg.fkills = 6;
        dg_retreat();
        ok4("  a retreat: a floor up, the level kept, the floor counted again", dg.plv == 9u && !dg.fkills &&
            dg.floor == f0 + 12u);
        dg_game_set(1);
        dg_party();
        ok4("  a new run: L1 again", dg.lvl[0] == 1u && dg.floor == 1u);
        dg_game_set(0);
    }
    printf("dungeon: %s\n", fails4 ? "FAILED" : "all checks ok");
    return fails4 != 0;
}
