/* SPDX-License-Identifier: GPL-3.0-only
 * Dungeon Tracker 1.0: MENU > GAME > RANDOM SONG (firmware/src/boot_song.c) through ui_test.c's harness: the real UI,
 * engines, mix. (1.0's first builds rolled such a song at every power-on; now the power-on keeps the default song.)
 *   build/host/boot_song_test [OUT_DIR]    (tests/run_rondo.sh; OUT_DIR: WAVs of four rolls, 8 bars each)
 * 1. 300 rolls: THE PARTY within its points, a ROGUE in it, the tracks in class order; every track a sound of its
 *    class's own list (a ROGUE a kit: FMKIT's or a SAMPLE kit) and a 16-step part with notes in their registers; the
 *    tempo 96..128.
 * 2. they differ: at least 280 of the 300 songs are distinct.
 * 3. the melodic notes belong to one seven-note scale (one key) per song.
 * 4. the dungeon: the four adventurers join, at L1; HOME says PRESS PLAY until PLAY.
 * 5. settled (as the screens use it): the autosave sees it, pattern A's copy is the song, nothing dirty.
 * 6. heard: 50 songs, 8 bars each: never full scale, never silent for a bar, no voice left after the stop.
 * 7. the MENU: GAME's last row RANDOM SONG, OCT+ rolls one (the arrangement and the name cleared, as NEW), SAVE held
 *    undoes it: the song before back whole, its party too. */
#define UI_TEST_NO_MAIN 1
#include "ui_test.c"
#include "../firmware/src/boot_song.c"

static int fails;
static void okb(const char *what, int ok) { fails += check(what, ok); }
static char what[256];

static uint32_t song_hash(void)
{
    uint32_t h = 2166136261u, k;
    for (k = 0; k < NTRK; k++) {
        h = as_mix(h, trk[k].step, sizeof trk[k].step);
        h = as_mix(h, &trk[k].eng_req, 1);
        h = as_mix(h, &trk[k].preset, 1);
    }
    return as_mix(h, &song.g[G_BPM], 2);
}
static void power_on_song(uint32_t seed)
{
    ui_power_on();
    song.master_q12 = 4096;
    random_song_make(seed);
    boot_song_settle();
    frames(2);                                            /* (the engine switches: on the audio side) */
}
static uint32_t sounding(void)
{
    uint32_t k, i, n = 0;
    for (k = 0; k < NTRK; k++)
        for (i = 0; i < NVOICE; i++)
            n += trk[k].v[i].active;
    return n;
}

int main(int argc, char **argv)
{
    static uint32_t seen[300];
    uint32_t s, k, i, bad = 0, distinct = 0, offkey = 0, kits[2] = {0, 0}, engs = 0;
    (void)offkey;
    char *badwhy = what;

    /* 1., 2., 3. */
    for (s = 0; s < 300u; s++) {
        uint16_t pcs = 0;
        uint8_t c[NTRK];
        uint32_t nr = 0, p, any[NTRK] = {0, 0, 0, 0};
        power_on_song(0x9E3779B9u * (s + 1u));
        seen[s] = song_hash();
        for (k = 0; k < NTRK; k++) {
            c[k] = (uint8_t)trk_cls(k);
            nr += CLS_ROLE[c[k]] == ROLE_DRUMS;          /* (1.1: a drummer, a rogue or a monk; the parts' order) */
            bad += k && CLS_ROLE[c[k]] * 8u + c[k] < CLS_ROLE[c[k - 1u]] * 8u + c[k - 1u];
        }
        bad += party_points(c) > CLS_POINTS || !nr;
        bad += bank.used != 0x0Fu || bank.cur != 1u || chain_config.count != 7u || chain_config.row[0].slot[0] != 0u ||
               chain_config.row[2].slot[3] != 2u || chain_config.row[5].slot[1] != 3u;
        for (k = 0; k < NTRK; k++) {
            uint32_t e = trk[k].eng_req % NENGINES, cat = preset_cat(e, trk[k].preset);
            engs |= 1u << e;
            bad += !cls_allows(c[k], cat);
            if (CLS_ROLE[c[k]] == ROLE_DRUMS) {
                bad += cat != CAT_DRUM || !ENG_KIT(e);   /* (1.5: GEN KIT or MOD KIT) */
                kits[e == ENGI_MKIT]++;
            }
            for (p = 0; p < 4u; p++) {
                const step_t *st0 = bank_pat[p].step[k];
                bad += bank_pat[p].timing[k][0] != 32;    /* (P_SLEN) */
                for (i = 0; i < 32u; i++) {
                    const step_t *st = &st0[i];
                    uint32_t j;
                    if (st->time != ST_NOTE)
                        continue;
                    any[k]++;
                    if (CLS_ROLE[c[k]] == ROLE_DRUMS) {
                        bad += ENG_KIT(e) ? st->n != 0u || !st->hit : !st->n || st->hit;
                        continue;
                    }
                    for (j = 0; j < st->n; j++) {
                        uint32_t n = st->note[j];
                        bad += c[k] == CLS_CLERIC ? n < 48u || n > 104u : c[k] == CLS_DRUID ? n < 36u || n > 96u :
                               CLS_ROLE[c[k]] == ROLE_BASS ? n < 16u || n > 64u : c[k] == CLS_BARD ? n < 48u || n > 100u :
                               n < 60u || n > 100u;
                        pcs |= (uint16_t)(1u << (n % 12u));
                    }
                }
            }
            bad += !any[k];
            bad += memcmp(trk[k].step, bank_pat[1].step[k], sizeof trk[k].step) != 0;   /* (the tracks: the verse) */
        }
        bad += song.g[G_BPM] < 96 || song.g[G_BPM] > 128;
        {   /* one key: the pitch classes fit some rotation of a seven-note mode (minor's, its rotations: dorian, major) */
            static const uint16_t NAT = 0x0AD5;            /* C D E F G A B: the diatonic set (every mode here is one) */
            uint32_t r, fit = 0;
            for (r = 0; r < 12u; r++) {
                uint16_t rot = (uint16_t)(((NAT << r) | (NAT >> (12u - r))) & 0xFFFu);
                fit |= (pcs & ~rot) == 0;
            }
            offkey += !fit;
            if (!fit && getenv("BS_DBG")) {
                printf("  off: song %u pcs %03X classes %u %u %u %u\n", s, pcs, c[0], c[1], c[2], c[3]);
                for (k = 0; k < NTRK; k++) for (p = 0; p < 4u; p++) for (i = 0; i < 32u; i++) {
                    const step_t *st = &bank_pat[p].step[k][i];
                    if (st->time == ST_NOTE && CLS_ROLE[c[k]] != ROLE_DRUMS) {
                        uint32_t j;
                        printf("    T%u %c%02u:", k + 1u, 'A' + p, i);
                        for (j = 0; j < st->n; j++) printf(" %u", st->note[j]);
                        printf("\n");
                    }
                }
            }
        }
    }
    for (s = 0; s < 300u; s++) {
        uint32_t j, dup = 0;
        for (j = 0; j < s; j++)
            dup |= seen[j] == seen[s];
        distinct += !dup;
    }
    for (k = 0, i = 0; k < NENGINES; k++)
        i += (engs >> k) & 1u;
    snprintf(badwhy, sizeof what, "300 rolls: the party within 12, a drummer (1.1: rogue or monk), the parts' order; its sounds; PATTERN BANK A..D (intro verse "
             "chorus break, 32 steps) in key and register; SONG A B C B C D C; on B (%u off; %u GEN KIT / %u MOD KIT kits; %u engines)", bad, kits[0], kits[1], i);
    okb(what, !bad && kits[0] && kits[1] && i >= 8u);
    snprintf(what, sizeof what, "  they differ: %u of 300 distinct", distinct);
    okb(what, distinct >= 280u);
    snprintf(what, sizeof what, "  one key each: the melodic notes in a seven-note scale (%u songs off)", offkey);
    okb(what, !offkey);

    /* 4. the dungeon */
    power_on_song(12345u);
    go_home();
    {   /* (the party is counted once a second: dg_party) */
        uint32_t t0 = fm1_ms;
        while (fm1_ms - t0 < 1500u)
            frame();
    }
    {
        uint32_t lv = 9, hi = 0;
        for (k = 0; k < 4u; k++) {
            if (dg.lvl[k] < lv) lv = dg.lvl[k];
            if (dg.lvl[k] > hi) hi = dg.lvl[k];
        }
        snprintf(what, sizeof what, "the dungeon: %u adventurers join, all at L%u (1.0: the party starts at L1)", dg_party_n(), lv);
        okb(what, dg_party_n() == 4u && lv == 1u && hi == 1u);
    }
    okb("  HOME says PRESS PLAY (stopped, never played)", !dg_played && !song.playing);
    seq_start();
    song.playing = 1;
    frames(20);
    song.playing = 0;
    seq_stop();
    frames(5);
    okb("  after PLAY it does not come back", dg_played);

    /* 5. settled */
    power_on_song(777u);
    okb("settled: the autosave sees the power-on state, pattern A's copy is the song, nothing dirty",
        as.saved == autosave_sig() && bank_same(bank.cur) && !bank.dirty);

    /* 6. heard */
    {
        uint32_t over = 0, quiet = 0, hung = 0, n;
        for (s = 0; s < 50u; s++) {
            int32_t o[2 * CTL];
            uint32_t bar, bars = 8, per = FS * 60u / (uint32_t)song.g[G_BPM] * 4u;
            power_on_song(0xC0FFEEu + s * 7919u);
            per = FS * 60u / (uint32_t)song.g[G_BPM] * 4u;
            seq_start();
            song.playing = 1;
            for (bar = 0; bar < bars; bar++) {
                double e = 0;
                int32_t pk = 0;
                for (n = 0; n < per; n += CTL) {
                    mix_block(o, CTL);
                    for (i = 0; i < 2u * CTL; i++) {
                        e += (double)o[i] * o[i];
                        pk = abs(o[i]) > pk ? abs(o[i]) : pk;
                    }
                }
                over += pk >= 32767;
                quiet += bar && e / (2.0 * per) < 100.0 * 100.0;   /* (under -50 dB for a whole bar) */
            }
            song.playing = 0;
            seq_stop();
            for (n = 0; n < FS * 6u; n += CTL)
                mix_block(o, CTL);
            hung += sounding() != 0u;
        }
        snprintf(what, sizeof what, "50 songs heard, 8 bars each: full scale %u, a silent bar %u, voices left after 6 s %u", over, quiet, hung);
        okb(what, !over && !quiet && !hung);
    }

    /* 7. the MENU */
    {
        uint32_t before, rolled, again;
        uint8_t cls_before[NTRK];
        ui_power_on();
        song.playing = 0;
        bank_reset();
        project_save_as(4, "MY SONG");                     /* (a song of the user's, named; the bank: its pattern only) */
        before = song_hash();
        for (k = 0; k < NTRK; k++) cls_before[k] = (uint8_t)trk_cls(k);
        okb("MENU > GAME: RANDOM SONG, the tab's last row, an action (OCT+)", MI_TAB[MI_RANDOM] == MTAB_GAME &&
            mtab_rows(MTAB_GAME) == 7u && str_eq(MI_NAME[MI_RANDOM], "RANDOM SONG") && MI_ACTION(MI_RANDOM) &&
            !menu_valued(MI_RANDOM));
        go_home(); frames(50);
        hold(B_HOME); frames(100);
        ui.menu_sel = MI_RANDOM;
        press(B_OCTUP); frames(50);
        rolled = song_hash();
        okb("  OCT+: a song rolled: patterns A..D, SONG's sections, on B (\"RANDOM SONG\", the name cleared), the MENU up",
            rolled != before && ui.menu && msg_is("RANDOM SONG") && !proj_name[0] && trk[0].p[P_SLEN] == 32 &&
            bank.used == 0x0Fu && bank.cur == 1u && chain_config.count == 7u);
        press(B_OCTUP); frames(50);
        again = song_hash();
        okb("  OCT+ again: another roll at once (nothing changed since the last)", again != rolled && msg_is("RANDOM SONG"));
        hold(B_HOME); frames(100);
        hold(B_SAVE); frames(50);
        for (k = 0, i = 0; k < NTRK; k++) i += trk_cls(k) != cls_before[k];
        okb("  SAVE held: undone, the song before back whole (its party, its pattern in A), the rolled patterns gone",
            song_hash() == before && !i && bank.cur == 0u && bank.used == 1u);
        /* the user's patterns in the bank: the first OCT+ warns, the second rolls */
        {
            uint32_t f = motion_guard();
            bank_take(2);                                  /* (C: a pattern of the user's) */
            motion_unguard(f);
        }
        hold(B_HOME); frames(100);
        ui.menu_sel = MI_RANDOM;
        press(B_OCTUP); frames(50);
        okb("  with patterns of the user's in the bank: the first OCT+ says REPLACES PATTERNS A-F, nothing rolled",
            song_hash() == before && msg_is("REPLACES PATTERNS A-F"));
        press(B_OCTUP); frames(50);
        okb("  .. the second (within 4 s) rolls", song_hash() != before && msg_is("RANDOM SONG") && bank.used == 0x0Fu);
        hold(B_HOME); frames(100);
    }

    if (argc > 1) {                                       /* four rolls as WAVs */
        uint32_t d;
        for (d = 0; d < 4u; d++) {
            char path[512];
            FILE *w;
            int32_t o[2 * CTL];
            uint32_t n, total;
            power_on_song(0xD06u + d * 104729u);
            total = FS * 60u / (uint32_t)song.g[G_BPM] * 4u * 8u;
            snprintf(path, sizeof path, "%s/random_song_%u.wav", argv[1], d + 1u);
            if (!(w = fopen(path, "wb")))
                break;
            wav_hdr(w, total);
            seq_start();
            song.playing = 1;
            for (n = 0; n < total; n += CTL) {
                mix_block(o, CTL);
                for (i = 0; i < CTL; i++)
                    wav_put(w, o[2 * i], o[2 * i + 1]);
            }
            song.playing = 0;
            seq_stop();
            fclose(w);
            printf("boot_song: %s  %d BPM  T1 kit %u  T2 %s %u  T3 %s %u  T4 %s %u\n", path, song.g[G_BPM], trk[0].preset,
                   ENGINES[trk[1].eng_req]->name, trk[1].preset, ENGINES[trk[2].eng_req]->name, trk[2].preset,
                   ENGINES[trk[3].eng_req]->name, trk[3].preset);
        }
    }
    printf("boot_song: %s\n", fails ? "FAILED" : "all checks ok");
    return fails != 0;
}
