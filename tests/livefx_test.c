/* SPDX-License-Identifier: GPL-3.0-only
 * RONDO: the live effects. The track's INSERT gains FILTR, DUCK and REPT (src/fx.c), ridden with ARP held (src/ui_layer.c);
 * the FX layer's four knobs are master effects picked from a pool, and they stay where they are left (src/perform.c).
 *   build/host/livefx_test            (tests/run_rondo.sh)
 * 1. FILTR: a held FMTONE chord through a low LP is quieter and duller than open; HP cuts it from below; high RES
 *    rings but stays bounded; MIX 0 is the track bit for bit as TYPE OFF.
 * 2. DUCK: at a kick of track 1 the track drops by DEPTH and is back after REL; DEPTH 0 leaves it alone; nothing
 *    ducks before any kick.
 * 3. REPT: MIX from 0 arms it; from the next boundary the track's slice repeats (pass n like pass n + 1); MIX back to 0:
 *    the live sound; with the SLICER on STUT it passes the sound by.
 * 4. picking FILTR / DUCK / REPT on the INSERT page brings their own A B C (REPT at MIX 0); INSERT 2 is A B C MIX.
 * 5. ARP held: KNOB 1..4 ride the selected track's A B C MIX; a black key 1..4 makes T1..T4 the target while held;
 *    let go, the track it opened on; armed and playing, the turns are recorded; a tap still opens ARP's page.
 * 6. the master knobs: FILTER CRUSH THROW DEPTH at first (an older project's 0); FX > MASTER picks each knob's effect from
 *    the pool, past the ones taken, the one left back at rest; saved in G_ENGGO (its range holds them); FX held, a
 *    knob turned, FX let go: it stays (the LED lit), through the menu too; FX + OCT-: all back.
 * 7. the master effects' sound: ISOLATR left takes the lows away, PUMP dips on the beat, WASH feeds
 *    the reverb, RIDE brightens an FMTONE note. */
#define UI_TEST_NO_MAIN 1
#include "ui_test.c"

static int fails5;
static void ok5(const char *what, int ok) { fails5 += check(what, ok); }
static char what[200];

static int32_t outb[16384u];
static void render(uint32_t blocks, int32_t *keep)          /* blocks of the mix; keep: the left channel, if wanted */
{
    int32_t o[2u * CTL];
    uint32_t b, i;
    for (b = 0; b < blocks; b++) {
        memset(o, 0, sizeof o);
        mix_block(o, CTL);
        if (keep)
            for (i = 0; i < CTL; i++)
                keep[b * CTL + i] = o[2u * i];
    }
}
static uint64_t energy(uint32_t blocks)
{
    int32_t o[2u * CTL];
    uint64_t e = 0;
    uint32_t b, i;
    for (b = 0; b < blocks; b++) {
        memset(o, 0, sizeof o);
        mix_block(o, CTL);
        for (i = 0; i < 2u * CTL; i++)
            e += (uint64_t)(o[i] < 0 ? -o[i] : o[i]);
    }
    return e;
}
static uint64_t hf_energy(uint32_t blocks)                   /* the sample-to-sample differences: the highs */
{
    int32_t o[2u * CTL], last = 0;
    uint64_t e = 0;
    uint32_t b, i;
    for (b = 0; b < blocks; b++) {
        memset(o, 0, sizeof o);
        mix_block(o, CTL);
        for (i = 0; i < CTL; i++) {
            int32_t d = o[2u * i] - last;
            e += (uint64_t)(d < 0 ? -d : d);
            last = o[2u * i];
        }
    }
    return e;
}
static void solo_t2(void)                                  /* a quiet song: T2's FMTONE alone, no sends */
{
    uint32_t k;
    ui_power_on();
    song.master_q12 = 4096;
    for (k = 0; k < NTRK; k++) {
        track_defaults_steps(&trk[k]);
        trk[k].p[P_REV] = trk[k].p[P_DLY] = trk[k].p[P_CHOR] = 0;
        trk[k].p[P_ITYPE] = 0;
    }
    trk[1].p[P_SLCR] = 0;
    render(64, 0);
}
static void chord_on(void)
{
    trk_note_on(&trk[1], 48, 110);
    trk_note_on(&trk[1], 55, 110);
    trk_note_on(&trk[1], 64, 110);
}

int main(void)
{
    uint64_t open, shut, hf_open, hf_shut, e0, e1;

    /* 1. FILTR */
    solo_t2();
    chord_on();
    render(200, 0);
    trk[1].p[P_ITYPE] = IT_FILTER;
    trk[1].p[P_IA] = 127; trk[1].p[P_IB] = 0; trk[1].p[P_IC] = 0; trk[1].p[P_IMIX] = 127;
    render(20, 0);
    open = energy(300);
    hf_open = hf_energy(300);
    trk[1].p[P_IA] = 10;
    render(20, 0);
    shut = energy(300);
    hf_shut = hf_energy(300);
    snprintf(what, sizeof what, "FILTR LP: CUT 127 -> 10 quieter (%.0f%%) and duller (the highs %.0f%%)",
             100.0 * shut / open, 100.0 * hf_shut / hf_open);
    ok5(what, shut < open && hf_shut * 4u < hf_open);
    trk[1].p[P_IC] = 127;                                   /* HP */
    trk[1].p[P_IA] = 100;
    render(20, 0);
    e0 = energy(300);
    snprintf(what, sizeof what, "  MODE HP, CUT 100: the chord's body cut (%.0f%% of open)", 100.0 * e0 / open);
    ok5(what, e0 * 3u < open);
    {
        int32_t pk = 0;
        uint32_t i, c;
        trk[1].p[P_IC] = 0;
        trk[1].p[P_IB] = 127;                               /* RES 127, swept */
        for (c = 0; c < 128u; c += 4u) {
            trk[1].p[P_IA] = (int16_t)c;
            render(8, outb);
            for (i = 0; i < 8u * CTL; i++)
                pk = outb[i] > pk ? outb[i] : -outb[i] > pk ? -outb[i] : pk;
        }
        snprintf(what, sizeof what, "  RES 127 swept over the whole range: bounded (peak %d)", pk);
        ok5(what, pk > 0 && pk < (1 << 23));
    }
    {   /* MIX 0 = OFF, bit for bit (the insert on a block: nothing changed), each new type */
        int32_t a[CTL], b[CTL];
        uint32_t i, ty, same = 1;
        for (ty = IT_FILTER; ty <= IT_REPEAT; ty++) {
            memset(&ins[1], 0, sizeof ins[1]);
            trk[1].p[P_ITYPE] = (int16_t)ty;
            trk[1].p[P_IA] = 30;
            trk[1].p[P_IMIX] = 0;
            for (i = 0; i < CTL; i++)
                a[i] = b[i] = (int32_t)(i * 2654435761u >> 16) - 32768;
            duck_kick();
            track_insert(&trk[1], b, CTL);
            same &= !memcmp(a, b, sizeof a) && !trk[1].ins_run;
        }
        ok5("  MIX 0 (FILTR DUCK REPT): the block bit for bit, nothing running", same);
        trk[1].p[P_ITYPE] = IT_FILTER;
        trk[1].p[P_IMIX] = 127;
    }

    /* 2. DUCK */
    solo_t2();
    duck_seen = 0;
    trk[1].p[P_ITYPE] = IT_DUCK;
    trk[1].p[P_IA] = 127; trk[1].p[P_IB] = 50; trk[1].p[P_IC] = 0; trk[1].p[P_IMIX] = 127;
    chord_on();
    render(200, 0);
    e0 = energy(40);
    ok5("DUCK: no kick yet: nothing ducked", e0 > 0u);
    duck_kick();
    e1 = energy(10);                                        /* the first 7 ms after the kick */
    {
        uint64_t late;
        render(400, 0);                                     /* (REL 50: well past it) */
        late = energy(40) / 4u;
        snprintf(what, sizeof what, "  a kick: right after it %.0f%% of the level, %u ms on %.0f%%",
                 100.0 * e1 / (e0 / 4u), 290, 100.0 * late / (e0 / 4u));
        ok5(what, e1 * 4u < e0 / 4u && late * 10u > (e0 / 4u) * 8u);
    }
    trk[1].p[P_IA] = 0;
    duck_kick();
    e1 = energy(10);
    snprintf(what, sizeof what, "  DEPTH 0: a kick leaves it alone (%.0f%%)", 100.0 * e1 / (e0 / 4u));
    ok5(what, e1 * 10u > (e0 / 4u) * 9u);
    {   /* the kick from the sequencer: FMKIT's KICK lane on T1 */
        uint32_t at = duck_at;
        trk_note_on(&trk[0], 36, 110);                      /* (T1 FMKIT at power-on: 36 its kick) */
        render(1, 0);
        ok5("  FMKIT's KICK lane on T1 is the kick", duck_at != at || trk[0].engine != ENGI_CKIT);
        at = duck_at;
        render(10, 0);
        trk_note_on(&trk[0], 42, 110);                      /* a hat: no */
        render(1, 0);
        ok5("  its hat is not", duck_at == at);
        trk_note_off(&trk[0], 36);
        trk_note_off(&trk[0], 42);
    }

    /* 3. REPT */
    {
        static int32_t a[8192];
        uint32_t len, i, n;
        int64_t diff = 0, live = 0;
        solo_t2();
        song.g[G_BPM] = 120;
        trk[1].p[P_ITYPE] = IT_REPEAT;
        trk[1].p[P_IA] = 60;                                /* 1/16 (2 of 6) */
        trk[1].p[P_IB] = 0;
        trk[1].p[P_IC] = 64;                                /* PITCH 0 */
        trk[1].p[P_IMIX] = 0;
        chord_on();
        len = beat_samples() / 4u;                          /* 5512 */
        n = len / CTL;
        render(40, 0);
        for (i = 0; i < 6u; i++) {                          /* the chord moving under it: a new note a while */
            trk_note_on(&trk[1], 60 + i, 110);
            render(20, 0);
        }
        trk[1].p[P_IMIX] = 127;                             /* armed */
        render(3u * n + 10u, 0);                            /* boundary, record a step, a pass or two */
        render(2u * n + 2u, outb);                          /* two passes and a bit, one run */
        (void)a;
        for (i = SL_RAMP; i < len - SL_RAMP; i++) {         /* (the passes' windowed ends left out) */
            int32_t d = outb[i] - outb[i + len];
            diff += d < 0 ? -d : d;
            live += outb[i] < 0 ? -outb[i] : outb[i];
        }
        snprintf(what, sizeof what, "REPT: armed by MIX, a 1/16 slice repeats: pass n vs n+1 differ by %.1f%%", 100.0 * diff / (live ? live : 1));
        ok5(what, live > 0 && diff * 50 < live && ins[1].rst == 2u);
        trk[1].p[P_IMIX] = 0;
        render(20, 0);
        ok5("  MIX back to 0: the live sound, armed afresh", ins[1].rst == 0u && !ins[1].rw);
        trk[1].p[P_SLCR] = SL_STUT;
        trk[1].p[P_IMIX] = 127;
        render(3u * n, 0);
        ok5("  the SLICER on STUT (its buffer): REPT passes by", ins[1].rst == 0u);
        trk[1].p[P_SLCR] = 0;
        trk[1].p[P_IMIX] = 0;
    }

    /* 4. the page */
    {
        uint32_t k;
        ui_power_on();
        track_select(1);
        go_page(GR_INS);
        for (k = 0; k < 20u && TSEL->p[P_ITYPE] != IT_REPEAT; k++)
            turn(EN_K1, 1);
        ok5("INSERT: TYPE to REPT brings RATE 1/16, DECAY 0, PITCH 0, MIX 0", TSEL->p[P_ITYPE] == IT_REPEAT &&
            TSEL->p[P_IA] == 53 && TSEL->p[P_IB] == 0 && TSEL->p[P_IC] == 64 && TSEL->p[P_IMIX] == 0);
        turn(EN_K1, -1);
        turn(EN_K1, -1);
        ok5("  TYPE to FILTR: CUT 80, RES 40, MODE LP, MIX 100 %", TSEL->p[P_ITYPE] == IT_FILTER && TSEL->p[P_IA] == 80 &&
            TSEL->p[P_IB] == 40 && TSEL->p[P_IC] == 0 && TSEL->p[P_IMIX] == 127);
        for (k = 0; k < NPAGES && !(str_eq(PAGES[k].title, "INSERT 2")); k++)
            ;
        ok5("  INSERT 2: A B C MIX", k < NPAGES && PAGES[k].id[0] == P_IA && PAGES[k].id[1] == P_IB &&
            PAGES[k].id[2] == P_IC && PAGES[k].id[3] == P_IMIX);
        ok5("  the names: FILTR DUCK REPT", str_eq(N_ITYPE[IT_FILTER], "FILTR") && str_eq(N_ITYPE[IT_DUCK], "DUCK") &&
            str_eq(N_ITYPE[IT_REPEAT], "REPT") && str_eq(ins_desc(IT_DUCK, 1)->label, "REL"));
    }

    /* 5. ARP held */
    {
        uint32_t mc;
        int16_t a0;
        ui_power_on();
        trk[2].p[P_ITYPE] = IT_FILTER;
        trk[2].p[P_IA] = 80;
        trk[2].p[P_IMIX] = 127;
        track_select(0);
        go_home();
        frames(100);
        btn_down(B_ARP);
        frames(900);
        ok5("ARP held: the TRACK FX layer", ui.layer == LAYER_ARP && cur_page() == &PAGES[ui.page]);
        key_down(5);                                     /* A#3: black key 3 */
        frame();
        key_up(5);
        frames(50);
        ok5("  black key 3: T3 the target", song.sel == 2u);
        turn(EN_K1, -5);
        turn(EN_K4, -10);
        snprintf(what, sizeof what, "  KNOB 1 CUT 80 -> %d, KNOB 4 MIX 127 -> %d (T3's)", trk[2].p[P_IA], trk[2].p[P_IMIX]);
        ok5(what, trk[2].p[P_IA] < 80 && trk[2].p[P_IMIX] < 127 && trk[0].p[P_IA] == TP[P_IA].def);
        btn_up(B_ARP);
        frames(100);
        ok5("  ARP let go: back on T1", song.sel == 0u && !ui.layer);
        trk[0].p[P_ITYPE] = IT_DUCK;
        song.rec = 1;                                    /* T1 armed, playing: recorded */
        seq_start();
        song.playing = 1;
        mc = motion.count;
        a0 = trk[0].p[P_IA];
        btn_down(B_ARP);
        frames(900);
        turn(EN_K1, -8);
        frames(50);
        btn_up(B_ARP);
        frames(100);
        snprintf(what, sizeof what, "  armed and playing: the ride recorded (%u records, DEPTH %d -> %d)", motion.count - mc, a0, trk[0].p[P_IA]);
        ok5(what, motion.count > mc);
        song.rec = 0;
        song.playing = 0;
        press(B_ARP);
        frames(100);
        ok5("  a tap: ARP's page", !ui.home && str_eq(cur_page()->title, "ARP"));
    }

    /* 6. the master knobs */
    {
        uint32_t k, def = 1;
        ui_power_on();
        song.g[G_ENGGO] = 0;
        for (k = 0; k < 4u; k++)
            def &= mk_eff(k) == k;
        ok5("master knobs: FILTER CRUSH THROW DEPTH at first (G_ENGGO 0)", def && str_eq(MK_NAME[mk_eff(0)], "FILTER"));
        go_page(GR_MFX);
        ok5("  FX > MASTER", str_eq(cur_page()->title, "MASTER"));
        perf_k[0] = -40;                                 /* (the FILTER on: left behind, back to rest) */
        turn(EN_K1, 1);
        ok5("  KNOB 1 right: past CRUSH THROW DEPTH (taken) to ISOLATR; FILTER back to rest",
            mk_eff(0) == MK_ISO && perf_k[0] == 0 && song.g[G_ENGGO] != 0);
        turn(EN_K2, 1);
        turn(EN_K3, 1);
        turn(EN_K3, 1);
        ok5("  KNOB 2: WASH; KNOB 3: PUMP, RIDE", mk_eff(1) == MK_WASH && mk_eff(2) == MK_RIDE);
        ok5("  G_ENGGO's range holds every assignment (a project load clamps to it)", GP[G_ENGGO].max >= 4095 &&
            GP[G_ENGGO].min == 0 && (uint32_t)song.g[G_ENGGO] <= 4095u);
        go_home();
        frames(50);
        btn_down(B_FX);
        frames(900);
        turn(EN_K1, -6);                                 /* ISOLATR left: the lows */
        turn(EN_K2, 5);                                  /* WASH */
        frames(50);
        btn_up(B_FX);
        frames(100);
        snprintf(what, sizeof what, "  FX held, KNOB 1 ISOLATR %d, KNOB 2 WASH %d; let go: they stay", mk_x[0], mk_x[1]);
        ok5(what, mk_x[0] < 0 && mk_x[1] > 0 && !ui.layer);
        ok5("  the FX LED lit (a master effect on)", mk_on());
        hold(B_HOME);                                    /* the menu */
        frames(100);
        ok5("  the MENU open: still there", ui.menu && mk_x[0] < 0 && mk_x[1] > 0);
        hold(B_HOME);
        frames(100);
        btn_down(B_FX);
        frames(900);
        press(B_OCTDN);
        frames(50);
        btn_up(B_FX);
        frames(100);
        ok5("  FX + OCT-: all back to rest", !mk_on() && !ui.layer);
        song.g[G_ENGGO] = 0;
    }

    /* 7. the master effects' sound */
    {
        uint64_t a, b;
        solo_t2();
        trk_note_on(&trk[1], 36, 110);                   /* a low note: lows */
        trk_note_on(&trk[1], 48, 110);
        render(200, 0);
        mk_x[0] = 0;
        a = energy(200);
        mk_x[0] = -100;
        render(20, 0);
        b = energy(200);
        snprintf(what, sizeof what, "ISOLATR -100 (the lows away): %.0f%% of the level", 100.0 * b / a);
        ok5(what, b * 2u < a);
        mk_x[0] = 0;
        render(40, 0);
        {
            uint64_t e1, e2;
            int32_t o[2u * CTL];
            uint32_t beat = beat_samples(), i, n = 0;
            seq_start();                                 /* (perform's clock from 0: the beat's start) */
            song.playing = 1;
            mk_x[2] = 100;
            render(beat / CTL * 2u, 0);
            while ((pf.ph % beat) >= CTL)                /* to a beat's start */
                render(1, 0);
            e1 = energy(beat / CTL / 10u);               /* its first 10 % */
            render(beat / CTL * 6u / 10u - beat / CTL / 10u, 0);
            e2 = energy(beat / CTL / 10u);               /* 60 .. 70 % */
            snprintf(what, sizeof what, "PUMP 100: the beat's first tenth %.0f%% of its seventh", 100.0 * e1 / (e2 ? e2 : 1));
            ok5(what, e1 * 2u < e2);
            mk_x[2] = 0;
            song.playing = 0;
            (void)o; (void)i; (void)n;
        }
        {
            uint64_t dry, wet;
            int32_t rv[2u * CTL];
            uint32_t bb;
            render(100, 0);
            dry = 0;
            for (bb = 0; bb < 200u; bb++) {              /* the reverb bus alone: what the send got */
                memset(rv, 0, sizeof rv);
                mix_block(rv, CTL);
                dry += (uint64_t)(send_r[0] < 0 ? -send_r[0] : send_r[0]);
            }
            mk_x[1] = 100;
            render(40, 0);
            wet = 0;
            for (bb = 0; bb < 200u; bb++) {
                memset(rv, 0, sizeof rv);
                mix_block(rv, CTL);
                wet += (uint64_t)(send_r[0] < 0 ? -send_r[0] : send_r[0]);
            }
            snprintf(what, sizeof what, "WASH 100: the reverb's send %llu -> %llu, SIZE %d -> %d", (unsigned long long)dry,
                     (unsigned long long)wet, song.g[G_RSIZE], rev_size());
            ok5(what, wet > dry && rev_size() > song.g[G_RSIZE]);
            mk_x[1] = 0;
        }
        {
            uint64_t h0, h1;
            solo_t2();
            trk_note_on(&trk[1], 60, 100);
            render(100, 0);
            h0 = hf_energy(100) * 1000u / (energy(100) | 1u);
            mk_x[3] = 100;
            trk_note_off(&trk[1], 60);
            render(200, 0);
            trk_note_on(&trk[1], 60, 100);
            render(100, 0);
            h1 = hf_energy(100) * 1000u / (energy(100) | 1u);
            snprintf(what, sizeof what, "RIDE 100: an FMTONE note brighter (the highs' share %llu -> %llu)", (unsigned long long)h0,
                     (unsigned long long)h1);
            ok5(what, trk[1].engine != ENGI_CTONE || h1 > h0);
            mk_x[3] = 0;
        }
    }

    printf("livefx: %s\n", fails5 ? "FAILED" : "all checks ok");
    return fails5 != 0;
}
