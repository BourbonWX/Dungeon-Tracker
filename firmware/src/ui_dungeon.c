/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright (C) 2026 RONDO contributors. Built on Felucca (C) 2026 Leo Kuroshita (@kurogedelic), Hügelton Instruments */
/* RONDO's HOME: a first-person dungeon crawl in the style of the old grid crawlers, played by the song.
 * TWO STATES. Off (the default, at every power-on): HOME's corridor is scenery only. While the transport plays the
 * party walks it a square a beat, and the HUD shows each track's adventurer and level; nothing is rolled, no monster
 * comes, nothing is cut. MENU > GAME > DUNGEON ON (hold HOME for the MENU; dg_game, not saved) starts a run at B1:
 * the game below plays on every page until it is switched off, which ends the run and lifts every cut at once.
 * The game: the party walks one square a beat (the tempo's), monsters come up the corridor and are fought a
 * round a beat. The song is the party:
 *   - every track that has steps and is not muted is one adventurer: T1 the FIGHTER (a sword), T2 the CLERIC (heals
 *     the party), T3 the DWARF (an axe, more hit points), T4 the MAGE (a bolt, every other beat, hard);
 *   - each adventurer's LEVEL (1..9) is how much its track does: notes on, distinct pitches (a kit: distinct drums),
 *     chords, parameter locks, chances, conditions, ratchets and nudges (dg_level);
 *   - a floor is cleared after a walk of DG_FLOOR squares: the stairs, one floor down. Monsters get stronger with the
 *     depth (every fifth floor a boss); a party that falls retreats one floor up, healed. A full, busy song goes deep,
 *     a lone simple loop stays near the top.
 * Each adventurer's health is hidden (no bar: a wounded one's name blinks): its most is 6 + 4 per level (the dwarf 4
 * more), so a busier track takes more blows; a track made busier mid-fight gains the difference at once.
 * A FALL CUTS THE TRACK (whatever HOME shows, whatever page is up): an adventurer at 0 health
 * falls and its track is muted as by a PERFORM black key (dg_cut: perform.c ramps it out and back, the sequencer runs
 * on, nothing is saved) until it is back on its feet:
 *   - the cleric, standing, raises a fallen one: a chance each beat walked (1 in 5), at half health;
 *   - the stairs: everyone fallen gets up at half health on the next floor;
 *   - the whole party fallen: the song is silent for a bar (DG_WIPE beats), then they retreat a floor up, healed;
 *   - stopped: no cuts; stopping is a camp, the fallen are raised.
 * Muting or emptying a fallen track does not raise it: it stays fallen until one of the above.
 * The game runs on every page (ui_input.c calls dg_tick), not only while HOME is drawn.
 * Stopped: the party camps (no walking, they heal). Everything is drawn from the palette's colours (the walls RAISE
 * into BG with the distance, the monsters ACCENT, the steel TEXT), so it follows MENU > COLOR. The game is the UI's
 * own: nothing of it is stored, and it starts over at power-on. RONDO_LAYOUT=0 builds (Felucca's behaviour) never cut. */

#define DG_X0 6                                          /* the view: x 6..233, y 3..94 */
#define DG_W 228
#define DG_Y0 3
#define DG_H 91
#define DG_HUD 97                                        /* the party's row */
#define DG_FLOOR 24                                      /* squares a floor */
#define DG_NSEG 6                                        /* corridor segments drawn */
#define DG_WIPE 4                                        /* beats of silence when the whole party falls */
#define DG_RAISE 5                                       /* the cleric raises: 1 in DG_RAISE beats walked */

enum { DG_FTR, DG_CLR, DG_DWF, DG_MAG };
static const char *const DG_CLASS[4] = {"FTR", "CLR", "DWF", "MAG"};
static const char *const DG_FALL[2][4] = {{"FIGHTER FALLS", "CLERIC FALLS", "DWARF FALLS", "MAGE FALLS"},
                                          {"FIGHTER FALLS: T1 CUT", "CLERIC FALLS: T2 CUT", "DWARF FALLS: T3 CUT",
                                           "MAGE FALLS: T4 CUT"}};
static const char *const DG_RAISED[4] = {"T1 BACK", "T2 BACK", "T3 BACK", "T4 BACK"};
enum { DM_NONE, DM_GOBLIN, DM_SKEL, DM_SLIME, DM_OGRE };
static const char *const DM_NAME[5] = {"", "GOBLIN", "SKELETON", "SLIME", "OGRE"};

static struct {
    uint32_t t_ms;                                       /* fm1_ms at the last tick */
    uint32_t phase;                                      /* the way into the current square, 0..65535 */
    uint32_t walked;                                     /* squares walked on this floor */
    uint32_t cell;                                       /* squares walked since power-on (the walls' pattern) */
    uint8_t floor, best, in[4], lvl[4];
    int16_t hp[4];
    uint8_t mon, mon_dist;                               /* the monster and its squares ahead (0: none) */
    int16_t mon_hp, mon_max;
    uint16_t kills;
    uint8_t swing[4];                                    /* frames left of an adventurer's attack shown */
    uint8_t mon_hit, mon_atk;                            /* frames: the monster hit (flash), its attack (the frame) */
    uint8_t beat_odd, msg_f;
    uint8_t dead;                                        /* adventurers fallen (bit k: T(k+1)), cut while DUNGEON plays */
    uint8_t wipe;                                        /* beats left of a fallen party's silence (0: none) */
    uint32_t fphase;                                     /* the beat's way round while fighting, 0..65535 */
    int16_t dmg;                                         /* the last damage dealt, shown over the monster */
    const char *msg;                                     /* a message over the view while msg_f > 0 */
    uint32_t rng;
    uint32_t lvl_ms;                                     /* fm1_ms of the last levels count */
} dg = {0, 0, 0, 0, 1, 1};

static uint32_t dg_rand(void)
{
    uint32_t s = dg.rng ? dg.rng : 0x2545F491u;
    s ^= s << 13;
    s ^= s >> 17;
    s ^= s << 5;
    dg.rng = s;
    return s;
}
static int dg_roll(uint32_t pct) { return dg_rand() % 100u < pct; }
static void dg_say(const char *m)
{
    dg.msg = m;
    dg.msg_f = 40;
}

/* an adventurer's level 1..9: what the track does (see the top) */
static uint32_t dg_level(const track_t *t)
{
    uint32_t len = (uint32_t)clamp(t->p[P_SLEN], 1, NSTEP), i, k, on = 0, chords = 0, extras = 0, dp = 0;
    uint32_t seen[4] = {0, 0, 0, 0}, score;
    const step_t *s = seq_steps(t);
    for (i = 0; i < len; i++) {
        uint32_t lanes = step_lanes(&s[i]), nh = 0;
        if (!step_on(&s[i]))
            continue;
        on++;
        for (k = 0; k < s[i].n && k < 4u; k++)
            seen[(s[i].note[k] >> 5) & 3u] |= 1u << (s[i].note[k] & 31u);
        for (k = 0; k < NLANE; k++)
            if ((lanes >> k) & 1u) {
                nh++;
                seen[3] |= 1u << (24u + k);
            }
        chords += s[i].n > 1u || nh > 1u;
        extras += ev_sval(&s[i], EVK_CHANCE) != 100 || ev_sval(&s[i], EVK_RATCH) > 1 || ev_sval(&s[i], EVK_NUDGE) != 0;
    }
    for (k = 0; k < 4u; k++)
        for (i = 0; i < 32u; i++)
            dp += (seen[k] >> i) & 1u;
    score = on / 2u + 2u * (dp < 12u ? dp : 12u) + chords + motion_lock_count(t) + 2u * extras;
    return (uint32_t)clamp(1 + (int32_t)score / 5, 1, 9);
}
static int16_t dg_hpmax(uint32_t k) { return (int16_t)(6 + 4 * dg.lvl[k] + (k == DG_DWF ? 4 : 0)); }
static int dg_fallen(uint32_t k) { return (dg.dead >> k) & 1u; }
static int dg_deadly(void)                               /* a fall cuts its track: the game on, playing */
{
#if RONDO_LAYOUT
    return dg_game && song.playing;
#else
    return 0;
#endif
}
static void dg_raise(uint32_t k, int16_t hp)
{
    dg.dead &= (uint8_t)~(1u << k);
    dg.hp[k] = hp > 0 ? hp : 1;
}

/* who is in the party: the tracks with steps, not muted; their levels (once a second) */
static void dg_party(void)
{
    uint32_t k;
    if (dg.lvl_ms && fm1_ms - dg.lvl_ms < 1000u)
        return;
    dg.lvl_ms = fm1_ms | 1u;
    for (k = 0; k < 4u; k++) {
        const track_t *t = &trk[k];
        uint32_t in = !t->p[P_MUTE] && !seq_is_empty(t), joins = in && !dg.in[k];
        int16_t was = dg_hpmax(k);
        dg.in[k] = (uint8_t)in;
        dg.lvl[k] = (uint8_t)(in ? dg_level(t) : 0u);
        if (dg_fallen(k))
            dg.hp[k] = 0;                                /* (fallen: until raised, joining or not) */
        else if (joins)
            dg.hp[k] = dg_hpmax(k);                      /* a new adventurer: at full */
        else if (in && dg_hpmax(k) > was)
            dg.hp[k] = (int16_t)(dg.hp[k] + dg_hpmax(k) - was);   /* a busier track: tougher at once */
        if (dg.hp[k] > dg_hpmax(k))
            dg.hp[k] = dg_hpmax(k);
    }
}
static uint32_t dg_up(void)                              /* adventurers standing */
{
    uint32_t k, n = 0;
    for (k = 0; k < 4u; k++)
        n += dg.in[k] && !dg_fallen(k);
    return n;
}
static uint32_t dg_party_n(void) { return (uint32_t)(dg.in[0] + dg.in[1] + dg.in[2] + dg.in[3]); }

/* a beat while walking: one square on; a monster may come; the stairs at the end of the floor */
static void dg_walk(void)
{
    uint32_t k;
    dg.walked++;
    dg.cell++;
    for (k = 0; k < 4u; k++)                             /* walking heals a little, the cleric more */
        if (dg.in[k] && !dg_fallen(k) && dg.hp[k] < dg_hpmax(k))
            dg.hp[k] = (int16_t)(dg.hp[k] + (dg.in[DG_CLR] && !dg_fallen(DG_CLR) ? 2 : 1) - (dg_rand() & 1u));
    if (dg.dead && dg.in[DG_CLR] && !dg_fallen(DG_CLR) && dg_roll(100u / DG_RAISE)) {   /* the cleric raises one */
        for (k = 0; k < 4u && !dg_fallen(k); k++)
            ;
        dg_raise(k, (int16_t)(dg_hpmax(k) / 2));
        dg.swing[DG_CLR] = 10;
        dg_say(DG_RAISED[k]);
    }
    if (dg.walked >= DG_FLOOR) {
        dg.walked = 0;
        for (k = 0; k < 4u; k++)                         /* the stairs: the fallen get up */
            if (dg_fallen(k))
                dg_raise(k, (int16_t)(dg_hpmax(k) / 2));
        dg.floor = (uint8_t)(dg.floor < 99u ? dg.floor + 1u : 99u);
        dg.best = dg.floor > dg.best ? dg.floor : dg.best;
        dg_say(dg.floor % 5u == 0u ? "BOSS FLOOR" : "STAIRS DOWN");
        return;
    }
    if (dg_roll(12u + dg.floor) || (dg.floor % 5u == 0u && dg.walked == DG_FLOOR / 2u)) {
        uint32_t boss = dg.floor % 5u == 0u && dg.walked == DG_FLOOR / 2u;
        dg.mon = (uint8_t)(boss ? DM_OGRE : DM_GOBLIN + dg_rand() % 3u);
        dg.mon_dist = 4;
        dg.mon_max = dg.mon_hp = (int16_t)((4 + 3 * dg.floor) * (boss ? 3 : 1) + (dg.mon == DM_SLIME ? -2 : 0));
        dg_say(DM_NAME[dg.mon]);
    }
}

/* a beat of the fight: the party strikes, then the monster */
static void dg_fight(void)
{
    uint32_t k, n = 0, pick, alive[4];
    for (k = 0; k < 4u; k++) {
        int32_t lv = dg.lvl[k], d;
        if (!dg.in[k] || dg_fallen(k))
            continue;
        alive[n++] = k;
        if (k == DG_CLR) {                               /* the cleric heals the weakest, or strikes */
            uint32_t j, w = 4;
            for (j = 0; j < 4u; j++)
                if (dg.in[j] && !dg_fallen(j) && dg.hp[j] < dg_hpmax(j) / 2 && (w == 4u || dg.hp[j] < dg.hp[w]))
                    w = j;
            if (w < 4u) {
                dg.hp[w] = (int16_t)clamp(dg.hp[w] + 3 + lv, 0, dg_hpmax(w));
                dg.swing[k] = 10;
                continue;
            }
        }
        if (k == DG_MAG && dg.beat_odd)                  /* the mage casts every other beat */
            continue;
        dg.swing[k] = 10;
        if (!dg_roll((uint32_t)(50 + 5 * lv)))
            continue;
        d = lv + (int32_t)(dg_rand() % (uint32_t)(lv + 1)) + (k == DG_FTR ? 2 : k == DG_DWF ? 1 : k == DG_MAG ? lv : 0);
        dg.mon_hp = (int16_t)(dg.mon_hp - d);
        dg.dmg = (int16_t)d;
        dg.mon_hit = 6;
    }
    if (dg.mon_hp <= 0) {
        dg.kills++;
        dg_say(dg.mon == DM_OGRE ? "OGRE SLAIN" : "SLAIN");
        dg.mon = DM_NONE;
        dg.mon_dist = 0;
        return;
    }
    if (!n)
        return;
    pick = alive[dg_rand() % n];
    if (dg_roll(60u)) {
        dg.hp[pick] = (int16_t)(dg.hp[pick] - (1 + dg.floor / 2 + (dg.mon == DM_OGRE ? 2 : 0)));
        dg.mon_atk = 5;
        if (dg.hp[pick] <= 0) {                          /* it falls: its track is cut (dg_tick) */
            dg.hp[pick] = 0;
            dg.dead |= (uint8_t)(1u << pick);
            dg_say(DG_FALL[dg_deadly()][pick]);
        }
    }
    if (!dg_up()) {                                      /* the party falls: a bar of silence, then the retreat */
        dg.wipe = DG_WIPE;
        dg.mon = DM_NONE;
        dg.mon_dist = 0;
        dg_say("THE PARTY FALLS");
    }
}
static void dg_retreat(void)                             /* after the fallen party's bar: one floor up, healed */
{
    uint32_t k;
    dg.floor = (uint8_t)(dg.floor > 1u ? dg.floor - 1u : 1u);
    dg.walked = 0;
    dg.wipe = 0;
    dg.dead = 0;
    for (k = 0; k < 4u; k++)
        dg.hp[k] = dg.in[k] ? dg_hpmax(k) : 0;
    dg_say("RETREAT");
}

/* every drawing of HOME: the time since the last one, in beats of the tempo */
/* MENU > GAME > DUNGEON: on, a new run from B1 (everyone at full); off, the run over, every cut lifted at once */
static void dg_game_set(uint32_t on)
{
    uint32_t r = dg.rng ? dg.rng : fm1_ms * 2654435761u + 1u, cell = dg.cell;
    memset(&dg, 0, sizeof dg);
    dg.floor = dg.best = 1;
    dg.rng = r;
    dg.cell = cell;                                      /* (the corridor goes on where it was) */
    dg.t_ms = fm1_ms;
    dg_game = (uint8_t)(on != 0u);
    dg_cut = 0;
    dg_party();                                          /* (all of them join at full) */
    if (on)
        dg_say("INTO THE DUNGEON");
}

/* off: the scenery's walk only, a square a beat while playing */
static void dg_stroll(uint32_t dt)
{
    uint32_t bpm = (uint32_t)clamp(song.g[G_BPM], 20, 300);
    dg_cut = 0;
    if (!song.playing)
        return;
    dg.phase += dt * bpm * 65536u / 60000u;
    while (dg.phase >= 65536u) {
        dg.phase -= 65536u;
        dg.cell++;
    }
}

static void dg_tick(void)
{
    uint32_t now = fm1_ms, dt = dg.t_ms ? now - dg.t_ms : 0, k;
    dg.t_ms = now;
    if (dt > 200u)
        dt = 200u;
    dg_party();
    if (!dg_game) {
        dg_stroll(dt);
        return;
    }
    for (k = 0; k < 4u; k++)
        if (dg.swing[k])
            dg.swing[k]--;
    if (dg.mon_hit)
        dg.mon_hit--;
    if (dg.mon_atk)
        dg.mon_atk--;
    if (dg.msg_f)
        dg.msg_f--;
    if (!song.playing || !dg_party_n()) {                /* camping: no walking, the fallen raised, healing */
        if (dg.wipe)
            dg_retreat();
        for (k = 0; k < 4u; k++)
            if (dg_fallen(k))
                dg_raise(k, 1);
        if ((now >> 9) != ((now - dt) >> 9))
            for (k = 0; k < 4u; k++)
                if (dg.in[k] && dg.hp[k] < dg_hpmax(k))
                    dg.hp[k]++;
        dg_cut = 0;
        return;
    }
    {
        uint32_t bpm = (uint32_t)clamp(song.g[G_BPM], 20, 300), add = dt * bpm * 65536u / 60000u;   /* a beat: 65536 */
        if (!dg.wipe && !dg_up())                        /* (fallen, and nobody else: as a fallen party) */
            dg.wipe = DG_WIPE;
        if (dg.wipe || (dg.mon && dg.mon_dist <= 1u)) {  /* fighting: a round a beat, the view standing still */
            dg.fphase += add;
            if (dg.fphase >= 65536u) {
                dg.fphase &= 0xFFFFu;
                dg.beat_odd ^= 1u;
                if (dg.wipe) {
                    if (!--dg.wipe)
                        dg_retreat();
                } else
                    dg_fight();
            }
            dg.phase = 0;
            dg_cut = dg_deadly() ? dg.dead & 15u : 0u;
            return;
        }
        dg.fphase = 0;
        dg.phase += add;
        while (dg.phase >= 65536u) {
            dg.phase -= 65536u;
            dg.beat_odd ^= 1u;
            if (dg.mon && dg.mon_dist > 1u)
                dg.mon_dist--;                           /* (it comes, they walk on to meet it) */
            if (!dg.mon || dg.mon_dist > 1u)
                dg_walk();
            if (dg.mon && dg.mon_dist <= 1u) {
                dg.phase = 0;
                break;
            }
        }
    }
    dg_cut = dg_deadly() ? dg.dead & 15u : 0u;
}

/* ------------------------------------------------------------------------------ the drawing --- */
/* 12 x 12 sprites: b body, e eyes / glow */
static const char *const DG_SPR[5][12] = {
    {0},
    {"b..bbbbbb..b", "bb.bbbbbb.bb", ".bbbebbebbb.", "..bbbbbbbb..", "..bb.bb.bb..", "...bbbbbb...",
     "..bbbbbbbb..", ".bbbbbbbbbb.", "b.bbbbbbbb.b", "..bbbbbbbb..", "..bb....bb..", ".bbb....bbb."},
    {"...bbbbbb...", "..bbbbbbbb..", "..beebbeeb..", "..bbbbbbbb..", "...b.bb.b...", "....bbbb....",
     "..bbbbbbbb..", ".b..b..b..b.", "....bbbb....", "...b....b...", "...b....b...", "..bb....bb.."},
    {"............", "............", "....bbbb....", "..bbbbbbbb..", ".bbbebbebbb.", ".bbbbbbbbbb.",
     "bbbbbbbbbbbb", "bbbbbbbbbbbb", "bbbbbbbbbbbb", ".bbbbbbbbbb.", "............", "............"},
    {".bb......bb.", ".bbbbbbbbbb.", "bbbebbbbebbb", "bbbbbbbbbbbb", "bbe.e..e.ebb", ".bbbbbbbbbb.",
     "bbbbbbbbbbbb", "bbbbbbbbbbbb", "b.bbbbbbbb.b", "..bbbbbbbb..", "..bbb..bbb..", ".bbbb..bbbb."},
};

static int32_t dg_cx(void) { return DG_X0 + DG_W / 2; }
static void dg_vrect(int32_t x, int32_t y, int32_t w, int32_t h, uint16_t c)   /* a rect clipped to the view */
{
    int32_t x1 = x + w, y1 = y + h;
    x = x < DG_X0 ? DG_X0 : x;
    y = y < DG_Y0 ? DG_Y0 : y;
    x1 = x1 > DG_X0 + DG_W ? DG_X0 + DG_W : x1;
    y1 = y1 > DG_Y0 + DG_H ? DG_Y0 + DG_H : y1;
    if (x1 > x && y1 > y)
        cv_rect(x, y, x1 - x, y1 - y, c);
}
/* a wall between the near (half width wa, half height ha) and the far edge (wb, hb), on side s (-1 left, +1 right),
 * filled column by column; brick rows on it */
static void dg_wall(int32_t cx, int32_t cy, int32_t wa, int32_t ha, int32_t wb, int32_t hb, int32_t s, uint16_t c,
                    uint16_t mortar, int open)
{
    int32_t xa = cx + s * wa, xb = cx + s * wb, x, i, step = xb >= xa ? 1 : -1, n = xa < xb ? xb - xa : xa - xb;
    for (i = 0; i < n; i++) {
        int32_t h = ha + (hb - ha) * i / n, top = cy - h, k;
        x = xa + step * i;
        if (x < DG_X0 || x >= DG_X0 + DG_W)
            continue;
        if (open) {                                      /* a side passage: the dark beyond, its edges */
            dg_vrect(x, cy - hb, 1, 2 * hb, mortar);
            continue;
        }
        dg_vrect(x, top, 1, 2 * h, c);
        for (k = 1; k < 4; k++)                          /* the mortar: three rows */
            dg_vrect(x, top + 2 * h * k / 4, 1, 1, mortar);
    }
    if (xa >= DG_X0 && xa < DG_X0 + DG_W)                /* the joint at the near edge */
        dg_vrect(xa, cy - ha, 1, 2 * ha, mortar);
}

static void dg_sprite(uint32_t m, int32_t cx, int32_t foot, int32_t px, uint16_t body, uint16_t eye)
{
    uint32_t r, c;
    int32_t x0 = cx - 6 * px, y0 = foot - 12 * px;
    if (m == DM_NONE || m > DM_OGRE)
        return;
    for (r = 0; r < 12u; r++)
        for (c = 0; c < 12u; c++) {
            char ch = DG_SPR[m][r][c];
            if (ch != '.')
                dg_vrect(x0 + (int32_t)c * px, y0 + (int32_t)r * px, px, px, ch == 'e' ? eye : body);
        }
}

/* the adventurers' weapons, while their attack shows (frames 10..1) */
static void dg_weapons(int32_t mx, int32_t my)
{
    int32_t f;
    if ((f = dg.swing[DG_FTR]) != 0) {                   /* the sword: from the right, across */
        static const int8_t TIP[10][2] = {{-70, -40}, {-62, -48}, {-50, -54}, {-36, -56}, {-20, -54},
                                          {-6, -48}, {6, -38}, {14, -26}, {18, -14}, {20, -4}};
        int32_t hx = 196, hy = DG_Y0 + DG_H - 4, tx = hx + TIP[10 - f][0], ty = hy + TIP[10 - f][1];
        cv_line_t(hx, hy, tx, ty, T_TEXT, 3);
        cv_line_t(hx - 6, hy + 4, hx + 6, hy - 4, T_ACCENT, 3);   /* the crossguard */
    }
    if ((f = dg.swing[DG_DWF]) != 0) {                   /* the axe: from the left, down */
        int32_t hx = 36, hy = DG_Y0 + DG_H - 2, a = 10 - f, tx = hx + 20 + a * 3, ty = hy - 52 + a * 4;
        cv_line_t(hx, hy, tx, ty, T_MID, 3);
        dg_vrect(tx - 6, ty - 4, 12, 9, T_TEXT);
    }
    if ((f = dg.swing[DG_MAG]) != 0 && dg.mon) {         /* the bolt: zigzag to the monster */
        int32_t x0 = dg_cx(), y0 = DG_Y0 + DG_H - 2, k, px = x0, py = y0;
        for (k = 1; k <= 5; k++) {
            int32_t x = x0 + (mx - x0) * k / 5 + ((k & 1) ? 5 : -5) * (k < 5), y = y0 + (my - y0) * k / 5;
            cv_line_t(px, py, x, y, T_ACCENT, 2);
            px = x, py = y;
        }
    }
}

static void graph_dungeon(void)
{
    int32_t cx = dg_cx(), cy, k, bob = 0;
    uint32_t ph = dg.phase;
    uint16_t ceil = ux_mix(T_SURF, T_BG, 60), flo = ux_mix(T_SURF, T_RAISE, 40), far = T_BG;
    char b[16];
    dg_party();                                          /* (once a second at most: the HUD current even before a tick) */
    if (song.playing && (!dg_game || (dg_up() && !(dg.mon && dg.mon_dist <= 1u))))   /* the walk's bob */
        bob = (int32_t)((ph < 32768u ? ph : 65535u - ph) >> 13) - 2;
    cy = DG_Y0 + DG_H / 2 - 4 + bob;
    dg_vrect(DG_X0, DG_Y0, DG_W, cy - DG_Y0, ceil);
    dg_vrect(DG_X0, cy, DG_W, DG_Y0 + DG_H - cy, flo);
    /* the corridor, far to near: segment k from depth k to k + 1 (squares), moved on by the walk's phase */
    for (k = DG_NSEG; k >= 0; k--) {
        int32_t za = (k << 8) - (int32_t)(ph >> 8) + 160, zb;   /* depth in 1/256 square (+0.6: the eye) */
        zb = za + 256;
        za = za < 96 ? 96 : za;                          /* (the nearest segment, passing the eye: held at the frame) */
        int32_t wa = 70 * 256 / za, ha = 46 * 256 / za, wb = 70 * 256 / zb, hb = 46 * 256 / zb;
        uint32_t cell = dg.cell + (uint32_t)k, h = cell * 2654435761u ^ dg.floor * 40503u;
        int32_t shade = clamp(k * 14 + (int32_t)(ph >> 12), 0, 90);
        uint16_t wc = ux_mix(T_RAISE, far, shade), mo = ux_mix(wc, far, 45);
        if (k == DG_NSEG) {                              /* the dark at the end of the corridor */
            dg_vrect(cx - wb, cy - hb, 2 * wb, 2 * hb, far);
            continue;
        }
        dg_wall(cx, cy, wa, ha, wb, hb, -1, wc, mo, (h >> 7) % 7u == 0u);
        dg_wall(cx, cy, wa, ha, wb, hb, 1, wc, mo, (h >> 11) % 7u == 1u);
        dg_vrect(cx - wa, cy + ha, 2 * wa, 1, ux_mix(flo, far, 30));   /* the floor's line, the ceiling's */
        dg_vrect(cx - wa, cy - ha, 2 * wa, 1, ux_mix(ceil, far, 30));
        if ((h >> 15) % 5u == 0u && !((h >> 7) % 7u == 0u)) {   /* a torch on the left wall */
            int32_t tx = cx - (wa + wb) / 2, ty = cy - (ha + hb) / 3, fl = (int32_t)((fm1_ms >> 6) & 1u);
            dg_vrect(tx - 1, ty, 3, 6 * 256 / za + 2, T_MID);
            dg_vrect(tx - 2, ty - 4 - fl, 5, 4 + fl, T_ACCENT);
        }
    }
    if (!dg_game)                                        /* off: the scenery and the party, nothing else */
        goto party;
    /* the monster */
    if (dg.mon && dg.mon_dist) {
        int32_t z = ((int32_t)dg.mon_dist << 8) - (int32_t)(ph >> 8) + 60;
        int32_t foot = cy + 46 * 256 / (z > 64 ? z : 64), px = clamp(46 * 2 * 256 / (z > 64 ? z : 64) / 16, 1, 7);
        uint16_t body = dg.mon_hit & 2u ? T_TEXT : T_ACCENT;
        if (dg.mon == DM_OGRE)
            px = clamp(px * 3 / 2, 1, 9);
        dg_sprite(dg.mon, cx, foot, px, body, T_BG);
        dg_weapons(cx, foot - 6 * px);
        if (dg.mon_dist <= 1u) {                         /* its health */
            int32_t w = 60 * clamp(dg.mon_hp, 0, dg.mon_max) / (dg.mon_max ? dg.mon_max : 1);
            dg_vrect(cx - 30, DG_Y0 + 3, 60, 4, ux_mix(T_BG, T_SURF, 50));
            dg_vrect(cx - 30, DG_Y0 + 3, w, 4, T_ACCENT);
            if (dg.mon_hit) {
                b[0] = '-';
                fmt_int(b + 1, dg.dmg);
                cv_text_on(cx + 40, DG_Y0 + 10, &AF_S, b, T_TEXT, ceil);
            }
        }
    } else {
        dg_weapons(cx, cy);
    }
    if (dg.mon_atk)                                      /* the monster strikes: the view's frame */
        cv_frame(DG_X0, DG_Y0, DG_W, DG_H, T_ACCENT);
    /* the depth, the kills, a message */
    b[0] = 'B';
    fmt_int(b + 1, dg.floor);
    cv_text_on(DG_X0 + 4, DG_Y0, &AF_S, b, T_TEXT, ceil);
    fmt_int(b, dg.kills);
    str_cpy(b + str_len(b), " KO", 8);
    cv_text_r(DG_X0 + DG_W - 4, DG_Y0, &AF_S, b, T_MID, ceil);
    if (!song.playing || !dg_party_n())
        cv_text_in(DG_X0, DG_Y0 + 14, DG_W, &AF_S, !dg_party_n() ? "NO PARTY: ADD STEPS" : "CAMP", T_TEXT, ceil);
    else if (dg.msg_f && dg.msg)
        cv_text_in(DG_X0, DG_Y0 + 14, DG_W, &AF_S, dg.msg, T_TEXT, ceil);
party:
    /* the party: four slots, class, level, health */
    for (k = 0; k < 4; k++) {
        int32_t x = 6 + k * 58, w = 54;
        uint16_t bg = dg.in[k] ? T_RAISE : T_SURF;
        cv_rrect(x, DG_HUD, w, 22, 4, bg, T_SURF);
        if (!dg.in[k]) {
            cv_text_on(x + 4, DG_HUD + 3, &AF_S, DG_CLASS[k], T_DIM, bg);
            continue;
        }
        if (dg_fallen(k)) {                              /* fallen: its track cut (DUNGEON) or only down (the slot: the track) */
            cv_text_in(x, DG_HUD + 3, w, &AF_S, dg_deadly() ? "CUT" : "KO", dg_deadly() ? T_ACCENT : T_DIM, bg);
            continue;
        }
        {   /* (health hidden: a wounded one's name blinks, under a third) */
            uint32_t hurt = dg.hp[k] * 3 < dg_hpmax(k) && ((fm1_ms >> 8) & 1u);
            cv_text_on(x + 4, DG_HUD + 3, &AF_S, DG_CLASS[k], dg.swing[k] || hurt ? T_ACCENT : T_TEXT, bg);
        }
        b[0] = 'L';
        b[1] = (char)('0' + dg.lvl[k] % 10u);
        b[2] = 0;
        cv_text_r(x + w - 4, DG_HUD + 3, &AF_S, b, T_MID, bg);
    }
}
