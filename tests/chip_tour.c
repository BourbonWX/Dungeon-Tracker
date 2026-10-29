/* SPDX-License-Identifier: GPL-3.0-only
 * RE-1: audio tours of the CHIP engine (Felucca's LOFI) through ui_test.c's harness (the real engines, sequencer-free, the mix).
 *   build/host/chip_tour presets OUT.wav    each of RE-1's chip presets on T4, two bars of a phrase that suits it
 *   build/host/chip_tour dice ROLE OUT.wav  ROLE 2 chords, 3 bass, 4 lead: that track on CHIP over T1's FMKIT
 *                                           beat, eight DICE rolls (OCT+), a bar each, the phrase the same
 * 120 BPM, 16ths. */
#define UI_TEST_NO_MAIN 1
#include "ui_test.c"

#define STEP_N (FS * 60u / 120u / 4u)

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
static uint32_t preset_index(const char *nm)
{
    const engine_t *e = ENGINES[ENGI_LOFI];
    uint32_t i;
    for (i = 0; i < e->npresets && !str_eq(e->presets[i].name, nm); i++)
        ;
    return i < e->npresets ? i : 0u;
}
static void beat(uint32_t s)                         /* T1 FMKIT: kick, snare, hats */
{
    track_t *t = &trk[0];
    if (s % 4u == 0u || s == 10u) trk_note_on(t, 36, 115);
    if (s % 8u == 4u) trk_note_on(t, 38, 105);
    if (s % 2u == 0u) trk_note_on(t, 42, s % 4u ? 70 : 90);
}
/* one bar of a role's phrase on track t (16 steps; note 0 = rest, the last note let go) */
static void phrase_step(track_t *t, uint32_t role, uint32_t s, uint32_t bar, uint8_t *on)
{
    static const uint8_t BASS[2][16] = {{33, 0, 45, 33, 0, 33, 43, 0, 33, 0, 45, 33, 0, 36, 38, 0},
                                        {29, 0, 41, 29, 0, 29, 41, 0, 31, 0, 43, 31, 0, 31, 35, 0}};
    static const uint8_t LEAD[2][16] = {{69, 0, 72, 0, 76, 0, 74, 72, 0, 0, 69, 0, 67, 0, 69, 0},
                                        {72, 0, 74, 76, 0, 79, 0, 76, 74, 0, 72, 0, 71, 0, 0, 0}};
    static const uint8_t CH[2][3] = {{57, 60, 64}, {53, 57, 60}};
    uint32_t k;
    if (role == 2u) {                                   /* chords: one a bar, held 3/4 */
        if (s == 0u) for (k = 0; k < 3u; k++) trk_note_on(t, CH[bar & 1u][k], 95);
        if (s == 12u) for (k = 0; k < 3u; k++) trk_note_off(t, CH[bar & 1u][k]);
        return;
    }
    {
        const uint8_t *p = role == 3u ? BASS[bar & 1u] : LEAD[bar & 1u];
        if ((p[s] || s == 15u) && *on) { trk_note_off(t, *on); *on = 0; }
        if (p[s]) { trk_note_on(t, p[s], 100); *on = p[s]; }
    }
}

static int tour(const char *out)
{
    /* name, phrase: 2 chords, 3 bass, 4 lead, 5 drum-style hits at note n, 6 one note a beat (arp) */
    static const struct { const char *nm; uint8_t kind, note; } T[] = {
        {"SQUARE 50", 4, 0}, {"TRI BASS", 3, 0}, {"PULSE BASS", 3, 0}, {"CHIP PLUCK", 4, 0}, {"FAST ARP", 6, 57},
        {"LASER ZAP", 5, 84}, {"CHIP KICK", 5, 45}, {"NOISE HAT", 5, 96}, {"NOISE SNR", 5, 84},
        {"WAVE ORGAN", 2, 0}, {"8BIT SAW", 4, 0}, {"VOX CHOIR", 2, 0},
    };
    FILE *w = fopen(out, "wb");
    uint32_t i, bar, s;
    if (!w) return 1;
    wav_hdr(w, NELEM(T) * (2u * 16u * STEP_N + CTL) + FS);
    ui_power_on();
    song.master_q12 = 4096;
    for (i = 0; i < NELEM(T); i++) {
        track_t *t = &trk[3];
        if (getenv("CHIP_SOLO") && !str_eq(getenv("CHIP_SOLO"), T[i].nm))
            continue;
        uint8_t on = 0;
        set_engine_of(t, ENGI_LOFI);
        apply_preset_to(t, preset_index(T[i].nm));
        mixw(w, CTL);                                   /* (the audio side switches the engine, as on the device) */
        printf("chip_tour: %5.1f s  %s\n", (double)(i * 2u * 16u * STEP_N) / FS, T[i].nm);
        if (getenv("CHIP_DBG")) {
            uint32_t v;
            for (v = 0; v < NVOICE; v++)
                if (t->v[v].active)
                    printf("  voice %u: note %u stage %u env %d gate %u s6 %d s3 %d\n", v, t->v[v].note, t->v[v].stage,
                           (int)t->v[v].env, t->v[v].gate, (int)t->v[v].s[6], (int)t->v[v].s[3]);
        }
        for (bar = 0; bar < 2u; bar++)
            for (s = 0; s < 16u; s++) {
                if (T[i].kind <= 4u)
                    phrase_step(t, T[i].kind, s, bar, &on);
                else if (T[i].kind == 5u) {
                    int hit = str_eq(T[i].nm, "NOISE HAT") ? s % 2u == 0u :
                              str_eq(T[i].nm, "NOISE SNR") ? s % 8u == 4u :
                              str_eq(T[i].nm, "CHIP KICK") ? s % 4u == 0u || s == 10u : s % 8u == 0u || s == 11u;
                    if (hit) {                          /* (a short gate: half a step) */
                        trk_note_on(t, T[i].note, 110);
                        mixw(w, STEP_N / 2u);
                        trk_note_off(t, T[i].note);
                        mixw(w, STEP_N - STEP_N / 2u);
                        continue;
                    }
                } else {
                    static const uint8_t R[2] = {57, 53};
                    if (s == 0u) trk_note_on(t, R[bar], 95);
                    if (s == 14u) trk_note_off(t, R[bar]);
                }
                mixw(w, STEP_N);
            }
        for (s = 0; s < 128u; s++) trk_note_off(t, (uint8_t)s);
    }
    mixw(w, getenv("CHIP_SOLO") ? 4u * FS : FS);
    fclose(w);
    return 0;
}

static int dice_run(uint32_t role, const char *out)
{
    FILE *w = fopen(out, "wb");
    track_t *t = &trk[role - 1u];
    uint32_t r, s;
    uint8_t on = 0;
    if (!w) return 1;
    wav_hdr(w, 8u * 2u * 16u * STEP_N + CTL + FS);
    ui_power_on();
    song.master_q12 = 4096;
    set_engine_of(t, ENGI_LOFI);
    apply_preset_to(t, role == 2u ? 14u : role == 3u ? 6u : 5u);   /* (WAVE ORGAN, TRI BASS, SQUARE 50 to start) */
    mixw(w, CTL);                                       /* (the engine switch, on the audio side) */
    track_select(role - 1u);
    go_page(GR_DICE);
    dice.rng = 0xC41Bu * role + 7u;
    for (r = 0; r < 8u; r++) {
        uint32_t bar;
        uint8_t ids[DICE_VALS];
        dice_roll();
        (void)ids;
        printf("chip_tour: roll %u  CHIP %d WAVE %d DUTY %d CRSH %d SWP %d VIB %d ARP %d TONE %d  ENV %d %d %d %d\n",
               r + 1u, t->p[P_E0], t->p[P_E1], t->p[P_E2], t->p[P_E3], t->p[P_E4], t->p[P_E5], t->p[P_E6], t->p[P_E7],
               t->p[P_ATK], t->p[P_DEC], t->p[P_SUS], t->p[P_REL]);
        for (bar = 0; bar < 2u; bar++)
            for (s = 0; s < 16u; s++) {
                beat(s);
                phrase_step(t, role, s, bar, &on);
                mixw(w, STEP_N);
            }
    }
    for (s = 0; s < 128u; s++) trk_note_off(t, (uint8_t)s);
    mixw(w, FS);
    fclose(w);
    return 0;
}

int main(int argc, char **argv)
{
    if (argc > 2 && str_eq(argv[1], "presets"))
        return tour(argv[2]);
    if (argc > 3 && str_eq(argv[1], "dice"))
        return dice_run((uint32_t)clamp(atoi(argv[2]), 2, 4), argv[3]);
    fprintf(stderr, "chip_tour presets OUT.wav | chip_tour dice ROLE OUT.wav\n");
    return 2;
}
