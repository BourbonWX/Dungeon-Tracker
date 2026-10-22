/* SPDX-License-Identifier: GPL-3.0-only
 * RE-1: a chiptune demo of the CHIP engine (Felucca-Engines eng_lofi.c, Felucca's LOFI) through ui_test.c's harness: the real
 * engines, sequencer and mix. T1 FMKIT, T2 CHIP ARP 8BIT (its arpeggio plays the chords), T3 CHIP WAVE BASS,
 * T4 CHIP PULSE LD, then STEP LEAD for the second half.
 *   build/host/chip_demo OUT.wav */
#define UI_TEST_NO_MAIN 1
#include "ui_test.c"

static void note(step_t *s, uint8_t n, uint8_t v)
{
    step_clear(s);
    s->time = ST_NOTE;
    s->n = 1;
    s->note[0] = n;
    s->vel = v;
}
static void preset_named(track_t *t, const char *nm)
{
    const engine_t *e = ENGINES[t->eng_req % NENGINES];
    uint32_t i;
    for (i = 0; i < e->npresets && !str_eq(e->presets[i].name, nm); i++)
        ;
    apply_preset_to(t, i < e->npresets ? i : 0u);
}
static void rest(step_t *s) { step_clear(s); s->time = ST_REST; }

int main(int argc, char **argv)
{
    static const uint8_t BASS[32] = {45, 0, 57, 45, 0, 45, 57, 0, 41, 0, 53, 41, 0, 41, 53, 0,
                                     48, 0, 60, 48, 0, 48, 60, 0, 43, 0, 55, 43, 0, 43, 55, 47};
    static const uint8_t LEAD[32] = {81, 0, 84, 0, 88, 0, 86, 84, 0, 81, 0, 79, 81, 0, 0, 0,
                                     84, 0, 83, 0, 79, 0, 76, 0, 79, 0, 83, 0, 81, 0, 0, 0};
    static const uint8_t CH[4] = {57, 53, 60, 55};          /* the arp's roots: Am F C G (MIN, MAJ, MAJ, MAJ) */
    const char *out = argc > 1 ? argv[1] : "chip_demo.wav";
    FILE *w = fopen(out, "wb");
    uint32_t i, sec, done = 0, total;
    int32_t o[2 * CTL];
    if (!w) return 1;
    ui_power_on();
    song.master_q12 = 4096;
    song.g[G_BPM] = 140;
    set_engine_of(&trk[1], 3); preset_named(&trk[1], "ARP 8BIT");
    set_engine_of(&trk[2], 3); preset_named(&trk[2], "WAVE BASS");
    set_engine_of(&trk[3], 3); preset_named(&trk[3], "PULSE LD");
    {   /* T1: the kit, a busy chip beat */
        track_t *t = &trk[0];
        track_defaults_steps(t);
        t->p[P_SLEN] = 16;
        for (i = 0; i < 16u; i++) {
            step_t *s = &t->step[i];
            step_clear(s);
            s->hit = (uint8_t)((i % 8u == 0u || i == 10u ? 1u : 0u) | (i % 8u == 4u ? 2u : 0u) | (i % 2u == 0u ? 4u : 0u));
            s->time = s->hit ? ST_NOTE : ST_REST;
        }
    }
    {   /* T2: one arp note a bar (the arp makes the chord), held */
        track_t *t = &trk[1];
        track_defaults_steps(t);
        t->p[P_SLEN] = 64;
        for (i = 0; i < 64u; i++) {
            if (i % 16u == 0u) note(&t->step[i], CH[i / 16u], 90);
            else { step_clear(&t->step[i]); t->step[i].time = i % 16u < 15u ? ST_TIE : ST_REST; }
        }
        t->p[P_E0 + 6] = 2;                                   /* ARP MAJ, and MIN on the A minor bar (automation) */
        motion_set_event(t, 0, P_E0 + 6, 3);
        motion_set_event(t, 16, P_E0 + 6, 2);
    }
    {   /* T3, T4 */
        track_t *b = &trk[2], *l = &trk[3];
        track_defaults_steps(b);
        track_defaults_steps(l);
        b->p[P_SLEN] = 32;
        l->p[P_SLEN] = 32;
        for (i = 0; i < 32u; i++) {
            if (BASS[i]) note(&b->step[i], BASS[i], 105); else rest(&b->step[i]);
            if (LEAD[i]) note(&l->step[i], LEAD[i], 100);
            else { step_clear(&l->step[i]); l->step[i].time = i && LEAD[i - 1u] ? ST_TIE : ST_REST; }
        }
    }
    total = FS * 60u / 140u * 4u * 16u;                       /* 16 bars */
    wav_hdr(w, total + FS + CTL);
    mix_block(o, CTL);                                        /* (the engine switches, on the audio side as on the device) */
    for (i = 0; i < CTL; i++) wav_put(w, o[2 * i], o[2 * i + 1]);
    seq_start();
    song.playing = 1;
    for (sec = 0; sec < 2u; sec++) {
        uint32_t end = total / 2u * (sec + 1u);
        if (sec == 1u) preset_named(&trk[3], "STEP LEAD");
        while (done < end) {
            uint32_t n = end - done < CTL ? end - done : CTL, k;
            mix_block(o, n);
            for (k = 0; k < n; k++) wav_put(w, o[2 * k], o[2 * k + 1]);
            done += n;
        }
    }
    seq_stop();
    song.playing = 0;
    for (done = 0; done < FS; ) {
        uint32_t k;
        mix_block(o, CTL);
        for (k = 0; k < CTL; k++) wav_put(w, o[2 * k], o[2 * k + 1]);
        done += CTL;
    }
    fclose(w);
    printf("chip_demo: %s\n", out);
    return 0;
}
