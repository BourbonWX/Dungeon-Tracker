/* SPDX-License-Identifier: GPL-3.0-only
 * Dungeon Tracker: the WAVES engine (src/mutable/eng_waves.c, after Ambika by Emilie Gillet, GPL-3.0) on the host,
 * through hostsim.c as the other tests.
 *   build/host/waves_test [DEMODIR]          (tests/run_rondo.sh: build/rondo_demo)
 *   build/host/waves_test --scan             (each SHAPE's brightness at PRM 0 / 64 / 127: for voicing presets)
 * 1. every preset held (a chord, or a line on a MONO one) and released: it sounds (peak above -24 dBFS of the part),
 *    never full scale, no DC, and its voices free within its release + 1 s.
 * 2. every SHAPE sounds; PRM moves a wavetable (the brightness changes); GRIT changes the sound; SUB adds an octave
 *    below (zero crossings fewer).
 * 3. the lists: WAVES in PRESETS and on the EDIT layer's keys (in the first NENG_KEYS), DRUM still on a key, NOISE
 *    PRESETS only; its presets' categories and pattern hints in range.
 * 4. cost: host ns per voice-sample against ANALOG (printed; under 2 x).
 * 5. demos into DEMODIR: every preset, and a short song on the four tracks all WAVES. */
#define main hostsim_main
#include "hostsim.c"
#undef main
#ifndef CAT_BASS
#include "category.c"                   /* (preset_cat: hostsim has no ui.c) */
#endif

static int fails;
static void check(const char *what, int ok)
{
    printf("waves: %-92s %s\n", what, ok ? "ok" : "FAIL");
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
static void raw(uint32_t part, int sh, int prm, int cut, int res, int sub, int subt, int grit)
{
    host_preset(&trk[part], ENGI_WAVES, 0);
    trk[part].p[P_E0] = (int16_t)sh; trk[part].p[P_E1] = (int16_t)prm; trk[part].p[P_E2] = 0;
    trk[part].p[P_E3] = (int16_t)sub; trk[part].p[P_E4] = (int16_t)cut; trk[part].p[P_E5] = (int16_t)res;
    trk[part].p[P_E6] = (int16_t)subt; trk[part].p[P_E7] = (int16_t)grit;
    trk[part].p[P_ATK] = 0; trk[part].p[P_DEC] = 64; trk[part].p[P_SUS] = 127; trk[part].p[P_REL] = 20;
    trk[part].p[P_ED_FLT] = 0;
}

static void scan(void)
{
    static const int PRM[3] = {0, 64, 127};
    printf("SHAPE   brightness at PRM 0 / 64 / 127 (first-difference share, A3), rms at 64\n");
    for (int sh = 0; sh < WV_N; sh++) {
        double b[3], r = 0;
        for (int k = 0; k < 3; k++) {
            parts_clear();
            raw(1, sh, PRM[k], 127, 0, 0, 0, 0);
            trk_note_on(&trk[1], 57, 100);
            render_part(1, FS / 2);
            b[k] = bright(FS / 10, FS / 2);
            if (k == 1) r = rms(FS / 10, FS / 2);
            trk_note_off(&trk[1], 57);
        }
        printf("%-6s  %.4f %.4f %.4f   %.3f\n", N_WV_SHAPE[sh], b[0], b[1], b[2], r);
    }
}

/* 1 (the level: WAVES sits with ANALOG, whose one voice at full velocity goes past full scale before the part's level
 * and the mix: so the check is that level, and no preset far from the others) */
static double an_rms;
static void presets(void)
{
    static const uint8_t CHORD[3] = {57, 60, 64}, LINE[4] = {36, 43, 39, 41};
    uint32_t pi;
    uint64_t quiet = 0, loud = 0, dc = 0, held = 0;              /* (bit pi: a preset; 1.0: more than 32 of them) */
    double r1[64], mean = 0;
    for (pi = 0; pi < ENG_ANALOG.npresets; pi++) {
        parts_clear();
        host_preset(&trk[1], 0, pi);
        trk_note_on(&trk[1], ENG_ANALOG.presets[pi].mono ? 36 : 57, 127);
        render_part(1, FS / 2);
        an_rms += rms(FS / 10, FS / 2) / ENG_ANALOG.npresets;
    }
    for (pi = 0; pi < ENG_WAVES.npresets; pi++) {
        const preset_t *pr = &WV_PRESETS[pi];
        uint32_t part = pr->mono ? 2 : 1, k, rel;
        int32_t pk, p1;
        parts_clear();
        host_preset(&trk[part], ENGI_WAVES, pi);
        trk_note_on(&trk[part], pr->mono ? 36 : 57, 127);
        p1 = render_part(part, FS / 2);
        r1[pi % 64u] = p1;                                /* (peaks: the plucks fall silent within the window) */
        mean += 20.0 * log10(p1 + 1.0) / ENG_WAVES.npresets;
        an_rms += 0;
        parts_clear();
        host_preset(&trk[part], ENGI_WAVES, pi);
        if (pr->mono) {
            pk = 0;
            for (k = 0; k < 4u; k++) {
                int32_t p2;
                trk_note_on(&trk[part], LINE[k], 100);
                p2 = render_part(part, FS / 4);
                pk = p2 > pk ? p2 : pk;
                if (k < 3u)
                    trk_note_off(&trk[part], LINE[k]);
            }
            render_part(part, FS);
            {   /* (over whole periods of the lowest tone: the note's, an octave or two down with SUB) */
                double fl = 440.0 * pow(2.0, (LINE[3] - 69) / 12.0) / 4.0, n = floor(0.75 * fl);
                uint32_t e = FS / 4 + (uint32_t)(n * FS / fl);
                double m = 0, pk2 = 0;
                for (uint32_t i = FS / 4; i < e; i++) m += buf[i], pk2 = fabs(buf[i]) > pk2 ? fabs(buf[i]) : pk2;
                m = pk2 > 0 ? fabs(m / (double)(e - FS / 4)) / pk2 : 0;
                if (m > 0.01) printf("waves:   (%s: DC %.4f)\n", pr->name, m);
                dc |= m > 0.01 ? 1ull << pi : 0u;
            }
            trk_note_off(&trk[part], LINE[3]);
        } else {
            for (k = 0; k < 3u; k++)
                trk_note_on(&trk[part], CHORD[k], 100);
            pk = render_part(part, FS * 6 / 10);
            dc |= dc_from(FS / 4) > 0.01 ? 1ull << pi : 0u;
            for (k = 0; k < 3u; k++)
                trk_note_off(&trk[part], CHORD[k]);
        }
        quiet |= pk < 2048 ? 1ull << pi : 0u;          /* -24 dBFS */
        rel = (uint32_t)(TIME_MS_X10[trk[part].p[P_REL] & 127] / 10u) * FS / 1000u * 2u + FS;
        render_part(part, rel < NMAX ? rel : NMAX);
        held |= !voices_free(part) ? 1ull << pi : 0u;
        printf("waves:   %-12s one voice %5d (%.1f dBFS), the part %5d\n", pr->name, p1, 20.0 * log10((p1 + 1) / 32768.0), pk);
    }
    for (pi = 0; pi < ENG_WAVES.npresets; pi++)
        loud |= fabs(20.0 * log10(r1[pi] + 1.0) - mean) > 6.0 ? 1ull << pi : 0u;
    {   /* the level against ANALOG's: one voice's rms, the sustained presets of each (SUS above 60) */
        double w = 0, a = 0;
        uint32_t nw = 0, na = 0;
        for (pi = 0; pi < ENG_WAVES.npresets; pi++)
            if (WV_PRESETS[pi].env[2] > 60) {
                parts_clear(); host_preset(&trk[1], ENGI_WAVES, pi); trk_note_on(&trk[1], 57, 127); render_part(1, FS / 2);
                w += rms(FS / 4, FS / 2); nw++;
            }
        for (pi = 0; pi < ENG_ANALOG.npresets; pi++)
            if (ENG_ANALOG.presets[pi].env[2] > 60) {
                parts_clear(); host_preset(&trk[1], 0, pi); trk_note_on(&trk[1], 57, 127); render_part(1, FS / 2);
                a += rms(FS / 4, FS / 2); na++;
            }
        mean = nw ? w / nw : 0;
        an_rms = na ? a / na : 1;
    }
    printf("waves:   (quiet 0x%llX, over 6 dB from the others 0x%llX, DC 0x%llX, voices held 0x%llX; one voice's rms: WAVES %.3f, ANALOG %.3f)\n",
           (unsigned long long)quiet, (unsigned long long)loud, (unsigned long long)dc, (unsigned long long)held, mean, an_rms);
    check("every preset sounds, none 6 dB from the others, no DC, its voices free after the release; level within 3 dB of ANALOG",
          !quiet && !loud && !dc && !held && fabs(20.0 * log10(mean / an_rms)) < 3.0);
}

/* 2 */
static void controls(void)
{
    uint32_t sh, ok = 1, z0, z1;
    double b0, b1, e0, e1;
    for (sh = 0; sh < WV_N; sh++) {
        parts_clear();
        raw(1, (int)sh, 64, 127, 0, 0, 0, 0);
        trk_note_on(&trk[1], 57, 100);
        ok &= render_part(1, FS / 3) > 2048;
    }
    check("every SHAPE sounds (16 wavetables, WAVEQ, 8BIT, DPWM, QSAW)", ok);
    parts_clear(); raw(1, 4, 0, 127, 0, 0, 0, 0); trk_note_on(&trk[1], 45, 100); render_part(1, FS / 2); b0 = bright(FS / 10, FS / 2);
    parts_clear(); raw(1, 4, 127, 127, 0, 0, 0, 0); trk_note_on(&trk[1], 45, 100); render_part(1, FS / 2); b1 = bright(FS / 10, FS / 2);
    check("PRM moves through a wavetable (the brightness changes)", fabs(b1 - b0) > 0.1 * (b0 > b1 ? b0 : b1));
    parts_clear(); raw(1, 6, 64, 127, 0, 0, 0, 0); trk_note_on(&trk[1], 69, 100); render_part(1, FS / 2); e0 = bright(FS / 10, FS / 2);
    parts_clear(); raw(1, 6, 64, 127, 0, 0, 0, 127); trk_note_on(&trk[1], 69, 100); render_part(1, FS / 2); e1 = bright(FS / 10, FS / 2);
    check("GRIT changes the sound (held, 8-bit)", fabs(e1 - e0) > 0.02 * e0);
    parts_clear(); raw(1, 18, 0, 127, 0, 0, 0, 0); trk_note_on(&trk[1], 69, 100); render_part(1, FS / 2); z0 = zc(FS / 10, FS / 2);
    parts_clear(); raw(1, 18, 0, 127, 0, 127, 0, 0); trk_note_on(&trk[1], 69, 100); render_part(1, FS / 2); z1 = zc(FS / 10, FS / 2);
    printf("waves:   (SUB 0 / 127 on DPWM: %u / %u zero crossings)\n", z0, z1);
    check("SUB at 127: a square an octave below (fewer crossings, about half)", z1 * 3u < z0 * 2u);
}

/* 3 */
static void lists(void)
{
    uint32_t n, wv = 99, dr = 99, nz = 99, ok = 1;
    for (n = 0; n < NENG_SHOWN; n++) {
        if (eng_vis(n) == ENGI_WAVES) wv = n;
        if (eng_vis(n) == ENGI_DRUM) dr = n;
        if (eng_vis(n) == 11u) nz = n;
    }
    check("WAVES in PRESETS and on an EDIT layer key; DRUM still on a key; NOISE PRESETS only (after the 14)",
          wv < NENG_KEYS && dr < NENG_KEYS && nz >= NENG_KEYS && nz < NENG_SHOWN && NENG_SHOWN == NENGINES - 2u);
    for (n = 0; n < ENG_WAVES.npresets; n++) {
        ok &= preset_cat(ENGI_WAVES, n) != 0u;
        ok &= WV_PRESETS[n].pat <= NPATTERNS;
        ok &= str_len(WV_PRESETS[n].name) <= 12u;
    }
    check("its presets: a category each, pattern hints among the PHRASES, names that fit", ok && ENG_WAVES.npresets >= 16u);
}

/* 4 */
static double ns_per(uint32_t e, uint32_t pi, int sh)
{
    struct timespec a, b;
    uint32_t i;
    int32_t o[CTL];
    parts_clear();
    host_preset(&trk[1], e, pi);
    if (sh >= 0) trk[1].p[P_E0] = (int16_t)sh;
    for (i = 0; i < 4u; i++) trk_note_on(&trk[1], 48 + 5 * i, 100);
    track_render(&trk[1], o, CTL);
    clock_gettime(CLOCK_MONOTONIC, &a);
    for (i = 0; i < FS * 2 / CTL; i++) track_render(&trk[1], o, CTL);
    clock_gettime(CLOCK_MONOTONIC, &b);
    return ((b.tv_sec - a.tv_sec) * 1e9 + (b.tv_nsec - a.tv_nsec)) / (FS * 2.0 * 4.0);
}
static void cost(void)
{
    double an = ns_per(0, 1, -1), wt = ns_per(ENGI_WAVES, 0, 3), qs = ns_per(ENGI_WAVES, 0, WV_QSAW);
    char what[160];
    snprintf(what, sizeof what, "cost per voice-sample: WAVES %.1f ns (a wavetable) / %.1f (QSAW), ANALOG %.1f: %.2f x", wt, qs,
             an, wt / an);
    check(what, wt < 2.0 * an && qs < 2.0 * an);
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
    FILE *w = wav_open(dir, "waves_presets.wav", 0x7FFFFFF);
    if (!w) return;
    mixed = 0;
    for (pi = 0; pi < ENG_WAVES.npresets; pi++) {
        const preset_t *pr = &WV_PRESETS[pi];
        uint32_t part = pr->mono ? 2 : 1;
        parts_clear();
        host_preset(&trk[part], ENGI_WAVES, pi);
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

/* 8 bars at 100 BPM on the four tracks, T1 the CYCLE kit, T2-T4 WAVES: bars 1-4 MANA POOL, DEEP DELVE, CRYSTAL CAVE;
 * bars 5-8 TORCHLIGHT stabs, ACID WAVE, BOSS FIGHT, COIN GET on the last beat */
static void demo_song(const char *dir)
{
    static const uint8_t CH[4][3] = {{57, 60, 64}, {53, 57, 60}, {50, 53, 57}, {52, 56, 59}};   /* Am F Dm E */
    static const uint8_t ROOT[4] = {33, 29, 26, 28};
    static const uint8_t BELL[16] = {76, 0, 0, 72, 0, 0, 69, 0, 76, 0, 0, 0, 74, 0, 72, 0};
    static const uint8_t LEAD[16] = {81, 0, 84, 0, 83, 0, 79, 0, 81, 0, 0, 76, 0, 79, 0, 0};
    static const uint8_t ACID[16] = {0, 0, 12, 0, 0, 0, 12, 7, 0, 0, 12, 0, 3, 0, 12, 10};
    uint32_t step = FS * 60 / 100 / 4, s, k, bars = 8;
    uint8_t lead_on = 0, bass_on = 0;
    FILE *w = wav_open(dir, "waves_song.wav", step * 16 * bars + FS * 3);
    if (!w) return;
    parts_clear();
    host_preset(&trk[0], ENGI_CKIT, 0);
    ck_kit_load(&trk[0], 0);
    host_preset(&trk[1], ENGI_WAVES, 17);             /* MANA POOL */
    host_preset(&trk[2], ENGI_WAVES, 14);             /* DEEP DELVE */
    host_preset(&trk[3], ENGI_WAVES, 9);              /* CRYSTAL CAVE */
    trk[1].p[P_LEVEL] = 96; trk[3].p[P_LEVEL] = 100;
    for (s = 0; s < 16 * bars; s++) {
        uint32_t q = s % 16, bar = s / 16, c = bar % 4, two = bar >= 4;
        if (s == 64) {                                 /* bar 5: the other sounds */
            for (k = 0; k < 3u; k++) trk_note_off(&trk[1], CH[3][k]);
            if (bass_on) trk_note_off(&trk[2], bass_on);
            if (lead_on) trk_note_off(&trk[3], lead_on);
            bass_on = lead_on = 0;
            host_preset_req(&trk[1], ENGI_WAVES, 8);   /* TORCHLIGHT */
            host_preset_req(&trk[2], ENGI_WAVES, 11);  /* ACID WAVE */
            host_preset_req(&trk[3], ENGI_WAVES, 12);  /* BOSS FIGHT */
            trk[3].p[P_LEVEL] = 92;
        }
        if (bar >= 1 && (q % 8 == 0 || (q == 10 && bar % 2))) trk_note_on(&trk[0], 36, 120);
        if (bar >= 2 && (q == 4 || q == 12)) trk_note_on(&trk[0], 38, 110);
        if (bar >= 1 && q % 2 == 0) trk_note_on(&trk[0], q == 14 ? 46 : 42, q % 4 == 2 ? 92 : 64);
        if (!two && q == 0) {
            if (c) for (k = 0; k < 3u; k++) trk_note_off(&trk[1], CH[c - 1][k]);
            for (k = 0; k < 3u; k++) trk_note_on(&trk[1], CH[c][k], 86);
        }
        if (two && (q == 2 || q == 6 || q == 10 || q == 14)) {   /* TORCHLIGHT: the off-beat stabs */
            for (k = 0; k < 3u; k++) trk_note_on(&trk[1], CH[c][k] + 12u, 80);
        }
        if (two && (q == 4 || q == 8 || q == 12 || q == 0))
            for (k = 0; k < 3u; k++) trk_note_off(&trk[1], CH[(q == 0 ? c + 3u : c) % 4u][k] + 12u);
        if (!two && (q == 0 || q == 10)) {             /* DEEP DELVE: the root, held */
            if (bass_on) trk_note_off(&trk[2], bass_on);
            bass_on = ROOT[c];
            trk_note_on(&trk[2], bass_on, 110);
        }
        if (two) {                                     /* ACID WAVE: sixteenths on the root */
            if (bass_on) trk_note_off(&trk[2], bass_on);
            bass_on = (uint8_t)(ROOT[c] + 12u + ACID[q]);
            trk_note_on(&trk[2], bass_on, q % 4 == 0 ? 120 : 90);
        }
        {
            const uint8_t *L = two ? LEAD : BELL;
            uint32_t n = L[q] ? (uint32_t)L[q] - (c == 1 ? 1u : c == 2 ? 3u : 0u) : 0u;
            if (n) {
                if (lead_on) trk_note_off(&trk[3], lead_on);
                lead_on = (uint8_t)n;
                trk_note_on(&trk[3], lead_on, 100);
            } else if (two && q == 15 && lead_on) {
                trk_note_off(&trk[3], lead_on);
                lead_on = 0;
            }
        }
        mix_to(w, step);
    }
    for (k = 0; k < 3u; k++) trk_note_off(&trk[1], CH[3][k] + 12u);
    if (bass_on) trk_note_off(&trk[2], bass_on);
    if (lead_on) trk_note_off(&trk[3], lead_on);
    host_preset_req(&trk[3], ENGI_WAVES, 10);          /* COIN GET to end it */
    mix_to(w, CTL * 4);
    trk_note_on(&trk[3], 84, 110);
    trk_note_on(&trk[0], 36, 120);
    mix_to(w, FS / 8);
    trk_note_off(&trk[3], 84);
    trk_note_on(&trk[3], 91, 110);
    mix_to(w, FS / 8);
    trk_note_off(&trk[3], 91);
    mix_to(w, FS * 3 - FS / 4 - CTL * 4);
    fclose(w);
}

int main(int argc, char **argv)
{
    fm6_init();
    if (argc > 1 && !strcmp(argv[1], "--scan")) {
        scan();
        return 0;
    }
    if (argc > 1 && !strcmp(argv[1], "--levels")) {      /* one voice at velocity 127: each engine's presets, peak and rms */
        static const uint8_t E[4] = {0, ENGI_CTONE, ENGI_FM6, ENGI_WAVES};
        for (uint32_t q = 0; q < 4u; q++) {
            double sp = 0, sr = 0;
            uint32_t n = ENGINES[E[q]]->npresets, pi;
            for (pi = 0; pi < n; pi++) {
                int32_t pk;
                parts_clear();
                host_preset(&trk[1], E[q], pi);
                trk_note_on(&trk[1], ENGINES[E[q]]->presets[pi].mono ? 40 : 57, 127);
                pk = render_part(1, FS / 2);
                sp += pk; sr += rms(FS / 10, FS / 2);
            }
            printf("%-7s mean one-voice peak %6.0f, rms %.3f\n", ENGINES[E[q]]->name, sp / n, sr / n);
        }
        return 0;
    }
    presets();
    controls();
    lists();
    if (!getenv("NOCOST"))
        cost();
    if (argc > 1) {
        demo_presets(argv[1]);
        demo_song(argv[1]);
        printf("waves: demos in %s\n", argv[1]);
    }
    printf("waves: %s\n", fails ? "FAILED" : "all checks ok");
    return fails != 0;
}
