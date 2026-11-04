/* SPDX-License-Identifier: GPL-3.0-only
 * 1.5: SOUND MORPH demos between two saved sounds (src/morph.c, MO_USER targets), through ui_test.c's harness: the
 * real dice, the real user presets, the real morph and mix.
 *   build/host/morph_demo OUTDIR
 * Each demo: a track's engine; DICE rolls sound A (saved to a user slot), then rolls B from the same engine and class
 * (of 60 rolls the one that differs most in what MORPH blends; what it keeps, the wave, bank, sample or hits, as
 * A's, as if those dice were held), saved to the next slot. A is loaded, B picked as the TARGET, and a part plays 10 bars at 110 BPM:
 * 2 bars at 0 %, 4 bars sweeping 0 -> 100 %, 2 bars at 100 %, 2 bars back to 0 %.
 * OUTDIR/morph_<n>_<engine>.wav each, morph_all.wav one after the other, morphs.txt: what A and B are */
#define UI_TEST_NO_MAIN 1
#include "ui_test.c"

#define MD_BPM 110u
static uint8_t held[NTRK][8], nheld[NTRK];
static uint32_t step_frames(void) { return FS * 60u / MD_BPM / 4u; }
static void on(uint32_t k, uint32_t n, uint32_t v)
{
    trk_note_on(&trk[k], n, v);
    if (nheld[k] < 8u) held[k][nheld[k]++] = (uint8_t)n;
}
static void off_all(uint32_t k)
{
    while (nheld[k]) trk_note_off(&trk[k], held[k][--nheld[k]]);
}
enum { PT_CHORDS, PT_LEAD, PT_BASS, PT_BEAT, PT_ACID, PT_ARP };
/* D minor: Dm Bb F C */
static const uint8_t CHORD[4][3] = {{62, 65, 69}, {58, 62, 65}, {60, 65, 69}, {60, 64, 67}};
static const uint8_t ROOT[4] = {38, 34, 41, 36};
static void part(uint32_t k, uint32_t pt, uint32_t b, uint32_t s)
{
    uint32_t c = b % 4u, i;
    switch (pt) {
    case PT_CHORDS:                                       /* a chord a bar, held, a pulse on the "and" of 3 */
        if (s == 0u || s == 10u) {
            off_all(k);
            for (i = 0; i < 3u; i++) on(k, CHORD[c][i], s ? 80 : 96);
        }
        if (s == 15u) off_all(k);
        break;
    case PT_LEAD: {                                       /* a two-bar motif, answered */
        static const int8_t MEL[2][16] = {{74, 0, 0, 77, 0, 76, 74, 0, 72, 0, 74, 0, 0, 0, -1, 0},
                                          {69, 0, 0, 72, 0, 74, 0, 72, 70, 0, 69, 0, 67, 0, -1, 0}};
        int32_t n = MEL[b % 2u][s];
        if (n) off_all(k);
        if (n > 0) on(k, (uint32_t)n + ((b / 2u) % 2u ? 0u : 0u), s % 4u ? 92 : 108);
        break;
    }
    case PT_BASS: {                                       /* syncopated, the octave on the off-beats */
        static const uint8_t HIT[16] = {1, 0, 0, 1, 0, 0, 2, 0, 1, 0, 1, 0, 0, 0, 2, 0};
        if (HIT[s]) { off_all(k); on(k, ROOT[c] + (HIT[s] == 2u ? 12u : 0u), s % 4u ? 92 : 112); }
        else if (s % 2u) off_all(k);
        break;
    }
    case PT_ACID: {                                       /* 16ths, accents and octave jumps */
        static const int8_t ST[16] = {0, 0, 12, 0, 3, 0, 15, 0, 0, 7, 0, 10, 12, 0, 3, 5};
        static const uint8_t ACCS[16] = {1, 0, 0, 0, 1, 0, 0, 1, 0, 0, 1, 0, 1, 0, 0, 0};
        off_all(k);
        on(k, (uint32_t)(ROOT[c] + ST[s]), ACCS[s] ? 120 : 84);
        break;
    }
    case PT_ARP: {                                        /* the chord broken up and down in 16ths, an octave up */
        static const uint8_t ORD[8] = {0, 1, 2, 3, 4, 3, 2, 1};
        uint32_t o = ORD[s % 8u], n = CHORD[c][o % 3u] + 12u * (o / 3u);
        off_all(k);
        on(k, n, s % 4u ? 88 : 104);
        break;
    }
    default:                                              /* the beat: kick, snare, hats, an open hat, a perc */
        off_all(k);
        if (s == 0u || s == 7u || s == 10u) on(k, 36, 120);
        if (s == 4u || s == 12u) on(k, 38, 112);
        if (s == 14u) on(k, 46, 90);
        else if (s % 2u == 0u) on(k, 42, s % 4u ? 70 : 92);
        if (s == 11u && b % 2u) on(k, 45, 90);
        break;
    }
}
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
static void quiet_all(void)
{
    uint32_t k;
    for (k = 0; k < NTRK; k++) {
        off_all(k);
        memset(trk[k].v, 0, sizeof trk[k].v);
    }
}

static const struct { uint32_t trk, eng, pt, seed; const char *file; } DEMO[] = {
    {1, ENGI_GENESIS, PT_CHORDS, 0x1A16u, "morph_1_genesis_chords.wav"},
    {3, ENGI_WT32, PT_LEAD, 0x2B22u, "morph_2_wave32_lead.wav"},
    {3, ENGI_MOD, PT_LEAD, 0x3C33u, "morph_3_mod_lead.wav"},
    {0, ENGI_MKIT, PT_BEAT, 0x4D44u, "morph_4_modkit_beat.wav"},
    {2, 0u, PT_ACID, 0x5E55u, "morph_5_analog_acid.wav"},
    {3, ENGI_SID, PT_ARP, 0x6F66u, "morph_6_sid_arp.wav"},
};
#define ND (sizeof DEMO / sizeof DEMO[0])
static FILE *rep;
static void roll(uint32_t k)
{
    track_select(k);
    dice.wild[k] = 0;
    dice_roll();
    trk[k].engine = trk[k].eng_req;
}
/* how far B is from A in what MORPH blends; -1: B differs in what it keeps (wave, bank, sample...) */
static int32_t distance(const track_t *t, const int16_t *a, const int16_t *b)
{
    uint32_t id;
    int32_t d = 0, n = 0;
    for (id = 0; id < P_COUNT; id++) {
        if (param_kept(id) || a[id] == b[id])
            continue;
        if (!morph_ok(t, id))
            return -1;
        d += abs(a[id] - b[id]) > 40 ? 40 : abs(a[id] - b[id]);
        n++;
    }
    return d + n * 4;
}
static void describe(uint32_t k, const int16_t *a, const int16_t *b)
{
    uint32_t id, n = 0;
    for (id = 0; id < P_COUNT; id++) {
        const param_desc_t *d;
        char va[16], vb[16];
        const char *ua, *ub;
        if (param_kept(id) || a[id] == b[id] || !morph_ok(&trk[k], id))
            continue;
        d = track_desc(&trk[k], id);
        param_format(d, a[id], va, &ua);
        param_format(d, b[id], vb, &ub);
        fprintf(rep, "    %-6s %s%s -> %s%s\n", d->label, va, ua ? ua : "", vb, ub ? ub : "");
        n++;
    }
    fprintf(rep, "    (%u values blend)\n", n);
}

int main(int argc, char **argv)
{
    const char *dir = argc > 1 ? argv[1] : "build/morph_demo";
    char path[600], na[13], nb[13];
    uint32_t d, step = step_frames(), bars = 10u, tail = 8u;
    FILE *all;
    static int16_t a[P_COUNT], best[P_COUNT];
    snprintf(path, sizeof path, "%s/morphs.txt", dir);
    if (!(rep = fopen(path, "w"))) { fprintf(stderr, "cannot write %s\n", path); return 1; }
    fprintf(rep, "SOUND MORPH between two saved sounds, D minor, %u BPM: 2 bars at 0 %%, 4 bars 0 -> 100 %%, 2 bars at 100 %%, "
                 "2 bars back to 0 %%\n\n", MD_BPM);
    snprintf(path, sizeof path, "%s/morph_all.wav", dir);
    if (!(all = fopen(path, "wb"))) return 1;
    wav_hdr(all, (uint32_t)ND * (bars * 16u + tail) * step);
    for (d = 0; d < ND; d++) {
        uint32_t k = DEMO[d].trk, sa = 20u + 2u * d, sb = sa + 1u, c, b, s;
        int32_t bd = -1;
        FILE *w;
        ui_power_on();
        song.master_q12 = 4096;
        song.g[G_BPM] = (int16_t)MD_BPM;
        transport_req = 0; song.playing = 0;
        quiet_all();
        set_engine_of(&trk[k], DEMO[d].eng);
        trk[k].engine = trk[k].eng_req;
        if (getenv("MD_ONLY") && (uint32_t)atoi(getenv("MD_ONLY")) != d + 1u) continue;
        dice.rng = getenv("MD_SEED") ? (uint32_t)strtoul(getenv("MD_SEED"), 0, 0) : DEMO[d].seed;
        roll(k);                                          /* A */
        memcpy(a, trk[k].p, sizeof a);
        snprintf(na, sizeof na, "ROLL %u A", d + 1u);
        if (sb >= UP_SLOTS || (up_store(sa, na) & ~3) != 0) { fprintf(stderr, "demo %u: cannot save A\n", d + 1u); return 1; }
        for (c = 0; c < 60u; c++) {                       /* B: the roll furthest from A in what blends */
            int32_t x;
            uint32_t id;
            roll(k);
            for (id = 0; id < P_COUNT; id++)              /* (what MORPH keeps stays A's: as if those dice were held) */
                if (!param_kept(id) && (!morph_ok(&trk[k], id) || track_desc(&trk[k], id)->fmt == F_SEMI))   /* (and a
                                                           * transpose: a sweep through semitones leaves the key) */
                    trk[k].p[id] = a[id];
            x = distance(&trk[k], a, trk[k].p);
            if (x > bd) { bd = x; memcpy(best, trk[k].p, sizeof best); }
        }
        if (bd < 0) { fprintf(stderr, "demo %u: no B like A\n", d + 1u); return 1; }
        memcpy(trk[k].p, best, sizeof best);
        snprintf(nb, sizeof nb, "ROLL %u B", d + 1u);
        if ((up_store(sb, nb) & ~3) != 0) { fprintf(stderr, "demo %u: cannot save B\n", d + 1u); return 1; }
        up_load(sa);
        trk[k].engine = trk[k].eng_req;
        track_select(k);
        morph_set(k, MO_USER + sb, 0);
        morph_poll(); morph_poll();
        fprintf(rep, "%u. %s on T%u: U%02u %s -> U%02u %s\n", d + 1u, ENGINES[trk[k].engine]->name, k + 1u, sa + 1u, na,
                sb + 1u, nb);
        describe(k, a, best);
        fprintf(rep, "\n");
        snprintf(path, sizeof path, "%s/%s", dir, DEMO[d].file);
        if (!(w = fopen(path, "wb"))) return 1;
        wav_hdr(w, (bars * 16u + tail) * step);
        quiet_all();
        for (b = 0; b < bars; b++)
            for (s = 0; s < 16u; s++) {
                uint32_t i = b * 16u + s, amt;
                int16_t o[2 * CTL];
                amt = b < 2u ? 0u : b < 6u ? (i - 32u) * 100u / 63u : b < 8u ? 100u : 100u - (i - 128u) * 100u / 31u;
                if (getenv("MD_STATIC")) amt = (uint32_t)atoi(getenv("MD_STATIC"));
                morph_set(k, MO_USER + sb, amt);
                morph_poll();
                if (getenv("MD_DBG") && s == 0u)
                    printf("  d%u bar %u amt %u: tgt %u on %u eng %u/%u P_E4 %d (a %d b %d) SUS %d\n", d + 1u, b, amt, mo.tgt[k],
                           mo.on[k], mo.eng[k], trk[k].eng_req, trk[k].p[P_E4], mo.a[k][P_E4], mo.b[k][P_E4], trk[k].p[P_SUS]);
                part(k, DEMO[d].pt, b, s);
                {
                    uint32_t f, j;
                    int32_t blk[2 * CTL];
                    for (f = 0; f < step; f += CTL) {
                        mix_block(blk, CTL);
                        for (j = 0; j < CTL; j++) {
                            wav_put(w, blk[2 * j], blk[2 * j + 1]);
                            wav_put(all, blk[2 * j], blk[2 * j + 1]);
                        }
                    }
                }
                (void)o;
            }
        off_all(k);
        {
            uint32_t f, j;
            int32_t blk[2 * CTL];
            for (f = 0; f < tail * step; f += CTL) {
                mix_block(blk, CTL);
                for (j = 0; j < CTL; j++) {
                    wav_put(w, blk[2 * j], blk[2 * j + 1]);
                    wav_put(all, blk[2 * j], blk[2 * j + 1]);
                }
            }
        }
        fclose(w);
        printf("morph_demo: %s (%s, B %d away)\n", DEMO[d].file, ENGINES[trk[k].engine]->name, bd);
    }
    fclose(all);
    fclose(rep);
    (void)mixw;
    return 0;
}
