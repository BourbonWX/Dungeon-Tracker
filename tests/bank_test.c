/* SPDX-License-Identifier: GPL-3.0-only
 * RONDO: the PATTERN BANK (src/song_bank.c, ui_layer.c LAYER_ENV) and SONG on it, through ui_test.c's harness.
 *   build/host/bank_test              (tests/run_rondo.sh)
 * 1. power-on: A current, the rest empty. ENV held: the PATTERNS layer; an empty key (G3) copies the pattern in and is
 *    current from then on; A keeps it as it was.
 * 2. stopped, a key with a pattern: there at once; the edits stay in their key (B's in B, A as left); the header's
 *    letter; an undo of the steps from the other pattern goes (a sound's stays).
 * 3. playing: the key queues (NEXT, "A>B"); at the end of A's loop B's step 1, on time to the sample (the block's
 *    carry), every track from its step 1 together, nothing skipped.
 * 4. the pattern's end is its longest track's loop (T2 32 steps under T1's 16: after 32).
 * 5. the queued key again, or OCT- tapped: staying; STOP with one queued: it now; PLAY with one queued: from it.
 * 6. OCT+ held + a key: a copy (the current one stays); OCT- held + a key: cleared (not the current one's).
 * 7. SONG on the bank: KNOB 3 turns A .. F then "-"; sections of A, B, E play those patterns; an empty one refused
 *    ("PATTERN F EMPTY"); a song stores the current one first; the ENV keys say SONG PLAYING meanwhile.
 * 8. the map, the cards, the LEDs drawn, every palette.
 * 9. RECORD THE ARRANGEMENT: on SONG, REC armed; PLAY plays the patterns; A twice, B once, A to STOP: three sections
 *    (A x2, B x1, A x1), the song before replaced.
 * 10. MIDI: a Program Change on channel 16 (program 2: B) goes to B stopped, queues it playing; on channel 1 nothing. */
#define UI_TEST_NO_MAIN 1
#include "ui_test.c"

static int fails8;
static void ok8(const char *what, int ok) { fails8 += check(what, ok); }
static char what[240];

static void line(track_t *t, uint32_t len, uint8_t note)   /* len steps of 1/16, every one `note` */
{
    uint32_t i;
    track_defaults_steps(t);
    t->p[P_SLEN] = (int16_t)len;
    for (i = 0; i < len; i++) {
        step_t *s = &t->step[i];
        step_clear(s);
        s->time = ST_NOTE;
        s->n = 1;
        s->note[0] = note;
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
    line(&trk[0], 16, 60);
}
static void bkey(uint32_t k) { key_down(k); frame(); key_up(k); frames(30); }   /* a key in a layer (the ISR's) */
static void env_key(uint32_t p)                        /* ENV held, white key p (F3 = 0), ENV let go */
{
    btn_down(B_ENV);
    frames(700);
    bkey(white(p));
    frames(50);
    btn_up(B_ENV);
    frames(100);
}
static void env_down(void) { btn_down(B_ENV); frames(700); }
static void env_up(void) { btn_up(B_ENV); frames(100); }
static void play_blocks(uint32_t n) { while (n--) events_block(CTL); }

int main(void)
{
    track_t *t = &trk[0];
    char sl[4];

    /* 1. */
    fresh();
    ok8("power-on: A current, B..F empty", bank.cur == 0u && bank.used == 1u && bank.queue == CHAIN_OFF);
    env_down();
    ok8("ENV held: the PATTERNS layer", ui.layer == LAYER_ENV);
    bkey(white(1));
    frames(50);
    ok8("  G3 (empty B): the pattern copied in, B current (NEW PATTERN B)", bank.cur == 1u && bank.used == 3u &&
        !memcmp(bank_pat[1].step[0], t->step, sizeof t->step) && msg_is("NEW PATTERN B"));
    ok8("  A keeps it as it was", !memcmp(bank_pat[0].step[0], t->step, sizeof t->step));
    env_up();
    ok8("ENV let go: no ENV page", !ui.layer && str_eq(cur_page()->title, PAGES[ui.page].title));

    /* 2. stopped */
    t->step[3].note[0] = 72;                            /* an edit on B */
    t->p[P_SLEN] = 12;
    step_undo_take(t, 0);
    t->step[5].note[0] = 74;
    motion_undo_done(t);
    env_key(0);
    ok8("stopped, F3 (A): A at once (its 16 steps of note 60)", bank.cur == 0u && t->step[3].note[0] == 60 && t->p[P_SLEN] == 16);
    ok8("  B kept its edits (step 4 note 72, LEN 12)", bank_pat[1].step[0][3].note[0] == 72 && bank_pat[1].timing[0][0] == 12);
    ok8("  the undo of B's step edit went (SAVE held: nothing)", undo.trk == 0u);
    head_slot(sl);
    ok8("  the header's letter: A", sl[0] == 'A' && !sl[1]);
    env_key(1);
    ok8("  back to B: its edits", bank.cur == 1u && t->step[3].note[0] == 72 && t->step[5].note[0] == 74 && t->p[P_SLEN] == 12);
    env_key(0);

    /* 3. playing: the switch at the loop's end */
    fresh();
    env_key(1);                                         /* B = A */
    line(t, 16, 72);                                    /* B: note 72 */
    env_key(0);                                         /* A: note 60 */
    {
        uint32_t step = step_samples(t, div_samples((uint32_t)t->p[P_SDIV]), 0), pos = 0, at15 = 0, at0 = 0, b, prev = 0xFFFF;
        uint32_t seen = 0, skip = 0;
        seq_start();
        song.playing = 1;
        play_blocks(4);
        env_down();
        bkey(white(1));
        frames(50);
        head_slot(sl);
        ok8("playing, G3 (B): queued (NEXT: B), A still plays, the header \"A>B\"", bank.queue == 1u && bank.cur == 0u &&
            msg_is("NEXT: B") && sl[0] == 'A' && sl[1] == '>' && sl[2] == 'B');
        env_up();
        for (b = 0; b < 40000u && !at0; b++) {
            events_block(CTL);
            pos += CTL;
            if (t->seq_idx != prev) {
                if (prev != 0xFFFFu && t->seq_idx != (prev + 1u) % 16u)
                    skip++;
                prev = t->seq_idx;
                if (t->seq_idx == 15u) at15 = pos - CTL + t->seq_pos % CTL;
                if (t->seq_idx == 0u && bank.cur == 1u) at0 = pos - CTL + t->seq_pos % CTL;
                seen++;
            }
        }
        snprintf(what, sizeof what, "  at A's end B's step 1 (note %u), %u samples after step 16 (a step: %u), none skipped",
                 t->step[0].note[0], at0 - at15, step);
        ok8(what, bank.cur == 1u && t->step[0].note[0] == 72 && skip == 0u && at0 > at15 &&
            (at0 - at15 + CTL >= step && at0 - at15 <= step + CTL));
        ok8("  every track from its step 1 together", trk[1].seq_idx == 0u && trk[2].seq_idx == 0u && trk[3].seq_idx == 0u);
        play_blocks(2000);
        ok8("  B goes on (no more switches)", bank.cur == 1u && bank.queue == CHAIN_OFF);
        song.playing = 0;
        seq_stop();
    }

    /* 4. the longest track */
    fresh();
    env_key(1);
    line(t, 16, 72);
    env_key(0);
    line(&trk[1], 32, 50);                              /* T2: two bars under T1's one */
    {
        uint32_t loops = 0, b, prev = 0;
        seq_start();
        song.playing = 1;
        play_blocks(4);
        env_key(1);
        for (b = 0; b < 80000u && bank.cur == 0u; b++) {
            events_block(CTL);
            if (t->seq_idx == 0u && prev == 15u)
                loops++;
            prev = t->seq_idx;
        }
        snprintf(what, sizeof what, "T2 32 steps under T1's 16: the switch as T1 comes round the %u%s time (T2's loop: the pattern's end)",
                 loops, loops == 2u ? "nd" : "th");
        ok8(what, bank.cur == 1u && loops == 2u && bank.clk == 1u);
        song.playing = 0;
        seq_stop();
    }

    /* 5. staying, STOP, PLAY */
    fresh();
    env_key(1);
    env_key(0);
    seq_start();
    song.playing = 1;
    env_key(1);
    env_key(1);
    ok8("the queued key again: staying (STAYING ON A)", bank.queue == CHAIN_OFF && bank.cur == 0u && msg_is("STAYING ON A"));
    env_key(1);
    env_down();
    press(B_OCTDN);
    frames(50);
    ok8("  OCT- tapped in the layer: staying, no octave", bank.queue == CHAIN_OFF && song.octave == 0);
    bkey(white(1));
    frames(50);
    env_up();
    song.playing = 0;
    seq_stop();
    frames(50);
    ok8("STOP with B queued: B now", bank.cur == 1u && bank.queue == CHAIN_OFF);
    bank.queue = 0;                                     /* (queued as the ISR saw it at PLAY) */
    seq_start();
    ok8("PLAY with A queued: from A", bank.cur == 0u && bank.queue == CHAIN_OFF);
    song.playing = 0;
    seq_stop();

    /* 6. copy, clear */
    fresh();
    t->step[2].note[0] = 65;
    env_down();
    btn_down(B_OCTUP);
    frame();
    bkey(white(4));
    btn_up(B_OCTUP);
    frames(50);
    ok8("OCT+ held + C4 (E): A copied to E, A stays current", bank.cur == 0u && bank_has(4) &&
        bank_pat[4].step[0][2].note[0] == 65 && msg_is("COPIED A TO E") && song.octave == 0);
    btn_down(B_OCTDN);
    frame();
    bkey(white(4));
    frames(50);
    ok8("  OCT- held + C4: E cleared", !bank_has(4) && msg_is("CLEARED E"));
    bkey(white(0));
    btn_up(B_OCTDN);
    frames(50);
    ok8("  OCT- held + F3 (A, current): refused", bank_has(0) && bank.cur == 0u && song.octave == 0);
    env_up();

    /* 7. SONG */
    fresh();
    env_key(1);
    line(t, 8, 72);                                     /* B: 8 steps of 72 */
    env_key(4);                                         /* E: (B copied) */
    line(t, 4, 80);                                     /* E: 4 steps of 80 */
    env_key(0);                                         /* A: 16 of 60 */
    {
        uint32_t i, o, v = 0, seen[3] = {0, 0, 0}, b;
        go_page(GR_SONG);
        frames(400);                                    /* (KNOB 1..4 quiet after a layer: LY_QUIET_MS) */
        ui.song_row = 0;
        turn(EN_K3, 1);                                 /* (+ ADD: the section) */
        frames(50);
        for (o = 0; o < 7u; o++) {
            v |= (uint32_t)chain_config.row[0].slot[ui.song_trk % NTRK] << (4u * o);
            turn(EN_K3, 1);
            frames(30);
        }
        ok8("SONG KNOB 3: A B C D E F then - (and stays)", chain_ord(chain_config.row[0].slot[0]) == CHAIN_PATS &&
            chain_pat((v >> 16) & 15u) == 4u && chain_pat((v >> 20) & 15u) == 5u && ((v >> 24) & 15u) == CHAIN_SILENT);
        chain_config.count = 3;
        chain_config.row[0] = chain_row_of(0, 1);
        chain_config.row[1] = chain_row_of(1, 1);
        chain_config.row[2] = chain_row_of(chain_val(4), 1);
        chain_config.row[2].slot[3] = chain_val(5);
        ok8("  a section on F (empty): refused (PATTERN F EMPTY)", chain_prepare() == 3u + 5u);
        chain_config.row[2].slot[3] = CHAIN_SILENT;
        t->step[0].note[0] = 61;                        /* A as it is now: a song stores it first */
        ok8("  A B E: prepared", chain_prepare() == 0u && bank_pat[0].step[0][0].note[0] == 61);
        frames(50);
        for (b = 0; b < 200000u && (song.playing || chain_busy()); b++) {
            events_block(CTL);
            if (t->seq_n) {
                uint32_t n = t->seq_notes[0];
                seen[0] |= n == 61u;
                seen[1] |= n == 72u;
                seen[2] |= n == 80u;
            }
            if (b == 100u) {
                env_key(2);
                ok8("  ENV keys while the song plays: SONG PLAYING", msg_is("SONG PLAYING") && bank.queue == CHAIN_OFF);
            }
        }
        ok8("  played A's 61, B's 72, E's 80; stopped at the end, A still current", seen[0] && seen[1] && seen[2] &&
            !song.playing && bank.cur == 0u && t->step[0].note[0] == 61);
        for (i = 0; i < NPALETTES; i++) {
            palette_set(i);
            ui.force = 1;
            frame();
        }
        ok8("  SONG drawn on the bank, every palette", 1);
    }

    /* 8. the map */
    {
        uint32_t p, br = 0, lit;
        env_key(1);
        seq_start();
        song.playing = 1;
        env_down();
        bkey(white(4));
        frames(50);
        lit = layer_leds(&br);
        ok8("the LEDs: B lit, A and E (stored) breathe or flash, D3 (empty C) dark", ((lit >> white(1)) & 1u) &&
            ((br >> white(0)) & 1u) && !(((lit | br) >> white(2)) & 1u));
        for (p = 0; p < NPALETTES; p++) {
            palette_set(p);
            ui.force = 1;
            frame();
        }
        env_up();
        song.playing = 0;
        seq_stop();
        ok8("the PATTERNS map drawn, every palette", 1);
    }

    /* 9. the arrangement */
    fresh();
    env_key(1);
    line(t, 16, 72);
    env_key(0);
    chain_config.count = 2;                             /* (a song before: replaced) */
    chain_config.row[0] = chain_row_of(2, 3);
    go_page(GR_SONG);
    frames(400);
    press(B_REC);
    frames(50);
    ok8("SONG, REC tapped: the arrangement armed (no track armed)", arr.armed && !song.rec && !arr.on);
    press(B_PLAY);
    frames(20);
    events_block(CTL);
    ok8("  PLAY: the patterns play (no song), recording", song.playing && !chain.running && arr.on);
    {
        uint32_t wraps = 0, prev = 0, b;
        for (b = 0; b < 200000u && wraps < 2u; b++) {   /* A: two whole loops, B queued in the second */
            events_block(CTL);
            if (t->seq_idx == 0u && prev == 15u) wraps++;
            prev = t->seq_idx;
            if (wraps == 1u && t->seq_idx == 4u && bank.queue == CHAIN_OFF && bank.cur == 0u) env_key(1);
        }
        for (b = 0; b < 200000u && bank.cur == 1u; b++) {   /* B: once, then back to A */
            events_block(CTL);
            if (t->seq_idx == 4u && bank.queue == CHAIN_OFF) env_key(0);
        }
        for (b = 0; b < 3000u; b++)
            events_block(CTL);                          /* (A: part of a loop) */
        press(B_PLAY);
        frames(50);
        events_block(CTL);
        frames(50);
    }
    snprintf(what, sizeof what, "  STOP: %u sections: %c x%u, %c x%u, %c x%u", chain_config.count,
             'A' + chain_pat(chain_config.row[0].slot[0]), chain_config.row[0].repeat,
             'A' + chain_pat(chain_config.row[1].slot[0]), chain_config.row[1].repeat,
             'A' + chain_pat(chain_config.row[2].slot[0]), chain_config.row[2].repeat);
    ok8(what, !arr.on && chain_config.count == 3u && chain_pat(chain_config.row[0].slot[0]) == 0u &&
        chain_config.row[0].repeat == 2u && chain_pat(chain_config.row[1].slot[3]) == 1u && chain_config.row[1].repeat == 1u &&
        chain_pat(chain_config.row[2].slot[2]) == 0u && chain_config.row[2].repeat == 1u && msg_is("3 SECTIONS RECORDED"));
    ok8("  it plays as a song (PLAY on SONG)", chain_prepare() == 0u);
    transport_req = 0;
    chain.armed = 0;

    /* 10. MIDI Program Change */
    fresh();
    env_key(1);
    env_key(0);
    midi_event(0xC0u, 15u, 1u, 0u);
    frames(50);
    ok8("MIDI channel 16, program 2, stopped: B", bank.cur == 1u);
    seq_start();
    song.playing = 1;
    midi_event(0xC0u, 15u, 0u, 0u);
    frames(50);
    ok8("  playing, program 1: A queued", bank.queue == 0u && bank.cur == 1u);
    bank.queue = CHAIN_OFF;
    midi_event(0xC0u, 0u, 0u, 0u);
    midi_event(0xC0u, 15u, 4u, 0u);                     /* (E: empty) */
    frames(50);
    ok8("  channel 1 (a track's): no pattern; program 5 (E, empty): nothing", bank.queue == CHAIN_OFF && bank.cur == 1u);
    song.playing = 0;
    seq_stop();

    printf("bank: %s\n", fails8 ? "FAILED" : "all checks ok");
    return fails8 != 0;
}
