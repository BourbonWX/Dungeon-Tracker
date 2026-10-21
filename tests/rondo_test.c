/* SPDX-License-Identifier: GPL-3.0-only
 * RONDO: the CYCLE engines (src/eng_cycle.c) and the fixed voice layout (core.h TRK_CAP / NBUDGET), on the host
 * through hostsim.c as the other tests.
 *   build/host/rondo_test [DEMODIR]          (tests/run_rondo.sh: build/rondo_demo)
 * 1. FMKIT, each voice alone: it sounds (peak above -24 dBFS of the part), never at full scale, no DC, and it ends
 *    (its voice frees itself) within 2 x its DECAY time + 0.1 s.
 * 2. the controls move the sound the way their names say: PITCH up -> pitch up (zero crossings), DECAY up -> longer,
 *    COLOR up -> brighter (the share of the first difference's energy), SWEEP up -> a higher start, SHAPE changes it,
 *    LEVEL 64 about -12 dB, the kit's TUNE moves every voice, VELO 0 = every hit full.
 * 3. the GM map: 35 36 KICK, 38 40 SNARE, 42 44 46 HAT (one voice: the closed hat chokes the open one), the rest
 *    PERC; one voice per drum: four drums at once = four voices, a fifth note on a drum's voice reuses it.
 * 4. FMTONE: every preset held 0.5 s and released: it sounds, no full scale, no DC, the voices free after the
 *    release; COLOR up -> brighter; SUB adds energy an octave down; STACK beats.
 * 5. the layout: T1 at most 4 voices, T2 4, T3 1 (MONO: the last note), T4 3; all four tracks full at once (12)
 *    and nothing taken from another track (no voice given up); the MIDI channels 1..4 play tracks 1..4.
 * 6. the KIT's labels and pages: track_desc names its voice values (PITCH .. SHAPE, the voices' levels), other
 *    engines keep theirs; the EDIT layer mutes silence one voice.
 * 7. cost: host ns per sample per voice of FMKIT and FMTONE against FM6 and ANALOG (the device runs FM6 and
 *    ANALOG on 8 voices: ours should cost less per voice).
 * 8. demos into DEMODIR: each kit voice, every kit's beat, every TONE preset, and an 8-bar song on the four tracks. */
#define main hostsim_main
#include "hostsim.c"
#undef main

static int fails;
static void check(const char *what, int ok)
{
    printf("rondo: %-92s %s\n", what, ok ? "ok" : "FAIL");
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
    memset(drum_mute, 0, sizeof drum_mute);
    for (p = 0; p < NPART; p++)
        memset(trk[p].v, 0, sizeof trk[p].v);
}
static void kit_on(uint32_t part, uint32_t kit)
{
    host_preset(&trk[part], ENGI_CKIT, kit);
    ck_kit_load(&trk[part], kit);
}

/* part `part` alone (track_render: its own output, no sends) for n samples into buf; returns the peak (Q15 units) */
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
static double energy(uint32_t a, uint32_t e)
{
    double s = 0;
    for (uint32_t i = a; i < e && i < buf_n; i++)
        s += (double)buf[i] * buf[i];
    return s;
}
static double dc_from(uint32_t a)                /* |mean| over the peak of buf[a..] (as drum_test: under 1 %) */
{
    double s = 0, pk = 0;
    for (uint32_t i = a; i < buf_n; i++)
        s += buf[i], pk = fabs(buf[i]) > pk ? fabs(buf[i]) : pk;
    return pk > 0 ? fabs(s / (buf_n - a)) / pk : 0;
}
static double dc_ratio(void) { return dc_from(0); }
/* brightness: the first difference's energy over the signal's, over [a, e) */
static double bright(uint32_t a, uint32_t e)
{
    double d = 0, s = 0;
    for (uint32_t i = a + 1; i < e && i < buf_n; i++) {
        double x = buf[i] - buf[i - 1];
        d += x * x;
        s += (double)buf[i] * buf[i];
    }
    return s > 0 ? d / s : 0;
}
/* zero crossings per second over [a, e) */
static double zc(uint32_t a, uint32_t e)
{
    uint32_t n = 0, i;
    for (i = a + 1; i < e && i < buf_n; i++)
        n += (buf[i - 1] < 0) != (buf[i] < 0);
    return n * (double)FS / (double)(e - a);
}
/* t-30: the time from the loudest 10 ms frame to the first one 30 dB below it, s */
static double t30(void)
{
    uint32_t f, nf = buf_n / 441u, best = 0;
    double mx = 0, l;
    for (f = 0; f < nf; f++)
        if ((l = energy(f * 441u, f * 441u + 441u)) > mx)
            mx = l, best = f;
    for (f = best; f < nf; f++)
        if (energy(f * 441u, f * 441u + 441u) < mx * 1e-3)
            return (f - best) * 0.01;
    return nf * 0.01;
}
static uint32_t voices_of(const track_t *t)
{
    uint32_t i, n = 0;
    for (i = 0; i < NVOICE; i++)
        n += t->v[i].active;
    return n;
}

/* ---------------------------------------------------------------- 1, 2 KIT --- */
static const uint8_t VNOTE[CK_N] = {36, 38, 42, 45};   /* the voices' notes: kick, snare, closed hat, tom */
static const char *const VNAME[CK_N] = {"KICK", "SNARE", "HAT", "PERC"};

static int32_t hit(uint32_t lane, uint32_t note, uint32_t n)
{
    int32_t pk;
    trk_note_on(&trk[0], note, 110);
    trk_note_off(&trk[0], note);                       /* (a released key does not end a hit) */
    pk = render_part(0, n);
    (void)lane;
    return pk;
}

static void kit_voices(void)
{
    uint32_t l;
    for (l = 0; l < CK_N; l++) {
        char what[96];
        int32_t pk;
        double dt, t;
        uint32_t ended;
        parts_clear();
        kit_on(0, 0);
        dt = 0.002 * pow(2.0, (trk[0].p[CK_DECAY(l)] * 120 / 127 + 12) / 12.0);   /* the DECAY time (60 dB) */
        pk = hit(l, VNOTE[l], (uint32_t)(FS * (2 * dt + 0.15)));
        ended = !voices_of(&trk[0]);
        t = t30();
        snprintf(what, sizeof what, "%s alone: peak %.1f dB, DC %.4f, t-30 %.3f s (DECAY %.3f s), ends", VNAME[l],
                 20 * log10(pk / 32768.0), dc_ratio(), t, dt);
        check(what, pk > 2000 && pk < 32767 && dc_ratio() < 0.01 && ended && t > 0.002 && t < dt * 1.5 + 0.05);
    }
}

/* render one hit of voice l with parameter id at value v: fills buf */
static void hit_with(uint32_t l, uint32_t id, int16_t v, uint32_t note, uint32_t n)
{
    parts_clear();
    kit_on(0, 0);
    if (l == CK_SNARE && id != CK_SHAPE(l) && id != CK_COLOR(l))
        trk[0].p[CK_SHAPE(l)] = 0;                     /* (the body alone: its pitch) */
    trk[0].p[id] = v;
    trk[0].p[P_E7] = 0;                                /* VARI off: the same hit each time */
    if (id != P_E4)
        trk[0].p[P_E4] = 0;                            /* CLICK off: the voice's own sound */
    hit(l, note, n);
}

static void kit_controls(void)
{
    uint32_t l, ok;
    char what[120];
    for (l = 0; l < CK_N; l++) {
        double lo, hi, a, b;
        uint32_t n = FS / 2;
        /* PITCH: zero crossings of 10..60 ms (after most of the sweep) */
        hit_with(l, CK_PITCH(l), 40, VNOTE[l], n);
        lo = l == CK_HAT ? bright(0, FS / 50) : zc(FS / 50, FS / 10);
        hit_with(l, CK_PITCH(l), 90, VNOTE[l], n);
        hi = l == CK_HAT ? bright(0, FS / 50) : zc(FS / 50, FS / 10);
        snprintf(what, sizeof what, l == CK_HAT ? "%s PITCH 40 -> 90: brightness %.3f -> %.3f" : "%s PITCH 40 -> 90: zero crossings %.0f -> %.0f /s", VNAME[l], lo, hi);
        check(what, hi > lo * 1.2);
        /* DECAY */
        hit_with(l, CK_DECAY(l), 30, VNOTE[l], FS * 2);
        a = t30();
        hit_with(l, CK_DECAY(l), 90, VNOTE[l], FS * 4);
        b = t30();
        snprintf(what, sizeof what, "%s DECAY 30 -> 90: t-30 %.3f -> %.3f s", VNAME[l], a, b);
        check(what, b > a * 2);
        /* COLOR: brighter over the first 60 ms */
        hit_with(l, CK_COLOR(l), 10, VNOTE[l], n);
        a = bright(0, FS * 3 / 100);
        hit_with(l, CK_COLOR(l), 120, VNOTE[l], n);
        b = bright(0, FS * 3 / 100);
        snprintf(what, sizeof what, "%s COLOR 10 -> 120: brightness %.4f -> %.4f", VNAME[l], a, b);
        check(what, b > a * 1.15);
        /* SWEEP (not the hat's): the first 5 ms higher */
        if (l != CK_HAT) {
            hit_with(l, CK_SWEEP(l), 0, VNOTE[l], n);
            a = zc(0, FS / 40);
            hit_with(l, CK_SWEEP(l), 120, VNOTE[l], n);
            b = zc(0, FS / 40);
            snprintf(what, sizeof what, "%s SWEEP 0 -> 120: the first 10 ms' zero crossings %.0f -> %.0f /s", VNAME[l], a, b);
            check(what, b > a * 1.3);
        }
        /* SHAPE changes the sound */
        hit_with(l, CK_SHAPE(l), 0, VNOTE[l], n);
        a = bright(0, FS / 10);
        lo = energy(0, FS / 10);
        hit_with(l, CK_SHAPE(l), 127, VNOTE[l], n);
        b = bright(0, FS / 10);
        hi = energy(0, FS / 10);
        snprintf(what, sizeof what, "%s SHAPE 0 -> 127 changes it (brightness %.4f -> %.4f, energy %.1f -> %.1f)",
                 VNAME[l], a, b, lo, hi);
        check(what, fabs(b / a - 1) > 0.1 || fabs(hi / lo - 1) > 0.1);
        /* LEVEL 64: about -12 dB */
        hit_with(l, P_LN0 + l, 127, VNOTE[l], n);
        a = energy(0, n);
        hit_with(l, P_LN0 + l, 64, VNOTE[l], n);
        b = energy(0, n);
        snprintf(what, sizeof what, "%s LEVEL 127 -> 64: %.1f dB", VNAME[l], 10 * log10(b / a));
        check(what, 10 * log10(b / a) < -10 && 10 * log10(b / a) > -14);
    }
    /* TUNE moves all; VELO */
    {
        double a, b;
        hit_with(CK_PERC, P_E0, -24, 45, FS / 4);
        a = zc(FS / 50, FS / 10);
        hit_with(CK_PERC, P_E0, 36, 45, FS / 4);
        b = zc(FS / 50, FS / 10);
        snprintf(what, sizeof what, "kit TUNE -24 -> +36 moves PERC: %.0f -> %.0f /s", a, b);
        check(what, b > a * 1.3);
        parts_clear();
        kit_on(0, 0);
        trk[0].p[P_E5] = 0;
        trk[0].p[P_E7] = 0;
        trk[0].p[P_E4] = 0;
        trk_note_on(&trk[0], 36, 30);
        a = energy(0, render_part(0, FS / 4) ? FS / 4 : 0);
        parts_clear();
        kit_on(0, 0);
        trk[0].p[P_E5] = 0;
        trk[0].p[P_E7] = 0;
        trk[0].p[P_E4] = 0;
        trk_note_on(&trk[0], 36, 127);
        render_part(0, FS / 4);
        b = energy(0, FS / 4);
        snprintf(what, sizeof what, "VELO 0: a kick at velocity 30 as loud as 127 (%.2f dB)", 10 * log10(a / b));
        check(what, fabs(10 * log10(a / b)) < 0.5);
        parts_clear();
        kit_on(0, 0);
        trk[0].p[P_E5] = 127;
        trk[0].p[P_E7] = 0;
        trk[0].p[P_E4] = 0;
        trk_note_on(&trk[0], 36, 32);
        render_part(0, FS / 4);
        a = energy(0, FS / 4);
        snprintf(what, sizeof what, "VELO 127: velocity 32 about -12 dB under 127 (%.1f dB)", 10 * log10(a / b));
        check(what, 10 * log10(a / b) < -9 && 10 * log10(a / b) > -15);
    }
    /* the corners: no full scale, ends */
    {
        static const int16_t C[2] = {0, 127};
        uint32_t over = 0, hung = 0, k, kit;
        for (kit = 0; kit < NELEM(CK_PRESETS); kit++)
            for (l = 0; l < CK_N; l++)
                for (k = 0; k < 32u; k++) {
                    parts_clear();
                    kit_on(0, kit);
                    trk[0].p[CK_PITCH(l)] = C[k & 1];
                    trk[0].p[CK_DECAY(l)] = (k >> 1 & 1) ? 127 : 0;
                    trk[0].p[CK_COLOR(l)] = C[k >> 2 & 1];
                    trk[0].p[CK_SWEEP(l)] = C[k >> 3 & 1];
                    trk[0].p[CK_SHAPE(l)] = C[k >> 4 & 1];
                    trk[0].p[P_E6] = 127;
                    trk[0].p[P_E4] = 127;
                    trk_note_on(&trk[0], VNOTE[l], 127);
                    over += render_part(0, FS * 6) >= 32767;
                    over += render_part(0, FS * 4) >= 32767;
                    hung += voices_of(&trk[0]) != 0;
                }
        snprintf(what, sizeof what, "every kit, every voice at the corners (DRV, CLICK full): no full scale (%u), all end (%u hung)",
                 over, hung);
        check(what, !over && !hung);
    }
}

/* ------------------------------------------------------------------ 3 the map --- */
static void kit_map(void)
{
    static const uint8_t KICK[] = {35, 36}, SNARE[] = {38, 40}, HAT[] = {42, 44, 46, 49, 51}, PERC[] = {37, 39, 41, 45, 56, 60, 75};
    uint32_t i, bad = 0;
    char what[96];
    for (i = 0; i < NELEM(KICK); i++) bad += ck_lane_of(KICK[i]) != CK_KICK;
    for (i = 0; i < NELEM(SNARE); i++) bad += ck_lane_of(SNARE[i]) != CK_SNARE;
    for (i = 0; i < NELEM(HAT); i++) bad += ck_lane_of(HAT[i]) != CK_HAT;
    for (i = 0; i < NELEM(PERC); i++) bad += ck_lane_of(PERC[i]) != CK_PERC;
    for (i = 0; i < 128u; i++) bad += ck_lane_of(i) > CK_PERC;
    check("GM map: kicks, snares, hats / cymbals, the rest PERC; every note a voice", !bad);
    for (i = 0; i < NLANE; i++) bad += ck_lane_of(DRUM_LANE_NOTE[i]) != (uint32_t)(i == 0 ? CK_KICK : i == 1 ? CK_SNARE :
                                                                               i == 3 || i == 4 ? CK_HAT : CK_PERC);
    check("the grid's 8 rows (BD SD CP CH OH TM RS CB) play KICK SNARE PERC HAT HAT PERC PERC PERC", !bad);
    parts_clear();
    kit_on(0, 0);
    trk_note_on(&trk[0], 36, 110);
    trk_note_on(&trk[0], 38, 110);
    trk_note_on(&trk[0], 46, 110);
    trk_note_on(&trk[0], 45, 110);
    render_part(0, CTL * 4);
    i = voices_of(&trk[0]);
    trk_note_on(&trk[0], 42, 110);                     /* the closed hat on the open one's voice */
    trk_note_on(&trk[0], 37, 110);                     /* a rim on PERC's */
    trk_note_on(&trk[0], 36, 110);
    render_part(0, CTL * 4);
    snprintf(what, sizeof what, "four drums at once: %u voices; three more hits on them: %u voices", i, voices_of(&trk[0]));
    check(what, i == 4u && voices_of(&trk[0]) == 4u);
    /* choke: an open hat, a closed one 100 ms in: the open one's long tail is gone */
    {
        double open_tail, choked_tail;
        parts_clear();
        kit_on(0, 0);
        trk[0].p[P_E7] = 0;
        trk_note_on(&trk[0], 46, 110);
        render_part(0, FS / 2);
        open_tail = energy(FS * 15 / 100, FS * 3 / 10);
        parts_clear();
        kit_on(0, 0);
        trk[0].p[P_E7] = 0;
        trk_note_on(&trk[0], 46, 110);
        render_part(0, FS / 10);
        trk_note_on(&trk[0], 42, 110);
        render_part(0, FS * 4 / 10);
        choked_tail = energy(FS * 5 / 100, FS * 2 / 10);
        snprintf(what, sizeof what, "the closed hat chokes the open one: its tail %.1f dB under", 10 * log10(choked_tail / open_tail));
        check(what, choked_tail < open_tail * 1e-3);
    }
}

/* ------------------------------------------------------------------ 4 TONE --- */
static void tone(void)
{
    uint32_t pi, bad = 0, over = 0, hung = 0;
    char what[120];
    for (pi = 0; pi < NELEM(CT_PRESETS); pi++) {
        int32_t pk;
        static const uint8_t CH[3] = {48, 55, 64};
        uint32_t k;
        parts_clear();
        host_preset(&trk[1], ENGI_CTONE, pi);
        trk_note_on(&trk[1], CH[0], 127);
        over += render_part(1, FS / 2) >= 32767;      /* one voice: never full scale */
        trk_note_off(&trk[1], CH[0]);
        render_part(1, FS * 5);
        parts_clear();
        host_preset(&trk[1], ENGI_CTONE, pi);
        for (k = 0; k < 3u; k++)
            trk_note_on(&trk[1], CH[k], 100);
        pk = render_part(1, FS / 2);
        bad += pk < 1500 || dc_from(FS / 4) > 0.01;      /* (the held part: no attack in the mean) */
        if (pk < 1500 || dc_from(FS / 4) > 0.01)
            printf("rondo:   %s: peak %d DC %.4f\n", CT_PRESETS[pi].name, pk, dc_from(FS / 4));
        for (k = 0; k < 3u; k++)
            trk_note_off(&trk[1], CH[k]);
        render_part(1, FS * 5);
        hung += voices_of(&trk[1]) != 0;
    }
    snprintf(what, sizeof what, "FMTONE: every preset (3 notes) sounds, no DC, no full scale (%u), all free after the release (%u)", over, hung);
    check(what, !bad && !over && !hung);
    {
        double a, b;
        parts_clear();
        host_preset(&trk[1], ENGI_CTONE, 0);
        trk[1].p[P_E1] = 10;
        trk[1].p[P_E3] = 127;
        trk_note_on(&trk[1], 60, 100);
        render_part(1, FS / 4);
        a = bright(FS / 10, FS / 4);
        parts_clear();
        host_preset(&trk[1], ENGI_CTONE, 0);
        trk[1].p[P_E1] = 120;
        trk[1].p[P_E3] = 127;
        trk_note_on(&trk[1], 60, 100);
        render_part(1, FS / 4);
        b = bright(FS / 10, FS / 4);
        snprintf(what, sizeof what, "FMTONE COLOR 10 -> 120: brightness %.4f -> %.4f", a, b);
        check(what, b > a * 1.5);
        /* CNTR: the index falls toward MSUS: brighter at the start than later with MSUS 0 */
        parts_clear();
        host_preset(&trk[1], ENGI_CTONE, 0);
        trk[1].p[P_E1] = 110;
        trk[1].p[P_E2] = 70;
        trk[1].p[P_E3] = 0;
        trk[1].p[P_ATK] = 0;
        trk_note_on(&trk[1], 60, 100);
        render_part(1, FS);
        a = bright(0, FS / 20);
        b = bright(FS * 6 / 10, FS * 7 / 10);
        snprintf(what, sizeof what, "FMTONE CNTR 70, MSUS 0: brightness falls %.4f -> %.4f", a, b);
        check(what, a > b * 1.5);
    }
}

/* ------------------------------------------------------------------ 5 layout --- */
static void layout(void)
{
    static const uint8_t N[8] = {48, 52, 55, 59, 62, 65, 69, 72};
    uint32_t p, i, kills;
    char what[120];
#if RONDO_LAYOUT
    check("TRK_CAP 4 4 1 3, budget 12", TRK_CAP[0] == 4 && TRK_CAP[1] == 4 && TRK_CAP[2] == 1 && TRK_CAP[3] == 3 && NBUDGET == 12);
    parts_clear();
    kit_on(0, 0);
    host_preset(&trk[1], ENGI_CTONE, 0);
    host_preset(&trk[2], ENGI_CTONE, 5);
    host_preset(&trk[3], ENGI_CTONE, 8);
    for (p = 1; p < NPART; p++)
        trk[p].p[P_SUS] = 127;
    kills = voice_kills;
    for (i = 0; i < 8u; i++) {
        trk_note_on(&trk[0], (uint8_t[]){36, 38, 42, 45, 37, 46, 39, 56}[i], 110);
        for (p = 1; p < NPART; p++)
            trk_note_on(&trk[p], N[i] - (p == 2 ? 24 : 0), 100);
        render_part(0, CTL);
        for (p = 1; p < NPART; p++)
            render_part(p, CTL);
    }
    snprintf(what, sizeof what, "8 notes on every track: voices %u %u %u %u (caps 4 4 1 3), %u given up", voices_of(&trk[0]),
             voices_of(&trk[1]), voices_of(&trk[2]), voices_of(&trk[3]), voice_kills - kills);
    check(what, voices_of(&trk[0]) == 4 && voices_of(&trk[1]) == 4 && voices_of(&trk[2]) == 1 && voices_of(&trk[3]) == 3);
    check("no track took a voice from another (the budget is the caps' sum)", voices_busy() == 12u);
    {
        uint32_t held = 1;
        for (i = 0; i < NVOICE; i++)
            if (trk[2].v[i].active)
                held = trk[2].v[i].note == N[7] - 24;
        snprintf(what, sizeof what, "the bass (T3, MONO) plays the last note held");
        check(what, held && trk[2].p[P_VOICE] != V_POLY);
    }
    /* the lead: 3 notes, the newest among them and the lowest kept (voice.c: the bass of a chord is never taken) */
    {
        uint32_t newest = 0, low = 0;
        for (i = 0; i < NVOICE; i++) {
            newest += trk[3].v[i].active && trk[3].v[i].note == N[7];
            low += trk[3].v[i].active && trk[3].v[i].note == N[0];
        }
        snprintf(what, sizeof what, "T4 (3 voices) holds the newest note and the lowest (%u, %u)", newest, low);
        check(what, newest == 1u && low == 1u);
    }
#endif
    /* the power-on sounds */
#if RONDO_LAYOUT
    check("power-on: T1 FMKIT, T2 FMTONE FM PAD, T3 FMTONE FM BASS, T4 FMTONE FM LEAD",
          TRK_DEF[0][0] == ENGI_CKIT && TRK_DEF[1][0] == ENGI_CTONE && TRK_DEF[2][0] == ENGI_CTONE && TRK_DEF[3][0] == ENGI_CTONE &&
          !strcmp(CT_PRESETS[TRK_DEF[1][1]].name, "FM PAD") && !strcmp(CT_PRESETS[TRK_DEF[2][1]].name, "FM BASS") &&
          CT_PRESETS[TRK_DEF[2][1]].mono && !strcmp(CT_PRESETS[TRK_DEF[3][1]].name, "FM LEAD"));
#endif
}

/* ------------------------------------------------------------------ 6 labels, mutes --- */
static void labels(void)
{
    char what[96];
    uint32_t l, bad = 0;
    parts_clear();
    kit_on(0, 0);
    host_preset(&trk[1], ENGI_CTONE, 0);
    for (l = 0; l < CK_N; l++) {
        bad += strcmp(track_desc(&trk[0], CK_PITCH(l))->label, "PITCH") != 0;
        bad += strcmp(track_desc(&trk[0], CK_DECAY(l))->label, "DECAY") != 0;
        bad += strcmp(track_desc(&trk[0], CK_COLOR(l))->label, "COLOR") != 0;
        bad += strcmp(track_desc(&trk[0], CK_SWEEP(l))->label, "SWEEP") != 0;
        bad += strcmp(track_desc(&trk[0], P_LN0 + l)->label, VNAME[l]) != 0;
    }
    bad += strcmp(track_desc(&trk[1], P_FM1_ATK)->label, "ATK") != 0 || strcmp(track_desc(&trk[1], P_LN2)->label, "CLAP") != 0;
    check("FMKIT's labels (PITCH DECAY COLOR SWEEP, KICK SNARE HAT PERC); other engines keep theirs", !bad);
    for (l = 0; l < CK_N; l++) {
        double on, off;
        parts_clear();
        kit_on(0, 0);
        trk_note_on(&trk[0], VNOTE[l], 110);
        render_part(0, FS / 4);
        on = energy(0, FS / 4);
        parts_clear();
        kit_on(0, 0);
        drum_mute[0] = (uint8_t)(1u << l);
        trk_note_on(&trk[0], VNOTE[l], 110);
        render_part(0, FS / 4);
        off = energy(0, FS / 4);
        bad += off != 0 || on == 0;
    }
    drum_mute[0] = 0;
    snprintf(what, sizeof what, "the EDIT layer's mutes silence each voice");
    check(what, !bad);
}

/* ------------------------------------------------------------------ 7 cost --- */
static double ns_per_voice_sample(uint32_t e, uint32_t pi, uint32_t nv)
{
    static const uint8_t KIT[4] = {36, 38, 46, 45}, CH[8] = {48, 52, 55, 59, 62, 65, 69, 72};
    int32_t o[CTL];
    uint32_t i, k, rep = 0;
    uint64_t t0, t = 0;
    parts_clear();
    if (e == ENGI_CKIT) kit_on(0, pi); else host_preset(&trk[0], e, pi);
    trk[0].p[P_SUS] = 127;
    trk[0].p[P_VOICE] = V_POLY;
    for (rep = 0; rep < 40; rep++) {
        for (k = 0; k < nv; k++) {
            if (e == ENGI_CKIT) {
                trk[0].p[CK_DECAY(k)] = 127;
                trk_note_on(&trk[0], KIT[k], 110);
            } else {
                trk_note_on(&trk[0], CH[k], 100);
            }
        }
        t0 = now_ns();
        for (i = 0; i < 64u; i++)
            track_render(&trk[0], o, CTL);
        t += now_ns() - t0;
    }
    return (double)t / (40.0 * 64 * CTL * nv);
}
static void cost(void)
{
    double kit = ns_per_voice_sample(ENGI_CKIT, 0, 4), tone = ns_per_voice_sample(ENGI_CTONE, 0, 4);
    double tone2 = ns_per_voice_sample(ENGI_CTONE, 5, 4), fm6 = ns_per_voice_sample(ENGI_FM6, 0, 4), an = ns_per_voice_sample(0, 0, 4);
    char what[160];
    snprintf(what, sizeof what, "cost per voice-sample: FMKIT %.1f ns, FMTONE %.1f (STACK) / %.1f (SUB), FM6 %.1f, ANALOG %.1f",
             kit, tone, tone2, fm6, an);
    check(what, kit < fm6 && tone < fm6 && tone2 < fm6);
}

/* ------------------------------------------------------------------ 8 demos --- */
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
static void mix_to(FILE *w, uint32_t frames)
{
    int32_t o[2 * CTL];
    uint32_t i, k;
    for (i = 0; i < frames; i += CTL) {
        mix_block(o, CTL);
        for (k = 0; k < CTL; k++)
            wav_put(w, o[2 * k], o[2 * k + 1]);
    }
}
static void demo_kit(const char *dir)
{
    uint32_t kit, s;
    for (kit = 0; kit < NELEM(CK_PRESETS); kit++) {
        char name[64];
        FILE *w;
        uint32_t step = FS * 60 / 120 / 4, bars = 2;
        snprintf(name, sizeof name, "kit_%u_%s.wav", kit + 1, CK_PRESETS[kit].name);
        for (char *c = name; *c; c++) if (*c == ' ') *c = '_';
        w = wav_open(dir, name, step * 16 * bars + FS);
        if (!w) return;
        parts_clear();
        kit_on(0, kit);
        for (s = 0; s < 16 * bars; s++) {
            uint32_t q = s % 16;
            if (q % 4 == 0 || q == 10) trk_note_on(&trk[0], 36, q % 8 == 0 ? 120 : 100);
            if (q == 4 || q == 12) trk_note_on(&trk[0], 38, 115);
            if (q == 15 && s >= 16) trk_note_on(&trk[0], 39, 90);
            if (q % 2 == 0) trk_note_on(&trk[0], q == 14 ? 46 : 42, q % 4 == 2 ? 100 : 80);
            if (q == 3 || q == 11) trk_note_on(&trk[0], 45, 90);
            if (q == 7) trk_note_on(&trk[0], 56, 80);
            mix_to(w, step);
        }
        mix_to(w, FS);
        fclose(w);
    }
}
static void demo_tone(const char *dir)
{
    uint32_t pi;
    for (pi = 0; pi < NELEM(CT_PRESETS); pi++) {
        static const uint8_t CHORD[4] = {48, 55, 60, 64}, LINE[8] = {36, 36, 43, 36, 39, 36, 41, 43};
        const preset_t *pr = &CT_PRESETS[pi];
        char name[64];
        FILE *w;
        uint32_t k, part = pr->mono ? 2 : 1;
        snprintf(name, sizeof name, "tone_%02u_%s.wav", pi + 1, pr->name);
        for (char *c = name; *c; c++) if (*c == ' ') *c = '_';
        w = wav_open(dir, name, FS * 3);
        if (!w) return;
        parts_clear();
        host_preset(&trk[part], ENGI_CTONE, pi);
        if (pr->mono) {
            for (k = 0; k < 8u; k++) {
                trk_note_on(&trk[part], LINE[k], 100);
                mix_to(w, FS / 4 - FS / 32);
                trk_note_off(&trk[part], LINE[k]);
                mix_to(w, FS / 32);
            }
            mix_to(w, FS);
        } else {
            for (k = 0; k < 4u; k++)
                trk_note_on(&trk[part], CHORD[k], 100);
            mix_to(w, FS * 3 / 2);
            for (k = 0; k < 4u; k++)
                trk_note_off(&trk[part], CHORD[k]);
            mix_to(w, FS * 3 / 2);
        }
        fclose(w);
    }
}
/* 8 bars at 112 BPM: T1 the kit, T2 four-note chords (Am7 Fmaj7 C G6), T3 the bass, T4 a three-note lead */
static void demo_song(const char *dir)
{
    static const uint8_t CH[4][4] = {{57, 60, 64, 67}, {53, 57, 60, 64}, {55, 60, 64, 67}, {55, 59, 62, 64}};
    static const uint8_t ROOT[4] = {33, 29, 36, 31};
    static const uint8_t LEAD[16] = {76, 0, 79, 0, 81, 0, 79, 76, 72, 0, 74, 0, 76, 0, 0, 0};
    uint32_t step = FS * 60 / 112 / 4, s, k, bars = 8;
    uint8_t lead_on = 0, bass_on = 0;
    FILE *w = wav_open(dir, "song_4_tracks.wav", step * 16 * bars + FS * 2);
    if (!w) return;
    parts_clear();
    kit_on(0, 0);
    host_preset(&trk[1], ENGI_CTONE, 1);               /* CHORD EP */
    host_preset(&trk[2], ENGI_CTONE, 5);               /* FM BASS */
    host_preset(&trk[3], ENGI_CTONE, 8);               /* FM LEAD */
    trk[3].p[P_DLY] = 50;
    for (s = 0; s < 16 * bars; s++) {
        uint32_t q = s % 16, bar = s / 16, c = bar % 4;
        if (q % 4 == 0 || (q == 10 && bar % 2)) trk_note_on(&trk[0], 36, 120);
        if (q == 4 || q == 12) trk_note_on(&trk[0], 38, 112);
        if (q % 2 == 0) trk_note_on(&trk[0], q == 6 || q == 14 ? 46 : 42, q % 4 == 2 ? 96 : 76);
        if (bar >= 4 && (q == 3 || q == 13)) trk_note_on(&trk[0], 45, 84);
        if (q == 0) {                                  /* the chord, held two beats, again on beat 3 */
            for (k = 0; k < 4u; k++) trk_note_on(&trk[1], CH[c][k], 90);
        }
        if (q == 6 || q == 14)
            for (k = 0; k < 4u; k++) trk_note_off(&trk[1], CH[c][k]);
        if (q == 8)
            for (k = 0; k < 4u; k++) trk_note_on(&trk[1], CH[c][k], 80);
        if (q % 2 == 0 || q == 7) {                    /* the bass: eighths, an octave jump on the off-beat */
            if (bass_on) trk_note_off(&trk[2], bass_on);
            bass_on = (uint8_t)(ROOT[c] + (q % 4 == 2 ? 12 : 0) + (q == 7 ? 7 : 0));
            trk_note_on(&trk[2], bass_on, q % 4 ? 90 : 110);
        }
        if (bar >= 2 && LEAD[q]) {
            if (lead_on) trk_note_off(&trk[3], lead_on);
            lead_on = (uint8_t)(LEAD[q] - (c == 1 ? 2 : 0));
            trk_note_on(&trk[3], lead_on, 100);
        } else if (q == 14 && lead_on) {
            trk_note_off(&trk[3], lead_on);
            lead_on = 0;
        }
        mix_to(w, step);
    }
    for (k = 0; k < 4u; k++) trk_note_off(&trk[1], CH[3][k]);
    if (bass_on) trk_note_off(&trk[2], bass_on);
    if (lead_on) trk_note_off(&trk[3], lead_on);
    mix_to(w, FS * 2);
    fclose(w);
}

int main(int argc, char **argv)
{
    fm6_init();
    kit_voices();
    kit_controls();
    kit_map();
    tone();
    layout();
    labels();
    if (!getenv("NOCOST"))
        cost();
    if (argc > 1) {
        demo_kit(argv[1]);
        demo_tone(argv[1]);
        demo_song(argv[1]);
        printf("rondo: demos in %s\n", argv[1]);
    }
    printf("rondo: %s\n", fails ? "FAILED" : "all checks ok");
    return fails != 0;
}
