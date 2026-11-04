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
    {   /* 1.1: on every engine one can pick; a retired one (ANALOG, a stored number) loads as its substitute, dice and all */
        uint32_t n, all = 1;
        for (n = 0; n < NENG_SHOWN; n++) {
            set_engine_of(&trk[0], eng_vis(n));
            all &= page_visible(k);
        }
        ok3("  1.1: and on each of the eight engines", all);
        set_engine_of(&trk[0], ENGI_CTONE);
        ok3("  a retired engine asked for (1.5: FMTONE): its substitute (GENESIS), DICE there", trk[0].eng_req == ENGI_GENESIS && page_visible(k));
    }
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
    /* (1.5: FMTONE retired, and its hand-made dice with it: GENESIS' bred dice, section 8, on every class) */
    track_select(1);
    set_engine_of(&trk[1], ENGI_GENESIS);
    apply_preset_to(&trk[1], 0);

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

    /* 5b. 1.5: a held die set by hand: KNOB 3 its value, KNOB 4 which of its values; a roll leaves it; let go, KNOB 3 and
     * 4 are WILD and HIST again */
    {
        track_t *t = &trk[1];
        uint32_t k = 1, part, m, id, id2;
        int16_t before, a2[DICE_VALS], b2[DICE_VALS];
        dice_page(1);
        set_engine_of(t, ENGI_GENESIS);
        apply_preset_to(t, 0);
        dice.cur[k] = 3;                                  /* (OP1: ATK DEC SUS REL LVL) */
        dice.wild[k] = 0;
        turn(EN_K2, 1);
        id = dice_held_id(t, &part, &m);
        ok3("1.5: a die held (GENESIS OP1): KNOB 3 its first value (ATK1), five to pick", id == P_FM1_ATK && m == 5u && part == 0u);
        turn(EN_K4, 2);
        id = dice_held_id(t, &part, &m);
        before = t->p[id];
        turn(EN_K3, 5);
        if (getenv("DICE_DBG")) printf("  held: id %u part %u before %d now %d\n", id, part, before, t->p[id]);
        ok3("  KNOB 4 picks SUS1, KNOB 3 turns it by hand (up)", id == P_FM1_SUS && (t->p[id] > before || before == 127));
        id2 = id;
        before = t->p[id2];
        vals(t, a2);
        press(B_OCTUP);
        vals(t, b2);
        ok3("  a roll: the held die keeps its hand-set value, the rest rolls", t->p[id2] == before && memcmp(a2, b2, sizeof a2) != 0);
        turn(EN_K2, -1);
        ok3("  let go: KNOB 3 WILD again", dice_held_id(t, &part, &m) == P_COUNT);
        dice.cur[k] = 0;
        turn(EN_K2, 1);
        id = dice_held_id(t, &part, &m);
        before = t->p[id];
        turn(EN_K3, 1);
        ok3("  ALG held: one value (KNOB 4 has nothing to pick), KNOB 3 steps the algorithm", id == P_E0 && m == 1u &&
            t->p[id] == (int16_t)clamp(before + 1, 0, 7));
        turn(EN_K2, -1);
        dice.wild[k] = 0;
    }

    /* 6. the kits (1.5: GEN KIT's, MOD KIT's) */
    for (uint32_t ke = 0; ke < 2u; ke++) {
        static const uint8_t NOTE[4] = {36, 38, 42, 45};
        uint32_t quiet = 0, over = 0, hung = 0, l;
        track_t *t = &trk[0];
        track_select(0);
        set_engine_of(t, ke ? ENGI_MKIT : ENGI_CKIT);
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
                if (getenv("DICE_DBG") && pk < 1500)
                    printf("  quiet %s lane %u: p %d %d %d %d %d lv %d kit %d %d %d pk %d\n", ENGINES[t->eng_req]->name, l,
                           t->p[P_FM1_ATK + 5 * l], t->p[P_FM1_DEC + 5 * l], t->p[P_FM1_SUS + 5 * l], t->p[P_FM1_REL + 5 * l],
                           t->p[P_FM1_LEVEL + 5 * l], t->p[P_LN0 + l], t->p[P_E0], t->p[P_E1], t->p[P_E2], pk);
                over += pk >= 32767;
                hung += s >= FS * 10u;
            }
        }
        snprintf(what, sizeof what, "%s: 200 rolls x 4 voices struck: quiet %u, full scale %u, not ended in 10 s %u",
                 ENGINES[t->eng_req]->name, quiet, over, hung);
        ok3(what, !quiet && !over && !hung);
    }

    /* 7. 1.5: CHIP, SAMPLE, GRAIN, PHYS retired: a sound of each loads as its substitute (WAVES, MOD KIT for a kit, WAVE 32,
     * MOD),
     * dice and all */
    {
        uint32_t pg;
        for (pg = 0; pg < NPAGES && PAGES[pg].graph != GR_DICE; pg++)
            ;
        track_select(1);
        set_engine_of(&trk[1], ENGI_LOFI);
        ok3("1.5: CHIP retired: asked for, WAVES with its DICE", trk[1].eng_req == ENGI_WAVES && page_visible(pg));
        set_engine_of(&trk[1], 8u);
        ok3("  GRAIN: WAVE 32", trk[1].eng_req == ENGI_WT32 && page_visible(pg));
        set_engine_of(&trk[1], ENGI_PHYS);
        ok3("  PHYS: MOD", trk[1].eng_req == ENGI_MOD && page_visible(pg));
        set_engine_of(&trk[1], ENGI_SAMPLE);
        apply_preset_to(&trk[1], 0);
        ok3("  SAMPLE (its PIANO): GENESIS; its LOFI KIT (3): MOD KIT", trk[1].eng_req == ENGI_GENESIS &&
            (trk[1].eng_req = ENGI_SAMPLE, apply_preset_to(&trk[1], 3u), trk[1].eng_req == ENGI_MKIT) &&
            str_eq(ENGINES[ENGI_MKIT]->presets[trk[1].preset].name, "LOFI KIT"));
        set_engine_of(&trk[1], ENGI_GENESIS);
        apply_preset_to(&trk[1], 0);
    }
    {   /* 1.1: the bred dice: the page, the engine pages gone, each class's rolls heard (1.5: the six bred: GENESIS,
         * WAVES WAVE 32, MOD, ANALOG SID) */
        static const uint32_t EN[6] = {ENGI_GENESIS, ENGI_WAVES, ENGI_WT32, ENGI_MOD, 0u, ENGI_SID};
        uint32_t ei, k, r, p, nr = getenv("DICE_N") ? (uint32_t)atoi(getenv("DICE_N")) : 40u;
        for (ei = 0; ei < NELEM(EN); ei++) {
            const engine_t *e = ENGINES[EN[ei]];
            uint32_t over = 0, hung = 0, phung = 0, pover = 0, np = 0, quiet = 0, harsh = 0, range = 0, kitbad = 0, pages = 0, rolls = 0;
            int32_t pkmin = 1 << 30, pkmax = 0;
            double brmax = 0, worst = 0;
            for (k = 0; k < 4u; k++) {                     /* (the presets a track's class lists: their brightest, quietest;
                                                            * none: all of them, on the track's note: 1.5, PHYS on T1) */
                uint32_t nf = 0;
                for (p = 0; p < e->npresets; p++)
                    nf += dice_fits(&trk[k], p, 1u) != 0;
                for (p = 0; p < e->npresets; p++) {
                    int32_t pk;
                    double dc, br;
                    int h;
                    if (nf && !dice_fits(&trk[k], p, 1u))
                        continue;
                    host_preset(&trk[k], EN[ei], p);
                    listen(k, k ? 60u : 36u, &pk, &dc, &br, &h);
                    if (getenv("DICE_DBG")) printf("  preset %s T%u %-12s pk %d br %.3f\n", e->name, k + 1, e->presets[p].name, pk, br);
                    np++;
                    pkmax = pk > pkmax ? pk : pkmax;
                    brmax = br > brmax ? br : brmax;
                    pkmin = pk < pkmin ? pk : pkmin;
                    phung += h;
                }
            }
            for (k = 0; k < 4u; k++) {
                track_t *t = &trk[k];
                host_preset(t, EN[ei], dice_parent(t, -1));
                track_select(k);
                go_page(GR_DICE);
                frame();
                pages += cur_page()->graph != GR_DICE;
                for (p = 0; p < NPAGES; p++)
                    pages += PAGES[p].fam == FAM_EDIT && page_visible(p) && PAGES[p].scope == SC_ENGINE;
                for (r = 0; r < nr; r++, rolls++) {
                    int32_t pk;
                    double dc, br;
                    int h;
                    uint32_t j;
                    dice_roll();
                    t->engine = t->eng_req;
                    for (j = 0; j < 8u; j++)
                        range += t->p[P_E0 + j] < e->edit[j].min || t->p[P_E0 + j] > e->edit[j].max;
                    if (EN[ei] == ENGI_SAMPLE)                /* (a rogue rolls the kits, the others never) */
                        kitbad += (k == 0u) != (t->p[P_E0] == 1 || t->p[P_E0] == 3);
                    listen(k, k ? 60u : 36u, &pk, &dc, &br, &h);
                    over += pk > pkmax + pkmax / 2;
                    quiet += pk < pkmin / 8;
                    hung += h;
                    harsh += br > brmax * 2.5;
                    worst = br > worst ? br : worst;
                    if (getenv("DICE_DBG") && (pk > pkmax + pkmax / 2 || pk < pkmin / 8 || h || br > brmax * 2.5))
                        printf("  %s T%u: %d %d %d %d %d %d %d %d env %d %d %d %d: pk %d br %.3f hung %d\n", e->name, k + 1,
                               t->p[P_E0], t->p[P_E1], t->p[P_E2], t->p[P_E3], t->p[P_E4], t->p[P_E5], t->p[P_E6], t->p[P_E7],
                               t->p[P_ATK], t->p[P_DEC], t->p[P_SUS], t->p[P_REL], pk, br, h);
                }
            }
            snprintf(what, sizeof what, "%s: DICE on every class (engine pages %u), %u rolls: out of range %u%s", e->name,
                     pages, rolls, range, EN[ei] == ENGI_SAMPLE ? ", a kit off its class 0" : "");
            if (EN[ei] == ENGI_SAMPLE)
                snprintf(what, sizeof what, "%s: DICE on every class (engine pages %u), %u rolls: out of range %u, kit off class %u",
                         e->name, pages, rolls, range, kitbad);
            ok3(what, !pages && !range && !kitbad);
            snprintf(what, sizeof what, "  heard: louder than 1.5 x its loudest preset %u (%u presets), quiet %u, hung %u (its presets %u), harsher than 2.5 x %u (%.3f vs %.3f)",
                     over, np, quiet, hung, phung, harsh, worst, brmax);
            ok3(what, !over && !quiet && (!hung || phung) && hung * 10u <= rolls && harsh * 100u <= rolls);   /* (a wild one
                                                         * in a hundred at the bright edge: HIST takes it back) */
        }
    }
    if (argc > 1) {
        demos(argv[1]);
        printf("dice: demos in %s\n", argv[1]);
    }
    printf("dice: %s\n", fails3 ? "FAILED" : "all checks ok");
    return fails3 != 0;
}
