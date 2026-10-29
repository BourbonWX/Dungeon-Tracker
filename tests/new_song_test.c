/* SPDX-License-Identifier: GPL-3.0-only
 * Dungeon Tracker: SAVE > TOOLS' NEW and NEW + DEFAULT SOUNDS (project.c new_song, new_undo) through ui_test.c's harness.
 *   build/host/new_song_test        (tests/run_rondo.sh)
 * 1. KNOB 1 on TOOLS: right CLEAR, NEW, NEW+S (stays), left back NEW, CLEAR, none, one stage a turn (a fast turn picks
 *    CLEAR); the footer's action NEW.
 * 2. NEW (the power-on song, OCT+ asks, OCT+ again): every track empty (steps, automation), the song order empty, the
 *    other patterns' keys empty, the name gone; the sounds, the tempo kept.
 * 3. SAVE held right after: the song back (steps, sounds, the song order, the patterns' keys, the name).
 * 4. NEW + DEFAULT SOUNDS: the same, and the power-on sounds (TRK_DEF); SAVE held: the sounds back too.
 * 5. a note written after a NEW: SAVE held no longer undoes it (NOTHING TO UNDO; the note stays).
 * 6. stopped only for a SONG playing; with the transport running a pattern, NEW still works. */
#define UI_TEST_NO_MAIN 1
#include "ui_test.c"
#include "../firmware/src/boot_song.c"

static int fails;
static void okn(const char *what, int ok) { fails += check(what, ok); }
static char what[200];

static uint32_t steps_all(void)
{
    uint32_t k, n = 0;
    for (k = 0; k < NTRK; k++)
        n += !seq_is_empty(&trk[k]);
    return n;
}
static uint32_t sound_sig(void)
{
    uint32_t h = 2166136261u, k;
    for (k = 0; k < NTRK; k++) {
        h = as_mix(h, &trk[k].eng_req, 1);
        h = as_mix(h, &trk[k].preset, 1);
        h = as_mix(h, &trk[k].p[P_E0], 8 * sizeof trk[k].p[0]);
    }
    return h;
}
static uint32_t music_sig(void)
{
    uint32_t h = 2166136261u, k;
    for (k = 0; k < NTRK; k++)
        h = as_mix(h, trk[k].step, sizeof trk[k].step);
    h = as_mix(h, &motion, sizeof motion);
    return as_mix(h, &chain_config, sizeof chain_config);
}
static void to_tools(void)
{
    go_title("TOOLS");
    frames(3);
}
static void do_new(uint32_t which)                     /* 1 NEW, 2 NEW + DEFAULT SOUNDS */
{
    to_tools();
    turn(EN_K1, 1);
    frame();
    for (; which; which--) {
        turn(EN_K1, 1);
        frame();
    }
    press(B_OCTUP);
    frames(2);
    press(B_OCTUP);
    frames(3);
}
/* a song with something everywhere NEW clears: the power-on song, a sound changed off its default, a song order,
 * a second pattern stored, a name */
static void a_song(void)
{
    ui_power_on();
    boot_song(4242u);
    boot_song_settle();
    frames(2);
    chain_config.count = 2;
    chain_config.row[0] = chain_row_of(0, 2);
    chain_config.row[1] = chain_row_of(3, 1);
    bank_take(3);                                        /* (D: a copy of it) */
    str_cpy(proj_name, "MY SONG", sizeof proj_name);
}

int main(void)
{
    uint32_t snd, mus, k;
    int16_t bpm;

    /* 1. */
    a_song();
    to_tools();
    {
        char seq[6][8];
        int32_t dir[6] = {1, 1, 1, 1, -1, -1};
        for (k = 0; k < 6u; k++) {
            turn(EN_K1, dir[k]);
            frame();
            str_cpy(seq[k], ui.act != 1u ? "-" : ui.tl_new == 2u ? "NEW+S" : ui.tl_new ? "NEW" : "CLEAR", sizeof seq[k]);
        }
        turn(EN_K1, -1);
        frame();
        snprintf(what, sizeof what, "KNOB 1 on TOOLS: %s %s %s %s, back %s %s, then %s", seq[0], seq[1], seq[2], seq[3],
                 seq[4], seq[5], ui.act ? "still on" : "none");
        okn(what, str_eq(seq[0], "CLEAR") && str_eq(seq[1], "NEW") && str_eq(seq[2], "NEW+S") && str_eq(seq[3], "NEW+S") &&
            str_eq(seq[4], "NEW") && str_eq(seq[5], "CLEAR") && !ui.act);
        turn(EN_K1, 3);                                  /* (a fast turn: CLEAR, one stage a turn) */
        frame();
        k = ui.act == 1u && !ui.tl_new;
        turn(EN_K1, 1);
        frame();
        okn("  a fast turn picks CLEAR (never NEW at once)", k);
        okn("  the footer's action: NEW", str_eq(act_name(0), "NEW") && act_ready());
        turn(EN_K1, -1);
        frame();
        turn(EN_K1, -1);
        frame();
    }

    /* 2. NEW */
    a_song();
    snd = sound_sig();
    mus = music_sig();
    bpm = song.g[G_BPM];
    okn("the song: four tracks of steps, a song order, a second pattern, a name",
        steps_all() == 4u && chain_config.count == 2u && bank_has(3) && proj_name[0]);
    do_new(1);
    snprintf(what, sizeof what, "NEW: tracks with steps %u, song rows %u, D %s, name '%s', '%s'", steps_all(),
             chain_config.count, bank_has(3) ? "kept" : "empty", proj_name, ui.msg);
    okn(what, !steps_all() && !chain_config.count && !bank_has(3) && !proj_name[0] && !motion_count(&trk[0]) &&
              ui.confirm == 0u && str_eq(ui.msg, "NEW SONG"));
    okn("  the sounds and the tempo kept", sound_sig() == snd && song.g[G_BPM] == bpm);

    /* 3. undo */
    hold(B_SAVE);
    frames(3);
    snprintf(what, sizeof what, "SAVE held: the song back ('%s'; name '%s', D %s)", ui.msg, proj_name, bank_has(3) ? "back" : "gone");
    okn(what, music_sig() == mus && sound_sig() == snd && bank_has(3) && str_eq(proj_name, "MY SONG") &&
              str_eq(ui.msg, "NEW UNDONE"));

    /* 4. NEW + DEFAULT SOUNDS */
    a_song();
    for (k = 1; k < NTRK; k++)                           /* (the power-on song's own sounds: off TRK_DEF) */
        apply_preset_to(&trk[k], 3u);
    frames(2);
    snd = sound_sig();
    mus = music_sig();
    do_new(2);
    {
        uint32_t def = 1;
        for (k = 0; k < NTRK; k++)
            def &= trk[k].eng_req == TRK_DEF[k][0] && trk[k].preset == preset_orig(ENGINES[TRK_DEF[k][0]], TRK_DEF[k][1]);
        snprintf(what, sizeof what, "NEW + DEFAULT SOUNDS: empty, the power-on sounds (%s), '%s'", def ? "yes" : "no", ui.msg);
        okn(what, !steps_all() && !chain_config.count && def && song.g[G_BPM] == bpm && str_eq(ui.msg, "NEW SONG + SOUNDS"));
    }
    hold(B_SAVE);
    frames(3);
    okn("  SAVE held: the song and its sounds back", music_sig() == mus && sound_sig() == snd);

    /* 5. a change after NEW */
    a_song();
    do_new(1);
    trk[1].step[0].time = ST_NOTE;
    trk[1].step[0].n = 1;
    trk[1].step[0].note[0] = 60;
    hold(B_SAVE);
    frames(3);
    snprintf(what, sizeof what, "a note after NEW: SAVE held leaves it ('%s')", ui.msg);
    okn(what, steps_all() == 1u && str_eq(ui.msg, "NOTHING TO UNDO"));

    /* 6. playing */
    a_song();
    seq_start();
    song.playing = 1;
    frames(10);
    do_new(1);
    okn("playing a pattern: NEW clears it (the transport on)", !steps_all() && song.playing);
    song.playing = 0;
    seq_stop();
    frames(3);

    printf("new_song: %s\n", fails ? "FAILED" : "all checks ok");
    return fails != 0;
}
