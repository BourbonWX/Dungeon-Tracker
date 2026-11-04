/* SPDX-License-Identifier: GPL-3.0-only
 * Dungeon Tracker 1.0: the power-on fanfare (src/jingle.c), through fx.c's mix as on the FM-1.
 *   build/host/jingle_test [WAV]             (tests/run_rondo.sh; WAV: write it, 4 s stereo)
 * 1. it sounds, about as loud as music (within 6 dB of a lead, a pad chord and a bass), no DC, never full scale,
 *    and ends by itself in about three seconds (3.0 .. 3.9 s);
 * 2. a stop (a key or a button under the splash, main.c) fades it out within 0.1 s. */
#define main hostsim_main
#include "hostsim.c"
#undef main

static int fails;
static void check(const char *what, int ok)
{
    printf("jingle: %-94s %s\n", what, ok ? "ok" : "FAIL");
    fails += !ok;
}
static double rms_of(uint32_t secs_x10, int32_t *pk, double *dc, uint32_t *ended, FILE *w)
{
    int32_t o[2 * CTL];
    uint32_t i, k, n = FS * secs_x10 / 10u;
    double s = 0, m = 0;
    *pk = 0;
    *ended = 0;
    for (i = 0; i < n; i += CTL) {
        mix_block(o, CTL);
        for (k = 0; k < 2u * CTL; k++) {
            int32_t v = o[k];
            if (abs(v) > *pk) *pk = abs(v);
            if (i < FS * 25u / 10u) s += (double)v * v, m += v;
            if (w) { int16_t q = (int16_t)(v > 32767 ? 32767 : v < -32767 ? -32767 : v); fwrite(&q, 2, 1, w); }
        }
        if (!*ended && !jingle_on()) *ended = i + CTL;
    }
    *dc = m / (FS * 5.0) / 32768.0;
    return sqrt(s / (FS * 5.0));
}
int main(int argc, char **argv)
{
    int32_t pk, pk2;
    double r, rr, dc, dc2;
    uint32_t end, end2, i;
    FILE *w = argc > 1 ? fopen(argv[1], "wb") : 0;
    fm6_init();
    host_tracks_init();
    song.master_q12 = 2048;
    if (w) for (i = 0; i < 44; i++) fputc(0, w);
    jingle_start();
    r = rms_of(40, &pk, &dc, &end, w);
    if (w) {
        uint32_t d = FS * 4u * 4u, sr = FS, br = FS * 4u, c = 36 + d, fl = 16; uint16_t a = 1, ch = 2, ba = 4, bits = 16;
        fseek(w, 0, SEEK_SET); fwrite("RIFF", 1, 4, w); fwrite(&c, 4, 1, w); fwrite("WAVEfmt ", 1, 8, w); fwrite(&fl, 4, 1, w);
        fwrite(&a, 2, 1, w); fwrite(&ch, 2, 1, w); fwrite(&sr, 4, 1, w); fwrite(&br, 4, 1, w); fwrite(&ba, 2, 1, w);
        fwrite(&bits, 2, 1, w); fwrite("data", 1, 4, w); fwrite(&d, 4, 1, w); fclose(w);
    }
    host_tracks_init();
    host_preset(&trk[3], ENGI_CTONE, 8); host_preset(&trk[1], ENGI_CTONE, 0); host_preset(&trk[2], ENGI_CTONE, 5);
    trk_note_on(&trk[3], 72, 100); trk_note_on(&trk[2], 36, 100);
    for (i = 0; i < 3u; i++) trk_note_on(&trk[1], 60u + 4u * i - (i == 2u), 100);
    rr = rms_of(25, &pk2, &dc2, &end2, 0);
    printf("jingle:   (rms %.0f, a lead + pad + bass %.0f: %.1f dB; peak %d; DC %.4f; ended at %.2f s)\n", r, rr,
           20.0 * log10(r / rr), pk, dc, end / (double)FS);
    check("it sounds about as loud as music (within 6 dB), no DC, never full scale", fabs(20.0 * log10(r / rr)) < 6.0 &&
          fabs(dc) < 0.002 && pk < 32767);
    check("  and ends by itself in about three seconds", end > FS * 3u && end < FS * 39u / 10u);
    host_tracks_init();
    jingle_start();
    rms_of(5, &pk, &dc, &end, 0);
    jingle_stop();
    rms_of(2, &pk, &dc, &end, 0);
    check("a stop (a key or a button under the splash) fades it out within 0.1 s", !jingle_on() && end < FS / 10u);
    printf("jingle: %s\n", fails ? "FAILED" : "all checks ok");
    return fails != 0;
}
