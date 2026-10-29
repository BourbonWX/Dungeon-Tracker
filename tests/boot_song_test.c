/* SPDX-License-Identifier: GPL-3.0-only
 * Dungeon Tracker: the power-on song (firmware/src/boot_song.c) through ui_test.c's harness: the real UI, engines, mix.
 *   build/host/boot_song_test [OUT_DIR]    (tests/run_rondo.sh; OUT_DIR: WAVs of four power-ons, 8 bars each)
 * 1. 300 seeds: every track has steps, T1 an FMKIT kit (lane hits), T2..T4 FMTONE or CHIP; the tempo 96..128; LEN 16 /
 *    64; the notes in their registers; T2's chords three notes; the lead inside its range.
 * 2. they differ: at least 280 of the 300 songs are distinct (no two power-ons alike, in practice).
 * 3. the notes of T2..T4 belong to one seven-note scale (one key) per song.
 * 4. the dungeon: the four adventurers join (dg_party_n) at a level above 1; HOME says PRESS PLAY until PLAY.
 * 5. settled: the autosave sees the power-on state (nothing to write), pattern A's copy is the song and nothing is
 *    dirty in the bank.
 * 6. heard: 50 songs, 8 bars each: never full scale, never silent for a bar, no voice left after the stop.
 * 7. wanted only with no music of the user's: an autosave restored or a stored pattern keeps it from running. */
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
    boot_song(seed);
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
    uint32_t s, k, i, bad = 0, distinct = 0, offkey = 0;
    char *badwhy = what;

    /* 1., 2., 3. */
    for (s = 0; s < 300u; s++) {
        uint16_t pcs = 0;
        uint32_t steps[NTRK] = {0};
        power_on_song(0x9E3779B9u * (s + 1u));
        seen[s] = song_hash();
        for (k = 0; k < NTRK; k++) {
            track_t *t = &trk[k];
            for (i = 0; i < (uint32_t)t->p[P_SLEN]; i++) {
                const step_t *st = &t->step[i];
                uint32_t j;
                if (st->time != ST_NOTE)
                    continue;
                steps[k]++;
                if (k == 0u) {
                    bad += st->n != 0u || !st->hit;
                    continue;
                }
                bad += st->n != (k == 1u ? 3u : 1u);
                for (j = 0; j < st->n; j++) {
                    uint32_t n = st->note[j];
                    bad += k == 1u ? n < 48u || n > 92u : k == 2u ? n < 28u || n > 64u : n < 60u || n > 100u;
                    pcs |= (uint16_t)(1u << (n % 12u));
                }
            }
            bad += !steps[k];
            bad += k == 0u ? t->eng_req != ENGI_CKIT || t->p[P_SLEN] != 16 :
                   (t->eng_req != ENGI_CTONE && t->eng_req != ENGI_LOFI) || t->p[P_SLEN] != 64;
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
        }
    }
    for (s = 0; s < 300u; s++) {
        uint32_t j, dup = 0;
        for (j = 0; j < s; j++)
            dup |= seen[j] == seen[s];
        distinct += !dup;
    }
    snprintf(badwhy, sizeof what, "300 power-ons: every track has steps in its register, T1 a kit, the rest FMTONE / CHIP, "
             "96..128 BPM (%u off)", bad);
    okb(what, !bad);
    snprintf(what, sizeof what, "  they differ: %u of 300 distinct", distinct);
    okb(what, distinct >= 280u);
    snprintf(what, sizeof what, "  one key each: T2..T4's notes in a seven-note scale (%u songs off)", offkey);
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

    /* 7. wanted only without the user's music */
    okb("wanted: nothing restored, no stored pattern", (bank_flash_used = 0, boot_song_wanted(0)));
    okb("  not after an autosave was restored", !boot_song_wanted(1));
    bank_flash_used = 4;
    okb("  not with a pattern stored in the bank", !boot_song_wanted(0));
    bank_flash_used = 0;

    if (argc > 1) {                                       /* four power-ons as WAVs */
        uint32_t d;
        for (d = 0; d < 4u; d++) {
            char path[512];
            FILE *w;
            int32_t o[2 * CTL];
            uint32_t n, total;
            power_on_song(0xD06u + d * 104729u);
            total = FS * 60u / (uint32_t)song.g[G_BPM] * 4u * 8u;
            snprintf(path, sizeof path, "%s/power_on_%u.wav", argv[1], d + 1u);
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
