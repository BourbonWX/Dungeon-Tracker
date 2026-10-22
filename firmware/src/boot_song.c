/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright (C) 2026 Dungeon Tracker contributors. Built on Felucca (C) 2026 Leo Kuroshita (@kurogedelic), Hügelton Instruments */
/* Dungeon Tracker: a song of its own at power-on (main.c, web/emu felucca_web.c), so the dungeon on HOME has a party to
 * walk with the moment PLAY is pressed. Only when there is no music of the user's to bring back: no autosave restored
 * and no pattern stored in the PATTERN BANK; the song is then not saved anywhere (the autosave and the bank take it
 * as the power-on state: nothing is written until it is edited), so each such power-on rolls a new one, and the first
 * edit makes it the user's (restored from then on, as any music).
 * From one seed (boot_entropy): a tempo, a key, a mode (minor, dorian, major) and a four-bar chord progression, then
 *   T1 one of the FMKIT kits, a one-bar beat (kick, backbeat snare, hats 8ths or 16ths, now and then an open hat or a
 *      tom), accents on the beats;
 *   T2 chords (FMTONE pads and keys, or CHIP's WAVE ORGAN / VOX CHOIR) held for the bar or stabbed;
 *   T3 a bass line on each chord's root (FMTONE basses, or CHIP's TRI BASS / PULSE BASS) from a few grooves;
 *   T4 a lead (FMTONE leads, or CHIP's square and step leads): a two-bar phrase on the scale, chord tones on the beats,
 *      played twice with its ending varied.
 * Stopped: it waits for PLAY (HOME says so). Nothing here runs in the audio ISR; the sounds are switched as any load */

static uint32_t bs_rng;
static uint32_t bs_rand(void)
{
    uint32_t s = bs_rng ? bs_rng : 0x2545F491u;
    s ^= s << 13;
    s ^= s >> 17;
    s ^= s << 5;
    return bs_rng = s;
}
static uint32_t bs_in(uint32_t n) { return n ? bs_rand() % n : 0u; }   /* 0..n-1 */
static int bs_chance(uint32_t pct) { return bs_in(100) < pct; }
static int32_t bs_abs(int32_t v) { return v < 0 ? -v : v; }   /* (no abs() in the firmware's libc) */

static void bs_note(step_t *s, const uint8_t *n, uint32_t k, uint32_t vel, uint32_t acc)
{
    uint32_t i;
    step_clear(s);
    s->time = ST_NOTE;
    s->n = (uint8_t)k;
    for (i = 0; i < k && i < 4u; i++)
        s->note[i] = n[i];
    s->vel = (uint8_t)vel;
    if (acc)
        s->flags |= SF_ACCENT;
}
static void bs_tie(step_t *s) { step_clear(s); s->time = ST_TIE; }

/* the scale's degree d (any, wrapping by octaves) above the tonic t */
static int32_t bs_deg(const int8_t *sc, int32_t t, int32_t d)
{
    int32_t o = d >= 0 ? d / 7 : -((-d + 6) / 7);
    return t + 12 * o + sc[d - 7 * o];
}

static void boot_song(uint32_t seed);
/* after it: the autosave and the PATTERN BANK take it as the power-on state (nothing written until it is edited) */
static void boot_song_settle(void)
{
#if RONDO_LAYOUT
    fm1_irq_off();
    (void)bank_keep();                                    /* (pattern A's copy: the song) */
    fm1_irq_on();
    bank.dirty = 0;
#endif
    as.saved = as.seen = autosave_sig();
    as.t = fm1_ms;
}
/* power-on: no music of the user's (no autosave restored, no pattern in the bank's flash) */
static int boot_song_wanted(int restored)
{
#if RONDO_LAYOUT
    return !restored && !bank_flash_used;
#else
    (void)restored;
    return 0;                                             /* (Felucca's behaviour: the sequencers empty) */
#endif
}
static void boot_song(uint32_t seed)
{
    static const int8_t MODE[3][7] = {{0, 2, 3, 5, 7, 8, 10}, {0, 2, 3, 5, 7, 9, 10}, {0, 2, 4, 5, 7, 9, 11}};
    static const int8_t PROG[3][4][4] = {                 /* scale degrees, a bar each */
        {{0, 5, 2, 6}, {0, 3, 4, 0}, {0, 6, 5, 6}, {0, 5, 3, 4}},   /* minor */
        {{0, 3, 0, 6}, {0, 6, 3, 0}, {0, 3, 6, 4}, {0, 1, 3, 0}},   /* dorian */
        {{0, 4, 5, 3}, {0, 5, 3, 4}, {0, 3, 0, 4}, {5, 3, 0, 4}},   /* major */
    };
    static const uint8_t BASS_GROOVE[4][16] = {           /* 1 the root, 2 its octave, 3 the fifth; 0 rest */
        {1, 0, 0, 1, 0, 0, 1, 0, 1, 0, 0, 1, 0, 0, 2, 0},
        {1, 0, 1, 0, 1, 0, 1, 0, 1, 0, 1, 0, 1, 0, 2, 0},
        {1, 0, 0, 0, 0, 0, 2, 0, 1, 0, 0, 0, 3, 0, 0, 0},
        {1, 1, 0, 1, 0, 2, 0, 1, 0, 1, 0, 3, 0, 2, 1, 0},
    };
    static const uint8_t LEAD_RHYTHM[4][16] = {           /* 1 a note, 2 held on (a tie); 0 rest */
        {1, 0, 1, 0, 1, 2, 0, 1, 1, 0, 1, 0, 1, 2, 2, 0},
        {1, 2, 2, 1, 0, 1, 1, 0, 1, 2, 0, 1, 1, 0, 0, 0},
        {1, 0, 0, 1, 0, 0, 1, 0, 1, 0, 1, 1, 1, 2, 2, 2},
        {1, 1, 0, 1, 1, 0, 1, 0, 1, 2, 2, 0, 1, 0, 1, 0},
    };
    static const uint8_t PAD_TONE[] = {0, 1, 3, 4, 2}, BASS_TONE[] = {5, 6, 7}, LEAD_TONE[] = {8, 9, 10, 11};
    static const uint8_t PAD_CHIP[] = {14, 16}, BASS_CHIP[] = {6, 7}, LEAD_CHIP[] = {5, 4, 0, 15};
    uint32_t mode, pr, i, k, bar, groove, rhythm, stab;
    int32_t tonic, prev;
    const int8_t *sc;
    track_t *t;
    bs_rng = seed | 1u;
    undo_depth++;                                         /* (no undo copy of the power-on music) */
    song.g[G_BPM] = (int16_t)(96 + 2 * bs_in(17));        /* 96..128 */
    mode = bs_in(10) < 5u ? 0u : bs_in(2) ? 1u : 2u;      /* minor half the time, dorian or major */
    sc = MODE[mode];
    pr = bs_in(4);
    tonic = 45 + (int32_t)bs_in(12);                      /* the chords' tonic: A2..G#3 (+12: their register) */

    /* T1: a kit and its beat (the lanes' hits: 1 kick, 2 snare, 8 closed hat, 16 open hat, 32 tom) */
    t = &trk[0];
    set_engine_of(t, ENGI_CKIT);
    apply_preset_to(t, bs_in(6));
    track_defaults_steps(t);
    {
        uint32_t sixteen = bs_chance(40), kicks = bs_in(4);
        static const uint16_t KICK[4] = {0x0101, 0x0121, 0x0105, 0x2101};   /* bit i: step i (beats 1 and 3, plus) */
        for (i = 0; i < 16u; i++) {
            step_t *s = &t->step[i];
            uint32_t hit = 0;
            if ((KICK[kicks] >> i) & 1u) hit |= 1u;
            if (i == 4u || i == 12u) hit |= 2u;
            if (sixteen || !(i & 1u)) hit |= 8u;
            if (i == 14u && bs_chance(50)) hit = (hit & ~8u) | 16u;
            if (i == 15u && bs_chance(25)) hit |= 32u;
            if (hit) {
                s->time = ST_NOTE;
                s->hit = (uint8_t)hit;
                s->acc = (uint8_t)(i % 4u == 0u ? hit & 3u : 0u);
            }
        }
        t->p[P_SLEN] = 16;
    }

    /* T2: the chords, a bar each (64 steps): held most of the bar, or stabbed on 1, 2-and, 4 */
    t = &trk[1];
    if (bs_chance(20)) {
        set_engine_of(t, ENGI_LOFI);
        apply_preset_to(t, PAD_CHIP[bs_in(NELEM(PAD_CHIP))]);
    } else {
        set_engine_of(t, ENGI_CTONE);
        apply_preset_to(t, PAD_TONE[bs_in(NELEM(PAD_TONE))]);
    }
    track_defaults_steps(t);
    stab = t->eng_req == ENGI_CTONE && t->preset == 2u;   /* (STAB: stabs) */
    for (bar = 0; bar < 4u; bar++) {
        int32_t d = PROG[mode][pr][bar];
        uint8_t ch[3];
        for (k = 0; k < 3u; k++)
            ch[k] = (uint8_t)clamp(bs_deg(sc, tonic + 12, d + 2 * (int32_t)k), 36, 96);
        for (i = 0; i < 16u; i++) {
            step_t *s = &t->step[bar * 16u + i];
            if (stab ? (i == 0u || i == 6u || i == 12u) : i == 0u)
                bs_note(s, ch, 3, 92, 0);
            else if (!stab && i < 14u)
                bs_tie(s);
            else if (stab && (i == 1u || i == 7u || i == 13u))
                bs_tie(s);
        }
    }
    t->p[P_SLEN] = 64;

    /* T3: the bass, on each bar's chord root (64 steps) */
    t = &trk[2];
    if (bs_chance(25)) {
        set_engine_of(t, ENGI_LOFI);
        apply_preset_to(t, BASS_CHIP[bs_in(NELEM(BASS_CHIP))]);
    } else {
        set_engine_of(t, ENGI_CTONE);
        apply_preset_to(t, BASS_TONE[bs_in(NELEM(BASS_TONE))]);
    }
    track_defaults_steps(t);
    groove = bs_in(4);
    for (bar = 0; bar < 4u; bar++) {
        int32_t root = bs_deg(sc, tonic - 12, PROG[mode][pr][bar]);
        while (root > 50) root -= 12;                    /* (the bass's register: E1..D3) */
        while (root < 28) root += 12;
        for (i = 0; i < 16u; i++) {
            uint32_t g = BASS_GROOVE[groove][i];
            uint8_t n = (uint8_t)(g == 2u ? root + 12 : g == 3u ? root + 7 : root);
            if (g)
                bs_note(&t->step[bar * 16u + i], &n, 1, 100, i % 4u == 0u);
        }
    }
    t->p[P_SLEN] = 64;

    /* T4: the lead, a two-bar phrase played twice (its last half bar varied the second time) */
    t = &trk[3];
    if (bs_chance(30)) {
        set_engine_of(t, ENGI_LOFI);
        apply_preset_to(t, LEAD_CHIP[bs_in(NELEM(LEAD_CHIP))]);
    } else {
        set_engine_of(t, ENGI_CTONE);
        apply_preset_to(t, LEAD_TONE[bs_in(NELEM(LEAD_TONE))]);
    }
    track_defaults_steps(t);
    rhythm = bs_in(4);
    prev = 7 + (int32_t)bs_in(3);                         /* (degrees above the tonic, an octave up: the lead's register) */
    for (bar = 0; bar < 4u; bar++) {
        int32_t chord = PROG[mode][pr][bar];
        for (i = 0; i < 16u; i++) {
            step_t *s = &t->step[bar * 16u + i];
            uint32_t r = LEAD_RHYTHM[(rhythm + (bar & 1u)) % 4u][i];
            if (bar >= 2u && !(bar == 3u && i >= 8u)) {   /* the repeat: bars 3, 4 as 1, 2 (but the ending) */
                *s = t->step[(bar - 2u) * 16u + i];
                continue;
            }
            if (r == 1u) {
                int32_t d;
                uint8_t n;
                if (i % 4u == 0u) {                       /* on the beat: a chord tone near the last note */
                    int32_t best = chord + 7, j;
                    for (j = chord; j <= chord + 14; j += 2)
                        if (bs_abs(j - prev) < bs_abs(best - prev))
                            best = j;
                    d = best;
                } else {                                  /* between: a step or two along the scale */
                    d = prev + (int32_t)bs_in(5) - 2;
                }
                d = clamp(d, 5, 16);
                n = (uint8_t)bs_deg(sc, tonic + 12, d);
                bs_note(s, &n, 1, i % 4u == 0u ? 104 : 88, 0);
                prev = d;
            } else if (r == 2u) {
                bs_tie(s);
            }
        }
    }
    t->p[P_SLEN] = 64;

    for (i = 0; i < NTRK; i++) {
        pat_sig[i] = steps_sig(&trk[i]);                  /* (a power-on pattern, not the user's) */
        pat_last[i] = 0;
    }
    undo_depth--;
    song.sel = 0;
    ui.force = 1;
}
