/* SPDX-License-Identifier: GPL-3.0-only
 * Dungeon Tracker 1.5: the WAVE 32 engine (src/eng_wt32.c) on the host, through hostsim.c.
 *   build/host/wt32_test [DEMODIR]
 * 1. every preset sounds, no DC, its voices free; the level near WAVES'. 2. every bank sounds and POS moves through
 * it; WARP brightens; DTUN beats; SUB adds the octave below. 3. aliasing under -30 dB up to C7. 4. the lists, its presets.
 * 5. cost against WAVES. 6. a demo of every preset. */
#define main hostsim_main
#include "hostsim.c"
#undef main
#ifndef CAT_BASS
#include "category.c"                   /* (preset_cat: hostsim has no ui.c) */
#endif

static int fails;
static void check(const char *what, int ok)
{
    printf("wave32: %-92s %s\n", what, ok ? "ok" : "FAIL");
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

static void raw(uint32_t part, int bank, int pos, int dt, int sub, int warp)
{
    host_preset(&trk[part], ENGI_WT32, 0);
    trk[part].p[P_E0] = (int16_t)bank; trk[part].p[P_E1] = (int16_t)pos; trk[part].p[P_E2] = 0; trk[part].p[P_E3] = (int16_t)dt;
    trk[part].p[P_E4] = 127; trk[part].p[P_E5] = 0; trk[part].p[P_E6] = (int16_t)sub; trk[part].p[P_E7] = (int16_t)warp;
    trk[part].p[P_ATK] = 0; trk[part].p[P_DEC] = 64; trk[part].p[P_SUS] = 127; trk[part].p[P_REL] = 20;
    trk[part].p[P_ED_FLT] = 0;
}
static double rms_of(uint32_t e, uint32_t pi)
{
    parts_clear(); host_preset(&trk[1], e, pi); trk_note_on(&trk[1], 57, 127); render_part(1, FS / 2);
    return rms(FS / 4, FS / 2);
}
/* 1 every preset: sounds, no DC, voices free; the level near WAVES' */
static void presets(void)
{
    static const uint8_t CHORD[3] = {57, 60, 64};
    uint32_t pi, quiet = 0, dc = 0, held = 0, over = 0, nw = 0, nv = 0;
    double w = 0, v = 0;
    for (pi = 0; pi < ENG_WT32.npresets; pi++) {
        const preset_t *pr = &WT32_PRESETS[pi];
        uint32_t part = pr->mono ? 2 : 1, k, rel;
        int32_t pk;
        parts_clear();
        host_preset(&trk[part], ENGI_WT32, pi);
        if (pr->mono)
            trk_note_on(&trk[part], 36, 110);
        else
            for (k = 0; k < 3u; k++) trk_note_on(&trk[part], CHORD[k], 100);
        pk = render_part(part, FS * 6 / 10);
        quiet |= (pk < 2048) << pi;
        over |= (pk >= 32767 * 3) << pi;                 /* (a chord of three: the part past full scale, as WAVES') */
        for (k = 0; k < 128u; k++) trk_note_off(&trk[part], k);
        rel = (uint32_t)(TIME_MS_X10[trk[part].p[P_REL] & 127] / 10u) * FS / 1000u * 2u + FS;
        render_part(part, rel < NMAX ? rel : NMAX);
        held |= !voices_free(part) << pi;
        {   /* DC: one note held (SUS full), over whole periods of its octave below (SUB) */
            double f = 440.0 * pow(2.0, (48 - 69) / 12.0) / 2.0, m = 0, mx = 0;
            uint32_t a = FS / 2, e = a + (uint32_t)(floor(0.4 * f) * FS / f), i;
            parts_clear(); host_preset(&trk[1], ENGI_WT32, pi); trk[1].p[P_SUS] = 127; trk[1].p[P_ED_FLT] = 0;
            trk_note_on(&trk[1], 48, 100); render_part(1, FS);
            for (i = a; i < e; i++) m += buf[i], mx = fabs(buf[i]) > mx ? fabs(buf[i]) : mx;
            m = mx > 0 ? fabs(m / (e - a)) / mx : 0;
            dc |= (m > 0.01) << pi;
        }
        printf("wave32:   %-12s the part %5d (%.1f dBFS)\n", pr->name, pk, 20.0 * log10((pk + 1) / 32768.0));
    }
    for (pi = 0; pi < ENG_WT32.npresets; pi++)
        if (WT32_PRESETS[pi].env[2] > 60) w += rms_of(ENGI_WT32, pi), nw++;
    for (pi = 0; pi < ENG_WAVES.npresets; pi++)
        if (ENG_WAVES.presets[pi].env[2] > 60) v += rms_of(ENGI_WAVES, pi), nv++;
    w /= nw ? nw : 1; v /= nv ? nv : 1;
    printf("wave32:   (quiet 0x%X, over 0x%X, DC 0x%X, held 0x%X; one voice's rms WAVE 32 %.3f, WAVES %.3f)\n", quiet, over, dc, held, w, v);
    check("every preset sounds, no DC, its voices free after the release; level within 3 dB of WAVES",
          !quiet && !over && !dc && !held && fabs(20.0 * log10(w / v)) < 3.0);
}
/* 2 the controls */
static void controls(void)
{
    uint32_t b, ok = 1, z0, z1;
    double b0, b1;
    char what[120];
    for (b = 0; b < WT32_NB; b++) {
        parts_clear(); raw(1, (int)b, 0, 0, 0, 0); trk_note_on(&trk[1], 45, 100); render_part(1, FS / 2); b0 = bright(FS / 10, FS / 2);
        parts_clear(); raw(1, (int)b, 127, 0, 0, 0); trk_note_on(&trk[1], 45, 100); render_part(1, FS / 2); b1 = bright(FS / 10, FS / 2);
        printf("wave32:   (%-8s POS 0 / 127: brightness %.4f / %.4f)\n", N_WT32_BANK[b], b0, b1);
        ok &= fabs(b1 - b0) > 0.1 * (b0 > b1 ? b0 : b1) && rms(FS / 10, FS / 2) > 0.02;
    }
    check("every bank sounds; POS moves through it (the brightness changes)", ok);
    parts_clear(); raw(1, 0, 60, 0, 0, 0); trk_note_on(&trk[1], 57, 100); render_part(1, FS / 2); b0 = bright(FS / 10, FS / 2);
    parts_clear(); raw(1, 0, 60, 0, 0, 120); trk_note_on(&trk[1], 57, 100); render_part(1, FS / 2); b1 = bright(FS / 10, FS / 2);
    snprintf(what, sizeof what, "WARP brightens (%.4f -> %.4f)", b0, b1);
    check(what, b1 > b0 * 1.2);
    parts_clear(); raw(1, 0, 0, 0, 0, 0); trk_note_on(&trk[1], 69, 100); render_part(1, FS); b0 = rms(FS / 2, FS);
    parts_clear(); raw(1, 0, 0, 127, 0, 0); trk_note_on(&trk[1], 69, 100); render_part(1, FS);
    {   /* DTUN: the two oscillators beat (the level over 50 ms frames moves) */
        double lo = 1e9, hi = 0;
        for (uint32_t f = FS / 2; f + FS / 20 <= FS; f += FS / 20) {
            double r = rms(f, f + FS / 20);
            lo = r < lo ? r : lo; hi = r > hi ? r : hi;
        }
        snprintf(what, sizeof what, "DTUN 127: the oscillators beat (frame level %.3f .. %.3f, alone %.3f)", lo, hi, b0);
        check(what, hi > lo * 1.3);
    }
    parts_clear(); raw(1, 3, 127, 0, 0, 0); trk_note_on(&trk[1], 69, 100); render_part(1, FS / 2); z0 = zc(FS / 10, FS / 2);
    parts_clear(); raw(1, 3, 127, 0, 127, 0); trk_note_on(&trk[1], 69, 100); render_part(1, FS / 2); z1 = zc(FS / 10, FS / 2);
    snprintf(what, sizeof what, "SUB at 127: a sine an octave below (%u -> %u zero crossings)", z0, z1);
    check(what, z1 * 3u < z0 * 2u);
}
/* 3 aliasing: a saw (CLASSIC, POS 3/7) up to C7 keeps its energy on its harmonics (C8 printed) */
static void alias(void)
{
    static const uint8_t NOTE[] = {48, 72, 84, 96, 108};
    double worst = 0;
    char what[120];
    for (uint32_t k = 0; k < NELEM(NOTE); k++) {
        double f0 = 440.0 * pow(2.0, (NOTE[k] - 69) / 12.0), off = 0, all = 0;
        uint32_t a = FS / 4, n = FS / 2, j, b;
        parts_clear(); raw(1, 0, 54, 0, 0, 0); trk_note_on(&trk[1], NOTE[k], 100); render_part(1, a + n);
        for (b = 20; b < n / 2; b += 5) {        /* a DFT, 2 Hz bins (every fifth), Hann */
            double re = 0, im = 0, f = b * (double)FS / n;
            for (j = 0; j < n; j++) {
                double w = 0.5 - 0.5 * cos(2 * M_PI * j / n), x = buf[a + j] * w;
                re += x * cos(2 * M_PI * f * j / FS); im += x * sin(2 * M_PI * f * j / FS);
            }
            double e = re * re + im * im, h = f / f0, d = fabs(h - floor(h + 0.5)) * f0;
            all += e;
            if (d > 25.0) off += e;
        }
        double r = all > 0 ? off / all : 0;
        printf("wave32:   (note %u, %.0f Hz: off-harmonic energy %.2e)\n", NOTE[k], f0, r);
        if (NOTE[k] <= 96u)
            worst = r > worst ? r : worst;
    }
    snprintf(what, sizeof what, "aliasing up to C7 (a saw): the energy off the harmonics under -30 dB (%.1f dB)", 10 * log10(worst + 1e-12));
    check(what, worst < 1e-3);
}
static void lists(void)
{
    uint32_t n, at = 99, ok = 1;
    for (n = 0; n < NENG_SHOWN; n++)
        if (eng_vis(n) == ENGI_WT32) at = n;
    check("WAVE 32 in PRESETS and on an EDIT layer key", at < NENG_KEYS && eng_ok(ENGI_WT32));
    for (n = 0; n < ENG_WT32.npresets; n++) {
        ok &= preset_cat(ENGI_WT32, n) != 0u;
        ok &= WT32_PRESETS[n].pat <= NPATTERNS;
        ok &= str_len(WT32_PRESETS[n].name) <= 12u;
    }
    check("its presets: a category each, pattern hints among the PHRASES, names that fit", ok && ENG_WT32.npresets >= 16u);
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
    double wv = ns_per(ENGI_WAVES, 0), w1 = ns_per(ENGI_WT32, 0), w2 = ns_per(ENGI_WT32, 21);   /* (21 PWM STRINGS: DTUN) */
    char what[160];
    snprintf(what, sizeof what, "cost per voice-sample: WAVE 32 %.1f ns / %.1f (DTUN), WAVES %.1f: %.2f x", w1, w2, wv, w2 / wv);
    check(what, w2 < 2.0 * wv);                         /* (DTUN: two oscillators; the host's timing wanders ~25 %) */
}
/* 5 */
static FILE *wav_open(const char *dir, const char *name, uint32_t frames)
{
    char path[512];
    FILE *f;
    snprintf(path, sizeof path, "%s/%s", dir, name);
    f = fopen(path, "wb");
    if (f)
        wav_hdr(f, frames);
    return f;
}
static uint32_t mixed;
static void mix_to(FILE *w, uint32_t frames)
{
    int32_t o[2 * CTL];
    uint32_t i, k;
    for (i = 0; i < frames; i += CTL) {
        mix_block(o, CTL);
        for (k = 0; k < CTL; k++)
            wav_put(w, o[2 * k], o[2 * k + 1]);
        mixed += CTL;
    }
}
static void demo_presets(const char *dir)
{
    static const uint8_t CHORD[3] = {57, 60, 64}, LINE[8] = {36, 36, 43, 36, 39, 36, 41, 43};
    uint32_t pi, k;
    FILE *w = wav_open(dir, "wave32_presets.wav", 0x7FFFFFF);
    if (!w) return;
    mixed = 0;
    for (pi = 0; pi < ENG_WT32.npresets; pi++) {
        const preset_t *pr = &WT32_PRESETS[pi];
        uint32_t part = pr->mono ? 2 : 1;
        parts_clear();
        host_preset(&trk[part], ENGI_WT32, pi);
        if (pr->mono) {
            for (k = 0; k < 8u; k++) {
                trk_note_on(&trk[part], LINE[k], 100);
                mix_to(w, FS / 5 - FS / 40);
                trk_note_off(&trk[part], LINE[k]);
                mix_to(w, FS / 40);
            }
        } else {
            for (k = 0; k < 3u; k++) trk_note_on(&trk[part], CHORD[k], 100);
            mix_to(w, FS * 6 / 5);
            for (k = 0; k < 3u; k++) trk_note_off(&trk[part], CHORD[k]);
        }
        mix_to(w, FS * 2 / 5);
    }
    fseek(w, 0, SEEK_SET);
    wav_hdr(w, mixed);
    fclose(w);
}
int main(int argc, char **argv)
{
    presets();
    controls();
    alias();
    lists();
    if (!getenv("NOCOST"))
        cost();
    if (argc > 1) {
        demo_presets(argv[1]);
        printf("wave32: demos in %s\n", argv[1]);
    }
    printf("wave32: %s\n", fails ? "FAILED" : "all checks ok");
    return fails != 0;
}
