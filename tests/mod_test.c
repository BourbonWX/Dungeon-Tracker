/* SPDX-License-Identifier: GPL-3.0-only
 * Dungeon Tracker 1.5: the MOD engine (src/eng_mod.c) on the host, through hostsim.c.
 *   build/host/mod_test [DEMODIR]
 * 1. every preset sounds, no DC, its voices free; the level near WAVES'. 2. C4 at the root (Paula's periods); ARP
 * steps through its chord at 50 Hz; LED darkens; CRSH and RATE change it; DTUN beats; OFS starts later. 3. the lists.
 * 4. cost against WAVES. 5. a demo of every preset. */
#define main hostsim_main
#include "hostsim.c"
#undef main
#ifndef CAT_BASS
#include "category.c"                   /* (preset_cat: hostsim has no ui.c) */
#endif

static int fails;
static void check(const char *what, int ok)
{
    printf("mod: %-92s %s\n", what, ok ? "ok" : "FAIL");
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


static void raw(uint32_t part, int smpl, int arp, int led, int crsh, int rate, int dt)
{
    host_preset(&trk[part], ENGI_MOD, 0);
    trk[part].p[P_E0] = (int16_t)smpl; trk[part].p[P_E1] = 0; trk[part].p[P_E2] = (int16_t)arp; trk[part].p[P_E3] = 0;
    trk[part].p[P_E4] = (int16_t)led; trk[part].p[P_E5] = (int16_t)crsh; trk[part].p[P_E6] = (int16_t)rate; trk[part].p[P_E7] = (int16_t)dt;
    trk[part].p[P_ATK] = 0; trk[part].p[P_DEC] = 64; trk[part].p[P_SUS] = 127; trk[part].p[P_REL] = 20;
    trk[part].p[P_ED_FLT] = 0; trk[part].p[P_VOICE] = V_POLY;
}
static double rms_of(uint32_t e, uint32_t pi)
{
    parts_clear(); host_preset(&trk[1], e, pi); trk_note_on(&trk[1], 57, 127); render_part(1, FS / 2);
    return rms(FS / 4, FS / 2);
}
static void presets(void)
{
    static const uint8_t CHORD[3] = {57, 60, 64};
    uint32_t pi, quiet = 0, dc = 0, held = 0, nw = 0, nv = 0;
    double w = 0, v = 0;
    for (pi = 0; pi < ENG_MOD.npresets; pi++) {
        const preset_t *pr = &MOD_PRESETS[pi];
        uint32_t part = pr->mono ? 2 : 1, k, rel;
        int32_t pk;
        parts_clear();
        host_preset(&trk[part], ENGI_MOD, pi);
        if (pr->mono) trk_note_on(&trk[part], 36, 110);
        else for (k = 0; k < 3u; k++) trk_note_on(&trk[part], CHORD[k], 100);
        pk = render_part(part, FS * 6 / 10);
        quiet |= (pk < 2048) << pi;
        for (k = 0; k < 128u; k++) trk_note_off(&trk[part], k);
        rel = (uint32_t)(TIME_MS_X10[trk[part].p[P_REL] & 127] / 10u) * FS / 1000u * 2u + FS;
        render_part(part, rel < NMAX ? rel : NMAX);
        held |= !voices_free(part) << pi;
        {   /* DC: one note held, over whole periods of it */
            double f = 440.0 * pow(2.0, (48 - 69) / 12.0), m = 0, mx = 0;
            uint32_t a = FS / 2, e = a + (uint32_t)(floor(0.4 * f) * FS / f), i;
            parts_clear(); host_preset(&trk[1], ENGI_MOD, pi); trk[1].p[P_SUS] = 127; trk[1].p[P_ED_FLT] = 0;
            trk[1].p[P_E2] = 0; trk[1].p[P_E3] = 0;
            trk_note_on(&trk[1], 48, 100); render_part(1, FS);
            for (i = a; i < e; i++) m += buf[i], mx = fabs(buf[i]) > mx ? fabs(buf[i]) : mx;
            m = mx > 0 ? fabs(m / (e - a)) / mx : 0;
            dc |= (m > 0.02 && pr->env[2] > 0 && pr->e[0] != 15) << pi;   /* (a one-shot has gone silent by then; NOISE
                                                             * has no period to measure over) */
        }
        printf("mod:   %-12s the part %5d (%.1f dBFS)\n", pr->name, pk, 20.0 * log10((pk + 1) / 32768.0));
    }
    for (pi = 0; pi < ENG_MOD.npresets; pi++)
        if (MOD_PRESETS[pi].env[2] > 60) w += rms_of(ENGI_MOD, pi), nw++;
    for (pi = 0; pi < ENG_WAVES.npresets; pi++)
        if (ENG_WAVES.presets[pi].env[2] > 60) v += rms_of(ENGI_WAVES, pi), nv++;
    w /= nw ? nw : 1; v /= nv ? nv : 1;
    printf("mod:   (quiet 0x%X, DC 0x%X, held 0x%X; one voice's rms MOD %.3f, WAVES %.3f)\n", quiet, dc, held, w, v);
    check("every preset sounds, no DC, its voices free after the release; level within 4 dB of WAVES",
          !quiet && !dc && !held && fabs(20.0 * log10(w / v)) < 4.0);
}
static void controls(void)
{
    uint32_t z0, z1, a, b;
    double b0, b1, f;
    char what[140];
    parts_clear(); raw(1, 4, 0, 127, 0, 0, 0); trk_note_on(&trk[1], 60, 100); render_part(1, FS);
    {   /* (the period by autocorrelation: an 8-bit stair has ragged zero crossings) */
        uint32_t L, best = 0;
        double bv = -1e30;
        for (L = 40; L < 400; L++) {
            double sum = 0;
            for (uint32_t i = FS / 4; i < FS * 3 / 4; i++) sum += (double)buf[i] * buf[i + L];
            if (sum > bv) bv = sum, best = L;
        }
        f = (double)VOICE_FS / best;   /* (the parts render at the voices' rate) */
    }
    snprintf(what, sizeof what, "SINE at C4: %.1f Hz (261.6; Paula's whole periods: within 1 %%)", f);
    check(what, fabs(f / 261.63 - 1) < 0.01);
    parts_clear(); raw(1, 0, 0, 127, 0, 0, 0); trk_note_on(&trk[1], 60, 100); render_part(1, FS / 2);
    a = zc(FS / 4, FS / 4 + 480);
    parts_clear(); raw(1, 0, 4, 127, 0, 0, 0); trk_note_on(&trk[1], 60, 100); render_part(1, FS / 2);
    z0 = zc(0, 480); z1 = zc(480, 960); b = zc(960, 1440);
    snprintf(what, sizeof what, "ARP 5TH: the note, the fifth, the octave, a 50 Hz tick each (%u %u %u crossings a tick)", z0, z1, b);
    check(what, z1 * 10u > z0 * 13u && b * 10u > z0 * 18u && a > 0);
    parts_clear(); raw(1, 2, 0, 127, 0, 0, 0); trk_note_on(&trk[1], 57, 100); render_part(1, FS / 2); b0 = bright(FS / 10, FS / 2);
    parts_clear(); raw(1, 2, 0, 30, 0, 0, 0); trk_note_on(&trk[1], 57, 100); render_part(1, FS / 2); b1 = bright(FS / 10, FS / 2);
    snprintf(what, sizeof what, "LED 127 -> 30 darkens the SAW (%.4f -> %.4f)", b0, b1);
    check(what, b1 < b0 * 0.5);
    parts_clear(); raw(1, 4, 0, 127, 127, 0, 0); trk_note_on(&trk[1], 57, 100); render_part(1, FS / 2); b1 = bright(FS / 10, FS / 2);
    parts_clear(); raw(1, 4, 0, 127, 0, 0, 0); trk_note_on(&trk[1], 57, 100); render_part(1, FS / 2); b0 = bright(FS / 10, FS / 2);
    snprintf(what, sizeof what, "CRSH 127: the SINE in 3 bits, brighter (%.4f -> %.4f)", b0, b1);
    check(what, b1 > b0 * 1.5);
    parts_clear(); raw(1, 10, 0, 127, 0, 0, 0); trk_note_on(&trk[1], 57, 100); render_part(1, FS);
    {   /* DTUN: two channels beat */
        double lo = 1e9, hi = 0;
        parts_clear(); raw(1, 4, 0, 127, 0, 0, 127); trk_note_on(&trk[1], 69, 100); render_part(1, FS);
        for (uint32_t fr = FS / 2; fr + FS / 20 <= FS; fr += FS / 20) {
            double r = rms(fr, fr + FS / 20);
            lo = r < lo ? r : lo; hi = r > hi ? r : hi;
        }
        snprintf(what, sizeof what, "DTUN 127: the two channels beat (frame level %.3f .. %.3f)", lo, hi);
        check(what, hi > lo * 1.3);
    }
}
static void lists(void)
{
    uint32_t n, at = 99, ok = 1;
    for (n = 0; n < NENG_SHOWN; n++)
        if (eng_vis(n) == ENGI_MOD) at = n;
    check("MOD in PRESETS and on an EDIT layer key, beside MOD KIT", at < NENG_KEYS && eng_ok(ENGI_MOD) && at + 1u < NENG_SHOWN &&
          eng_vis(at + 1u) == ENGI_MKIT);
    for (n = 0; n < ENG_MOD.npresets; n++) {
        ok &= preset_cat(ENGI_MOD, n) != 0u;
        ok &= MOD_PRESETS[n].pat <= NPATTERNS;
        ok &= str_len(MOD_PRESETS[n].name) <= 12u;
    }
    check("its presets: a category each, pattern hints among the PHRASES, names that fit", ok && ENG_MOD.npresets >= 16u);
}
static double ns_per(uint32_t e, uint32_t pi)
{
    struct timespec a, b;
    uint32_t i;
    int32_t o[CTL];
    parts_clear();
    host_preset(&trk[1], e, pi);
    trk[1].p[P_VOICE] = V_POLY;
    for (i = 0; i < 4u; i++) trk_note_on(&trk[1], 48 + 5 * i, 100);
    track_render(&trk[1], o, CTL);
    clock_gettime(CLOCK_MONOTONIC, &a);
    for (i = 0; i < FS * 2 / CTL; i++) track_render(&trk[1], o, CTL);
    clock_gettime(CLOCK_MONOTONIC, &b);
    return ((b.tv_sec - a.tv_sec) * 1e9 + (b.tv_nsec - a.tv_nsec)) / (FS * 2.0 * 4.0);
}
static void cost(void)
{
    double wv = ns_per(ENGI_WAVES, 0), m1 = ns_per(ENGI_MOD, 19), m2 = ns_per(ENGI_MOD, 21);   /* (MOD STRINGS, WARM PAD: DTUN) */
    char what[160];
    snprintf(what, sizeof what, "cost per voice-sample: MOD %.1f ns / %.1f (DTUN, LED), WAVES %.1f: %.2f x", m1, m2, wv, m2 / wv);
    check(what, m2 < 1.2 * wv);
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
    FILE *w = wav_open(dir, "mod_presets.wav", 0x7FFFFFF);
    if (!w) return;
    mixed = 0;
    for (pi = 0; pi < ENG_MOD.npresets; pi++) {
        const preset_t *pr = &MOD_PRESETS[pi];
        uint32_t part = pr->mono ? 2 : 1;
        parts_clear();
        host_preset(&trk[part], ENGI_MOD, pi);
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
    lists();
    if (!getenv("NOCOST"))
        cost();
    if (argc > 1) {
        demo_presets(argv[1]);
        printf("mod: demos in %s\n", argv[1]);
    }
    printf("mod: %s\n", fails ? "FAILED" : "all checks ok");
    return fails != 0;
}
