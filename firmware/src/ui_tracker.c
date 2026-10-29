/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright (C) 2026 RONDO contributors. Built on Felucca (C) 2026 Leo Kuroshita (@kurogedelic), Hügelton Instruments */
/* RONDO's TRACKER: SEQ's step editor, in place of Felucca's STEP / DETAIL pages (the piano roll, the DRUM grid). Each
 * track has its own tracker screen: its steps as rows top to bottom (00 .. LEN - 1), the cursor's row in the middle
 * (while the transport plays, the playing row, until a knob moves the cursor: ui.c follow), and columns across:
 *   a melodic track   NOTE  VEL  CHN  R  N  FX
 *   a kit (FMKIT, DRUM) BD SD HH PC  VEL  CHN  R  N  FX
 * NOTE: the step's note (a chord: its lowest and how many more), | a held step (TIE), --- nothing. VEL: hex 01..7F.
 * CHN: the chance (% , -- = always) or a condition (1:2 .. FIL, !FI). R: the ratchet (x1..x4). N: the nudge (-8..+7
 * sixteenths of a step). FX: a parameter lock, tracker style: the parameter's short name and its value in hex.
 * BD SD HH PC: the kit's four voices; BD and SD o, HH x closed and o open, PC c clap / t tom / r rim / b bell (the grid's
 * eight lanes on four columns); capitals are accents.
 * FX also holds the COMMANDS (first in KNOB 4's list; seq.c ph_*, core.h CMD_*), shown in words where a row has one:
 *   SPEED  HALF / DBL / SPx1  from this row: half time, double time, back to x1 (the loop's end puts it back anyway)
 *   LOOP   L2x3               after this row, back 2 rows, 3 times (1 2 4 8 rows, 1..8 times)
 *   PITCH  P+2                the LOOP's pitch step a pass (semitones; a kit's hits stay)
 * The pattern's loop keeps its length: when it comes round, row 00 at x1 (back to the bar).
 * KNOB 1 ROW (the cursor), KNOB 2 COL (the column), KNOB 3 the cell's value, KNOB 4 a second value of the column
 * (NOTE: LEN, VEL: FLAG ACC / SLD, FX: which parameter, a drum column: its accent). EDIT clears the cell (NOTE: the
 * step, its locks with it). Keys: as on Felucca's STEP: armed (REC) and stopped they write the cursor's step (a kit
 * too: their GM drums), otherwise they play.
 * SEQ held and SELECT turned: the four tracks to pick from (tk.pick); SEQ let go: that track's tracker. SEQ held alone
 * is still SEQ TOOLS. */

enum { TC_NOTE, TC_BD, TC_SD, TC_HH, TC_PC, TC_VEL, TC_CHN, TC_RAT, TC_NDG, TC_FX };
static const uint8_t TK_SYN[] = {TC_NOTE, TC_VEL, TC_CHN, TC_RAT, TC_NDG, TC_FX};
static const uint8_t TK_KIT[] = {TC_BD, TC_SD, TC_HH, TC_PC, TC_VEL, TC_CHN, TC_RAT, TC_NDG, TC_FX};
static const char *const TC_NAME[] = {"NOTE", "BD", "SD", "HH", "PC", "VEL", "CHN", "RAT", "NDG", "FX"};
static const char *const TC_HEAD[] = {"NOTE", "BD", "SD", "HH", "PC", "VL", "CHN", "R", "N", "FX"};
/* the PC column's lanes (the grid's CP TM RS CB) and its letters */
static const uint8_t TK_PC_LANE[4] = {2, 5, 6, 7};
static const char TK_PC_CH[4] = {'c', 't', 'r', 'b'};

static int tk_kit(const track_t *t) { return drum_track(t); }
static uint32_t tk_ncols(const track_t *t) { return tk_kit(t) ? sizeof TK_KIT : sizeof TK_SYN; }
static uint32_t tk_colkind(const track_t *t, uint32_t c)
{
    uint32_t n = tk_ncols(t);
    c = c < n ? c : n - 1u;
    return tk_kit(t) ? TK_KIT[c] : TK_SYN[c];
}
static uint32_t tk_col(void) { return tk_colkind(TSEL, tk.col[song.sel % NTRK]); }

/* -------------------------------------------------------------- the FX column's parameters --- */
/* the parameters a step can lock on track t, in the order KNOB 4 steps them: the engine's eight, a kit's voice values,
 * then the track's mix, envelope and sends. Only what motion records and the sound labels (ev_id_ok) */
static uint32_t tk_fx_list(const track_t *t, uint8_t *ids)
{
    static const uint8_t COMMON[] = {P_LEVEL, P_PAN, P_ATK, P_DEC, P_SUS, P_REL, P_DIST, P_CHOR, P_DLY, P_REV};
    uint32_t n = 0, i;
#if RONDO_LAYOUT
    ids[n++] = (uint8_t)CMD_SPD;                          /* the commands first */
    ids[n++] = (uint8_t)CMD_LOOP;
    ids[n++] = (uint8_t)CMD_LPIT;
    ids[n++] = (uint8_t)CMD_MORPH;
#endif
    for (i = 0; i < 8u; i++)
        if (ev_id_ok(t, P_E0 + i))
            ids[n++] = (uint8_t)(P_E0 + i);
    if (t->eng_req % NENGINES == ENGI_CKIT) {
        for (i = P_FM1_ATK; i <= P_FM4_LEVEL; i++)
            ids[n++] = (uint8_t)i;
        for (i = 0; i < CK_N; i++)
            ids[n++] = (uint8_t)(P_LN0 + i);
    }
    for (i = 0; i < sizeof COMMON; i++)
        if (ev_id_ok(t, COMMON[i]) && !(ENGINES[t->eng_req % NENGINES]->oneshot && COMMON[i] >= P_ATK && COMMON[i] <= P_REL))
            ids[n++] = COMMON[i];
    return n;
}
static uint32_t tk_fx_id(const track_t *t)
{
    uint8_t ids[64];
    uint32_t n = tk_fx_list(t, ids);
    return n ? ids[tk.fx[trk_index(t) % NTRK] % n] : 0xFFu;
}
/* the FX parameter's names: long (the cards, b holds 8: "BD PITCH" cut to "BD PIT", "DECAY") and short (the cells, 3
 * characters: "BDP", "DEC") */
static void tk_fx_name(const track_t *t, uint32_t id, char *b)
{
    b[0] = 0;
    if (id == 0xFFu)
        return;
    if (t->eng_req % NENGINES == ENGI_CKIT && id >= P_FM1_ATK && id <= P_FM4_LEVEL) {
        static const char *const V[5] = {"PIT", "DEC", "COL", "SWP", "SHP"};
        str_cpy(b, N_CK_ABBR[(id - P_FM1_ATK) / 5u], 8);
        str_cpy(b + 2, " ", 6);
        str_cpy(b + 3, V[(id - P_FM1_ATK) % 5u], 4);
        return;
    }
    if (t->eng_req % NENGINES == ENGI_CKIT && id >= P_LN0 && id < P_LN0 + CK_N) {
        str_cpy(b, N_CK_ABBR[id - P_LN0], 8);
        str_cpy(b + 2, " LVL", 6);
        return;
    }
    str_cpy(b, track_desc(t, id)->label, 7);
}
static void tk_fx_short(const track_t *t, uint32_t id, char *b)
{
    char nm[8];
    tk_fx_name(t, id, nm);
    if (nm[2] == ' ') {                                   /* a kit voice's: its two letters and the value's first */
        b[0] = nm[0];
        b[1] = nm[1];
        b[2] = nm[3];
        b[3] = 0;
    } else {
        str_cpy(b, nm, 4);
    }
}
/* a command's cell (b holds 8): HALF / DBL / SPx1, L2x3, P+2 */
static void tk_cmd_text(uint32_t id, int32_t v, char *b)
{
    if (id == CMD_SPD) {
        str_cpy(b, v == 1 ? "HALF" : v == 2 ? "DBL" : "SPx1", 8);
    } else if (id == CMD_LOOP) {
        b[0] = 'L';
        str_cpy(b + 1, N_CLOOP[clamp(v, 0, 31)], 7);
    } else if (id == CMD_MORPH) {
        b[0] = 'M';
        fmt_int(b + 1, clamp(v, 0, 100));
    } else {
        b[0] = 'P';
        b[1] = v < 0 ? '-' : '+';
        fmt_int(b + 2, v < 0 ? -v : v);
    }
}
static void tk_hex2(char *b, uint32_t v)
{
    static const char H[] = "0123456789ABCDEF";
    b[0] = H[(v >> 4) & 15u];
    b[1] = H[v & 15u];
    b[2] = 0;
}

/* --------------------------------------------------------------------------------- cells --- */
/* the text of column kind c at step st (b holds 8); returns 1 when the cell holds something (bright), 0 empty (DIM) */
static int tk_cell(const track_t *t, uint32_t si, uint32_t c, char *b)
{
    const step_t *st = &seq_steps(t)[si];
    uint32_t lanes = step_lanes(st), acc = step_accents(st), k;
    b[0] = 0;
    switch (c) {
    case TC_NOTE:
        if (st->time == ST_TIE) {
            str_cpy(b, " | ", 8);
            return 0;
        }
        if (!st->n) {
            str_cpy(b, "---", 8);
            return 0;
        }
        {
            uint32_t lo = st->note[0], o;
            for (k = 1; k < st->n && k < 4u; k++)
                lo = st->note[k] < lo ? st->note[k] : lo;
            str_cpy(b, N_NOTE[lo % 12u], 8);
            if (str_len(b) < 2u)
                str_cpy(b + 1, "-", 4);
            o = lo / 12u;
            b[2] = (char)(o ? '0' + (o - 1u) % 10u : '-');
            b[3] = 0;
        }
        return 1;
    case TC_BD:
    case TC_SD:
        k = c == TC_BD ? 0u : 1u;
        str_cpy(b, (lanes >> k) & 1u ? ((acc >> k) & 1u ? "O" : "o") : ".", 8);
        return (int)((lanes >> k) & 1u);
    case TC_HH:
        if ((lanes >> 4) & 1u)
            str_cpy(b, (acc >> 4) & 1u ? "O" : "o", 8);
        else if ((lanes >> 3) & 1u)
            str_cpy(b, (acc >> 3) & 1u ? "X" : "x", 8);
        else
            str_cpy(b, ".", 8);
        return (int)((lanes >> 3) & 3u) != 0;
    case TC_PC:
        str_cpy(b, ".", 8);
        for (k = 0; k < 4u; k++)
            if ((lanes >> TK_PC_LANE[k]) & 1u) {
                b[0] = (char)((acc >> TK_PC_LANE[k]) & 1u ? TK_PC_CH[k] - 32 : TK_PC_CH[k]);
                return 1;
            }
        return 0;
    case TC_VEL:
        if (!step_on(st)) {
            str_cpy(b, "..", 8);
            return 0;
        }
        tk_hex2(b, (uint32_t)(st->flags & SF_ACCENT ? 127 : ev_sval(st, EVK_VEL)));
        return ev_sval(st, EVK_VEL) != ev_sdefv(EVK_VEL) || (st->flags & SF_ACCENT);
    case TC_CHN: {
        int32_t v = ev_sval(st, EVK_CHANCE);
        if (v == 100) {
            str_cpy(b, "--", 8);
            return 0;
        }
        if (v > 100) {
            static const char *const C[NCOND] = {"1:2", "2:2", "1:3", "2:3", "3:3", "1:4", "2:4", "3:4", "4:4", "FIL", "!FI"};
            str_cpy(b, C[(v - 101) % (int32_t)NCOND], 8);
        } else {
            fmt_int(b, v);
        }
        return 1;
    }
    case TC_RAT: {
        int32_t v = ev_sval(st, EVK_RATCH);
        if (v <= 1) {
            str_cpy(b, "-", 8);
            return 0;
        }
        fmt_int(b, v);
        return 1;
    }
    case TC_NDG: {
        int32_t v = ev_sval(st, EVK_NUDGE);
        if (!v) {
            str_cpy(b, "--", 8);
            return 0;
        }
        b[0] = v < 0 ? '-' : '+';
        fmt_int(b + 1, v < 0 ? -v : v);
        return 1;
    }
    default: {                                            /* FX: the column's parameter, else the step's command, else */
        uint32_t id = tk_fx_id(t), i;                     /* its first lock */
        int16_t v;
        int on = id != 0xFFu && motion_lock_get(t, si, id, &v);
#if RONDO_LAYOUT
        for (i = P_COUNT; i < P_COUNT + NCMD && !on; i++)
            if (motion_lock_get(t, si, i, &v))
                on = 1, id = i;                           /* (a command: bright, it shapes the pattern) */
#endif
        if (!on)
            for (i = 0; i < P_COUNT && !on; i++)
                if (motion_lock_get(t, si, i, &v))
                    on = 2, id = i;
        if (!on) {
            str_cpy(b, "....", 8);
            return 0;
        }
        if (cmd_id(id)) {
            tk_cmd_text(id, v, b);
            return 1;
        }
        tk_fx_short(t, id, b);
        tk_hex2(b + str_len(b), (uint32_t)clamp(v - track_desc(t, id)->min, 0, 255));
        return on == 1 ? 1 : 2;                           /* (2: another parameter's lock, shown DIM) */
    }
    }
}

/* ---------------------------------------------------------------------------- editing --- */
/* KNOB 3 (alt 0) and KNOB 4 (alt 1) turned s on the cursor's cell */
static void tk_turn(uint32_t alt, int32_t s)
{
    track_t *t = TSEL;
    uint32_t i = ui.cursor, c = tk_col(), k;
    step_t *st = &t->step[i % NSTEP];
    step_undo_take(t, 0x4000u | (alt + 2u) << 6 | i);    /* (this knob on this step: one undo, SAVE held) */
    switch (c) {
    case TC_NOTE:
        if (alt) {                                        /* LEN: the TIEs after the note */
            uint32_t was, n;
            if (!step_on(st)) {
                ui_message("NO NOTE");
                break;
            }
            was = note_len(t, i);
            n = note_set_len(t, i, (uint32_t)clamp((int32_t)was + s, 1, NSTEP));
            if (s > 0 && n == was)
                ui_message(i + n < (uint32_t)clamp(t->p[P_SLEN], 1, NSTEP) ? "NOTE AHEAD" : "END OF PATTERN");
            break;
        }
        if (!st->n) {                                     /* an empty step: the last note played */
            if (st->time != ST_NOTE)
                step_clear(st);
            st->note[0] = last_note;
            st->n = 1;
            st->time = ST_NOTE;
            if (!st->vel)
                st->vel = 96;
            break;
        }
        for (k = 0; k < st->n; k++)
            st->note[k] = (uint8_t)clamp(st->note[k] + accel(EN_K3, s, 24), 1, 127);
        st->time = ST_NOTE;
        last_note = st->note[0];
        break;
    case TC_BD:
    case TC_SD: {
        uint32_t l = c == TC_BD ? 0u : 1u;
        if (alt)
            grid_acc(t, i, l, s > 0);
        else
            grid_hit(t, i, l, s > 0);
        break;
    }
    case TC_HH: {                                         /* . -> x closed -> o open */
        uint32_t lanes = step_lanes(st), v = (lanes >> 4) & 1u ? 2u : (lanes >> 3) & 1u ? 1u : 0u;
        if (alt) {
            if (v)
                grid_acc(t, i, v == 2u ? 4u : 3u, s > 0);
            break;
        }
        v = (uint32_t)clamp((int32_t)v + (s > 0 ? 1 : -1), 0, 2);
        grid_hit(t, i, 3, v == 1u);
        grid_hit(t, i, 4, v == 2u);
        break;
    }
    case TC_PC: {                                         /* . -> c -> t -> r -> b */
        uint32_t lanes = step_lanes(st), v = 0;
        for (k = 0; k < 4u; k++)
            if ((lanes >> TK_PC_LANE[k]) & 1u)
                v = k + 1u;
        if (alt) {
            if (v)
                grid_acc(t, i, TK_PC_LANE[v - 1u], s > 0);
            break;
        }
        v = (uint32_t)clamp((int32_t)v + (s > 0 ? 1 : -1), 0, 4);
        for (k = 0; k < 4u; k++)
            grid_hit(t, i, TK_PC_LANE[k], v == k + 1u);
        break;
    }
    case TC_VEL:
        if (!step_on(st)) {
            ui_message("NO NOTE");
            break;
        }
        if (alt) {                                        /* FLAG: - ACC SLD A+S */
            uint32_t f = (st->flags & SF_ACCENT ? 1u : 0u) | (st->flags & SF_SLIDE ? 2u : 0u);
            f = (uint32_t)clamp((int32_t)f + (s > 0 ? 1 : -1), 0, 3);
            st->flags = (uint8_t)((st->flags & ~(SF_ACCENT | SF_SLIDE)) | (f & 1u ? SF_ACCENT : 0u) | (f & 2u ? SF_SLIDE : 0u));
            break;
        }
        ev_sput(st, EVK_VEL, clamp(ev_sval(st, EVK_VEL) + accel(EN_K3, s, 126), 1, 127));
        if (st->flags & SF_ACCENT)
            ui_message("ACC PLAYS 127");
        break;
    case TC_CHN:
    case TC_RAT:
    case TC_NDG: {
        uint32_t kind = c == TC_CHN ? EVK_CHANCE : c == TC_RAT ? EVK_RATCH : EVK_NUDGE;
        int32_t v = ev_sval(st, kind);
        if (!sd_takes(st, kind)) {
            ui_message("NO NOTE");
            break;
        }
        v = kind == EVK_CHANCE ? ev_chance_turn(v, s, accel(EN_K3 + alt, s, 100)) : kind == EVK_RATCH ? clamp(v + s, 1, 4) :
            clamp(v + s, -8, 7);
        ev_sput(st, kind, v);
        if (kind == EVK_NUDGE && t->p[P_SQNT])
            ui_message("QUANTIZE IS ON");
        break;
    }
    default: {                                            /* FX */
        uint8_t ids[64];
        uint32_t n = tk_fx_list(t, ids), id;
        if (!n)
            break;
        if (alt) {
            tk.fx[song.sel % NTRK] = (uint8_t)((tk.fx[song.sel % NTRK] % n + n + (s > 0 ? 1u : n - 1u)) % n);
            break;
        }
        id = ids[tk.fx[song.sel % NTRK] % n];
        {
            const param_desc_t *d = track_desc(t, id);
            int16_t v;
            if (!motion_lock_get(t, i, id, &v))
                v = motion_base_value(t, id);
            motion_set_lock(t, i, id, (int16_t)param_turn(d, v, accel(EN_K3, s, d->fmt == F_ENUM ? 0 : d->max - d->min)));
        }
        break;
    }
    }
    motion_undo_done(t);
}

/* a tracker knob: 1 ROW, 2 COL, 3 the value, 4 the second value */
static void tk_edit(uint32_t slot, int32_t s)
{
    uint32_t n = tk_ncols(TSEL), k = song.sel % NTRK;
    if (slot)
        follow_hold();                                    /* (editing: the cursor's row stays on the screen) */
    if (slot == 0u)
        cursor_move(ui.cursor + s);
    else if (slot == 1u)
        tk.col[k] = (uint8_t)clamp((int32_t)(tk.col[k] < n ? tk.col[k] : n - 1u) + (s > 0 ? 1 : -1), 0, (int32_t)n - 1);
    else
        tk_turn(slot - 2u, s);
}

/* EDIT on the tracker: the cursor's cell cleared (NOTE: the whole step and its locks, as STEP's EDIT; FX: the
 * column parameter's lock). 1 = it handled the button */
static int tk_clear(void)
{
    track_t *t = TSEL;
    uint32_t i = ui.cursor, c = tk_col(), k;
    step_t *st = &t->step[i % NSTEP];
    if (c == TC_NOTE)
        return 0;                                         /* (ui_input: STEP's EDIT clears the step, as before) */
    if (chain_busy()) {
        ui_message("STOP TO EDIT");
        return 1;
    }
    step_undo_take(t, 0);
    if (c == TC_BD || c == TC_SD)
        grid_hit(t, i, c == TC_BD ? 0u : 1u, 0);
    else if (c == TC_HH) {
        grid_hit(t, i, 3, 0);
        grid_hit(t, i, 4, 0);
    } else if (c == TC_PC) {
        for (k = 0; k < 4u; k++)
            grid_hit(t, i, TK_PC_LANE[k], 0);
    } else if (c == TC_VEL) {
        ev_sput(st, EVK_VEL, ev_sdefv(EVK_VEL));
        st->flags &= (uint8_t)~(SF_ACCENT | SF_SLIDE);
    } else if (c == TC_CHN || c == TC_RAT || c == TC_NDG) {
        uint32_t kind = c == TC_CHN ? EVK_CHANCE : c == TC_RAT ? EVK_RATCH : EVK_NUDGE;
        ev_sput(st, kind, ev_sdefv(kind));
    } else {
        uint32_t id = tk_fx_id(t);
        if (id != 0xFFu)
            motion_delete_event(t, i, id);
    }
    motion_undo_done(t);
    ui_message("CLEARED");
    return 1;
}

/* the row at the middle of the tracker: the playing step while the transport plays and nothing holds the view,
 * else the cursor's */
static uint32_t tk_center(const track_t *t)
{
    uint32_t len = (uint32_t)clamp(t->p[P_SLEN], 1, NSTEP);
    if (song.playing && !follow.hold && t->seq_idx < len)
        return t->pr_on && t->pr_row < len ? t->pr_row : t->seq_idx;   /* (RONDO: the row the commands play) */
    return ui.cursor < len ? ui.cursor : len - 1u;
}

/* ------------------------------------------------------------------------ the track picker --- */
/* SEQ held, SELECT turned: the picker steps round the four tracks (it opens on the selected one) */
static void tk_pick_turn(int32_t s)
{
    uint32_t p = tk.pick ? tk.pick - 1u : song.sel;
    p = (p + NTRK + (s > 0 ? 1u : NTRK - 1u)) % NTRK;
    tk.pick = (uint8_t)(p + 1u);
    ui.force = 1;
}
static void tk_page_open(void);
/* SEQ let go after a pick: that track, its tracker */
static void tk_pick_done(void)
{
    uint32_t p = tk.pick;
    tk.pick = 0;
    if (!p)
        return;
    track_select(p - 1u);
    tk_page_open();
}
static void tk_page_open(void)
{
    uint32_t i;
    for (i = 0; i < NPAGES; i++)
        if (PAGES[i].graph == GR_ROLL)
            break;
    if (i >= NPAGES)
        return;
    ui.page = (uint8_t)i;
    ui.fam_last[FAM_SEQ] = (uint8_t)i;
    ui.home = 0;
    page_entered();
}
