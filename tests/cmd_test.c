/* SPDX-License-Identifier: GPL-3.0-only
 * RONDO: the TRACKER's commands (src/seq.c ph_*, src/ui_tracker.c): SPEED, LOOP and its PITCH, in the FX column.
 *   build/host/cmd_test               (tests/run_rondo.sh)
 * The sequencer runs (events_block); each step's row is logged from the track (pr_row, the notes it holds).
 * 1. no command: the rows are the steps, nothing changes (pr_on 0).
 * 2. SPEED 1/2 on row 8: 8 8 9 9 10 10 11 11, its GATE twice as long (at 94 % the note sounds into the second step);
 *    at the loop's end row 0 at x1.
 * 3. SPEED x2 on row 4: two rows a step from there (4 5, 6 7 .. 14 15, 0 1 ..), the second at the step's half.
 * 4. LOOP 2x3 on row 7: 0..7, then 6 7 three more times, then 8 9 (the 16 steps); PITCH +2 on it: each pass 2
 *    semitones up, back to the notes as written after it.
 * 5. a kit with LOOP and PITCH: the hits keep their drums (the pitch moves notes only).
 * 6. never drifts: a track with commands and one without come round together, loop after loop.
 * 7. the FX column: SPEED LOOP PITCH first in KNOB 4's list; KNOB 3 sets them on the row (HALF, L2x3, P+2 shown);
 *    EDIT clears; saved with the project and loaded back. */
#define UI_TEST_NO_MAIN 1
#include "ui_test.c"

static int fails6;
static void ok6(const char *what, int ok) { fails6 += check(what, ok); }
static char what[240];

static void melodic(track_t *t)                        /* 16 rows, row i note 60 + i, at 1/16 */
{
    uint32_t i;
    track_defaults_steps(t);
    t->p[P_SLEN] = 16;
    for (i = 0; i < 16u; i++) {
        step_t *s = &t->step[i];
        step_clear(s);
        s->time = ST_NOTE;
        s->n = 1;
        s->note[0] = (uint8_t)(60 + i);
        s->vel = 100;
    }
}
static void fresh(void)
{
    uint32_t k;
    ui_power_on();
    song.master_q12 = 4096;
    song.g[G_BPM] = 120;
    for (k = 0; k < NTRK; k++) {
        track_defaults_steps(&trk[k]);
        motion_clear(&trk[k]);
    }
}
/* play n steps of track t (from the top), logging each step's row(s) and first held note */
static uint32_t rows[64], notes[64], nlog;
static void run(track_t *t, uint32_t steps)
{
    uint32_t last = 0xFFFFu, lastrow = 0xFFFFu, b, guard = 0, per = div_samples((uint32_t)t->p[P_SDIV]);
    nlog = 0;
    seq_start();
    song.playing = 1;
    for (b = 0; nlog < steps && guard < 200000u; b++, guard++) {
        events_block(CTL);
        if (t->seq_idx != last || (t->pr_on && t->pr_row != lastrow)) {
            if (nlog < 64u && (t->seq_idx != last || t->pr_spd == 2u)) {
                rows[nlog] = t->pr_on ? t->pr_row : t->seq_idx;
                notes[nlog] = t->seq_n ? t->seq_notes[0] : 0u;
                nlog++;
            }
            last = t->seq_idx;
            lastrow = t->pr_on ? t->pr_row : t->seq_idx;
        }
    }
    (void)per;
    song.playing = 0;
    seq_release(t);
}
static int rows_are(const uint8_t *want, uint32_t n)
{
    uint32_t i;
    for (i = 0; i < n; i++)
        if (rows[i] != want[i])
            return 0;
    return 1;
}
static void rows_str(char *b, uint32_t n)
{
    uint32_t i;
    b[0] = 0;
    for (i = 0; i < n && i < nlog; i++) {
        char x[6];
        fmt_int(x, (int32_t)rows[i]);
        str_cpy(b + str_len(b), x, 200 - str_len(b));
        str_cpy(b + str_len(b), " ", 200 - str_len(b));
    }
}

int main(void)
{
    track_t *t = &trk[1];
    char r[200];

    /* 1. */
    fresh();
    melodic(t);
    run(t, 16);
    {
        static const uint8_t W[16] = {0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15};
        ok6("no command: the rows are the steps (pr_on 0)", rows_are(W, 16) && !t->pr_on);
    }

    /* 2. SPEED 1/2 on row 8 */
    fresh();
    melodic(t);
    motion_set_lock(t, 8, CMD_SPD, 1);
    t->p[P_SGATE] = 120;                                     /* (a long gate: a 1/2 row's note outlasts a step) */
    run(t, 18);
    rows_str(r, 18);
    {
        static const uint8_t W[18] = {0, 1, 2, 3, 4, 5, 6, 7, 8, 8, 9, 9, 10, 10, 11, 11, 0, 1};
        snprintf(what, sizeof what, "SPEED 1/2 on row 8: %s", r);
        ok6(what, rows_are(W, 18));
        ok6("  GATE 94 %: the 1/2 row's note sounds into its second step (the gate x2)", notes[8] == 68u && notes[9] == 68u);
        ok6("  the loop round: row 0 at x1", rows[16] == 0u && rows[17] == 1u);
    }

    /* 3. SPEED x2 on row 4 */
    fresh();
    melodic(t);
    motion_set_lock(t, 4, CMD_SPD, 2);
    run(t, 30);
    rows_str(r, 30);
    {   /* (logged: each step's first row and the half's second) */
        static const uint8_t W[29] = {0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 0, 1, 2, 3, 4, 5, 6, 7, 8, 9,
                                      10, 11, 12};
        snprintf(what, sizeof what, "SPEED x2 on row 4: %s", r);
        ok6(what, rows_are(W, 16) && rows[16] == 0u);
    }

    /* 4. LOOP 2x3 on row 7, PITCH +2 */
    fresh();
    melodic(t);
    motion_set_lock(t, 7, CMD_LOOP, 1 * 8 + 2);                /* back 2 rows (index 1), 3 times */
    run(t, 17);
    rows_str(r, 17);
    {
        static const uint8_t W[17] = {0, 1, 2, 3, 4, 5, 6, 7, 6, 7, 6, 7, 6, 7, 8, 9, 0};
        snprintf(what, sizeof what, "LOOP 2x3 on row 7: %s", r);
        ok6(what, rows_are(W, 17));
    }
    motion_set_lock(t, 7, CMD_LPIT, 2);
    run(t, 16);
    snprintf(what, sizeof what, "  PITCH +2: the passes' notes %u %u, %u %u, %u %u, then row 8 %u (as written)",
             notes[8], notes[9], notes[10], notes[11], notes[12], notes[13], notes[14]);
    ok6(what, notes[7] == 67u && notes[8] == 68u && notes[9] == 69u && notes[10] == 70u && notes[11] == 71u &&
        notes[12] == 72u && notes[13] == 73u && notes[14] == 68u);

    /* 5. a kit */
    fresh();
    {
        track_t *k = &trk[0];
        uint32_t i;
        track_defaults_steps(k);
        k->p[P_SLEN] = 16;
        for (i = 0; i < 16u; i++) {
            step_clear(&k->step[i]);
            k->step[i].time = ST_NOTE;
            k->step[i].hit = (uint8_t)(i & 1u ? 1u << 1 : 1u);  /* kick, snare, .. */
        }
        motion_set_lock(k, 3, CMD_LOOP, 0 * 8 + 1);          /* back 1 row, twice */
        motion_set_lock(k, 3, CMD_LPIT, 5);
        run(k, 8);
        snprintf(what, sizeof what, "a kit, LOOP 1x2 + PITCH on row 3: rows %u %u %u, its snare stays the snare (%u)",
                 rows[3], rows[4], rows[5], notes[4]);
        ok6(what, rows[3] == 3u && rows[4] == 3u && rows[5] == 3u && rows[6] == 4u && notes[4] == DRUM_LANE_NOTE[1]);
    }

    /* 6. never drifts */
    fresh();
    melodic(&trk[1]);
    melodic(&trk[2]);
    motion_set_lock(&trk[1], 2, CMD_SPD, 1);
    motion_set_lock(&trk[1], 9, CMD_LOOP, 3 * 8 + 7);           /* back 8, 8 times: far past the loop */
    {
        uint32_t b, together = 0, apart = 0, w1 = 0, w2 = 0;
        uint32_t l1 = 0, l2 = 0;
        seq_start();
        song.playing = 1;
        for (b = 0; b < 20000u; b++) {                       /* ~14.5 s: seven loops of 2 s */
            events_block(CTL);
            if (trk[1].seq_idx < l1) w1++;
            if (trk[2].seq_idx < l2) w2++;
            l1 = trk[1].seq_idx;
            l2 = trk[2].seq_idx;
            if (trk[1].seq_idx == trk[2].seq_idx) together++;
            else apart++;
        }
        song.playing = 0;
        snprintf(what, sizeof what, "never drifts: commands on T2, none on T3: %u and %u loops, every block on the same step (%u apart)",
                 w1, w2, apart);
        ok6(what, w1 == w2 && w1 >= 6u && !apart);
    }

    /* 7. the FX column */
    {
        uint8_t ids[64];
        uint32_t n, i;
        fresh();
        melodic(t);
        track_select(1);
        go_page(GR_ROLL);
        n = tk_fx_list(t, ids);
        ok6("FX column: SPEED LOOP PITCH first in KNOB 4's list", n > 3u && ids[0] == CMD_SPD && ids[1] == CMD_LOOP &&
            ids[2] == CMD_LPIT && str_eq(track_desc(t, CMD_SPD)->label, "SPEED"));
        for (i = 0; i < 12u && tk_col() != TC_FX; i++)
            turn(EN_K2, 1);
        ui.cursor = 8;
        tk.fx[1] = 0;                                        /* SPEED */
        turn(EN_K3, 1);
        tk_cell(t, 8, TC_FX, r);
        snprintf(what, sizeof what, "  KNOB 3 on row 8: SPEED 1/2 (shown %s)", r);
        ok6(what, str_eq(r, "HALF"));
        ui.cursor = 7;
        tk.fx[1] = 1;                                        /* LOOP */
        turn(EN_K3, 11);
        tk_cell(t, 7, TC_FX, r);
        snprintf(what, sizeof what, "  LOOP on row 7: %s", r);
        ok6(what, str_eq(r, "L2x4"));
        tk.fx[1] = 2;                                        /* PITCH: shown is still the LOOP (the first command) */
        turn(EN_K3, 2);
        {
            int16_t v;
            ok6("  PITCH +2 set on row 7 beside it", motion_lock_get(t, 7, CMD_LPIT, &v) && v == 2);
        }
        tk.fx[1] = 3;                                        /* another parameter picked: the row shows its command */
        tk_cell(t, 8, TC_FX, r);
        ok6("  another parameter picked: the row still shows HALF", str_eq(r, "HALF"));
        project_save(0);
        motion_clear(t);
        project_load(0);
        {
            int16_t a, b2, c;
            ok6("  saved with the project, loaded back", motion_lock_get(&trk[1], 8, CMD_SPD, &a) && a == 1 &&
                motion_lock_get(&trk[1], 7, CMD_LOOP, &b2) && b2 == 11 && motion_lock_get(&trk[1], 7, CMD_LPIT, &c) && c == 2);
        }
        t = &trk[1];
        track_select(1);
        go_page(GR_ROLL);
        ui.cursor = 8;
        tk.fx[1] = 0;
        for (i = 0; i < 12u && tk_col() != TC_FX; i++)
            turn(EN_K2, 1);
        press(B_EDIT);
        {
            int16_t v;
            ok6("  EDIT clears the row's SPEED", !motion_lock_get(t, 8, CMD_SPD, &v));
        }
        ok6("  a command is no parameter: the sound's values untouched as it plays", motion_param(CMD_LOOP) &&
            param_desc_of(0, CMD_LOOP)->max == 31);
    }

    printf("cmd: %s\n", fails6 ? "FAILED" : "all checks ok");
    return fails6 != 0;
}
