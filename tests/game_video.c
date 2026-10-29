/* SPDX-License-Identifier: GPL-3.0-only
 * RE-1: the dungeon game recorded as video (the real UI, engines and mix, through ui_test.c's harness).
 *   build/host/game_video OUT.mp4 EVENTS.txt SECONDS [SEED] [SPARSE] [START_FLOOR]
 * A four-track song in A minor at 120 BPM plays from the sequencer with MENU > GAME > DUNGEON on; HOME is drawn.
 * Each video frame (30 a second) the audio runs 1/30 s (mix_block), then the UI a pass (ui_input: dg_tick; ui_draw),
 * and the screen goes to ffmpeg; the audio is written beside it and muxed. EVENTS.txt logs the game's moments with
 * their time: a monster comes, a fall (the track cut), a raise, a wipe, the stairs, a turn. SPARSE (default 1): T4's
 * lead is a few notes only, a weak mage, so falls come often enough to film */
#define UI_TEST_NO_MAIN 1
#include "ui_test.c"

#define FPS 30u
static void note(step_t *s, uint8_t n, uint8_t v)
{
    step_clear(s);
    s->time = ST_NOTE;
    s->n = 1;
    s->note[0] = n;
    s->vel = v;
}
static void write_song(int sparse)
{
    static const uint8_t BASS[32] = {33, 0, 33, 45, 0, 33, 0, 43, 29, 0, 29, 41, 0, 29, 0, 31,
                                     36, 0, 36, 48, 0, 36, 0, 43, 31, 0, 31, 43, 0, 31, 33, 0};
    static const uint8_t LEAD[32] = {76, 0, 0, 74, 72, 0, 69, 0, 72, 0, 0, 74, 76, 0, 79, 0,
                                     76, 0, 74, 0, 72, 0, 71, 0, 72, 0, 0, 0, 0, 0, 0, 0};
    static const uint8_t CH[4][3] = {{57, 60, 64}, {53, 57, 60}, {55, 60, 64}, {55, 59, 62}};
    uint32_t i, j;
    track_t *t;
    t = &trk[0];                                         /* T1: the kit, busy */
    track_defaults_steps(t);
    t->p[P_SLEN] = 16;
    for (i = 0; i < 16u; i++) {
        step_t *s = &t->step[i];
        step_clear(s);
        s->hit = (uint8_t)((i % 4u == 0u || i == 10u ? 1u : 0u) | (i % 8u == 4u ? 2u : 0u) | (i % 2u == 0u ? 4u : 0u) |
                           (i == 14u ? 8u : 0u));
        s->time = s->hit ? ST_NOTE : ST_REST;
        if (i == 7u || i == 15u) s->acc = 0;
    }
    t = &trk[1];                                         /* T2: chords, one a bar, held */
    track_defaults_steps(t);
    t->p[P_SLEN] = 64;
    for (i = 0; i < 64u; i++) {
        step_t *s = &t->step[i];
        step_clear(s);
        if (i % 16u == 0u) {
            s->time = ST_NOTE;
            s->n = 3;
            for (j = 0; j < 3u; j++) s->note[j] = CH[i / 16u][j];
            s->vel = 90;
        } else
            s->time = i % 16u < 14u ? ST_TIE : ST_REST;
    }
    t = &trk[2];                                         /* T3: the bass */
    track_defaults_steps(t);
    t->p[P_SLEN] = 32;
    for (i = 0; i < 32u; i++) {
        if (BASS[i]) note(&t->step[i], BASS[i], 100);
        else { step_clear(&t->step[i]); t->step[i].time = ST_REST; }
    }
    t = &trk[3];                                         /* T4: the lead (sparse: two notes, a weak mage) */
    track_defaults_steps(t);
    t->p[P_SLEN] = 32;
    for (i = 0; i < 32u; i++) {
        uint8_t n = LEAD[i];
        if (sparse && i != 0u && i != 12u) n = 0;
        if (n) note(&t->step[i], n, 100);
        else { step_clear(&t->step[i]); t->step[i].time = i && LEAD[i] == 0u && !sparse && LEAD[i - 1u] ? ST_TIE : ST_REST; }
    }
}

int main(int argc, char **argv)
{
    const char *out = argc > 1 ? argv[1] : "game.mp4", *evp = argc > 2 ? argv[2] : "game_events.txt";
    uint32_t secs = argc > 3 ? (uint32_t)atoi(argv[3]) : 120u, seed = argc > 4 ? (uint32_t)strtoul(argv[4], 0, 0) : 0x0DDBA11u;
    int sparse = argc > 5 ? atoi(argv[5]) : 1;
    uint32_t start_floor = argc > 6 ? (uint32_t)atoi(argv[6]) : 1u;   /* (a run already deep: harder monsters) */
    char wavp[600], cmd[2000];
    FILE *wav, *vid, *ev;
    static uint8_t rgb[240 * 240 * 3];
    uint32_t f, nf = secs * FPS, done = 0, i, k;
    uint8_t dead0 = 0, mon0 = 0, floor0 = 0, wipe0 = 0;
    int8_t turn0 = 0;
    snprintf(wavp, sizeof wavp, "%s.wav", out);
    if (!(wav = fopen(wavp, "wb")) || !(ev = fopen(evp, "w"))) return 1;
    wav_hdr(wav, nf * (FS / FPS));
    snprintf(cmd, sizeof cmd, "ffmpeg -y -loglevel error -f rawvideo -pix_fmt rgb24 -s 240x240 -r %u -i - "
             "-vf scale=960:960:flags=neighbor,pad=1080:1080:60:60:color=0x111114 -c:v libx264 -preset slow -crf 14 "
             "-pix_fmt yuv420p '%s.video.mp4'", FPS, out);
    if (!(vid = popen(cmd, "w"))) return 1;

    ui_power_on();
    song.master_q12 = 4096;
    song.g[G_BPM] = 120;
    write_song(sparse);
    for (k = 0; k < NTRK; k++) trk[k].engine = trk[k].eng_req;
    go_home();
    dg_game_set(1);
    dg.rng = seed;
    if (start_floor > 1u)
        dg.floor = (uint8_t)start_floor;
    seq_start();
    song.playing = 1;
    for (f = 0; f < nf; f++) {
        uint32_t want = (f + 1u) * (FS / FPS);              /* (FS / FPS samples a frame) */
        int32_t o[2 * CTL];
        while (done < want) {
            uint32_t n = want - done < CTL ? want - done : CTL;
            mix_block(o, n);
            for (i = 0; i < n; i++) wav_put(wav, o[2 * i], o[2 * i + 1]);
            done += n;
        }
        {                                                   /* the UI: a pass at this frame's time */
            uint32_t ms = (f + 1u) * 1000u / FPS;
            host_ticks += (ms - fm1_ms) * 1000u;
            fm1_ms = ms;
            ui_input();
            bank_poll();
            ui_draw();
        }
        for (i = 0; i < 240u * 240u; i++) {
            uint16_t c = swap16(host_screen[i]);
            rgb[3 * i] = (uint8_t)((c >> 11) * 255u / 31u);
            rgb[3 * i + 1] = (uint8_t)(((c >> 5) & 63u) * 255u / 63u);
            rgb[3 * i + 2] = (uint8_t)((c & 31u) * 255u / 31u);
        }
        fwrite(rgb, 1, sizeof rgb, vid);
        {                                                   /* the moments */
            double t = (double)f / FPS;
            if (dg.mon != mon0 && dg.mon) fprintf(ev, "%7.2f MONSTER %s (floor B%u)\n", t, DM_NAME[dg.mon], dg.floor);
            for (k = 0; k < NTRK; k++) {
                if (((dg.dead >> k) & 1u) && !((dead0 >> k) & 1u)) fprintf(ev, "%7.2f FALL T%u (%s) CUT\n", t, k + 1u, DG_CLASS[k]);
                if (!((dg.dead >> k) & 1u) && ((dead0 >> k) & 1u)) fprintf(ev, "%7.2f RAISE T%u (%s) BACK\n", t, k + 1u, DG_CLASS[k]);
            }
            if (dg.wipe && !wipe0) fprintf(ev, "%7.2f WIPE (the whole party: a bar of silence)\n", t);
            if (dg.floor != floor0 && f) fprintf(ev, "%7.2f FLOOR B%u\n", t, dg.floor);
            if (dg.turn && !turn0) fprintf(ev, "%7.2f TURN %s\n", t, dg.turn < 0 ? "LEFT" : "RIGHT");
            dead0 = dg.dead; mon0 = dg.mon; floor0 = dg.floor; wipe0 = dg.wipe; turn0 = dg.turn;
        }
    }
    pclose(vid);
    fclose(wav);
    fclose(ev);
    snprintf(cmd, sizeof cmd, "ffmpeg -y -loglevel error -i '%s.video.mp4' -i '%s' -c:v copy -c:a aac -b:a 256k -shortest '%s'",
             out, wavp, out);
    return system(cmd) != 0;
}
