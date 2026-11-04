/* SPDX-License-Identifier: GPL-3.0-only
 * 1.5: GLIDE (voice.c glide_set, GLIDE_MS in params.c). Its time is what it shows: RATE (GLMODE 0) that long an
 * octave, TIME (GLMODE 1) that long for any jump; the whole range 5 ms .. 4 s (before 1.5 a glide could not last past
 * ~140 ms an octave); slow glides move smoothly (no 1/16-semitone stairs); 0 shows OFF; a SLIDE step with GLIDE OFF
 * slides in GLIDE_SLIDE_MS; on a kit there is no VOICE page; every melodic engine glides */
#define UI_TEST_NO_MAIN 1
#include "ui_test.c"

static int fails;
static void ok(const char *what, int c) { fails += check(what, c); }

/* legato from -> to on T4 (engine e): the ms until 90 % of the way, the largest Q8 move in one block */
static double glide_ms(uint32_t e, int16_t g, int16_t mode, uint32_t from, uint32_t to, int32_t *maxstep)
{
    int32_t o[2 * CTL], last, total, at90;
    uint32_t i, blocks = 0, k;
    voice_t *v = 0;
    ui_power_on();
    track_select(3);
    set_engine_of(TSEL, e); TSEL->engine = TSEL->eng_req; apply_preset_to(TSEL, 0);
    TSEL->p[P_VOICE] = V_LEGATO; TSEL->p[P_GLIDE] = g; TSEL->p[P_GLMODE] = mode;
    for (i = 0; i < 400u; i++) mix_block(o, CTL);       /* (the engine switch's fade first) */
    trk_note_on(TSEL, from, 100);
    for (i = 0; i < 40u; i++) mix_block(o, CTL);
    trk_note_on(TSEL, to, 100);
    for (k = 0; k < NVOICE; k++) if (TSEL->v[k].active && TSEL->v[k].note == to) v = &TSEL->v[k];
    if (!v) return -1;
    total = (int32_t)to * 4096 - (int32_t)from * 4096;
    at90 = (int32_t)from * 4096 + total * 9 / 10;
    last = v->pitch_cur * 256 + *glide_fr(TSEL, v);
    if (maxstep) *maxstep = 0;
    while (blocks < 20000u) {
        int32_t now;
        mix_block(o, CTL);
        blocks++;
        now = v->pitch_cur * 256 + *glide_fr(TSEL, v);
        if (maxstep && abs(now - last) > *maxstep) *maxstep = abs(now - last);
        last = now;
        if (total > 0 ? now >= at90 : now <= at90) break;
    }
    trk_note_off(TSEL, to); trk_note_off(TSEL, from);
    return blocks * (double)CTL * 1000.0 / FS;
}

int main(void)
{
    static const uint32_t MEL[6] = {18, 16, 20, 21, 0, 17};   /* GENESIS WAVES WAVE 32 MOD ANALOG SID */
    char v[16], what[160];
    const char *u;
    uint32_t j, f;
    int32_t ms;
    double t;

    param_format(&TP[P_GLIDE], 0, v, &u);
    ok("GLIDE 0 shows OFF", str_eq(v, "OFF"));
    param_format(&TP[P_GLIDE], 1, v, &u);
    {
        char w[16]; const char *uw;
        param_format(&TP[P_GLIDE], 127, w, &uw);
        snprintf(what, sizeof what, "GLIDE 1 shows %s %s, 127 %s %s (its own times)", v, u, w, uw);
        ok(what, str_eq(v, "5") && str_eq(u, "ms") && str_eq(w, "4.00") && str_eq(uw, "s"));
    }
    for (j = 1, f = 1; j < 128u; j++) f &= GLIDE_MS[j] >= GLIDE_MS[j - 1u];
    ok("  GLIDE_MS rises with the knob", f);

    {   /* RATE: an octave in its time, at the bottom, the middle and the top */
        static const int16_t G[4] = {20, 60, 100, 127};
        int bad = 0;
        for (j = 0; j < 4u; j++) {
            double want = GLIDE_MS[G[j]] * 0.9;           /* (to 90 % of the way) */
            t = glide_ms(0, G[j], 0, 48, 60, 0);
            bad += t < want * 0.85 || t > want * 1.15;
            printf("glide:   RATE GLIDE %3d (%u ms): C3-C4 to 90 %% in %.1f ms\n", G[j], GLIDE_MS[G[j]], t);
        }
        ok("RATE: an octave takes GLIDE's time (+-15 %), 4 s at the top (1.4: ~140 ms at most)", !bad);
        t = glide_ms(0, 100, 0, 48, 55, 0);
        snprintf(what, sizeof what, "  RATE: a fifth takes 7/12 of it (%.0f ms, an octave %u)", t, GLIDE_MS[100]);
        ok(what, t > GLIDE_MS[100] * 0.9 * 7 / 12 * 0.85 && t < GLIDE_MS[100] * 0.9 * 7 / 12 * 1.15);
    }
    {   /* TIME: any jump the same time */
        double a = glide_ms(0, 90, 1, 48, 50, 0), b = glide_ms(0, 90, 1, 48, 72, 0);
        snprintf(what, sizeof what, "TIME: a tone and two octaves both take GLIDE's time (%.0f, %.0f ms of %u)", a, b, GLIDE_MS[90]);
        ok(what, a > GLIDE_MS[90] * 0.9 * 0.85 && a < GLIDE_MS[90] * 0.9 * 1.15 && b > GLIDE_MS[90] * 0.9 * 0.85 &&
                 b < GLIDE_MS[90] * 0.9 * 1.15);
    }
    {   /* smooth: a 4 s octave moves under 1/16 semitone (4096/16 = 256 Q8... per block: well under a unit) */
        int32_t mx = 0;
        glide_ms(0, 127, 0, 48, 60, &mx);
        snprintf(what, sizeof what, "  slow glides are smooth: %d / 256 of a 1/16 semitone a block at 4 s an octave", mx);
        ok(what, mx > 0 && mx < 64);
    }
    for (j = 0; j < 6u; j++) {                          /* every melodic engine */
        t = glide_ms(MEL[j], 80, 0, 48, 60, 0);
        snprintf(what, sizeof what, "  %s glides (GLIDE 80: %.0f ms)", ENGINES[MEL[j]]->name, t);
        ok(what, t > GLIDE_MS[80] * 0.9 * 0.85 && t < GLIDE_MS[80] * 0.9 * 1.15);
    }
    {   /* a SLIDE step with GLIDE OFF: GLIDE_SLIDE_MS */
        voice_t *vv;
        int32_t o[2 * CTL];
        uint32_t i, blocks = 0;
        ui_power_on();
        track_select(3);
        TSEL->p[P_VOICE] = V_LEGATO; TSEL->p[P_GLIDE] = 0;
        for (i = 0; i < 400u; i++) mix_block(o, CTL);
        trk_note_on(TSEL, 48, 100);
        for (i = 0; i < 40u; i++) mix_block(o, CTL);
        TSEL->slide_glide = 1;
        trk_note_on(TSEL, 55, 100);
        vv = &TSEL->v[0];
        while (blocks < 2000u && vv->pitch_cur * 256 + *glide_fr(TSEL, vv) < 48 * 4096 + 7 * 4096 * 9 / 10) { mix_block(o, CTL); blocks++; }
        t = blocks * (double)CTL * 1000.0 / FS;
        snprintf(what, sizeof what, "a SLIDE with GLIDE OFF: %.0f ms to 90 %% (%u ms)", t, GLIDE_SLIDE_MS);
        ok(what, t > GLIDE_SLIDE_MS * 0.9 * 0.8 && t < GLIDE_SLIDE_MS * 0.9 * 1.2);
        TSEL->slide_glide = 0;
    }
    {   /* POLY / GLIDE OFF: the note at once */
        double z = glide_ms(0, 0, 0, 48, 60, 0);
        ok("GLIDE OFF: the new note at once", z < 1.0);
    }
    {   /* kits: no VOICE page */
        uint32_t e, k, vis[2] = {0, 0};
        for (k = 0; k < 2u; k++) {
            ui_power_on();
            track_select(0);
            e = k ? ENGI_MKIT : ENGI_CKIT;
            set_engine_of(TSEL, e); TSEL->engine = TSEL->eng_req;
            for (f = 0; f < NPAGES; f++) if (PAGES[f].id[0] == P_VOICE && page_visible(f)) vis[k] = 1;
        }
        ui_power_on();
        track_select(3);
        for (f = 0, j = 0; f < NPAGES; f++) if (PAGES[f].id[0] == P_VOICE && page_visible(f)) j = 1;
        ok("GEN KIT and MOD KIT: no VOICE page (their hits ignore it); a melodic track keeps it", !vis[0] && !vis[1] && j);
    }
    {   /* the knob: one step a detent, OFF -> 5 ms -> 5 ms -> 6 ms ... (VOICE, KNOB 2) */
        int16_t last;
        uint32_t i, stepped = 1;
        ui_power_on();
        track_select(3);
        go_title("VOICE"); frames(10);
        last = TSEL->p[P_GLIDE];
        for (i = 0; i < 20u; i++) { turn(EN_K2, 1); frames(3); stepped &= TSEL->p[P_GLIDE] == last + 1; last = TSEL->p[P_GLIDE]; }
        ok("VOICE: KNOB 2 turns GLIDE one step a detent", stepped && str_eq(cur_page()->title, "VOICE"));
    }
    (void)ms;
    printf("glide: %s\n", fails ? "FAILED" : "all checks ok");
    return fails != 0;
}
