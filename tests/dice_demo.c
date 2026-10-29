/* SPDX-License-Identifier: GPL-3.0-only
 * RE-1: DICE demos (src/ui_dice.c), through ui_test.c's harness: the real UI's dice, the real engines and mix.
 *   build/host/dice_demo OUTDIR [SEED]
 * Each SEED composes its own music (compose): a key and scale, a tempo, a four-chord progression, a drum groove, a
 * bassline, a chord rhythm and a melody (a motif, varied, answered, closed on the root).
 * OUTDIR/dice_kit.wav, dice_chords.wav, dice_bass.wav, dice_lead.wav: six fresh rolls (WILD 100) of that track's role,
 * each playing 4 bars of its part, a beat of silence between; dice_song.wav: the song, 4 sections of 8 bars (intro:
 * drums and bass; full; breakdown: chords, lead, hats; full again with the melody an octave up), every track rolled
 * again each section; patches.txt: the music and each roll's values as the DICE page names them */
#define UI_TEST_NO_MAIN 1
#include "ui_test.c"

/* ---- the composer ---- */
static uint32_t rs;
static uint32_t rnd(uint32_t n) { rs ^= rs << 13; rs ^= rs >> 17; rs ^= rs << 5; return n ? rs % n : 0u; }
static const int8_t SCALES[4][7] = {{0, 2, 3, 5, 7, 8, 10}, {0, 2, 3, 5, 7, 9, 10}, {0, 2, 4, 5, 7, 9, 11}, {0, 2, 4, 5, 7, 9, 10}};
static const char *const SCALE_NM[4] = {"minor", "dorian", "major", "mixolydian"};
static const char *const KEY_NM[12] = {"C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B"};
static const uint8_t PROGS[4][4][4] = {   /* scale degrees (0 = I), per scale */
    {{0, 5, 2, 6}, {0, 3, 4, 0}, {0, 6, 5, 6}, {0, 3, 5, 4}},
    {{0, 3, 0, 3}, {0, 6, 3, 0}, {0, 1, 3, 6}, {0, 3, 6, 4}},
    {{0, 4, 5, 3}, {0, 3, 4, 4}, {0, 5, 3, 4}, {5, 3, 0, 4}},
    {{0, 6, 3, 0}, {0, 3, 6, 3}, {0, 6, 0, 3}, {3, 6, 0, 0}}};
static const char *const DRUM_NM[4] = {"four on the floor", "breakbeat", "half-time", "electro"};
static const char *const BASS_NM[4] = {"offbeat eighths", "syncopated", "long notes", "arpeggio"};
static const char *const CHRD_NM[3] = {"held", "stabs", "offbeat pulses"};
static struct {
    uint32_t bpm, key, scale, prog, drums, bass, chords;
    uint8_t mel[4][16];                       /* a 4-bar phrase, 16 sixteenths a bar: a note (MIDI), 0 hold, 1 rest */
    uint8_t extra[16];                        /* the groove's own extra hits (perc) */
} m;
static int32_t deg_note(int32_t deg, int32_t oct)           /* scale degree (any) -> MIDI note */
{
    int32_t o = deg >= 0 ? deg / 7 : -((-deg + 6) / 7), d = deg - 7 * o;
    return 12 * (oct + o) + (int32_t)m.key + SCALES[m.scale][d];
}
static int32_t chord_deg(uint32_t bar) { return PROGS[m.scale][m.prog][bar % 4u]; }
static void compose(uint32_t seed)
{
    static const uint32_t BPMS[4][3] = {{124, 126, 128}, {92, 96, 100}, {84, 86, 88}, {110, 112, 116}};
    uint32_t b, s, i;
    uint16_t rhythm;
    int32_t motif[16];
    rs = seed ? seed : 1u;
    for (i = 0; i < 8u; i++) rnd(1);
    m.key = rnd(12);
    m.scale = rnd(4);
    m.prog = rnd(4);
    m.drums = rnd(4);
    m.bpm = 0;
    m.bass = rnd(4);
    m.chords = rnd(3);
    {   /* DEMO_PLAN="scale,drums,bass,chords" (numbers): the style chosen, the notes still the seed's */
        const char *pl = getenv("DEMO_PLAN");
        unsigned a, b2, c, d;
        if (pl && sscanf(pl, "%u,%u,%u,%u", &a, &b2, &c, &d) == 4) {
            m.scale = a % 4u; m.drums = b2 % 4u; m.bass = c % 4u; m.chords = d % 3u;
            m.prog = rnd(4);
        }
    }
    m.bpm = BPMS[m.drums][rnd(3)];
    for (s = 0; s < 16u; s++)
        m.extra[s] = (uint8_t)(s % 4u && rnd(9) == 0u);
    /* the motif: a rhythm (more notes for slower grooves), degrees walking near the chord's tones */
    do {
        rhythm = 0;
        for (s = 0; s < 16u; s++)
            if (rnd(100) < (s % 4u == 0u ? 55u : s % 2u == 0u ? 35u : 18u) + (m.drums == 2u ? 10u : 0u))
                rhythm |= (uint16_t)(1u << s);
    } while (__builtin_popcount(rhythm) < 4 || !(rhythm & 1u));
    {
        int32_t d = (int32_t)(rnd(3) * 2u);                 /* starts on a chord tone of I (1, 3 or 5) */
        for (s = 0; s < 16u; s++) {
            if ((rhythm >> s) & 1u) {
                if (s) d += (int32_t)rnd(5) - 2;            /* steps and small leaps */
                d = d < -2 ? -2 : d > 9 ? 9 : d;
                motif[s] = d;
            } else
                motif[s] = -100;
        }
    }
    /* the phrase: A, A moved to bar 2's chord, A again, B: the motif's rhythm, falling to the root */
    for (b = 0; b < 4u; b++) {
        int32_t shift = b == 1u ? chord_deg(1) - chord_deg(0) : b == 3u ? 0 : 0, last = 0;
        if (shift > 3) shift -= 7;
        for (s = 0; s < 16u; s++) {
            uint8_t v = s ? 0u : 1u;
            if (motif[s] > -100) {
                int32_t d = b == 3u ? (s >= 8u ? 0 : motif[s] - (int32_t)(s / 4u)) : motif[s] + shift;
                if (b == 3u && s >= 12u) d = 0;
                v = (uint8_t)deg_note(d, 6);
                last = d;
            } else if (s && motif[s - 1u] > -100 && rnd(3) == 0u) {
                v = 1;                                       /* a rest: the note before cut short */
            }
            m.mel[b][s] = v;
        }
        (void)last;
    }
    m.mel[3][12] = (uint8_t)deg_note(0, 6);                 /* (the phrase ends on the root) */
    for (s = 13; s < 16u; s++) m.mel[3][s] = s == 15u ? 1u : 0u;
}

/* ---- playing it ---- */
static uint8_t held[NTRK][8], nheld[NTRK];
static uint32_t step_frames(void) { return FS * 60u / m.bpm / 4u; }
static void on(uint32_t k, uint32_t n, uint32_t v)
{
    trk_note_on(&trk[k], n, v);
    if (nheld[k] < 8u) held[k][nheld[k]++] = (uint8_t)n;
}
static void off_all(uint32_t k)
{
    while (nheld[k]) trk_note_off(&trk[k], held[k][--nheld[k]]);
}
/* bar b (of the 4-bar phrase), sixteenth s: track k's part; lite: the drums only hats (the breakdown); up: the
 * melody an octave up */
static void part(uint32_t k, uint32_t b, uint32_t s, int lite, int up)
{
    int32_t cd = chord_deg(b), i;
    if (k == 0u) {
        off_all(0);
        if (lite) {
            if (s % 4u == 2u) on(0, 42, 70);
            return;
        }
        switch (m.drums) {
        case 0:                                             /* four on the floor: kick, clap on 2 and 4, open hats off */
            if (s % 4u == 0u) on(0, 36, 120);
            if (s % 8u == 4u) on(0, 39, 105);
            if (s % 4u == 2u) on(0, 46, 85);
            else if (s % 2u == 1u) on(0, 42, 60);
            break;
        case 1:                                             /* breakbeat */
            if (s == 0u || s == 10u || (b % 2u && s == 7u)) on(0, 36, 120);
            if (s == 4u || s == 12u) on(0, 38, 112);
            if (s == 15u || (b == 3u && s == 14u)) on(0, 38, 60);
            if (s % 2u == 0u) on(0, 42, s % 4u ? 70 : 90);
            break;
        case 2:                                             /* half-time */
            if (s == 0u || s == 11u) on(0, 36, 120);
            if (s == 8u) on(0, 38, 115);
            if (s % 4u == 0u || s == 14u) on(0, 42, 80);
            if (s == 6u) on(0, 37, 80);
            break;
        default:                                            /* electro */
            if (s == 0u || s == 6u || s == 10u) on(0, 36, 120);
            if (s == 4u || s == 12u) on(0, 39, 108);
            on(0, 42, s % 2u ? 55 : 80);
            break;
        }
        if (m.extra[s] && !(s == 4u || s == 12u)) on(0, 45, 85);
    } else if (k == 1u) {
        int st = m.chords == 0u ? s == 0u : m.chords == 1u ? s == 0u || s == 3u || s == 6u || s == 10u : s % 4u == 2u;
        int en = m.chords == 0u ? s == 15u : m.chords == 1u ? s == 2u || s == 5u || s == 8u || s == 12u : s % 4u == 3u;
        if (en) off_all(1);
        if (st)
            for (i = 0; i < 4; i++) on(1, (uint32_t)deg_note(cd + 2 * i, 4), m.chords == 1u ? 105 : 92);
    } else if (k == 2u) {
        uint32_t root = (uint32_t)deg_note(cd, 2), hit = 0, n = root;
        switch (m.bass) {
        case 0: hit = s % 4u == 2u; break;
        case 1: hit = s == 0u || s == 3u || s == 6u || s == 10u || s == 13u;
                n = s == 6u ? root + 12u : s == 13u ? (uint32_t)deg_note(cd + 4, 2) : root; break;
        case 2: hit = s == 0u || s == 8u; n = s == 8u ? (uint32_t)deg_note(cd + 4, 2) : root; break;
        default: hit = s % 2u == 0u; n = (uint32_t)deg_note(cd + 2 * (int32_t)((s / 2u) % 3u), 2) + (s >= 8u ? 12u : 0u); break;
        }
        if (hit || (m.bass != 2u && s % 4u == 3u)) off_all(2);
        if (hit) on(2, n, s % 4u == 0u ? 110 : 96);
    } else {
        uint8_t v = m.mel[b][s];
        if (v) off_all(3);
        if (v > 1u) on(3, v + (up ? 12u : 0u), s % 4u == 0u ? 105 : 92);
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
    song.g[G_BPM] = (int16_t)m.bpm;
    dice.rng = seed;
    quiet_all();
}

int main(int argc, char **argv)
{
    static const char *const NM[4] = {"dice_kit.wav", "dice_chords.wav", "dice_bass.wav", "dice_lead.wav"};
    static const char *const ROLE[4] = {"KIT (T1)", "CHORDS (T2)", "BASS (T3)", "LEAD (T4)"};
    const char *dir = argc > 1 ? argv[1] : "build/dice_demo";
    uint32_t seed = argc > 2 ? (uint32_t)strtoul(argv[2], 0, 0) : 0xD1CE2026u;
    uint32_t k, p, b, s, sec, step;
    char path[600], t[80];
    compose(seed);
    step = step_frames();
    snprintf(path, sizeof path, "%s/patches.txt", dir);
    if (!(rep = fopen(path, "w"))) { fprintf(stderr, "cannot write %s\n", path); return 1; }
    {
        static const char *const RN[7] = {"I", "II", "III", "IV", "V", "VI", "VII"};
        fprintf(rep, "MUSIC: %s %s, %u BPM, progression %s-%s-%s-%s, drums %s, bass %s, chords %s\n\n",
                KEY_NM[m.key], SCALE_NM[m.scale], m.bpm, RN[chord_deg(0)], RN[chord_deg(1)], RN[chord_deg(2)],
                RN[chord_deg(3)], DRUM_NM[m.drums], BASS_NM[m.bass], CHRD_NM[m.chords]);
    }
    for (k = 0; k < NTRK; k++) {                        /* each role: six rolls, its 4 bars each */
        FILE *w;
        snprintf(path, sizeof path, "%s/%s", dir, NM[k]);
        if (!(w = fopen(path, "wb"))) return 1;
        wav_hdr(w, 6u * (64u * step + 4u * step));
        fresh(seed + 977u * k);
        for (p = 0; p < 6u; p++) {
            roll(k);
            snprintf(t, sizeof t, "%s roll %u", ROLE[k], p + 1u);
            report(k, t);
            for (b = 0; b < 4u; b++)
                for (s = 0; s < 16u; s++) {
                    part(k, b, s, 0, 0);
                    mixw(w, step);
                }
            off_all(k);
            mixw(w, 4u * step);                         /* (a beat: the tail) */
            quiet_all();
        }
        fclose(w);
        fprintf(rep, "\n");
    }
    {                                                   /* the song: 4 sections of 8 bars, rolled again each section */
        FILE *w;
        snprintf(path, sizeof path, "%s/dice_song.wav", dir);
        if (!(w = fopen(path, "wb"))) return 1;
        wav_hdr(w, 4u * 128u * step + 16u * step);
        fresh(seed ^ 0x5EC7u);
        for (sec = 0; sec < 4u; sec++) {
            for (k = 0; k < NTRK; k++) {
                off_all(k);
                roll(k);
                snprintf(t, sizeof t, "SONG section %u, %s", sec + 1u, ROLE[k]);
                report(k, t);
            }
            for (b = 0; b < 8u; b++)
                for (s = 0; s < 16u; s++) {
                    for (k = 0; k < NTRK; k++) {
                        if (sec == 0u && (k == 1u || k == 3u)) continue;   /* intro: drums and bass */
                        if (sec == 0u && k == 2u && b < 4u) continue;     /* (the bass from bar 5) */
                        if (sec == 2u && k == 2u) continue;               /* breakdown: no bass */
                        part(k, b % 4u, s, sec == 2u && k == 0u, sec == 3u && k == 3u && b >= 4u);
                    }
                    mixw(w, step);
                }
        }
        for (k = 0; k < NTRK; k++)
            off_all(k);
        on(1, (uint32_t)deg_note(0, 4), 90);               /* the end: the root, ringing out */
        on(2, (uint32_t)deg_note(0, 2), 100);
        mixw(w, 8u * step);
        off_all(1);
        off_all(2);
        mixw(w, 8u * step);
        fclose(w);
    }
    fclose(rep);
    printf("dice_demo: %s (seed 0x%08X): %s %s, %u BPM, %s, bass %s, chords %s\n", dir, seed, KEY_NM[m.key],
           SCALE_NM[m.scale], m.bpm, DRUM_NM[m.drums], BASS_NM[m.bass], CHRD_NM[m.chords]);
    return 0;
}
