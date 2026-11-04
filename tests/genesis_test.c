/* SPDX-License-Identifier: GPL-3.0-only
 * Dungeon Tracker 1.2: the GENESIS engine (src/eng_genesis.c, after the YM2612) on the host, through hostsim.c.
 *   build/host/genesis_test [DEMODIR]          (tests/run_rondo.sh)
 * 1. every preset sounds, none 6 dB from the others, no DC, its voices free after the release; the level near WAVES'.
 * 2. the chip's behaviour: one operator alone a sine; MUL 2 an octave up; feedback brighter; the algorithms differ
 *    (a chain brighter than four carriers); GRIT on the DAC's steps; DTUNE beats; the release ends the voice.
 * 3. cost: host ns per voice-sample against FMTONE. 4. demos: every preset into DEMODIR. */
#define main hostsim_main
#include "hostsim.c"
#undef main
#ifndef CAT_BASS
#include "category.c"
#endif

static int fails;
static void check(const char *what, int ok)
{
    printf("genesis: %-90s %s\n", what, ok ? "ok" : "FAIL");
    fails += !ok;
}

#define NMAX (FS * 6)
static float buf[NMAX];
static uint32_t buf_n;

static void parts_clear(void)
{
    uint32_t p;
    host_tracks_init();
    memset(eng_mem, 0, sizeof eng_mem);
    for (p = 0; p < NPART; p++)
        memset(trk[p].v, 0, sizeof trk[p].v);
}
static int32_t render_part(uint32_t part, uint32_t n)
{
    int32_t o[CTL], pk = 0;
    uint32_t i, k;
    if (n > NMAX)
        n = NMAX;
    for (i = 0; i < n; i += CTL) {
        track_render(&trk[part], o, CTL);
        for (k = 0; k < CTL && i + k < n; k++) {
            int32_t a = abs(o[k]);
            pk = a > pk ? a : pk;
            buf[i + k] = o[k] / 32768.0f;
        }
    }
    buf_n = n;
    return pk;
}
static double rms(uint32_t a, uint32_t e)
{
    double s = 0;
    for (uint32_t i = a; i < e && i < buf_n; i++)
        s += (double)buf[i] * buf[i];
    return sqrt(s / (double)(e > a ? e - a : 1));
}
static uint32_t zc(uint32_t a, uint32_t e)
{
    uint32_t n = 0;
    for (uint32_t i = a + 1; i < e && i < buf_n; i++)
        n += (buf[i] >= 0) != (buf[i - 1] >= 0);
    return n;
}
static double bright(uint32_t a, uint32_t e)     /* the share of the first difference's energy: higher, brighter */
{
    double s = 0, d = 0;
    for (uint32_t i = a + 1; i < e && i < buf_n; i++) {
        s += (double)buf[i] * buf[i];
        d += (double)(buf[i] - buf[i - 1]) * (buf[i] - buf[i - 1]);
    }
    return s > 0 ? d / s : 0;
}
static double dc_from(uint32_t a)
{
    double s = 0, pk = 0;
    for (uint32_t i = a; i < buf_n; i++)
        s += buf[i], pk = fabs(buf[i]) > pk ? fabs(buf[i]) : pk;
    return pk > 0 ? fabs(s / (double)(buf_n - a)) / pk : 0;
}
static int voices_free(uint32_t part)
{
    for (uint32_t i = 0; i < NVOICE; i++)
        if (trk[part].v[i].active)
            return 0;
    return 1;
}


static void raw(uint32_t alg, uint32_t fb, const uint8_t *mul, uint32_t dt, uint32_t grit, const uint8_t (*ops)[5])
{
    uint32_t k, f;
    host_preset(&trk[1], ENGI_GENESIS, 0);
    trk[1].p[P_E0] = (int16_t)alg; trk[1].p[P_E1] = (int16_t)fb;
    for (k = 0; k < 4u; k++) trk[1].p[P_E2 + k] = mul[k];
    trk[1].p[P_E6] = (int16_t)dt; trk[1].p[P_E7] = (int16_t)grit;
    for (k = 0; k < 4u; k++) for (f = 0; f < 5u; f++) trk[1].p[P_FM1_ATK + 5u * k + f] = ops[k][f];
}
static double probe(uint32_t alg, uint32_t fb, const uint8_t *mul, uint32_t dt, uint32_t grit, const uint8_t (*ops)[5],
                    uint32_t *zcs)
{
    parts_clear();
    raw(alg, fb, mul, dt, grit, ops);
    trk_note_on(&trk[1], 57, 127);
    render_part(1, FS / 2);
    if (zcs) *zcs = zc(FS / 10, FS / 2);
    return bright(FS / 10, FS / 2);
}

/* 1 */
static void presets(void)
{
    const engine_t *e = &ENG_GENESIS;
    uint32_t pi, quiet = 0, loud = 0, dc = 0, held = 0, nw = 0, ns = 0;
    double r1[64], mean = 0, w = 0, sv = 0;
    for (pi = 0; pi < e->npresets; pi++) {
        uint32_t part = GEN_PRESETS[pi].mono ? 2 : 1, k;
        parts_clear();
        host_preset(&trk[part], ENGI_GENESIS, pi);
        trk_note_on(&trk[part], part == 2 ? 40 : 57, 127);
        render_part(part, FS / 2);
        {   /* (its loudness: the loudest 20 ms in the first half second, a pluck's attack as a pad's swell) */
            uint32_t w0; r1[pi] = 0;
            for (w0 = 0; w0 + FS / 50 <= FS / 2; w0 += FS / 100) { double r = rms(w0, w0 + FS / 50); r1[pi] = r > r1[pi] ? r : r1[pi]; }
        }
        quiet |= r1[pi] < 0.004;
        dc |= dc_from(FS / 20) > 0.02;
        trk_note_off(&trk[part], part == 2 ? 40 : 57);
        for (k = 0; k < 24u; k++) render_part(part, FS / 2);   /* (12 s: the longest release) */
        held |= !voices_free(part);
        if (getenv("GDBG")) printf("  %-12s rms %.3f\n", GEN_PRESETS[pi].name, r1[pi]);
    }
    for (pi = 0; pi < e->npresets; pi++) mean += 20.0 * log10(r1[pi] + 1e-9) / e->npresets;
    for (pi = 0; pi < e->npresets; pi++) loud += fabs(20.0 * log10(r1[pi] + 1e-9) - mean) > 6.0;
    for (pi = 0; pi < e->npresets; pi++) {               /* the level against WAVES' (both on one voice, sustained) */
        if (GEN_OPS[pi][3][2] < 60 || preset_cat(ENGI_GENESIS, pi) == CAT_FX) continue;
        parts_clear(); host_preset(&trk[1], ENGI_GENESIS, pi); trk_note_on(&trk[1], 57, 127); render_part(1, FS / 2);
        sv += rms(FS / 4, FS / 2); ns++;
    }
    for (pi = 0; pi < ENG_WAVES.npresets; pi++)
        if (ENG_WAVES.presets[pi].env[2] > 60 && preset_cat(ENGI_WAVES, pi) != CAT_FX) {
            parts_clear(); host_preset(&trk[1], ENGI_WAVES, pi); trk_note_on(&trk[1], 57, 127); render_part(1, FS / 2);
            w += rms(FS / 4, FS / 2); nw++;
        }
    sv /= ns ? ns : 1; w /= nw ? nw : 1;
    printf("genesis:   (%u presets; quiet %u, over 6 dB from the others %u, DC %u, held %u; sustained rms GENESIS %.3f, WAVES %.3f)\n",
           e->npresets, quiet, loud, dc, held, sv, w);
    check("every preset sounds, none 6 dB from the others, no DC, its voices free after the release; level within 4 dB of WAVES",
          !quiet && !loud && !dc && !held && fabs(20.0 * log10(sv / w)) < 4.0);
}

/* 2 */
static void behaviour(void)
{
    static const uint8_t M1[4] = {1, 1, 1, 1}, M2[4] = {1, 1, 1, 2};
    static const uint8_t SINE_ONLY[4][5] = {{0, 0, 127, 20, 0}, {0, 0, 127, 20, 0}, {0, 0, 127, 20, 0}, {0, 0, 127, 20, 127}};
    static const uint8_t CHAIN[4][5] = {{0, 0, 127, 20, 110}, {0, 0, 127, 20, 110}, {0, 0, 127, 20, 110}, {0, 0, 127, 20, 127}};
    static const uint8_t PLUCK[4][5] = {{0, 0, 127, 20, 0}, {0, 0, 127, 20, 0}, {0, 0, 127, 20, 0}, {0, 60, 0, 20, 127}};
    uint32_t z1, z2, i;
    double b0, b1, bc, ba, bg;
    b0 = probe(7, 0, M1, 0, 0, SINE_ONLY, &z1);
    check("ALG 8, only operator 4 sounding: a sine (as bright as a sine of the note)", b0 < 0.003);
    probe(7, 0, M2, 0, 0, SINE_ONLY, &z2);
    check("  MUL4 2: an octave up (twice the zero crossings)", z2 > z1 * 19u / 10u && z2 < z1 * 21u / 10u);
    {
        static const uint8_t FBO[4][5] = {{0, 0, 127, 20, 127}, {0, 0, 127, 20, 0}, {0, 0, 127, 20, 0}, {0, 0, 127, 20, 0}};
        b0 = probe(7, 0, M1, 0, 0, FBO, 0);
        b1 = probe(7, 7, M1, 0, 0, FBO, 0);
        check("FB: operator 1 brighter with its feedback (7 against 0)", b1 > b0 * 4.0);
    }
    bc = probe(0, 0, M1, 0, 0, CHAIN, 0);
    ba = probe(7, 0, M1, 0, 0, CHAIN, 0);
    check("the algorithms differ: a chain of four (1) brighter than four carriers (8)", bc > ba * 2.0);
    b0 = probe(7, 0, M1, 0, 0, SINE_ONLY, 0);
    bg = probe(7, 0, M1, 0, 120, SINE_ONLY, 0);
    {
        float prev = buf[FS / 10]; uint32_t steps = 0, flat = 0;
        for (i = FS / 10 + 1; i < FS / 2; i++) { flat += buf[i] == prev; steps += buf[i] != prev; prev = buf[i]; }
        if (getenv("GDBG")) printf("  grit: flat %u steps %u bright %.5f vs %.5f\n", flat, steps, bg, b0);
        check("GRIT 120: the DAC's steps (a sine of fewer levels, brighter)", flat > steps / 2u && bg > b0 * 1.5);
    }
    {   /* DTUNE: two carriers drawn apart beat (the level swings) */
        static const uint8_t TWO[4][5] = {{0, 0, 127, 20, 0}, {0, 0, 127, 20, 127}, {0, 0, 127, 20, 0}, {0, 0, 127, 20, 127}};
        double lo = 9, hi = 0;
        parts_clear(); raw(7, 0, M1, 127, 0, TWO); trk_note_on(&trk[1], 57, 127); render_part(1, FS * 3);
        for (i = 0; i < 12u; i++) { double r = rms(FS / 4 + i * FS / 5, FS / 4 + i * FS / 5 + FS / 50); lo = r < lo ? r : lo; hi = r > hi ? r : hi; }
        check("DTUNE: two carriers drawn apart beat (the level swings over seconds)", hi > lo * 1.5);
    }
    parts_clear(); raw(7, 0, M1, 0, 0, PLUCK); trk_note_on(&trk[1], 57, 127); render_part(1, FS * 2);
    check("an operator's decay to SUS 0: the note dies away and the voice ends, key still held", rms(FS * 3 / 2, FS * 2) < 0.002 &&
          voices_free(1));
}

static double ns_per(uint32_t e, uint32_t pi)
{
    struct timespec a, b;
    uint32_t i;
    int32_t o[CTL];
    parts_clear();
    host_preset(&trk[1], e, pi);
    for (i = 0; i < 4u; i++) trk_note_on(&trk[1], 48 + 5 * i, 100);
    track_render(&trk[1], o, CTL);
    clock_gettime(CLOCK_MONOTONIC, &a);
    for (i = 0; i < FS * 2 / CTL; i++) track_render(&trk[1], o, CTL);
    clock_gettime(CLOCK_MONOTONIC, &b);
    return ((b.tv_sec - a.tv_sec) * 1e9 + (b.tv_nsec - a.tv_nsec)) / (FS * 2.0 * 4.0);
}
static void cost(void)
{
    double g = ns_per(ENGI_GENESIS, 19), c = ns_per(ENGI_CTONE, 0);
    char what[160];
    snprintf(what, sizeof what, "cost per voice-sample: GENESIS %.1f ns (STRINGS, four operators), FMTONE %.1f: %.2f x", g, c, g / c);
    check(what, g < 2.0 * c);
}

static void wav_write(const char *path, uint32_t n)
{
    FILE *f = fopen(path, "wb");
    uint32_t i, sz = n * 2u + 36u, sr = FS, br = FS * 2u;
    if (!f) return;
    fwrite("RIFF", 1, 4, f); fwrite(&sz, 4, 1, f); fwrite("WAVEfmt ", 1, 8, f);
    { uint32_t l = 16; uint16_t pcm = 1, ch = 1, ba = 2, bits = 16; fwrite(&l, 4, 1, f); fwrite(&pcm, 2, 1, f); fwrite(&ch, 2, 1, f);
      fwrite(&sr, 4, 1, f); fwrite(&br, 4, 1, f); fwrite(&ba, 2, 1, f); fwrite(&bits, 2, 1, f); }
    sz = n * 2u; fwrite("data", 1, 4, f); fwrite(&sz, 4, 1, f);
    for (i = 0; i < n; i++) { int16_t s = (int16_t)clamp((int32_t)(buf[i] * 32767.0f * 1.5f), -32767, 32767); fwrite(&s, 2, 1, f); }
    fclose(f);
}
static void demos(const char *dir)
{
    static const uint8_t PH[8] = {0, 4, 7, 12, 7, 4, 0, 0};
    uint32_t pi, j, k;
    char path[512];
    static float all[FS * 3];
    for (pi = 0; pi < ENG_GENESIS.npresets; pi++) {
        uint32_t base = GEN_PRESETS[pi].mono ? 40 : 57, n = 0;
        parts_clear();
        host_preset(&trk[1], ENGI_GENESIS, pi);
        for (j = 0; j < 8u; j++) {                         /* (whole blocks: a part renders CTL at a time, and a cut
                                                           * mid-block would drop samples: a click in the demo) */
            trk_note_on(&trk[1], base + PH[j], 110);
            render_part(1, FS / 4 / CTL * CTL);
            memcpy(all + n, buf, FS / 4 / CTL * CTL * sizeof(float)); n += FS / 4 / CTL * CTL;
            trk_note_off(&trk[1], base + PH[j]);
        }
        render_part(1, FS / 2 / CTL * CTL);
        memcpy(all + n, buf, FS / 2 / CTL * CTL * sizeof(float)); n += FS / 2 / CTL * CTL;
        memcpy(buf, all, n * sizeof(float));
        snprintf(path, sizeof path, "%s/genesis_%02u_%s.wav", dir, pi, GEN_PRESETS[pi].name);
        for (k = 0; path[k]; k++) if (path[k] == ' ') path[k] = '_';
        wav_write(path, n);
    }
}

static void brightness(void)
{
    uint32_t e, pi;
    for (e = 0; e < 2u; e++) {
        uint32_t en = e ? ENGI_CTONE : ENGI_GENESIS;
        const engine_t *E = ENGINES[en];
        for (pi = 0; pi < E->npresets; pi++) {
            uint32_t part = E->presets[pi].mono ? 2 : 1, note = part == 2 ? 40 : 57;
            double b, b2;
            parts_clear(); host_preset(&trk[part], en, pi); trk_note_on(&trk[part], note, 110); render_part(part, FS / 2);
            b = bright(FS / 50, FS / 5);
            b2 = bright(FS / 4, FS / 2);
            printf("  %-7s %c %-12s bright %.4f late %.4f\n", E->name, "?BLPUKDFO"[preset_cat(en, pi)], E->presets[pi].name, b, b2);
        }
    }
}
int main(int argc, char **argv)
{
    fm6_init();
    if (getenv("GBRIGHT")) { brightness(); return 0; }
    presets();
    behaviour();
    if (!getenv("NOCOST"))
        cost();
    if (argc > 1) {
        demos(argv[1]);
        printf("genesis: demos in %s\n", argv[1]);
    }
    printf("genesis: %s\n", fails ? "FAILED" : "all checks ok");
    return fails != 0;
}
