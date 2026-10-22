/* SPDX-License-Identifier: GPL-3.0-only */
/* RONDO: the PATTERN BANK, QY70 style. Six patterns A..F, each the four tracks' steps (CHANCE RATCH NUDGE in them),
 * their LEN DIV SWING GATE and QUANTIZE, and the automation, locks and TRACKER commands (the whole motion store); the
 * sounds are the tracks' own, shared by every pattern (as SONG's sections always were).
 *   the current pattern (bank.cur): the tracks' own steps and motion, the one played and edited (so every page edits
 *     it); its copy in bank_pat[] is as of its last store: a switch away, a song played, the idle store (project.c)
 *   ENV held (ui_layer.c LAYER_ENV): F3 G3 A3 B3 C4 D4 are A..F. A key with a pattern: it next. Stopped, at once;
 *     playing, at the end of this pattern (bank_tick: the end of the loop of its longest track, bank_clock), the new
 *     one from its step 1 with all four tracks together; pressed again (or the current one's key): back to staying.
 *     An empty key: the current pattern copied into it, and it is the current one from then on (A done, B starts as
 *     A). OCT+ held: the current one copied into the key, staying here; OCT- held: the key cleared (not the current
 *     one's); OCT- tapped alone: the next one forgotten
 *   the flash: each pattern its own A/B object pair in the top of user sample slot 3 (storage.c), written stopped and
 *     idle (project.c bank_poll; an erase silences the audio); which pattern was current rides in the autosave
 *   SONG (song_chain.c): a section's tracks name these patterns A..F (chain_pat); PLAY SONG stores the current one
 *     first (project.c chain_prepare)
 * The ISR switches (bank_tick, bank_start); the main loop stores, queues and clears with the interrupts off. */
#if RONDO_LAYOUT
static uint32_t step_samples(const track_t *t, uint32_t period, uint32_t idx);

/* the tracks' pattern as it is now == pattern p's copy (no store needed) */
static int bank_same(uint32_t p)
{
    const chain_pattern_t *c = &bank_pat[p % NBANK];
    uint32_t k;
    for (k = 0; k < NTRK; k++) {
        const track_t *t = &trk[k];
        if (memcmp(c->step[k], t->step, sizeof c->step[k]) || memcmp(c->timing[k], &t->p[P_SLEN], sizeof c->timing[k]) ||
            c->qnt[k] != (t->p[P_SQNT] != 0))
            return 0;
    }
    return !memcmp(&c->motion, &motion, sizeof motion);
}
/* the tracks' pattern -> pattern p's copy (interrupts off, or the ISR) */
static void bank_take(uint32_t p)
{
    chain_pattern_t *c = &bank_pat[p % NBANK];
    uint32_t k;
    for (k = 0; k < NTRK; k++) {
        const track_t *t = &trk[k];
        memcpy(c->step[k], t->step, sizeof c->step[k]);
        memcpy(c->timing[k], &t->p[P_SLEN], sizeof c->timing[k]);
        c->qnt[k] = (uint8_t)(t->p[P_SQNT] != 0);
    }
    c->motion = motion;
    bank.used |= (uint8_t)(1u << (p % NBANK));
}
/* the current pattern stored when it changed (interrupts off, or the ISR); 1 = it had */
static int bank_keep(void)
{
    if (bank_same(bank.cur))
        return 0;
    bank_take(bank.cur);
    bank.dirty |= (uint8_t)(1u << bank.cur);
    return 1;
}
/* pattern p's copy -> the tracks (their notes let go, their automation back to the sound's values first) */
static void bank_give(uint32_t p)
{
    const chain_pattern_t *c = &bank_pat[p % NBANK];
    uint32_t k;
    for (k = 0; k < NTRK; k++) {
        seq_release(&trk[k]);
        motion_restore(&trk[k]);
    }
    motion = c->motion;
    for (k = 0; k < NTRK; k++) {
        track_t *t = &trk[k];
        memcpy(t->step, c->step[k], sizeof t->step);
        memcpy(&t->p[P_SLEN], c->timing[k], sizeof c->timing[k]);
        t->p[P_SQNT] = c->qnt[k];
    }
}
/* the track whose loop ends the current pattern: the longest loop (LEN x DIV; the first of a tie) */
static uint32_t bank_clock(void)
{
    uint32_t k, best = 0, most = 0;
    for (k = 0; k < NTRK; k++) {
        uint32_t n = pat_loop(trk[k].p[P_SLEN], trk[k].p[P_SDIV]);
        if (n > most) {
            most = n;
            best = k;
        }
    }
    return best;
}
/* to pattern p (one with a pattern, not the current one): this one stored, p given, playing: every track from its
 * step 1 together (as a SONG section starts). The ISR (bank_tick, bank_start) or the main loop, interrupts off */
static __attribute__((noinline)) void bank_switch(uint32_t p)
{
    uint32_t k;
    bank.queue = CHAIN_OFF;
    if (p >= NBANK || p == bank.cur || !((bank.used >> p) & 1u))
        return;
    bank_keep();
    bank_give(p);
    bank.cur = (uint8_t)p;
    bank.gen++;
    for (k = 0; k < NTRK; k++) {
        track_t *t = &trk[k];
        t->seq_idx = (uint16_t)(t->p[P_SLEN] - 1);
        t->seq_pos = 0x7FFFFFFFu;
        t->rh_n = t->rskip_n = 0;
        t->rat_left = 0;
        t->nd_late = t->nd_early = ND_NONE;
        t->nd_fired = 0;
    }
}
/* ISR, each block before the tracks: the queued pattern at the end of this one's loop (its clock track's last step
 * over: the new one's step 1 where that would have started, bank.carry) */
static void bank_tick(uint32_t n)
{
    const track_t *t = &trk[bank.clk % NTRK];
    uint32_t length, carry;
    bank.carry = 0;
    if (bank.queue >= NBANK || chain.running || !song.playing || t->seq_pos >= 0x7FFFFFFFu ||
        t->seq_idx + 1u != (uint32_t)t->p[P_SLEN])
        return;
    length = step_samples(t, div_samples((uint32_t)t->p[P_SDIV]), t->seq_idx);
    if (t->seq_pos + n < length)
        return;
    carry = t->seq_pos + n - length;
    bank_switch(bank.queue);
    bank.carry = carry;
}
/* ISR, PLAY: a pattern queued plays from the start (a song starting: none) */
static void bank_start(void)
{
    bank.carry = 0;
    if (chain.armed)
        bank.queue = CHAIN_OFF;
    else if (bank.queue < NBANK)
        bank_switch(bank.queue);
}

/* ---- the main loop: ENV held's keys (ui_layer.c) ---- */
static int chain_busy(void);
static void ui_message(const char *m);
static void ui_say(const char *a, const char *b);
static int bank_has(uint32_t p) { return p < NBANK && ((bank.used >> p) & 1u); }
static void bank_name(char *b, uint32_t p) { b[0] = (char)('A' + p % NBANK); b[1] = 0; }
/* a white key A..F: go there (stopped), next (playing), back to staying (the queued one, the current one); an empty
 * one: the current pattern copied in, the current one from now. 0 done, 1 refused */
static int bank_key(uint32_t p)
{
    char a[2], b[2];
    if (p >= NBANK)
        return 1;
    if (chain_busy()) {
        ui_message("SONG PLAYING");
        return 1;
    }
    bank_name(a, bank.cur);
    bank_name(b, p);
    if (p == bank.cur || p == bank.queue) {
        if (bank.queue < NBANK) {
            bank.queue = CHAIN_OFF;
            ui_say("STAYING ON ", a);
        } else
            ui_say("PLAYING ", a);
        return 0;
    }
    if (!bank_has(p)) {                                  /* an empty key: a copy of this one, the current one now */
        uint32_t f = motion_guard();
        bank_keep();
        bank_take(p);
        bank.dirty |= (uint8_t)(1u << p);
        if (bank.queue < NBANK) bank.queue = CHAIN_OFF;
        bank.cur = (uint8_t)p;
        bank.gen++;
        motion_unguard(f);
        ui_say("NEW PATTERN ", b);
        return 0;
    }
    if (song.playing) {
        bank.clk = (uint8_t)bank_clock();
        RING_PUBLISH();
        bank.queue = (uint8_t)p;
        ui_say("NEXT: ", b);
        return 0;
    }
    uint32_t f = motion_guard();
    bank_switch(p);
    motion_unguard(f);
    ui_say("PATTERN ", b);
    return 0;
}
/* OCT+ held + a key: the current pattern copied into it (it stays the current one) */
static int bank_copy(uint32_t p)
{
    char b[8] = "A TO A";
    if (p >= NBANK || p == bank.cur) {
        ui_message(p == bank.cur ? "THAT ONE IS PLAYING" : "NO PATTERN THERE");
        return 1;
    }
    if (chain_busy()) {
        ui_message("SONG PLAYING");
        return 1;
    }
    uint32_t f = motion_guard();
    bank_take(p);
    bank.dirty |= (uint8_t)(1u << p);
    if (bank.queue == p) bank.queue = CHAIN_OFF;
    motion_unguard(f);
    b[0] = (char)('A' + bank.cur);
    b[5] = (char)('A' + p);
    ui_say("COPIED ", b);
    return 0;
}
/* OCT- held + a key: pattern p cleared (an empty key; not the current one's: SEQ / REC CLEAR its tracks) */
static int bank_clear(uint32_t p)
{
    char b[2];
    if (p >= NBANK)
        return 1;
    if (p == bank.cur) {
        ui_message("THAT ONE IS PLAYING");
        return 1;
    }
    if (chain_busy()) {
        ui_message("SONG PLAYING");
        return 1;
    }
    bank_name(b, p);
    if (!bank_has(p)) {
        ui_say(b, " IS EMPTY");
        return 1;
    }
    uint32_t f = motion_guard();
    bank.used &= (uint8_t)~(1u << p);
    bank.dirty |= (uint8_t)(1u << p);
    if (bank.queue == p) bank.queue = CHAIN_OFF;
    memset(&bank_pat[p], 0, sizeof bank_pat[p]);
    motion_unguard(f);
    ui_say("CLEARED ", b);
    return 0;
}
/* OCT- tapped in the layer: the queued one forgotten (1: there was one) */
static int bank_unqueue(void)
{
    char a[2];
    if (bank.queue >= NBANK)
        return 0;
    bank.queue = CHAIN_OFF;
    bank_name(a, bank.cur);
    ui_say("STAYING ON ", a);
    return 1;
}
/* pattern p's track k as SONG and ENV's map draw it: one loop in 1/48 quarters (project.c len48_of), its picture
 * (pat_density: 2 bits a cell, *n cells); the current pattern from the tracks (a song playing: its stored copy, the
 * same). 0: an empty key */
static uint32_t pat_density(const step_t *st, int32_t slen, uint8_t *n);
static uint32_t len48_of(int32_t len, int32_t div);
static uint32_t bank_info(uint32_t p, uint32_t k, uint32_t *den, uint8_t *n)
{
    const chain_pattern_t *c = &bank_pat[p % NBANK];
    int live = p == bank.cur && !chain.running;
    const step_t *st = live ? trk[k % NTRK].step : c->step[k % NTRK];
    int32_t len = live ? trk[k % NTRK].p[P_SLEN] : c->timing[k % NTRK][0];
    int32_t div = live ? trk[k % NTRK].p[P_SDIV] : c->timing[k % NTRK][1];
    *den = 0;
    *n = 0;
    if (!bank_has(p))
        return 0;
    *den = pat_density(st, len, n);
    return len48_of(len, div);
}
/* power-on and the host tests: an empty bank, A current */
static void bank_reset(void)
{
    memset(bank_pat, 0, sizeof bank_pat);
    memset((void *)&bank, 0, sizeof bank);
    bank.queue = CHAIN_OFF;
    bank.used = 1;
}
#endif
