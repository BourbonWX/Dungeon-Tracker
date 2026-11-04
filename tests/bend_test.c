/* SPDX-License-Identifier: GPL-3.0-only
 * Dungeon Tracker 1.0: the pitch wheel records (a MIDI controller's bend, src/midi_control.c, as the track's P_BEND).
 *   build/host/bend_test                     (tests/run_rondo.sh)
 * 1. live: a bend moves the pitch at once (over the channel's 2 semitones), nothing recorded unarmed.
 * 2. REC armed, playing: a bend up on steps 4..7 and back at 8 records as BEND automation on the selected track.
 * 3. played back unarmed, the wheel untouched: the pitch rises on step 4, back down by step 8, as recorded, gliding.
 * 4. a drum track ignores the wheel; a project load and a sound load: the wheel's value as said (core.h P_BEND). */
#define main hostsim_main
#include "hostsim.c"
#undef main

static int fails;
static void check(const char *what, int ok)
{
    printf("bend: %-96s %s\n", what, ok ? "ok" : "FAIL");
    fails += !ok;
}
static void run_ms(uint32_t ms)
{
    int32_t o[2 * CTL];
    uint32_t n = FS * ms / 1000u, i;
    for (i = 0; i < n; i += CTL) {
        events_block(CTL);
        mix_block(o, CTL);
    }
}
static void until_step(track_t *t, uint32_t s)              /* the transport on until track t's step s starts */
{
    uint32_t guard = 0;
    while (t->seq_idx % 16u != s && guard++ < 20000u)
        run_ms(1);
}
static uint32_t bend_events(uint32_t k)
{
    uint32_t i, n = 0;
    for (i = 0; i < motion.count; i++)
        n += (motion.event[i].place >> 6) == k && MOTION_ID(&motion.event[i]) == P_BEND;
    return n;
}
static void wheel(uint32_t ch, int32_t v14) { midi_event(0xE0u, ch, (uint32_t)(v14 & 127), (uint32_t)(v14 >> 7)); }

int main(void)
{
    track_t *t = &trk[3];
    int32_t q_up, q_back, q_mid;
    uint32_t ev;
    fm6_init();
    host_tracks_init();
    song.g[G_ROUTE] = 0;                                    /* (CH1-4: channel 4 plays T4) */
    song.g[G_BPM] = 120;
    host_preset(t, ENGI_CTONE, 8);                          /* FM LEAD */
    song.sel = 3;

    /* 1. live */
    trk_note_on(t, 60, 100);
    wheel(3, 16383);
    run_ms(30);
    check("live: the wheel up bends T4 at once (2 semitones: 512 in Q8)", t->p[P_BEND] == 63 && midi_bend_q8[3] > 480);
    wheel(3, 8192);
    run_ms(30);
    check("  centred: back to the note; unarmed, nothing recorded", t->p[P_BEND] == 0 && midi_bend_q8[3] == 0 &&
          !bend_events(3));
    trk_note_off(t, 60);

    /* 2. record: REC on T4, playing; up on step 4, back on step 8 */
    song.rec = 1u << 3;
    seq_start();
    song.playing = 1;
    until_step(t, 1);
    until_step(t, 4);
    wheel(3, 12288);                                        /* (half up) */
    run_ms(20);
    wheel(3, 16383);                                        /* (all the way) */
    until_step(t, 8);
    run_ms(10);
    wheel(3, 8192);
    until_step(t, 12);
    ev = bend_events(3);
    check("REC armed, playing: the bend recorded as BEND automation on T4 (step 4 up, 8 back)", ev >= 2u && ev <= 4u);
    song.rec = 0;

    /* 3. play it back, the wheel untouched */
    until_step(t, 0);
    trk_note_on(t, 60, 100);
    until_step(t, 2);
    run_ms(40);
    q_mid = midi_bend_q8[3];
    until_step(t, 5);
    run_ms(60);
    q_up = midi_bend_q8[3];
    until_step(t, 9);
    run_ms(60);
    q_back = midi_bend_q8[3];
    printf("bend:   (played back: step 2 %d, step 5 %d, step 9 %d; Q8 semitones)\n", q_mid, q_up, q_back);
    check("played back unarmed: the pitch rises on step 4 as recorded and comes back down by step 8", q_mid == 0 &&
          q_up > 480 && q_back == 0 && t->p[P_BEND] == 0);
    trk_note_off(t, 60);
    song.playing = 0;
    seq_stop();
    run_ms(20);
    check("  stopped: the wheel's value back to the centre", t->p[P_BEND] == 0);

    /* 4. a drum track ignores it */
    song.playing = 0;
    host_preset(&trk[0], ENGI_CKIT, 0);
    wheel(0, 16383);
    run_ms(20);
    check("a kit (FMKIT on T1) ignores the wheel", trk[0].p[P_BEND] == 0 && midi_bend_q8[0] == 0);
    wheel(0, 8192);
    printf("bend: %s\n", fails ? "FAILED" : "all checks ok");
    return fails != 0;
}
