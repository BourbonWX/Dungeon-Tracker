/* SPDX-License-Identifier: GPL-3.0-only
 * RONDO: EDIT > DICE (src/ui_dice.c) through ui_test.c's harness: the real UI and sound sources.
 *   build/host/dice_test             (tests/run_rondo.sh)
 * 1. the page: on FMTONE and FMKIT tracks, not on others; KNOB 1 DIE, 2 HOLD, 3 WILD, 4 HIST; OCT+ rolls.
 * 2. a roll changes the dice not held and never a held one (500 rolls, random holds); all held: ALL DICE HELD.
 * 3. WILD 0: a roll changes nothing; WILD 100: every value inside its die's range for the role (T2 T3 T4, the kit).
 * 4. HIST: OCT- brings back the sound before the roll exactly, KNOB 4 left the newest again.
 * 5. the sound of 300 rolls per role (a note held 0.4 s, let go): never full scale for one voice, no DC over 1 % of the
 *    peak, the voice free after its release (10 s), and never brighter than 2.5 x the brightest factory preset.
 * 6. the kit: 200 rolls, each voice struck: it sounds, never at full scale, it ends.
 * 7. (RE-1) CHIP (Felucca's LOFI): the page shows; WILD 100 rolls stay in their role's choices (no NOISE, no chord ARP on chords or
 *    bass; DUTY on a table or a width; CRSH a decay or a light crush); 300 rolls per role heard as in 5, against
 *    CHIP's brightest tonal preset; the new presets: each sounds, never full scale, ends. */
#define UI_TEST_NO_MAIN 1
#include "ui_test.c"

static int fails3;
static void ok3(const char *what, int ok) { fails3 += check(what, ok); }

static void dice_page(uint32_t track)
{
    track_select(track);
    go_page(GR_DICE);
    frame();
}
static void vals(const track_t *t, int16_t *v)
{
    uint8_t ids[DICE_VALS];
    uint32_t n = dice_ids(t, ids), i;
    memset(v, 0, DICE_VALS * sizeof v[0]);
    for (i = 0; i < n; i++) v[i] = t->p[ids[i]];
}
/* one held note of track k (0.4 s), let go (2 s): peak, |mean| / peak, brightness, still sounding after */
static void listen(uint32_t k, uint32_t note, int32_t *pk, double *dc, double *br, int *hung)
{
    static int32_t o[CTL];
    double s = 0, d = 0, e = 0, prev = 0;
    uint32_t i, j, n = 0;
    track_t *t = &trk[k];
    memset(t->v, 0, sizeof t->v);
    *pk = 0;
    trk_note_on(t, note, 100);
    for (i = 0; i < FS * 4u / 10u; i += CTL) {
        track_render(t, o, CTL);
        for (j = 0; j < CTL; j++) {
            double x = o[j];
            *pk = abs(o[j]) > *pk ? abs(o[j]) : *pk;
            s += x;
            if (i >= FS / 10u) {
                e += x * x;
                d += (x - prev) * (x - prev);
            }
            prev = x;
            n++;
        }
    }
    trk_note_off(t, note);
    for (i = 0; i < FS * 10u; i += CTL)
        track_render(t, o, CTL);
    *dc = *pk ? fabs(s / n) / *pk : 0;
    *br = e > 0 ? d / e : 0;
    *hung = 0;
    for (i = 0; i < NVOICE; i++)
        *hung |= t->v[i].active;
}

/* demos: 8 rolls in a row on each role (a chord, a bass line, a lead phrase) and 4 kit rolls on a beat */
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
static void demos(const char *dir)
{
    static const char *const NM[4] = {"dice_kit.wav", "dice_chords.wav", "dice_bass.wav", "dice_lead.wav"};
    uint32_t role, r, s, k;
    char path[512];
    for (role = 0; role < 4u; role++) {
        track_t *t = &trk[role];
        FILE *w;
        uint32_t step = FS * 60 / 120 / 4;
        snprintf(path, sizeof path, "%s/%s", dir, NM[role]);
        if (!(w = fopen(path, "wb")))
            return;
        wav_hdr(w, step * 16u * 8u);
        ui_power_on();
        dice.rng = 0x1234567u + role;
        song.master_q12 = 4096;                        /* (ui_power_on: MASTER at 0; the pot is not read here) */
        track_select(role);
        for (k = 0; k < NTRK; k++)
            memset(trk[k].v, 0, sizeof trk[k].v);
        for (r = 0; r < 8u; r++) {
            dice_roll();
            t->engine = t->eng_req;
            for (s = 0; s < 16u; s++) {
                if (role == 0u) {
                    if (s % 4u == 0u) trk_note_on(t, 36, 120);
                    if (s % 8u == 4u) trk_note_on(t, 38, 110);
                    if (s % 2u == 0u) trk_note_on(t, s == 14u ? 46 : 42, 90);
                    if (s == 11u) trk_note_on(t, 45, 90);
                } else if (role == 1u) {
                    static const uint8_t C[4] = {57, 60, 64, 67};
                    if (s == 0u) for (k = 0; k < 4u; k++) trk_note_on(t, C[k], 100);
                    if (s == 12u) for (k = 0; k < 4u; k++) trk_note_off(t, C[k]);
                } else if (role == 2u) {
                    static const uint8_t B[16] = {33, 0, 45, 33, 0, 33, 43, 0, 33, 0, 45, 33, 0, 36, 38, 0};
                    static uint8_t on;
                    if (on) { trk_note_off(t, on); on = 0; }
                    if (B[s]) { trk_note_on(t, B[s], 100); on = B[s]; }
                } else {
                    static const uint8_t L[16] = {69, 0, 72, 0, 76, 0, 74, 72, 0, 0, 69, 0, 67, 0, 69, 0};
                    static uint8_t on;
                    if (L[s] || s == 15u) { if (on) trk_note_off(t, on); on = 0; }
                    if (L[s]) { trk_note_on(t, L[s], 100); on = L[s]; }
                }
                mixw(w, step);
            }
        }
        fclose(w);
    }
}

int main(int argc, char **argv)
{
    uint32_t i, r, k;
    int16_t a[DICE_VALS], b[DICE_VALS];
    char what[160];
    ui_power_on();

    /* 1. */
    for (k = 0; k < NPAGES && PAGES[k].graph != GR_DICE; k++)
        ;
    track_select(1);
    ok3("DICE shows on an FMTONE track", k < NPAGES && page_visible(k));
    track_select(0);
    ok3("  and on an FMKIT track", page_visible(k));
    set_engine_of(&trk[0], 0);
    ok3("  not on another engine (ANALOG)", !page_visible(k));
    set_engine_of(&trk[0], ENGI_CKIT);
    apply_preset_to(&trk[0], 0);
    dice_page(1);
    ok3("its knobs and OCT+: an action page (ROLL)", act_cols() != 0u && str_eq(act_name(4), "ROLL"));
    vals(&trk[1], a);
    press(B_OCTUP);
    vals(&trk[1], b);
    ok3("OCT+ rolls: the sound changes", memcmp(a, b, sizeof a) != 0 && msg_is("ROLLED"));

    /* 2. holds */
    {
        uint32_t bad = 0, rs = 99;
        for (r = 0; r < 500u; r++) {
            uint8_t ids[5];
            uint32_t d, m;
            rs = rs * 1103515245u + 12345u;
            dice.hold[1] = (uint16_t)((rs >> 8) & 0x1FEu);   /* (never all) */
            vals(&trk[1], a);
            dice_roll();
            vals(&trk[1], b);
            for (d = 0, i = 0; d < 9u; d++) {
                m = dice_die_ids(&trk[1], d, ids);
                if ((dice.hold[1] >> d) & 1u)
                    bad += memcmp(a + i, b + i, m * sizeof a[0]) != 0;
                i += m;
            }
        }
        ok3("500 rolls with random holds: a held die never changes", !bad);
        dice.hold[1] = 0x1FF;
        vals(&trk[1], a);
        dice_roll();
        vals(&trk[1], b);
        ok3("  all held: nothing rolls, ALL DICE HELD", !memcmp(a, b, sizeof a) && msg_is("ALL DICE HELD"));
        dice.hold[1] = 0;
    }

    /* 3. WILD */
    dice.wild[1] = 1;                                     /* (0 %) */
    vals(&trk[1], a);
    dice_roll();
    vals(&trk[1], b);
    ok3("WILD 0: a roll changes nothing", !memcmp(a, b, sizeof a));
    dice.wild[1] = 0;
    {
        static const int8_t LO[3][9] = {{0, 20, 60, 20, 0, -12, 0, 0}, {0, 40, 20, 0, 0, -12, 0, 0}, {0, 50, 50, 30, 0, -12, 0, 0}};
        static const int8_t HI[3][9] = {{15, 80, 127, 90, 90, 12, 90, 60}, {15, 100, 80, 60, 90, 12, 30, 110}, {15, 110, 120, 100, 90, 12, 50, 50}};
        uint32_t bad = 0, role;
        for (role = 0; role < 3u; role++)
            for (r = 0; r < 300u; r++) {
                track_t *t = &trk[1 + role];
                track_select(1 + role);
                set_engine_of(t, ENGI_CTONE);
                dice_roll();
                for (i = 0; i < 8u; i++)
                    bad += t->p[P_E0 + i] < LO[role][i] || t->p[P_E0 + i] > HI[role][i];
                bad += t->p[P_ATK] < 0 || t->p[P_REL] > 127;
            }
        ok3("WILD 100: every die inside its role's range (T2 chords, T3 bass, T4 lead; 900 rolls)", !bad);
    }

    /* 4. HIST */
    dice_page(1);
    vals(&trk[1], a);
    press(B_OCTUP);
    vals(&trk[1], b);
    press(B_OCTDN);
    {
        int16_t c[DICE_VALS];
        vals(&trk[1], c);
        ok3("HIST: OCT- brings back the sound before the roll", !memcmp(a, c, sizeof a) && dice.hpos[1] == 1u);
        turn(EN_K4, -1);
        vals(&trk[1], c);
        ok3("  KNOB 4 left: the newest again", !memcmp(b, c, sizeof b) && dice.hpos[1] == 0u);
    }

    /* 5. the sound of the rolls */
    {
        uint32_t role, over = 0, dcs = 0, hung = 0, harsh = 0;
        double brmax = 0, worst = 0;
        for (role = 0; role < 3u; role++) {           /* the brightest factory preset of the engine, on its note */
            uint32_t p;
            for (p = 0; p < NELEM(CT_PRESETS); p++) {
                int32_t pk;
                double dc, br;
                int h;
                host_preset(&trk[1 + role], ENGI_CTONE, p);
                listen(1 + role, role == 1u ? 36u : 60u, &pk, &dc, &br, &h);
                brmax = br > brmax ? br : brmax;
            }
        }
        for (role = 0; role < 3u; role++) {
            track_t *t = &trk[1 + role];
            host_preset(t, ENGI_CTONE, role == 1u ? 5u : role == 2u ? 8u : 0u);
            track_select(1 + role);
            for (r = 0; r < 300u; r++) {
                int32_t pk;
                double dc, br;
                int h;
                dice_roll();
                t->engine = t->eng_req;
                listen(1 + role, role == 1u ? 36u : 60u, &pk, &dc, &br, &h);
                over += pk >= 32767;
                dcs += dc > 0.01;
                hung += h;
                harsh += br > brmax * 2.5;
                worst = br > worst ? br : worst;
            }
        }
        snprintf(what, sizeof what, "900 rolls heard: full scale %u, DC %u, hung %u, harsher than 2.5 x the brightest preset %u (%.3f vs %.3f)",
                 over, dcs, hung, harsh, worst, brmax);
        ok3(what, !over && !dcs && !hung && !harsh);
    }

    /* 6. the kit */
    {
        static const uint8_t NOTE[4] = {36, 38, 42, 45};
        uint32_t quiet = 0, over = 0, hung = 0, l;
        track_t *t = &trk[0];
        track_select(0);
        set_engine_of(t, ENGI_CKIT);
        apply_preset_to(t, 0);
        t->engine = t->eng_req;
        for (r = 0; r < 200u; r++) {
            dice_roll();
            for (l = 0; l < 4u; l++) {
                static int32_t o[CTL];
                int32_t pk = 0;
                uint32_t j, s;
                memset(t->v, 0, sizeof t->v);
                trk_note_on(t, NOTE[l], 110);
                for (s = 0; s < FS * 10u; s += CTL) {
                    track_render(t, o, CTL);
                    for (j = 0; j < CTL; j++)
                        pk = abs(o[j]) > pk ? abs(o[j]) : pk;
                    if (!t->v[0].active && !t->v[1].active && !t->v[2].active && !t->v[3].active && s > FS / 10u)
                        break;
                }
                quiet += pk < 1500;
                over += pk >= 32767;
                hung += s >= FS * 10u;
            }
        }
        snprintf(what, sizeof what, "the kit: 200 rolls x 4 voices struck: quiet %u, full scale %u, not ended in 10 s %u", quiet, over, hung);
        ok3(what, !quiet && !over && !hung);
    }

    /* 7. CHIP */
    {
        uint32_t bad = 0, role, pg;
        for (pg = 0; pg < NPAGES && PAGES[pg].graph != GR_DICE; pg++)
            ;
        set_engine_of(&trk[1], ENGI_LOFI);
        track_select(1);
        ok3("CHIP: DICE shows, nine dice, MODE first", page_visible(pg) && dice_engine_ok(&trk[1]) &&
            dice_count(&trk[1]) == 9u && str_eq(dice_name(&trk[1], 0), "MODE"));
        for (role = 0; role < 3u; role++)
            for (r = 0; r < 300u; r++) {
                track_t *t = &trk[1 + role];
                int32_t chip, wave, duty, cr;
                track_select(1 + role);
                set_engine_of(t, ENGI_LOFI);
                dice.wild[1 + role] = 0;
                dice.hold[1 + role] = 0;
                dice_roll();
                chip = t->p[P_E0], wave = t->p[P_E1], duty = t->p[P_E2], cr = t->p[P_E3];
                bad += chip < 0 || chip > 4 || wave < 0 || wave > 4 || wave == 3;
                bad += role < 2u && t->p[P_E6] > 1;
                bad += wave == 4 && duty % 8 != 4;
                bad += wave == 0 && duty % 32 != 16;
                bad += chip == 4 ? (cr && (cr % 4 != 1 || cr / 4 > 26)) : cr > 64;
                bad += t->p[P_E4] && (role != 2u || t->p[P_E4] < 8 || t->p[P_E4] > 24);
                bad += t->p[P_E0] == 3 || (wave == 0 && duty < 32);
                bad += t->p[P_E7] < 70 || t->p[P_ATK] > 60;
            }
        snprintf(what, sizeof what, "CHIP WILD 100: every die in its role's choices (900 rolls, %u off)", bad);
        ok3(what, !bad);
    }
    {
        uint32_t role, over = 0, dcs = 0, hung = 0, harsh = 0, p, quiet = 0, nnew = 0;
        double brmax = 0, worst = 0;
        const engine_t *e = ENGINES[ENGI_LOFI];
        for (role = 0; role < 3u; role++)
            for (p = 0; p < e->npresets; p++) {
                int32_t pk;
                double dc, br;
                int h;
                if (p >= 5u)                                     /* (Felucca's five: the chip set is brighter) */
                    continue;
                host_preset(&trk[1 + role], ENGI_LOFI, p);
                listen(1 + role, role == 1u ? 36u : 60u, &pk, &dc, &br, &h);
                brmax = br > brmax ? br : brmax;
            }
        for (role = 0; role < 3u; role++) {
            track_t *t = &trk[1 + role];
            host_preset(t, ENGI_LOFI, role == 1u ? 6u : role == 2u ? 5u : 14u);
            track_select(1 + role);
            for (r = 0; r < 300u; r++) {
                int32_t pk;
                double dc, br;
                int h;
                dice_roll();
                t->engine = t->eng_req;
                listen(1 + role, role == 1u ? 36u : 60u, &pk, &dc, &br, &h);
                over += pk >= 32767;
                dcs += dc > 0.015;                           /* (a low square: 0.4 s ends inside a cycle) */
                hung += h;
                harsh += br > brmax * 2.5;
                worst = br > worst ? br : worst;
                if (getenv("DICE_DBG") && (pk >= 32767 || dc > 0.01 || br > brmax * 2.5))
                    printf("  role %u: %d %d %d %d %d %d %d %d env %d %d %d %d: pk %d dc %.3f br %.3f\n", role, t->p[P_E0],
                           t->p[P_E1], t->p[P_E2], t->p[P_E3], t->p[P_E4], t->p[P_E5], t->p[P_E6], t->p[P_E7], t->p[P_ATK],
                           t->p[P_DEC], t->p[P_SUS], t->p[P_REL], pk, dc, br);
            }
        }
        snprintf(what, sizeof what, "CHIP: 900 rolls heard: full scale %u, DC %u, hung %u, harsher than 2.5 x %u (%.3f vs %.3f)",
                 over, dcs, hung, harsh, worst, brmax);
        ok3(what, !over && !dcs && !hung && !harsh);
        over = hung = 0;
        for (p = 5; p < e->npresets; p++, nnew++) {          /* the chip set (RE-1) */
            int32_t pk;
            double dc, br;
            int h;
            host_preset(&trk[3], ENGI_LOFI, p);
            listen(3, e->presets[p].e[1] == 3 ? 84u : str_eq(e->presets[p].name, "CHIP KICK") ? 48u : 60u, &pk, &dc, &br, &h);
            quiet += pk < 1500;
            over += pk >= 32767;
            hung += h;
            if (getenv("DICE_DBG"))
                printf("  %-10s pk %d dc %.3f br %.3f\n", e->presets[p].name, pk, dc, br);
        }
        snprintf(what, sizeof what, "CHIP's %u chip presets: quiet %u, full scale %u, hung %u", nnew, quiet, over, hung);
        ok3(what, nnew >= 12u && !quiet && !over && !hung);
        {   /* the hits, a 1.5 ms gate (two control blocks: the shortest MIDI drum note): each still sounds 20 ms or more */
            uint32_t cut = 0;
            char names[160] = "";
            for (p = 5; p < e->npresets; p++) {
                static int32_t o[CTL];
                if (!str_eq(e->presets[p].name, "CHIP KICK") && !str_eq(e->presets[p].name, "NOISE HAT") &&
                    !str_eq(e->presets[p].name, "NOISE SNR") && !str_eq(e->presets[p].name, "LASER ZAP"))
                    continue;                                /* (the hits; a short note of a tone is short) */
                uint32_t j, n = 0, loud = 0;
                track_t *t = &trk[3];
                host_preset(t, ENGI_LOFI, p);
                memset(t->v, 0, sizeof t->v);
                trk_note_on(t, 60, 110);
                for (n = 0; n < FS / 2u; n += CTL) {
                    if (n == 2u * CTL)
                        trk_note_off(t, 60);
                    track_render(t, o, CTL);
                    for (j = 0; j < CTL; j++)
                        loud += abs(o[j]) > 1000;
                }
                if (loud < FS / 50u / 4u) {                  /* (a quarter of 20 ms' samples over -30 dB) */
                    cut++;
                    if (strlen(names) < 120u) { strcat(names, " "); strcat(names, e->presets[p].name); }
                }
            }
            snprintf(what, sizeof what, "  the hits at a 1.5 ms gate: each sounds out (cut short:%s)", cut ? names : " none");
            ok3(what, !cut);
        }
    }
    if (argc > 1) {
        demos(argv[1]);
        printf("dice: demos in %s\n", argv[1]);
    }
    printf("dice: %s\n", fails3 ? "FAILED" : "all checks ok");
    return fails3 != 0;
}
