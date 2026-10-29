/* SPDX-License-Identifier: GPL-3.0-only
 * RONDO: SEQ > TRACKER and the SEQ + SELECT track picker (src/ui_tracker.c), through ui_test.c's harness (the real UI
 * sources against stub buttons, knobs and display).
 *   build/host/tracker_test           (tests/run_rondo.sh)
 * 1. SEQ opens the TRACKER (the only step page; AUTOMATION and VARY after it), its columns per track kind.
 * 2. a kit track: BD / SD hits and accents, HH . -> x closed -> o open (one lane at a time), PC c t r b (one of the
 *    grid's four lanes), EDIT clears the cell.
 * 3. a melodic track: KNOB 3 on an empty NOTE writes the last note, then transposes; KNOB 4 LEN ties; VEL and FLAG;
 *    CHN RAT NDG; EDIT on NOTE clears the step.
 * 4. FX: KNOB 4 picks the parameter (a kit voice's values among them), KNOB 3 locks it on the step, EDIT removes it.
 * 5. each track keeps its own cursor row.
 * 6. SEQ held + SELECT: the picker steps round T1..T4 (wraps); SEQ let go: that track and its TRACKER, no SEQ TOOLS
 *    action and no tap. SEQ held alone still opens SEQ TOOLS and lets go without changing the track.
 * 7. armed and stopped, a key on a kit's tracker writes its drum into the cursor's step; disarmed it only plays.
 * 8. the drawing: every column of a few hundred random cells drawn without crashing (the stub display). */
#define UI_TEST_NO_MAIN 1
#include "ui_test.c"

static int fails2;
static void ok2(const char *what, int ok) { fails2 += check(what, ok); }

static void seq_open(uint32_t track)
{
    track_select(track);
    go_home();
    press(B_SEQ);
    frames(100);
}
static void col_to(uint32_t kind)                  /* KNOB 2 to the column of that kind */
{
    uint32_t i;
    for (i = 0; i < 12u; i++)
        turn(EN_K2, -1);
    for (i = 0; i < 12u && tk_col() != kind; i++)
        turn(EN_K2, 1);
}

int main(void)
{
    track_t *t;
    step_t *st;
    uint32_t i;
    ui_power_on();

    /* 1. */
    seq_open(0);
    ok2("SEQ opens the TRACKER (the step page), song.seq_mode on", !ui.home && cur_page()->graph == GR_ROLL &&
        str_eq(cur_page()->title, "TRACKER") && song.seq_mode);
    {
        uint32_t n = 0, auto_ok = 0, vary_ok = 0;
        for (i = 0; i < NPAGES; i++)
            if (PAGES[i].fam == FAM_SEQ) {
                n++;
                auto_ok |= PAGES[i].graph == GR_EVENTS;
                vary_ok |= PAGES[i].graph == GR_VARY;
            }
        ok2("SEQ's pages: TRACKER, AUTOMATION and VARY (no STEP, no DETAIL)", n == 3u && auto_ok && vary_ok);
    }
    ok2("a kit track: 9 columns BD SD HH PC VEL CHN RAT NDG FX; a melodic one 6", tk_ncols(&trk[0]) == 9u &&
        tk_colkind(&trk[0], 0) == TC_BD && tk_colkind(&trk[0], 8) == TC_FX && tk_ncols(&trk[1]) == 6u &&
        tk_colkind(&trk[1], 0) == TC_NOTE);
    ok2("no DRUM grid any more (grid_on 0: a kit's keys play)", !grid_on());

    /* 2. a kit */
    t = &trk[0];
    for (i = 0; i < 3u; i++)
        turn(EN_K1, 1);                                 /* row 3: empty in BEAT */
    st = &t->step[3];
    step_clear(st);
    col_to(TC_BD);
    turn(EN_K3, 1);
    ok2("BD: KNOB 3 right puts the kick's hit on the row", ui.cursor == 3u && (step_lanes(st) & 1u));
    turn(EN_K4, 1);
    ok2("  KNOB 4 its accent", (step_accents(st) & 1u) != 0u);
    col_to(TC_HH);
    turn(EN_K3, 1);
    ok2("HH: right once a closed hat", ((step_lanes(st) >> 3) & 3u) == 1u);
    turn(EN_K3, 1);
    ok2("  again an open hat (the closed one gone)", ((step_lanes(st) >> 3) & 3u) == 2u);
    turn(EN_K3, -1);
    turn(EN_K3, -1);
    ok2("  left twice: none (the kick stays)", ((step_lanes(st) >> 3) & 3u) == 0u && (step_lanes(st) & 1u));
    col_to(TC_PC);
    {
        int okp = 1;
        for (i = 0; i < 4u; i++) {
            turn(EN_K3, 1);
            okp &= ((step_lanes(st) >> TK_PC_LANE[i]) & 1u) && (step_lanes(st) & 0xE4u) == (1u << TK_PC_LANE[i]);
        }
        ok2("PC: c t r b in turn, one of the four lanes at a time", okp);
    }
    press(B_EDIT);
    ok2("EDIT clears the PC cell (the kick stays)", !(step_lanes(st) & 0xE4u) && (step_lanes(st) & 1u));

    /* 3. a melodic track */
    seq_open(1);
    t = &trk[1];
    st = &t->step[0];
    step_clear(st);
    col_to(TC_NOTE);
    last_note = 57;
    turn(EN_K3, 1);
    ok2("NOTE: KNOB 3 on an empty row writes the last note", st->n == 1u && st->note[0] == 57u && st->time == ST_NOTE);
    turn(EN_K3, 2);
    ok2("  then transposes it", st->note[0] == 59u);
    turn(EN_K4, 2);
    ok2("  KNOB 4 LEN 3: two TIEs after it", note_len(t, 0) == 3u && t->step[1].time == ST_TIE && t->step[2].time == ST_TIE);
    col_to(TC_VEL);
    turn(EN_K3, -6);
    ok2("VEL: KNOB 3 down from 96", ev_sval(st, EVK_VEL) < 96);
    turn(EN_K4, 1);
    ok2("  KNOB 4 FLAG: ACC", (st->flags & SF_ACCENT) != 0u);
    col_to(TC_CHN);
    turn(EN_K3, -5);
    ok2("CHN: under 100 %", ev_sval(st, EVK_CHANCE) < 100);
    col_to(TC_RAT);
    turn(EN_K3, 2);
    ok2("RAT: x3", ev_sval(st, EVK_RATCH) == 3);
    col_to(TC_NDG);
    turn(EN_K3, -2);
    ok2("NDG: -2", ev_sval(st, EVK_NUDGE) == -2);
    press(B_EDIT);
    ok2("  EDIT: back to 0", ev_sval(st, EVK_NUDGE) == 0);

    /* 4. FX */
    col_to(TC_FX);
    {
        uint8_t ids[64];
        uint32_t n = tk_fx_list(t, ids), id;
        int16_t v;
        turn(EN_K4, 1);
        id = tk_fx_id(t);
        ok2("FX: KNOB 4 picks the next parameter of the list", n > 8u && id == ids[1]);
        turn(EN_K3, 3);
        ok2("  KNOB 3 locks it on the row", motion_lock_get(t, 0, id, &v) && v != motion_base_value(t, id));
        press(B_EDIT);
        ok2("  EDIT removes the lock", !motion_lock_get(t, 0, id, &v));
        n = tk_fx_list(&trk[0], ids);
        for (i = 0; i < n && ids[i] != P_FM2_DEC; i++)
            ;
        ok2("  a kit's list holds its voices' values (SD DECAY)", i < n);
    }
    col_to(TC_NOTE);
    ui.cursor = 0;
    press(B_EDIT);
    ok2("EDIT on NOTE clears the step (STEP's)", !t->step[0].n);

    /* 5. */
    ui.cursor = 5;
    track_select(2);
    ui.cursor = 9;
    track_select(1);
    ok2("each track keeps its row (T2 5, T3 9)", ui.cursor == 5u);
    track_select(2);
    ok2("  back on T3: 9", ui.cursor == 9u);

    /* 6. the picker */
    seq_open(0);
    btn_down(B_SEQ);
    frame();
    turn(EN_SELECT, 1);
    turn(EN_SELECT, 1);
    ok2("SEQ held, SELECT x2: the picker on T3", tk.pick == 3u && ui.layer == LAYER_SEQ);
    turn(EN_SELECT, 1);
    turn(EN_SELECT, 1);
    ok2("  round: T4, then T1", tk.pick == 1u);
    turn(EN_SELECT, -1);
    ok2("  back: T4", tk.pick == 4u);
    {
        uint8_t bpm = (uint8_t)song.g[G_BPM];
        btn_up(B_SEQ);
        frames(100);
        ok2("SEQ let go: T4 selected, its TRACKER, no tempo change", song.sel == 3u && !ui.home &&
            cur_page()->graph == GR_ROLL && !ui.layer && !tk.pick && (uint8_t)song.g[G_BPM] == bpm);
    }
    go_home();
    frames(50);
    hold(B_SEQ);
    frames(100);
    ok2("SEQ held alone: SEQ TOOLS as before, the track kept, HOME kept", song.sel == 3u && ui.home && !tk.pick);

    /* 7. step recording on a kit */
    seq_open(0);
    stop_transport();
    t = &trk[0];
    ui.cursor = 6;
    step_clear(&t->step[6]);
    song.rec = 1;
    ui.entry_open = 0;
    {
        uint32_t k = 7;                                 /* key 7, the first C: the kick (ck_keys) */
        key_down(k);
        frame();
        key_up(k);
        frames(50);
        ok2("armed, stopped: a key on the kit's tracker writes its drum into the row", step_on(&t->step[6]) &&
            (step_lanes(&t->step[6]) & 1u));
        song.rec = 0;
        step_clear(&t->step[ui.cursor]);
        i = ui.cursor;
        key_down(k);
        frame();
        key_up(k);
        frames(50);
        ok2("  disarmed: it only plays", !step_on(&t->step[i]));
    }

    /* 8. drawing */
    {
        uint32_t r = 12345u, k;
        for (k = 0; k < 300u; k++) {
            r = r * 1103515245u + 12345u;
            track_select((r >> 8) % NTRK);
            go_page(GR_ROLL);
            ui.cursor = (uint8_t)((r >> 12) % 16u);
            tk.col[song.sel] = (uint8_t)((r >> 16) % 9u);
            tk.fx[song.sel] = (uint8_t)((r >> 20) % 40u);
            turn(EN_K3, (int32_t)((r >> 24) % 5u) - 2);
            ui.force = 1;
            frame();
        }
        ok2("300 random cells edited and drawn", 1);
    }
    printf("tracker: %s\n", fails2 ? "FAILED" : "all checks ok");
    return fails2 != 0;
}
