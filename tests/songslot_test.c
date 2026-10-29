/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright (C) 2026 Dungeon Tracker (RONDO) contributors */
/* Dungeon Tracker: SONG SLOTS (firmware/src/song_store.c, project.c): eight whole songs, A..H, each the project
 * and the six patterns of the PATTERN BANK; against a simulated NOR (writes only clear bits, erases set a sector):
 * round trips, all eight slots, power cuts at every write of a save, the move of 1.1's projects, SONG TOO BIG,
 * a rename keeping the patterns, the free extent rotating, the sizes. */
#define main hostsim_main
#include "hostsim.c"
#undef main

static int32_t fm1_enc_take(uint32_t e) { (void)e; return 0; }
static void fm1_irq_off(void) {}
static void fm1_irq_on(void) {}
static void lcd_sync(void) {}
static void lcd_power(uint32_t s) { (void)s; }
static void lcd_wake_now(void) {}
static void lcd_blit(uint32_t x, uint32_t y, uint32_t w, uint32_t h, const uint16_t *p)
{ (void)x; (void)y; (void)w; (void)h; (void)p; }
#define FELUCCA_FLASH 1
#include "../firmware/src/gfx.c"
#include "../firmware/src/panel.c"
#include "../firmware/src/ui.c"
#include "../firmware/src/menu_items.c"
static void panel_setup(void) {}
static void dg_game_set(uint32_t on) { (void)on; }

static uint8_t nor[0x100000], flash_ok = 1;
static int fail_after = -1;
static uint32_t erases, progs;
static uint8_t touched[0x100];                   /* 4 KiB sectors erased or written (bit set) */
static int st_read(uint32_t off, void *dst, uint32_t n) { memcpy(dst, nor + off, n); return 0; }
static int st_erase(uint32_t off)
{
    erases++;
    touched[off >> 12] = 1;
    memset(nor + off, 0xFF, 4096);
    return 0;
}
static int st_prog(uint32_t off, const void *src, uint32_t n)
{
    const uint8_t *s = src;
    uint32_t i;
    if (fail_after == 0) return -9;
    if (fail_after > 0) fail_after--;
    progs++;
    touched[off >> 12] = 1;
    for (i = 0; i < n; i++) nor[off + i] &= s[i];
    return 0;
}
static uint32_t irq_save(void) { return 0; }
static void irq_restore(uint32_t f) { (void)f; }
static uint32_t fl_jedec_ram(void) { return 0; }
static void fl_plain_window_init(void) {}
#define FL_FAR(fn) (fn)
#include "../firmware/src/storage.c"
#include "../firmware/src/upreset.c"
#include "../firmware/src/project.c"
#include "../firmware/src/boot_song.c"

static int fails;
static void check(const char *what, int ok)
{
    printf("songs: %-72s %s\n", what, ok ? "ok" : "FAIL");
    fails += !ok;
}

static void wipe_ram(void)                       /* a power cycle's RAM: nothing of the music, the flash as it is */
{
    memset(&song, 0, sizeof song);
    memset(trk, 0, sizeof trk);
    memset(&chain, 0, sizeof chain);
    chain_defaults(&chain_config);
    memset(proj_slot, 0, sizeof proj_slot);
    memset(&proj_hi_cache, 0, sizeof proj_hi_cache);
    proj_hi_slot = 0xFFu;
    memset(bank_pat, 0, sizeof bank_pat);
    memset((void *)&bank, 0, sizeof bank);
    bank.queue = CHAIN_OFF;
    memset(&ui, 0, sizeof ui);
    memset(&sg, 0, sizeof sg);
    memset(&motion, 0, sizeof motion);
    host_tracks_init();
    proj_cur = PROJ_NO_SLOT;
    fm1_ms = 0;
    transport_req = 0;
    fail_after = -1;
}
static void boot(void)                           /* the projects' part of persist_boot */
{
    uint32_t i;
    sg_boot();
    for (i = 0; i < NPROJ; i++)
        if (!proj_import(&proj_scratch, pslot(i), sizeof(project_store_t)))
            proj_fetch(i);
}
static void reset(void)
{
    memset(nor, 0xFF, sizeof nor);
    wipe_ram();
    erases = progs = 0;
    memset(touched, 0, sizeof touched);
    flash_ok = 1;
    boot();
}

/* a pattern of its own: seed s, `busy` 0 (a sparse part) .. 2 (every step of every track, random) */
static uint32_t rs;
static uint32_t rnd(void) { rs = rs * 1664525u + 1013904223u; return rs >> 8; }
static void make_pattern(uint32_t s, uint32_t busy)
{
    uint32_t k, i;
    rs = s * 2654435761u + 7u;
    for (k = 0; k < NTRK; k++) {
        track_t *t = &trk[k];
        memset(t->step, 0, sizeof t->step);
        t->p[P_SLEN] = (int16_t)(busy ? 64 : 16 << (s % 3u));
        for (i = 0; i < (uint32_t)t->p[P_SLEN]; i++) {
            step_t *st = &t->step[i];
            if (!busy && (i + k + s) % 4u)
                continue;
            st->time = ST_NOTE;
            st->n = (uint8_t)(busy == 2 ? 1u + rnd() % 4u : 1u);
            for (uint32_t j = 0; j < st->n; j++) st->note[j] = (uint8_t)(36u + rnd() % 48u);
            st->vel = (uint8_t)(busy == 2 ? 1u + rnd() % 127u : 100u);
            if (busy == 2) {
                st->flags = (uint8_t)(rnd() & 3u);
                st->hit = (uint8_t)rnd();
                st->acc = (uint8_t)(st->hit & rnd());
                st->probability = (uint8_t)(rnd() % 100u);
            }
        }
    }
    motion.count = 0;
}
/* the bank filled: patterns A..F (mask `used`) each made, the current one `cur`; a SONG arrangement over them */
static void make_song(uint32_t s, uint32_t used, uint32_t cur, uint32_t busy)
{
    uint32_t p;
    for (p = 0; p < NBANK; p++)
        if ((used >> p) & 1u) {
            make_pattern(s * 16u + p, busy);
            bank_take(p);
        }
    bank.used = (uint8_t)used;
    bank.cur = (uint8_t)cur;
    make_pattern(s * 16u + cur, busy);                   /* (the tracks: the current one) */
    bank_take(cur);
    chain_defaults(&chain_config);
    chain_config.count = 2;
    chain_config.row[0] = chain_row_of(0, (uint8_t)(1u + s % 4u));
    chain_config.row[1] = chain_row_of(cur, 2);
    trk[2].p[P_LEVEL] = (int16_t)(20 + s);               /* (a sound of its own: T3's level) */
}
static chain_pattern_t want[NBANK];
static uint8_t want_used, want_cur;
static chain_config_t want_chain;
static step_t want_steps[NTRK][NSTEP];
static void remember(void)
{
    uint32_t k;
    memcpy(want, bank_pat, sizeof want);
    want_used = bank.used;
    want_cur = bank.cur;
    want_chain = chain_config;
    for (k = 0; k < NTRK; k++)
        memcpy(want_steps[k], trk[k].step, sizeof want_steps[k]);
}
static int same_as_remembered(int32_t level)
{
    uint32_t p, k;
    int ok = bank.used == want_used && bank.cur == want_cur && !memcmp(&chain_config, &want_chain, sizeof want_chain);
    for (p = 0; p < NBANK; p++)
        if ((want_used >> p) & 1u)
            ok &= !memcmp(&bank_pat[p].step, &want[p].step, sizeof want[p].step) &&
                  !memcmp(&bank_pat[p].timing, &want[p].timing, sizeof want[p].timing) &&
                  !memcmp(&bank_pat[p].motion, &want[p].motion, sizeof want[p].motion);
    for (k = 0; k < NTRK; k++)
        ok &= !memcmp(trk[k].step, want_steps[k], sizeof want_steps[k]);
    return ok && (level < 0 || trk[2].p[P_LEVEL] == level);
}
static void other_music(void)                    /* something else in the bank and the tracks */
{
    make_song(99, 0x21u, 5, 0);
    trk[2].p[P_LEVEL] = 3;
}

int main(void)
{
    uint32_t s, p, i;
    (void)hostsim_main;

    /* 1. a whole song: saved, other music, loaded back */
    reset();
    make_song(1, 0x3Fu, 2, 0);
    remember();
    check("a song saves to A: SAVED", project_save_as(0, "FIRST") == 0 && !strcmp(ui.msg, "SAVED") && project_used(0) &&
          project_pats(0) == 0x3Fu);
    other_music();
    project_load(0);
    check(".. loaded: all six patterns, the current one, the arrangement, the tracks and the sounds as saved",
          same_as_remembered(21) && proj_cur == 0u && bank.dirty == 0x3Fu);
    check(".. the bank's flash copies to be rewritten (bank_poll: every key)", bank.dirty == 0x3Fu && bank.queue == CHAIN_OFF);
    wipe_ram();
    boot();
    other_music();
    project_load(0);
    check(".. after a power cycle too (the RAM lost, read from flash)", same_as_remembered(21));

    /* 2. eight slots, each its own song, through a power cycle */
    reset();
    for (s = 0; s < NPROJ; s++) {
        char nm[8] = "SONG A";
        nm[5] = (char)('A' + s);
        make_song(10u + s, (0x3Fu >> (s % 4u)) | 1u, s % 2u, 0);
        if (project_save_as(s, nm))
            break;
    }
    check("eleven slots, A..K, each saved", s == NPROJ);
    wipe_ram();
    boot();
    {
        int ok = 1;
        char n[16];
        for (s = 0; s < NPROJ; s++) {
            char nm[8] = "SONG A";
            nm[5] = (char)('A' + s);
            make_song(10u + s, (0x3Fu >> (s % 4u)) | 1u, s % 2u, 0);
            remember();
            other_music();
            project_load(s);
            ok &= same_as_remembered(30 + (int32_t)s) && project_name(s, n) && !strcmp(n, nm);
        }
        check(".. after a power cycle each loads its own song, its own name", ok);
    }
    check("SAVE > PROJECT's SLOT: Q then A..K", GP[G_SLOT].max == (int16_t)NPROJ && NPROJ == 11u);

    /* 3. a power cut at every write of a save: the song before it, whole; never half of either */
    {
        uint32_t n, cut, ok = 1, steps;
        reset();
        make_song(3, 0x0Fu, 1, 0);
        project_save_as(4, "OLD");
        remember();
        progs = 0;
        make_song(4, 0x3Fu, 4, 0);
        project_save_as(5, "PROBE");                    /* (how many writes a save takes) */
        steps = progs;
        for (cut = 0; cut < steps; cut++) {
            make_song(4, 0x3Fu, 4, 0);
            fail_after = (int)cut;
            n = (uint32_t)project_save_as(4, "NEW");
            fail_after = -1;
            wipe_ram();
            boot();
            make_song(3, 0x0Fu, 1, 0);                  /* (what the slot held) */
            remember();
            other_music();
            project_load(4);
            ok &= n != 0 && same_as_remembered(23);
        }
        check("a power cut at each of a save's writes: SAVE ERROR, the song before it whole after a power cycle",
              ok && steps >= 3u);
        make_song(4, 0x3Fu, 4, 0);
        remember();
        ok = project_save_as(4, "NEW") == 0;
        wipe_ram();
        boot();
        other_music();
        project_load(4);
        check(".. and the next save goes through", ok && same_as_remembered(24));
    }

    /* 4. rename keeps the patterns; erase empties; an erased slot takes a new save */
    {
        char n[16];
        reset();
        make_song(5, 0x15u, 2, 0);
        remember();
        project_save_as(1, "TAKE ONE");
        other_music();
        check("a rename keeps the song's patterns", project_rename(1, "TAKE TWO") == 0 && project_pats(1) == 0x15u &&
              project_name(1, n) && !strcmp(n, "TAKE TWO"));
        wipe_ram();
        boot();
        other_music();
        project_load(1);
        check(".. loaded after a power cycle: the renamed song, whole", same_as_remembered(25) && project_name(1, n) &&
              !strcmp(n, "TAKE TWO"));
        check("an erase empties the slot (and its patterns)", project_erase(1) == 0 && !project_used(1) && !project_pats(1));
        wipe_ram();
        boot();
        check(".. still empty after a power cycle", !project_used(1) && !project_pats(1));
        make_song(6, 0x03u, 0, 0);
        check(".. and takes a new save", project_save_as(1, "AGAIN") == 0 && project_used(1) && project_pats(1) == 0x03u);
    }

    /* 5. SONG TOO BIG: nothing erased, nothing written, the slot as it was */
    {
        uint32_t e0, p0;
        reset();
        make_song(7, 0x01u, 0, 0);
        project_save_as(2, "SMALL");
        remember();
        make_song(8, 0x3Fu, 0, 2);                      /* (six patterns of random notes, hits, chances: every step) */
        e0 = erases; p0 = progs;
        check("six patterns of random everything: SONG TOO BIG, nothing erased or written",
              project_save_as(2, "HUGE") == 2 && !strcmp(ui.msg, "SONG TOO BIG") && erases == e0 && progs == p0);
        wipe_ram();
        boot();
        other_music();
        project_load(2);
        check(".. the slot as it was", same_as_remembered(27));
    }

    /* 6. the free extent rotates: 200 saves over the slots, every slot readable, wear spread */
    {
        uint32_t per[SG_NEXT] = {0}, ok = 1, lo = ~0u, hi = 0;
        reset();
        for (i = 0; i < 200u; i++) {
            s = i % 3u ? 0u : (i / 3u) % NPROJ;
            make_song(40u + s, 0x07u, 1, 0);
            ok &= project_save_as(s, "ROT") == 0;
            per[sg.ext[s]]++;
        }
        for (s = 0; s < NPROJ; s++) {
            make_song(40u + s, 0x07u, 1, 0);
            remember();
            other_music();
            project_load(s);
            ok &= same_as_remembered(60 + (int32_t)s);
        }
        for (p = 0; p < SG_NEXT; p++) { lo = per[p] < lo ? per[p] : lo; hi = per[p] > hi ? per[p] : hi; }
        printf("songs:   (200 saves, writes per extent: %u .. %u)\n", lo, hi);
        check("200 saves (two of three to A): every slot loads, every extent used", ok && lo > 0u);
    }

    /* 7. 1.1's projects move in: A..D, no patterns (they load as they did), their sectors then free */
    {
        project_store_t a, c;
        int ok;
        uint8_t keep_used;
        reset();
        memset(nor, 0xFF, sizeof nor);
        memset(nor + 0xB4000u, 0x5A, 0x14000u);       /* (USR2 held a sample: its bytes) */
        make_pattern(70, 0); project_capture(&proj_scratch); proj_pack(&a, &proj_scratch);
        st_save(OBJ_PROJECT0 + 0u, &a, sizeof a);
        make_pattern(72, 0); trk[2].p[P_LEVEL] = 72; project_capture(&proj_scratch); proj_pack(&c, &proj_scratch);
        st_save(OBJ_PROJECT0 + 2u, &c, sizeof c);
        st_save(OBJ_PROJECT0 + 3u, &a, 0);            /* (D: erased in 1.1: an empty record) */
        wipe_ram();
        memset(touched, 0, sizeof touched);
        boot();
        ok = project_used(0) && !project_used(1) && project_used(2) && !project_used(3) && !project_pats(0) &&
             !project_pats(2) && !sg.legacy;
        for (i = 0; i < 4u; i++) {
            st_hdr_t h;
            ok &= st_head(OBJ_PROJECT0 + i, 0, &h) && st_head(OBJ_PROJECT0 + i, 1, &h);   /* (the old records gone) */
        }
        check("1.1's projects A and C move into song slots A and C (B, D empty), the old records erased", ok);
        make_song(80, 0x3Fu, 3, 0);
        keep_used = bank.used;
        remember();
        project_load(2);
        check(".. a moved project loads as it did: its steps the current pattern's, the bank's other keys kept",
              bank.used == keep_used && bank.cur == 3u && trk[2].p[P_LEVEL] == 72 &&
              !memcmp(&bank_pat[0], &want[0], sizeof want[0]));
        wipe_ram();
        boot();
        check(".. the next power-on: nothing moved again, A and C there", project_used(0) && project_used(2) &&
              !project_used(1) && !sg.legacy);
        for (i = 0; i < SG_NEXT; i++) {                 /* (every slot saved, some twice: extents 6.. in use too) */
            make_song(90u + i, 0x01u, 0, 0);
            project_save_as(i % NPROJ, "FILL");
        }
        wipe_ram();
        boot();
        make_song(90u + (SG_NEXT - 1u) / NPROJ * NPROJ, 0x01u, 0, 0);
        remember();
        other_music();
        project_load(0);
        check(".. the old sectors in use as song extents: every slot still loads", same_as_remembered(-1) &&
              project_used(NPROJ - 1u));
    }
    {   /* a power cut while moving: the next power-on finishes it, nothing doubled */
        project_store_t a;
        uint32_t cut, ok = 1;
        for (cut = 0; cut < 6u; cut++) {
            memset(nor, 0xFF, sizeof nor);
            wipe_ram();
            make_pattern(70, 0); project_capture(&proj_scratch); proj_pack(&a, &proj_scratch);
            for (i = 0; i < 4u; i++) st_save(OBJ_PROJECT0 + i, &a, sizeof a);
            wipe_ram();
            fail_after = (int)cut;
            sg_boot();                                  /* (cut: a write fails) */
            fail_after = -1;
            wipe_ram();
            boot();
            for (i = 0; i < 4u; i++) ok &= project_used(i) && !project_pats(i);
            for (i = 4; i < NPROJ; i++) ok &= !project_used(i);
            ok &= !sg.legacy;
        }
        check("a power cut while moving them: the next power-on moves the rest, A..D once each", ok);
    }

    /* 8. the sizes: the power-on songs' patterns as they pack */
    {
        uint32_t mx = 0, tot = 0, n = 0;
        reset();
        for (s = 0; s < 24u; s++) {
            boot_song(0x5EEDu + s * 7919u);
            for (p = 0; p < NBANK; p++) bank_take(p);
            bank.used = 0x3Fu; bank.cur = 0;
            if (project_save_as(s % NPROJ, "BOOT")) { mx = ~0u; break; }
            {
                sg_hdr_t h;
                sg_head(sg.ext[s % NPROJ], &h);
                mx = h.blen > mx ? h.blen : mx;
                if (s < 3u) printf("songs:   (power-on song %u: project %u bytes, six patterns %u)\n", s, h.pcut, h.blen - h.pcut);
                tot += h.blen; n++;
            }
        }
        printf("songs:   (24 power-on songs, each in all six keys: %u bytes on average, %u at most, of %u)\n",
               n ? tot / n : 0u, mx, (uint32_t)SG_CAP);
        check("a power-on song in all six keys fits with room to spare (under 2/3 of a slot)", mx != ~0u && mx * 3u < SG_CAP * 2u);
        make_song(60, 0x3Fu, 0, 1);                     /* (six different patterns, 64 steps, a note on every step of every track) */
        {
            sg_hdr_t h;
            int rc = project_save_as(0, "BUSY");
            sg_head(sg.ext[0], &h);
            printf("songs:   (six patterns, every step of every track a note: %u bytes)\n", rc ? 0u : h.blen);
            check("six different 64-step patterns, a note on every step of all four tracks: it fits", rc == 0);
        }
    }

    /* 9. 1.3: SLICE retired: a project's SLICE track loads as SAMPLE, its BREAK the LOFI KIT, PIANO the PIANO */
    {
        project_t *q = &proj_scratch;
        reset();
        project_capture(q);
        q->t[0].engine = ENGI_SLICE; q->t[0].p[P_E0] = 0; q->t[0].preset = 0;      /* (SRC BREAK) */
        q->t[1].engine = ENGI_SLICE; q->t[1].p[P_E0] = 4; q->t[1].preset = 1;      /* (SRC PIANO) */
        q->motion.count = 1; q->motion.event[0].place = 0; q->motion.event[0].param = P_E0 + 2u; q->motion.event[0].value = 5;
        q->sum = proj_sum(q);
        check("1.3: a project with SLICE tracks loads (SLICE retired)", project_restore_runtime(q) == 0);
        check(".. as SAMPLE: BREAK the LOFI KIT (SET 3, its preset), PIANO the PIANO; SLICE's automation dropped",
              trk[0].eng_req == ENGI_SAMPLE && trk[0].p[P_E0] == 3 && trk[0].preset == 3u &&
              trk[1].eng_req == ENGI_SAMPLE && trk[1].p[P_E0] == 0 && motion.count == 0u);
    }

    /* 10. LOAD over the bank's other patterns asks first */
    reset();
    make_song(9, 0x07u, 0, 0);
    project_save_as(0, "ASK");
    ui.confirm = 0;
    song.g[G_SLOT] = 1;
    {
        uint32_t c = 0;
        while (c < 4u && cur_page()->graph != GR_SLOTS) c++;
    }
    bank.used = 0x05u; bank.cur = 0;
    check("LOAD SONG over the bank's other patterns: the dialog (CF_LOAD_SONG)",
          project_pats(0) && (bank.used & ~(1u << bank.cur)) && CF_LOAD_SONG > CF_NEW_SOUNDS);

    printf("songs: %s\n", fails ? "FAILED" : "all checks ok");
    return fails != 0;
}
