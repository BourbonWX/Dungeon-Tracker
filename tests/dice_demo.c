/* SPDX-License-Identifier: GPL-3.0-only
 * RE-1: DICE demos (src/ui_dice.c), through ui_test.c's harness: the real UI's dice, the real engines and mix.
 *   build/host/dice_demo OUTDIR [SEED]
 * OUTDIR/dice_kit.wav, dice_chords.wav, dice_bass.wav, dice_lead.wav: six fresh rolls (WILD 100) of that track's role,
 * each playing a 2-bar phrase twice at 120 BPM (A minor: Am7 Fmaj7 C G), a beat of silence between; dice_song.wav:
 * all four tracks together, every track rolled again each 4-bar section (four sections); patches.txt: each roll's
 * values as the DICE page names them */
#define UI_TEST_NO_MAIN 1
#include "ui_test.c"

#define BPM 120u
#define STEP (FS * 60u / BPM / 4u)                     /* a sixteenth */
static const uint8_t CH[4][4] = {{57, 60, 64, 67}, {53, 57, 60, 64}, {55, 60, 64, 67}, {55, 59, 62, 67}};
static const uint8_t ROOT[4] = {33, 29, 36, 31};
static const uint8_t LEAD[32] = {76, 0, 0, 74, 72, 0, 69, 0, 72, 0, 0, 74, 76, 0, 79, 0,
                                 76, 0, 74, 0, 72, 0, 71, 0, 72, 0, 0, 0, 0, 0, 0, 0};
static uint8_t held[NTRK][8], nheld[NTRK];

static void on(uint32_t k, uint32_t n, uint32_t v)
{
    trk_note_on(&trk[k], n, v);
    if (nheld[k] < 8u) held[k][nheld[k]++] = (uint8_t)n;
}
static void off_all(uint32_t k)
{
    while (nheld[k]) trk_note_off(&trk[k], held[k][--nheld[k]]);
}
/* step s (0..31) of the 2-bar phrase, track k's part */
static void part(uint32_t k, uint32_t s)
{
    uint32_t c = s / 8u, i;
    if (k == 0u) {                                      /* the beat: kick, snare, hats, a perc */
        off_all(0);
        if (s % 8u == 0u || s == 22u) on(0, 36, 120);
        if (s % 8u == 4u) on(0, 38, 110);
        if (s % 2u == 0u) on(0, s % 16u == 14u ? 46 : 42, s % 4u ? 80 : 95);
        if (s == 11u || s == 27u) on(0, 45, 90);
    } else if (k == 1u) {                               /* the chords: one every two beats, held 7 sixteenths */
        if (s % 8u == 7u) off_all(1);
        if (s % 8u == 0u) for (i = 0; i < 4u; i++) on(1, CH[c][i], 95);
    } else if (k == 2u) {                               /* the bass: root, octave, root on each chord */
        off_all(2);
        if (s % 8u == 0u || s % 8u == 6u) on(2, ROOT[c], 105);
        if (s % 8u == 3u) on(2, ROOT[c] + 12u, 95);
    } else {                                            /* the lead: a phrase over it */
        if (LEAD[s] || s == 28u) off_all(3);
        if (LEAD[s]) on(3, LEAD[s], 100);
    }
}
static void mixw(FILE *w, uint32_t frames)
{
    int32_t o[2 * CTL];
    uint32_t i, k;
    for (i = 0; i < frames; i += CTL) {
        mix_block(o, CTL);
        for (k = 0; k < CTL; k++)
            wav_put(w, o[2 * k], o[2 * k + 1]);
    }
}
static void quiet_all(void)
{
    uint32_t k;
    for (k = 0; k < NTRK; k++) {
        off_all(k);
        memset(trk[k].v, 0, sizeof trk[k].v);
    }
}
static FILE *rep;
static void report(uint32_t k, const char *title)
{
    uint8_t ids[DICE_VALS];
    uint32_t n = dice_ids(&trk[k], ids), i;
    fprintf(rep, "%s\n", title);
    for (i = 0; i < n; i++) {
        const param_desc_t *d = track_desc(&trk[k], ids[i]);
        char v[16];
        const char *u;
        param_format(d, trk[k].p[ids[i]], v, &u);
        fprintf(rep, "  %-8s %s%s\n", d->label, v, u ? u : "");
    }
}
static void roll(uint32_t k)
{
    track_select(k);
    dice.wild[k] = 0;                                   /* (WILD 100 %: a fresh roll) */
    dice_roll();
    trk[k].engine = trk[k].eng_req;
}
static void fresh(uint32_t seed)
{
    ui_power_on();
    song.master_q12 = 4096;
    song.g[G_BPM] = BPM;
    dice.rng = seed;
    quiet_all();
}

int main(int argc, char **argv)
{
    static const char *const NM[4] = {"dice_kit.wav", "dice_chords.wav", "dice_bass.wav", "dice_lead.wav"};
    static const char *const ROLE[4] = {"KIT (T1)", "CHORDS (T2)", "BASS (T3)", "LEAD (T4)"};
    const char *dir = argc > 1 ? argv[1] : "build/dice_demo";
    uint32_t seed = argc > 2 ? (uint32_t)strtoul(argv[2], 0, 0) : 0xD1CE2026u;
    uint32_t k, p, rep_i, s, sec;
    char path[600], t[80];
    snprintf(path, sizeof path, "%s/patches.txt", dir);
    if (!(rep = fopen(path, "w"))) { fprintf(stderr, "cannot write %s\n", path); return 1; }
    for (k = 0; k < NTRK; k++) {                        /* each role: six rolls, the phrase twice each */
        FILE *w;
        snprintf(path, sizeof path, "%s/%s", dir, NM[k]);
        if (!(w = fopen(path, "wb"))) return 1;
        wav_hdr(w, 6u * (64u * STEP + 4u * STEP));
        fresh(seed + 977u * k);
        for (p = 0; p < 6u; p++) {
            roll(k);
            snprintf(t, sizeof t, "%s roll %u", ROLE[k], p + 1u);
            report(k, t);
            for (rep_i = 0; rep_i < 2u; rep_i++)
                for (s = 0; s < 32u; s++) {
                    part(k, s);
                    mixw(w, STEP);
                }
            off_all(k);
            mixw(w, 4u * STEP);                         /* (a beat: the tail) */
            quiet_all();
        }
        fclose(w);
        fprintf(rep, "\n");
    }
    {                                                   /* the song: all four, rolled again each section */
        FILE *w;
        snprintf(path, sizeof path, "%s/dice_song.wav", dir);
        if (!(w = fopen(path, "wb"))) return 1;
        wav_hdr(w, 4u * 64u * STEP + 8u * STEP);
        fresh(seed ^ 0x5EC7u);
        for (sec = 0; sec < 4u; sec++) {
            for (k = 0; k < NTRK; k++) {
                off_all(k);
                roll(k);
                snprintf(t, sizeof t, "SONG section %u, %s", sec + 1u, ROLE[k]);
                report(k, t);
            }
            for (rep_i = 0; rep_i < 2u; rep_i++)
                for (s = 0; s < 32u; s++) {
                    for (k = 0; k < NTRK; k++)
                        part(k, s);
                    mixw(w, STEP);
                }
        }
        for (k = 0; k < NTRK; k++)
            off_all(k);
        mixw(w, 8u * STEP);
        fclose(w);
    }
    fclose(rep);
    printf("dice_demo: %s (seed 0x%08X)\n", dir, seed);
    return 0;
}
