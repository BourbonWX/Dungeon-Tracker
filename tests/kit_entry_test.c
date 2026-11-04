/* SPDX-License-Identifier: GPL-3.0-only
 * Dungeon Tracker: drums written one at a time land on the same step (ui_input.c seq_entry_kit), through ui_test.c.
 *   build/host/kit_entry_test        (tests/run_rondo.sh)
 * 1. FMKIT, step entry (armed, stopped): a kick then a hat, one after the other: one each on steps 1 and 2 (letting go
 *    moves on, as before); back to step 1 and the hat: the kick stays, the hat joins it; the same key again: still one
 *    hit (nothing doubled).
 * 2. both keys together: one step, both drums.
 * 3. live recording (playing): a kick and a hat in the same step: both (as before).
 * 4. a melodic track: step entry still writes the step anew (a bass line's note replaces the one there). */
#define UI_TEST_NO_MAIN 1
#include "ui_test.c"

static int fails;
static void okk(const char *what, int ok) { fails += check(what, ok); }
static int key_of(const track_t *t, uint32_t note)
{
    uint32_t k;
    for (k = 0; k < 27u; k++)
        if (kb_map(t, k) == note)
            return (int)k;
    return -1;
}
static uint32_t lanes(uint32_t i) { return step_lanes(&TSEL->step[i]); }

int main(void)
{
    int kk, kh, kn;
    uint32_t i;
    char what[160];
    ui_power_on();
    track_select(0);
    for (i = 0; i < NSTEP; i++)
        step_clear(&trk[0].step[i]);
    kk = key_of(&trk[0], 36);
    kh = key_of(&trk[0], 42);
    open_family(FAM_SEQ);
    frames(3);
    press(B_REC);
    frames(2);
    song.playing = 0;
    transport_req = 0;
    okk("FMKIT on T1: a kick key and a closed-hat key", kk >= 0 && kh >= 0 && drum_track(&trk[0]));

    ui.cursor = 0;
    tap_key((uint32_t)kk);
    tap_key((uint32_t)kh);
    snprintf(what, sizeof what, "step entry: a kick, then a hat: steps 1 and 2 (%02x %02x), the cursor on 3", lanes(0), lanes(1));
    okk(what, lanes(0) == 1u && lanes(1) == 8u && ui.cursor == 2u);
    ui.cursor = 0;
    tap_key((uint32_t)kh);
    snprintf(what, sizeof what, "  back to step 1, the hat: it joins the kick (%02x)", lanes(0));
    okk(what, lanes(0) == 9u && trk[0].step[0].time == ST_NOTE);
    ui.cursor = 0;
    tap_key((uint32_t)kh);
    okk("  the hat again: still one hat, one kick", lanes(0) == 9u && trk[0].step[0].n == 2u);

    ui.cursor = 4;
    fm1_in.notes |= 1u << kk | 1u << kh;
    host_notes = fm1_in.notes;
    frame();
    fm1_in.notes = 0;
    frame();
    okk("both keys together: one step, both drums", lanes(4) == 9u && lanes(5) == 0u && ui.cursor == 5u);

    trk[0].seq_idx = 11;
    trk[0].seq_pos = 0;
    rec_note(&trk[0], 36, 100);
    rec_note(&trk[0], 42, 100);
    okk("live: a kick and a hat in one step, both", lanes(11) == 9u);

    track_select(2);                                        /* T3, the bass */
    go_title("TRACKER");
    frames(3);
    for (i = 0; i < NSTEP; i++)
        step_clear(&trk[2].step[i]);
    song.rec = 1u << 2;
    kn = key_of(&trk[2], kb_map(&trk[2], 7));
    ui.cursor = 0;
    tap_key(7);
    ui.cursor = 0;
    tap_key(9);
    snprintf(what, sizeof what, "a melodic track: step entry still replaces the step (%u note, %u)", trk[2].step[0].n, trk[2].step[0].note[0]);
    okk(what, trk[2].step[0].n == 1u && trk[2].step[0].note[0] == kb_map(&trk[2], 9));
    (void)kn;
    printf("kit_entry: %s\n", fails ? "FAILED" : "all checks ok");
    return fails != 0;
}
