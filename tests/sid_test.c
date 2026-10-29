/* SPDX-License-Identifier: GPL-3.0-only
 * Dungeon Tracker: the SID engine (src/eng_sid.c, after the C64's MOS 6581) and the LOFI KIT (SAMPLE's SET 3) on the
 * host, through hostsim.c as the other tests.
 *   build/host/sid_test [DEMODIR]            (tests/run_rondo.sh: build/rondo_demo)
 * 1. every SID preset held (a chord, or a line on a MONO one) and released: it sounds, none 6 dB from the others, no DC,
 *    voices free after the release, the level near ANALOG's.
 * 2. every WAVE sounds; PW moves the pulse (the brightness); SYNC, RING and SWEEP change the sound; NSE is noise; DRM
 *    starts as noise and turns to a tone.
 * 3. the lists: SID on an EDIT layer key, DRUM too, GRAIN and NOISE PRESETS only; the per-engine tables in line (13
 *    reserved, SLICE retired: FMKIT, FMTONE, WAVES, SID where they were).
 * 4. the LOFI KIT: SET 3, its hits on the General MIDI drum notes, the toms pitched, nothing elsewhere; 1.0: the
 *    DUNGEON KIT, SET 1, the same notes and the crashes.
 * 5. cost: host ns per voice-sample against ANALOG (under 2 x).
 * 6. demos into DEMODIR: every SID preset, and a short song: the LOFI KIT on T1, SID on T2-T4. */
#define main hostsim_main
#include "hostsim.c"
#undef main
#ifndef CAT_BASS
#include "category.c"                   /* (preset_cat: hostsim has no ui.c) */
#endif

static int fails;
static void check(const char *what, int ok)
{
    printf("sid: %-94s %s\n", what, ok ? "ok" : "FAIL");
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

static void raw(uint32_t part, int wave, int pw, int pwm, int mod, int tune, int sweep, int cut, int res)
{
    host_preset(&trk[part], ENGI_SID, 0);
    trk[part].p[P_E0] = (int16_t)wave; trk[part].p[P_E1] = (int16_t)pw; trk[part].p[P_E2] = (int16_t)pwm;
    trk[part].p[P_E3] = (int16_t)mod; trk[part].p[P_E4] = (int16_t)tune; trk[part].p[P_E5] = (int16_t)sweep;
    trk[part].p[P_E6] = (int16_t)cut; trk[part].p[P_E7] = (int16_t)res;
    trk[part].p[P_ATK] = 0; trk[part].p[P_DEC] = 64; trk[part].p[P_SUS] = 127; trk[part].p[P_REL] = 20;
    trk[part].p[P_ED_FLT] = 0;
}
static double probe(int wave, int pw, int mod, int tune, int sweep, uint32_t note, uint32_t *zcs)
{
    parts_clear();
    raw(1, wave, pw, 0, mod, tune, sweep, 127, 0);
    trk_note_on(&trk[1], note, 100);
    render_part(1, FS / 2);
    if (zcs) *zcs = zc(FS / 10, FS / 2);
    return bright(FS / 10, FS / 2);
}

/* 1 (the level: SID sits with ANALOG, whose one voice at full velocity goes past full scale before the part's level
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
    for (pi = 0; pi < ENG_SID.npresets; pi++) {
        const preset_t *pr = &SID_PRESETS[pi];
        uint32_t part = pr->mono ? 2 : 1, k, rel;
        int32_t pk, p1;
        parts_clear();
        host_preset(&trk[part], ENGI_SID, pi);
        trk_note_on(&trk[part], pr->mono ? 36 : 57, 127);
        p1 = render_part(part, FS / 2);
        r1[pi % 64u] = p1;                                /* (peaks: the plucks fall silent within the window) */
        mean += 20.0 * log10(p1 + 1.0) / ENG_SID.npresets;
        an_rms += 0;
        parts_clear();
        host_preset(&trk[part], ENGI_SID, pi);
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
                if (m > 0.01) printf("sid:   (%s: DC %.4f)\n", pr->name, m);
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
        printf("sid:   %-12s one voice %5d (%.1f dBFS), the part %5d\n", pr->name, p1, 20.0 * log10((p1 + 1) / 32768.0), pk);
    }
    for (pi = 0; pi < ENG_SID.npresets; pi++)
        loud |= fabs(20.0 * log10(r1[pi] + 1.0) - mean) > 6.0 ? 1ull << pi : 0u;
    {   /* the level against ANALOG's: one voice's rms, the sustained presets of each (SUS above 60) */
        double w = 0, a = 0;
        uint32_t nw = 0, na = 0;
        for (pi = 0; pi < ENG_SID.npresets; pi++)
            if (SID_PRESETS[pi].env[2] > 60) {
                parts_clear(); host_preset(&trk[1], ENGI_SID, pi); trk_note_on(&trk[1], 57, 127); render_part(1, FS / 2);
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
    printf("sid:   (quiet 0x%llX, over 6 dB from the others 0x%llX, DC 0x%llX, voices held 0x%llX; one voice's rms: SID %.3f, ANALOG %.3f)\n",
           (unsigned long long)quiet, (unsigned long long)loud, (unsigned long long)dc, (unsigned long long)held, mean, an_rms);
    check("every preset sounds, none 6 dB from the others, no DC, its voices free after the release; level within 3 dB of ANALOG",
          !quiet && !loud && !dc && !held && fabs(20.0 * log10(mean / an_rms)) < 3.0);
}

/* 2 */
static void controls(void)
{
    uint32_t w, ok = 1, z0, z1, za, zb;
    double b0, b1;
    for (w = 0; w < SW_N; w++) {
        parts_clear();
        raw(1, (int)w, 64, 0, 0, 0, 30, 127, 0);
        trk_note_on(&trk[1], 57, 100);
        ok &= render_part(1, FS / 3) > 2048;
    }
    check("every WAVE sounds (TRI SAW PLS NSE, the combined ST PT PS PST, DRM)", ok);
    b0 = probe(SW_PLS, 64, 0, 0, 0, 45, 0);
    b1 = probe(SW_PLS, 8, 0, 0, 0, 45, 0);
    check("PW: a narrow pulse is brighter than a square", b1 > b0 * 1.2);
    {   /* SYNC: the voice's period becomes the modulator's (the note's): its wave repeats every period of the note */
        double c0 = 0, c1 = 0;
        uint32_t per = (uint32_t)(FS / 110.0 + 0.5), i;
        probe(SW_SAW, 64, 0, 7, 0, 45, 0);
        for (i = FS / 10; i < FS / 10 + FS / 20; i++) c0 += fabs(buf[i] - buf[i + per]);
        probe(SW_SAW, 64, 1, 7, 0, 45, 0);
        for (i = FS / 10; i < FS / 10 + FS / 20; i++) c1 += fabs(buf[i] - buf[i + per]);
        printf("sid:   (SYNC: the difference a note's period apart, off %.1f, on %.1f)\n", c0, c1);
        check("SYNC: the voice (a fifth up) repeats at the note's period", c1 < c0 * 0.5);
    }
    probe(SW_TRI, 64, 0, 17, 0, 57, &z0);
    probe(SW_TRI, 64, 2, 17, 0, 57, &z1);
    check("RING changes TRI (crossings move)", z0 != z1);
    probe(SW_NSE, 64, 0, 0, 0, 57, &z0);
    probe(SW_TRI, 64, 0, 0, 0, 57, &z1);
    printf("sid:   (NSE / TRI at A3: %u / %u crossings)\n", z0, z1);
    check("NSE is noise (several times the crossings of the note's TRI: the chip clocks it with the note)", z0 > z1 * 3u);
    parts_clear();
    raw(1, SW_PLS, 64, 0, 0, 0, 63, 127, 0);
    trk[1].p[P_DEC] = 70; trk[1].p[P_SUS] = 0;
    trk_note_on(&trk[1], 45, 100);
    render_part(1, FS);
    za = zc(FS / 50, FS / 10); zb = zc(FS / 2, FS / 2 + FS * 8 / 100);
    check("SWEEP: the pitch falls with the envelope (more crossings early than late)", za > zb + zb / 2u);
    parts_clear();
    raw(1, SW_DRM, 64, 0, 0, 0, 36, 127, 0);
    trk[1].p[P_DEC] = 60; trk[1].p[P_SUS] = 0;
    trk_note_on(&trk[1], 36, 120);
    render_part(1, FS / 2);
    za = zc(0, SID_DRM_NOISE); zb = zc(FS / 10, FS / 10 + SID_DRM_NOISE);
    check("DRM: a burst of noise, then a tone", za > zb * 3u);
}

/* 3 */
static void lists(void)
{
    uint32_t n, sd = 99, dr = 99, nz = 99, gr = 99, ok = 1;
    for (n = 0; n < NENG_SHOWN; n++) {
        if (eng_vis(n) == ENGI_SID) sd = n;
        if (eng_vis(n) == ENGI_DRUM) dr = n;
        if (eng_vis(n) == 11u) nz = n;
        if (eng_vis(n) == 8u) gr = n;
    }
    check("SID in PRESETS and on an EDIT layer key; DRUM still on a key; GRAIN and NOISE PRESETS only (after the 14)",
          sd < NENG_KEYS && dr < NENG_KEYS && nz >= NENG_KEYS && gr >= NENG_KEYS && nz < NENG_SHOWN && NENG_SHOWN == NENGINES - 2u);
    for (n = 0; n < ENG_SID.npresets; n++) {
        ok &= preset_cat(ENGI_SID, n) != 0u;
        ok &= SID_PRESETS[n].pat <= NPATTERNS;
        ok &= str_len(SID_PRESETS[n].name) <= 12u;
    }
    check("its presets: a category each, pattern hints among the PHRASES, names that fit", ok && ENG_SID.npresets >= 16u);
    check("the engine tables in line: 13 reserved (SLICE retired), FMKIT 14, FMTONE 15, WAVES 16, SID 17",
          NENGINES == 18u && str_eq(ENGINES[13]->name, "-") && ENGINES[ENGI_CKIT] == &ENG_CKIT && ENGINES[ENGI_CTONE] == &ENG_CTONE &&
          ENGINES[ENGI_WAVES] == &ENG_WAVES && ENGINES[ENGI_SID] == &ENG_SID && !eng_ok(13u) && eng_ok(ENGI_SID) &&
          MIDI_CC_ENG[ENGI_CKIT] == 0x32 && MIDI_CC_ENG[ENGI_WAVES] == 0x56 && MIDI_CC_ENG[ENGI_SID] == 0x78 &&
          str_eq(N_ENGNAME[ENGI_SID], "SID") && str_eq(N_ENGNAME[13], "-"));
}

/* 4 */
static void lofi(void)
{
    static const uint8_t HIT[] = {35, 36, 37, 38, 39, 40, 41, 42, 43, 44, 45, 46, 47, 48, 50, 69, 70, 82};
    uint32_t k, ok = 1, z41, z45, z50;
    int32_t pk, pno;
    parts_clear(); host_preset(&trk[1], ENGI_SAMPLE, 0); trk_note_on(&trk[1], 60, 120); pno = render_part(1, FS / 4);
    check("SAMPLE's SET 3 is the LOFI KIT, its preset suggests BEAT", str_eq(SMP_SETS[3].name, "LOFI KIT") &&
          str_eq(ENG_SAMPLE.presets[3].name, "LOFI KIT") && ENG_SAMPLE.presets[3].pat == 12u);
    for (k = 0; k < sizeof HIT; k++) {
        parts_clear();
        host_preset(&trk[1], ENGI_SAMPLE, 3);
        trk_note_on(&trk[1], HIT[k], 120);
        pk = render_part(1, FS / 4);
        ok &= pk > pno / 4 && pk < pno * 2;               /* (one voice before the part's level: as PIANO's) */
        if (pk <= pno / 4 || pk >= pno * 2) printf("sid:   (LOFI KIT note %u: peak %d, PIANO %d)\n", HIT[k], pk, pno);
    }
    check("its hits sound on the GM drum notes (kick 35-36, rim 37, snares 38-40, toms, hats 42 44 46, shells 69, shaker 70 82)", ok);
    parts_clear(); host_preset(&trk[1], ENGI_SAMPLE, 3); trk_note_on(&trk[1], 60, 120); pk = render_part(1, FS / 4);
    check("a note with no hit is silent (60)", pk < 64);
    parts_clear(); host_preset(&trk[1], ENGI_SAMPLE, 3); trk_note_on(&trk[1], 41, 120); render_part(1, FS / 5); z41 = zc(0, FS / 5);
    parts_clear(); host_preset(&trk[1], ENGI_SAMPLE, 3); trk_note_on(&trk[1], 45, 120); render_part(1, FS / 5); z45 = zc(0, FS / 5);
    parts_clear(); host_preset(&trk[1], ENGI_SAMPLE, 3); trk_note_on(&trk[1], 50, 120); render_part(1, FS / 5); z50 = zc(0, FS / 5);
    printf("sid:   (LOFI KIT toms 41 / 45 / 50: %u / %u / %u crossings)\n", z41, z45, z50);
    check("the tom is pitched: lower on 41, higher on 50", z41 < z45 && z45 < z50);
}
/* 4b. 1.0: the DUNGEON KIT, SET 1 (once TRANH's alias): its hits on the LOFI KIT's notes and the crashes */
static void dungeon_kit(void)
{
    static const uint8_t HIT[] = {35, 36, 37, 38, 39, 40, 41, 42, 43, 44, 45, 46, 47, 48, 49, 50, 52, 55, 57, 69, 70, 82};
    uint32_t k, ok = 1, z41, z50;
    int32_t pk, pno;
    parts_clear(); host_preset(&trk[1], ENGI_SAMPLE, 0); trk_note_on(&trk[1], 60, 120); pno = render_part(1, FS / 4);
    check("1.0: SAMPLE's SET 1 is the DUNGEON KIT (ten hits), its preset suggests BEAT", str_eq(SMP_SETS[1].name, "DUNGEON KIT") &&
          str_eq(ENG_SAMPLE.presets[1].name, "DUNGEON KIT") && ENG_SAMPLE.presets[1].pat == 12u && SMP_SET_ORIG[1] == 1u &&
          preset_cat(ENGI_SAMPLE, 1) == CAT_DRUM);
    for (k = 0; k < sizeof HIT; k++) {
        parts_clear();
        host_preset(&trk[1], ENGI_SAMPLE, 1);
        trk_note_on(&trk[1], HIT[k], 120);
        pk = render_part(1, FS / 4);
        ok &= pk > pno / 4 && pk < pno * 2;
        if (pk <= pno / 4 || pk >= pno * 2) printf("sid:   (DUNGEON KIT note %u: peak %d, PIANO %d)\n", HIT[k], pk, pno);
    }
    check("  its hits sound on the GM drum notes (the LOFI KIT's, the GONG on 49 52 55 57)", ok);
    parts_clear(); host_preset(&trk[1], ENGI_SAMPLE, 1); trk_note_on(&trk[1], 60, 120); pk = render_part(1, FS / 4);
    check("  a note with no hit is silent (60)", pk < 64);
    parts_clear(); host_preset(&trk[1], ENGI_SAMPLE, 1); trk_note_on(&trk[1], 41, 120); render_part(1, FS / 5); z41 = zc(0, FS / 5);
    parts_clear(); host_preset(&trk[1], ENGI_SAMPLE, 1); trk_note_on(&trk[1], 50, 120); render_part(1, FS / 5); z50 = zc(0, FS / 5);
    check("  the TOLL is pitched: lower on 41, higher on 50", z41 < z50);
}

/* 5 */
static double ns_per(uint32_t e, uint32_t pi, int w)
{
    struct timespec a, b;
    uint32_t i;
    int32_t o[CTL];
    parts_clear();
    host_preset(&trk[1], e, pi);
    if (w >= 0) trk[1].p[P_E0] = (int16_t)w;
    for (i = 0; i < 4u; i++) trk_note_on(&trk[1], 48 + 5 * i, 100);
    track_render(&trk[1], o, CTL);
    clock_gettime(CLOCK_MONOTONIC, &a);
    for (i = 0; i < FS * 2 / CTL; i++) track_render(&trk[1], o, CTL);
    clock_gettime(CLOCK_MONOTONIC, &b);
    return ((b.tv_sec - a.tv_sec) * 1e9 + (b.tv_nsec - a.tv_nsec)) / (FS * 2.0 * 4.0);
}
static void cost(void)
{
    double an = ns_per(0, 1, -1), sp = ns_per(ENGI_SID, 0, SW_PLS), sn = ns_per(ENGI_SID, 0, SW_NSE);
    char what[160];
    snprintf(what, sizeof what, "cost per voice-sample: SID %.1f ns (PLS) / %.1f (NSE), ANALOG %.1f: %.2f x", sp, sn, an, sp / an);
    check(what, sp < 2.0 * an && sn < 2.0 * an);
}

/* 6 */
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
    FILE *w = wav_open(dir, "sid_presets.wav", 0x7FFFFFF);
    if (!w) return;
    mixed = 0;
    for (pi = 0; pi < ENG_SID.npresets; pi++) {
        const preset_t *pr = &SID_PRESETS[pi];
        uint32_t part = pr->mono ? 2 : 1, drum = pr->e[0] == SW_DRM || pr->e[0] == SW_NSE;
        parts_clear();
        host_preset(&trk[part], ENGI_SID, pi);
        if (drum) {                                    /* the drums: a bar of them */
            for (k = 0; k < 8u; k++) {
                trk_note_on(&trk[part], 48, k % 2 ? 90 : 120);
                mix_to(w, FS / 8);
                trk_note_off(&trk[part], 48);
            }
        } else if (pr->mono) {
            for (k = 0; k < 8u; k++) {
                trk_note_on(&trk[part], LINE[k] + (pr->e[0] == SW_TRI || pi >= 3 ? 24u : 0u), 100);
                mix_to(w, FS / 5 - FS / 40);
                trk_note_off(&trk[part], LINE[k] + (pr->e[0] == SW_TRI || pi >= 3 ? 24u : 0u));
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

/* 8 bars at 86 BPM: T1 the LOFI KIT (a lazy boom-bap, the shaker on the off-beats, the shells at the turn), T2 SID
 * STRINGS on the chords, T3 MONTY BASS, T4 SYNC LEAD from bar 5 (TRI FLUTE before) */
static void demo_song(const char *dir)
{
    static const uint8_t CH[4][3] = {{57, 60, 64}, {53, 57, 60}, {55, 59, 62}, {52, 55, 59}};   /* Am F G Em */
    static const uint8_t ROOT[4] = {33, 29, 31, 28};
    static const uint8_t BASS[16] = {0, 0, 12, 0, 0, 7, 0, 0, 0, 0, 12, 0, 10, 0, 7, 0};
    static const uint8_t MEL[16] = {76, 0, 0, 0, 74, 0, 72, 0, 71, 0, 0, 0, 72, 0, 69, 0};
    uint32_t step = FS * 60 / 86 / 4, s, k, bars = 8, sw = step * 3 / 10;
    uint8_t lead_on = 0, bass_on = 0;
    FILE *w = wav_open(dir, "sid_lofi_song.wav", step * 16 * bars + FS * 3);
    if (!w) return;
    parts_clear();
    host_preset(&trk[0], ENGI_SAMPLE, 3);              /* LOFI KIT */
    host_preset(&trk[1], ENGI_SID, 7);                 /* SID STRINGS */
    host_preset(&trk[2], ENGI_SID, 0);                 /* MONTY BASS */
    host_preset(&trk[3], ENGI_SID, 5);                 /* TRI FLUTE */
    trk[1].p[P_LEVEL] = 80; trk[3].p[P_LEVEL] = 92;
    for (s = 0; s < 16 * bars; s++) {
        uint32_t q = s % 16, bar = s / 16, c = bar % 4, two = bar >= 4;
        if (s == 64) {
            if (lead_on) trk_note_off(&trk[3], lead_on);
            lead_on = 0;
            host_preset_req(&trk[3], ENGI_SID, 3);    /* SYNC LEAD */
            trk[3].p[P_LEVEL] = 84;
        }
        if (q % 2 && sw) mix_to(w, sw);                 /* (swing: the off sixteenths late) */
        if (q == 0 || q == 7 || q == 10 || (bar % 2 && q == 11)) trk_note_on(&trk[0], 36, q ? 96 : 120);
        if (q == 4 || q == 12) trk_note_on(&trk[0], two ? 40 : 38, 118);
        if ((q == 6 || q == 15) && bar % 2) trk_note_on(&trk[0], 38, 30);
        if (q % 2 == 0) trk_note_on(&trk[0], q == 14 && two ? 46 : 42, q % 4 ? 70 : 92);
        if (q == 2 || q == 6 || q == 10 || q == 14) trk_note_on(&trk[0], 70, 80);
        if (bar % 4 == 3 && q >= 13) trk_note_on(&trk[0], q == 13 ? 50 : q == 14 ? 45 : 41, 100);
        if (bar % 4 == 1 && q == 3) trk_note_on(&trk[0], 69, 100);
        if (q == 0) {
            if (bar) for (k = 0; k < 3u; k++) trk_note_off(&trk[1], CH[(c + 3u) % 4u][k]);
            for (k = 0; k < 3u; k++) trk_note_on(&trk[1], CH[c][k], 76);
        }
        if (BASS[q] || q == 0) {
            if (bass_on) trk_note_off(&trk[2], bass_on);
            bass_on = (uint8_t)(ROOT[c] + (q ? BASS[q] : 0u));
            trk_note_on(&trk[2], bass_on, q ? 96 : 118);
        }
        if (MEL[q]) {
            uint32_t n = (uint32_t)MEL[q] - (c == 1 ? 3u : c == 3 ? 2u : 0u) + (two ? 0u : 0u);
            if (lead_on) trk_note_off(&trk[3], lead_on);
            lead_on = (uint8_t)n;
            trk_note_on(&trk[3], lead_on, 100);
        }
        mix_to(w, q % 2 && sw ? step - sw : step);
    }
    for (k = 0; k < 3u; k++) trk_note_off(&trk[1], CH[3][k]);
    if (bass_on) trk_note_off(&trk[2], bass_on);
    if (lead_on) trk_note_off(&trk[3], lead_on);
    trk_note_on(&trk[0], 36, 120);
    trk_note_on(&trk[0], 69, 100);
    mix_to(w, FS * 3);
    fclose(w);
}

int main(int argc, char **argv)
{
    fm6_init();
    presets();
    controls();
    lists();
    lofi();
    dungeon_kit();
    if (!getenv("NOCOST"))
        cost();
    if (argc > 1) {
        demo_presets(argv[1]);
        demo_song(argv[1]);
        printf("sid: demos in %s\n", argv[1]);
    }
    printf("sid: %s\n", fails ? "FAILED" : "all checks ok");
    return fails != 0;
}
