/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright (C) 2026 RONDO contributors. Built on Felucca (C) 2026 Leo Kuroshita (@kurogedelic), Hügelton Instruments */
/* RONDO's HOME: a first-person dungeon crawl in the style of the old grid crawlers, played by the song.
 * TWO STATES. Off (the default, at every power-on): HOME's corridor is scenery only. While the transport plays the
 * party walks it a square a beat, and the HUD shows each track's adventurer and level; nothing is rolled, no monster
 * comes, nothing is cut. MENU > GAME > DUNGEON ON (hold HOME for the MENU; dg_game, not saved) starts a run at B1:
 * the game below plays on every page until it is switched off, which ends the run and lifts every cut at once.
 * The game: the party walks one square a beat (the tempo's), monsters come up the corridor and are fought in turns
 * (Dungeon Tracker: a turn a sixteenth, the monster's then T1..T4's, each hero acting on what its track played;
 * dg_sixteenth below). The song is the party:
 *   - every track that has steps and is not muted is one adventurer: T1 the ROGUE (a sword; dodges on a drumless
 *     sixteenth), T2 the CLERIC (heals the party), T3 the FIGHTER (an axe, more hit points), T4 the MAGE (bolts);
 *   - the party's LEVEL (1..9, everyone's the same) starts at 1 with the run; every boss slain, everyone goes up one
 *     (dg.plv, at most DG_LVMAX);
 *   - a floor is cleared by its boss: DG_BOSS_AT monsters slain on it (a pair counts two) and the ogre comes; slain,
 *     the party levels up and takes the stairs DG_STAIRS squares on, one floor down. Monsters get stronger with the
 *     depth and the level and match the party (its size, how busy: DIFFICULTY, at dg_avg10); a party that falls
 *     retreats one floor up, healed, its level kept, and clears that floor again.
 * Each adventurer's health is hidden (no bar: a wounded one's name blinks): its most is 6 + 4 per level (the fighter 4
 * more); a level up gives the four at once.
 * A FALL CUTS THE TRACK (whatever HOME shows, whatever page is up): an adventurer at 0 health
 * falls and its track is muted as by a PERFORM black key (dg_cut: perform.c ramps it out and back, the sequencer runs
 * on, nothing is saved) until it is back on its feet:
 *   - the cleric, standing, raises a fallen one: a chance each beat walked (1 in 5), at half health;
 *   - the stairs: everyone fallen gets up at half health on the next floor;
 *   - the whole party fallen: the song is silent for a bar (DG_WIPE beats), then they retreat a floor up, healed;
 *   - stopped: no cuts; stopping is a camp, the fallen are raised.
 * Muting or emptying a fallen track does not raise it: it stays fallen until one of the above.
 * The game runs on every page (ui_input.c calls dg_tick), not only while HOME is drawn.
 * CROSSROADS (the game on or off): the sequence is counted in phrases (a pass of the longest pattern playing: dg_ref).
 * Every other phrase, as it starts, a crossroads is placed as many squares ahead as the phrase has beats, so the party
 * reaches it as the phrase ends; there it goes straight on, left or right, a third each (a party facing a monster goes
 * straight on). A turn pans the view to the new hallway over half a beat (DG_TURN), walking on, and the compass (N E S
 * W, by the floor) follows. With the game on a fight holds the walk: the crossroads comes when they get there.
 * Stopped: the party camps (no walking, they heal). Everything is drawn from the palette's colours (the walls RAISE
 * into BG with the distance, the monsters ACCENT, the steel TEXT), so it follows MENU > COLOR. The game is the UI's
 * own: nothing of it is stored, and it starts over at power-on. RONDO_LAYOUT=0 builds (Felucca's behaviour) never cut. */

#define DG_X0 6                                          /* the view: x 6..233, y 3..94 */
#define DG_W 228
#define DG_Y0 3
/* the view's height and the party's row: 91 and 97 on HOME's panel; 1.1, MENU > LARGE: the whole screen under the
 * header (dg_full_draw), 184 and 190, the corridor's heights x2 (DG_VS) and its widths x1.25 (DG_HS), drawn in two
 * bands of 108 rows (the canvas holds 124): dg_band 1 the top (the depth, the compass, the messages, the monster's
 * health), 2 the bottom (the party's row); 0 one canvas (the panel) */
static int32_t dg_vh = 91, dg_hud = 97, dg_vs = 256, dg_hs = 256;
static uint8_t dg_band;
#define DG_H dg_vh
#define DG_HUD dg_hud
#define DG_TOP (dg_band != 2u)                           /* draw what sits at the top of the view */
#define DG_LOW (dg_band != 1u)                           /* .. the party's row */
#define DG_BOSS_AT 10                                    /* monsters slain on a floor: its boss comes */
#define DG_STAIRS 2                                      /* squares from the boss slain to the stairs */
#define DG_LVMAX 9                                       /* the party's highest level */
#ifndef DG_EDGE
#define DG_EDGE 10                                       /* the monsters' match: the party's level + 1 (x 10) */
#endif
#define DG_NSEG 6                                        /* corridor segments drawn */
#define DG_WIPE 4                                        /* beats of silence when the whole party falls */
#define DG_RAISE 5                                       /* the cleric raises: 1 in DG_RAISE beats walked */

enum { DG_RGE, DG_CLR, DG_FTR, DG_MAG };   /* RE-1: T1 the rogue, T3 the fighter (once fighter and dwarf) */
static const char *const DG_CLASS[4] = {"RGE", "CLR", "FTR", "MAG"};
static const char *const DG_FALL[2][4] = {{"ROGUE FALLS", "CLERIC FALLS", "FIGHTER FALLS", "MAGE FALLS"},
                                          {"ROGUE FALLS: T1 CUT", "CLERIC FALLS: T2 CUT", "FIGHTER FALLS: T3 CUT",
                                           "MAGE FALLS: T4 CUT"}};
static const char *const DG_RAISED[4] = {"T1 BACK", "T2 BACK", "T3 BACK", "T4 BACK"};
#if RONDO_LAYOUT
/* Dungeon Tracker: THE PARTY (core.h): an adventurer's class is its track's (CLS_ROGUE .. CLS_MAGE are DG_RGE ..
 * DG_MAG), no longer its number: two rogues, no cleric, any party the points allow. dg_cls(k), track k's class */
static uint32_t dg_cls(uint32_t k) { return trk_cls(k); }
typedef char dg_cls_order[CLS_ROGUE == DG_RGE && CLS_CLERIC == DG_CLR && CLS_FIGHTER == DG_FTR && CLS_MAGE == DG_MAG ? 1 : -1];
#else
static uint32_t dg_cls(uint32_t k) { return k; }
#endif
static char dg_fall_msg[24];                             /* "FIGHTER FALLS: T3 CUT" (dg_fall) */
static const char *dg_fall(uint32_t p, int deadly)       /* track p's adventurer falls: what HOME says */
{
    static const char *const NM[4] = {"ROGUE", "CLERIC", "FIGHTER", "MAGE"};
    uint32_t n;
    str_cpy(dg_fall_msg, NM[dg_cls(p) & 3u], sizeof dg_fall_msg);
    str_cpy(dg_fall_msg + str_len(dg_fall_msg), deadly ? " FALLS: T1 CUT" : " FALLS", sizeof dg_fall_msg - str_len(dg_fall_msg));
    n = str_len(dg_fall_msg);
    if (deadly)
        dg_fall_msg[n - 5u] = (char)('1' + p % 4u);
    return dg_fall_msg;
}
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
    uint8_t mon2, mon2_hit, boss_due;                    /* 1.1: a second monster beside it (0: none), its hit's
                                                          * frames; the floor's boss waiting for this fight's end */
    int16_t mon2_hp, mon2_max;
    char pair_msg[24];                                   /* "GOBLIN AND SLIME" */
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
    /* the crossroads */
    uint32_t phrase;                                     /* phrases played (dg_ref's loops) */
    uint16_t last_idx;                                   /* dg_ref's step at the last tick */
    uint8_t ref;                                         /* the track whose loop is the phrase */
    int8_t turn;                                         /* a turn going: -1 left, +1 right; 0 none */
    uint8_t heading;                                     /* 0 N, 1 E, 2 S, 3 W */
    uint32_t junc, jlast;                                /* the crossroads' square (0: none), the one just passed */
    uint32_t tph;                                        /* the turn's way, 0..65535 */
    uint32_t seed;                                       /* the hallway: its walls' pattern */
    uint16_t way[3];                                     /* straight on, left, right: how often (tests/dungeon_test.c) */
    /* Dungeon Tracker: the fight in turns (dg_sixteenth) */
    uint32_t sstep;                                      /* the next sixteenth to read (seq.c dgq) */
    uint8_t tslot;                                       /* whose turn the next sixteenth is: 0 the monster, 1..4 T1..T4 */
    uint8_t charge[4];                                   /* turns passed without a note (the next action: +100 % each) */
    uint8_t chan[4];                                     /* the cleric's heal growing while held, the mage's power stored */
    uint8_t guard;                                       /* bit k: T(k+1) rested on its turn, harder to hit */
    struct { uint8_t on, fl, tie, rest; uint16_t w; } pw[4];   /* what each track played since its hero's last turn */
    uint16_t act[4];                                     /* how often a hero finds something to do, Q8 (dg_activity) */
    uint32_t dodged;                                     /* the rogue's dodges (tests/dungeon_test.c) */
    uint8_t plv;                                         /* the party's level, 1..DG_LVMAX (0: not yet: 1) */
    uint8_t fkills;                                      /* monsters slain on this floor (to DG_BOSS_AT: the boss) */
    uint8_t boss_up;                                     /* this floor's boss: 1 come (or coming), 2 slain */
    uint8_t stairs;                                      /* squares to the stairs (0: not yet) */
} dg = {0, 0, 0, 0, 1, 1};
#define DG_TURN 2u                                       /* a turn lasts 1 / DG_TURN beat */
static const char DG_HEAD[4] = {'N', 'E', 'S', 'W'};
static void dg_step_on(void);                          /* (a square walked: the crossroads there?) */

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
static uint8_t dg_played;                                /* the transport played since power-on (HOME's PRESS PLAY) */
/* a message on a plate of the view's far colour, centred across the view (over a monster too: it reads) */
static void dg_plate(int32_t cx, uint16_t ceil, const char *mm)
{
    int32_t w = text_w(&AF_S, mm) + 12, x = cx - w / 2;
    uint16_t plate = ux_mix(T_BG, ceil, 40);
    cv_rrect(x, DG_Y0 + 12, w, AF_S_CAP_H + 8, 3, plate, ceil);
    GFX_HOOK_ALIGN(DG_X0, 0, DG_X0 + DG_W, 0, AL_H | AL_PASS, "dungeon message centred across the view");
    cv_text_in(DG_X0, DG_Y0 + 14, DG_W, &AF_S, mm, T_TEXT, plate);
}
static void dg_say(const char *m)
{
    dg.msg = m;
    dg.msg_f = 40;
}

/* the party's level: everyone's (the run's bosses slain + 1) */
static uint32_t dg_plv(void) { return dg.plv ? dg.plv : 1u; }
/* how often the hero finds something to do on its turn (Q8, 256 = always): the share of its five sixteenths with a new
 * note (a tie: half for the casters, a little for the weapons), as one turn of five catches one. A quiet party meets
 * smaller monsters (dg_walk) */
static uint32_t dg_activity(const track_t *t, uint32_t k)
{
    static const uint8_t PER[10] = {4, 2, 1, 1, 1, 1, 8, 16, 32, 64};   /* sixteenths a step, by DIV (N_DIV) */
    uint32_t len = (uint32_t)clamp(t->p[P_SLEN], 1, NSTEP), i, on = 0, tie = 0, x, r = 256u;
    const step_t *s = seq_steps(t);
    for (i = 0; i < len; i++) {
        on += step_on(&s[i]);
        tie += s[i].time == ST_TIE;
    }
    x = (on * 256u + tie * (k == 1u || k == 3u ? 128u : 38u)) / (len * PER[clamp(t->p[P_SDIV], 0, 9)]);
    x = x > 256u ? 256u : x;
    for (i = 0; i < 5u; i++)
        r = r * (256u - x) / 256u;                       /* (1 - x)^5: a turn's five sixteenths, none with a note */
    return 256u - r;
}
static int16_t dg_hpmax(uint32_t k) { return (int16_t)(6 + 4 * dg.lvl[k] + (dg_cls(k) == DG_FTR ? 4 : 0)); }
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

/* who is in the party: the tracks with steps, not muted (once a second); their level, the party's */
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
        dg.lvl[k] = (uint8_t)(in ? dg_plv() : 0u);
        dg.act[k] = (uint16_t)(in ? dg_activity(t, k) : 0u);
        if (dg_fallen(k))
            dg.hp[k] = 0;                                /* (fallen: until raised, joining or not) */
        else if (joins)
            dg.hp[k] = dg_hpmax(k);                      /* a new adventurer: at full */
        else if (in && dg_hpmax(k) > was)
            dg.hp[k] = (int16_t)(dg.hp[k] + dg_hpmax(k) - was);   /* (more health: at once) */
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
/* DIFFICULTY (Dungeon Tracker 0.1, "moderate"): the monsters match the party as well as the depth, so a busy song meets
 * tougher ones and a fight lasts about three beats, long enough for them to strike back; walking heals every other
 * square. A monster's health 4 + 3 x floor + 16 x the party's average level (an ogre x 3); its blow 1 + floor / 4 +
 * 1.3 x the average level (an ogre + 3), at 60 %. Simulated (tests/dungeon_test.c too): a typical power-on song, ten
 * minutes: about 8 falls and a whole-party fall, the first fall near five minutes; a busy song falls less and goes
 * deepest, a sparse loop struggles most */
static int32_t dg_avg10(void)                            /* the party's average level x 10 (those in it) */
{
    uint32_t k, s = 0, n = dg_party_n();
    for (k = 0; k < 4u; k++)
        s += dg.in[k] ? dg.lvl[k] : 0u;
    return (n ? (int32_t)(s * 10u / n) : 10) + DG_EDGE;
}

static uint32_t dg_act_avg(void)                        /* the party's activity, Q8 (those in it) */
{
    uint32_t k, a = 0, n = dg_party_n();
    for (k = 0; k < 4u; k++)
        a += dg.in[k] ? dg.act[k] : 0u;
    return n ? a / n : 256u;
}
/* -------------------------------------------------------------------- FLOOR FX (1.1) --- */
/* Each floor down (the game on) puts one effect on one track: a track of the party at random, and an effect at random
 * among those that fit it (ECHO CAVERN CHORUS DRIVE: its sends, raised; CRUSH PHASER FLANGER MUFFLE PUMP: its INSERT,
 * when that is OFF; SLICER: its SLICER, when that is OFF; PUMP: ducked by T1's kick, not on T1). Never on a value
 * automation or a lock moves, or SOUND MORPH holds; not the same effect on the same track twice running. It lasts the
 * floor: the next floor, a retreat, a stop (the camp) or the game switched off lifts it, putting each value back unless
 * it was turned meanwhile (then the turn stays). It lives in the track's values only while playing, so no save holds it
 * (projects and user presets are saved stopped, the autosave writes stopped: by then it is lifted). HOME says which. */
enum { DGF_ECHO, DGF_CAVERN, DGF_CHORUS, DGF_DRIVE, DGF_CRUSH, DGF_PHASER, DGF_FLANGER, DGF_MUFFLE, DGF_PUMP, DGF_SLICER,
       DGF_N };
static const char *const DGF_NAME[DGF_N] = {"ECHO", "CAVERN", "CHORUS", "DRIVE", "CRUSH", "PHASER", "FLANGER", "MUFFLE",
                                            "PUMP", "SLICER"};
static struct {
    uint8_t on, trk, fx, n, last_trk, last_fx;
    uint8_t id[5];
    int16_t was[5], set[5];
    char msg[24];
} dgf = {0, 0, 0, 0, 0xFF, 0xFF, {0}, {0}, {0}, ""};
static int dgf_free(uint32_t k, uint32_t id)            /* nothing else moves id on track k while it plays */
{
    return !((motion_active[k][id / 32u] >> (id % 32u)) & 1u) && !(mo.on[k] && morph_ok(&trk[k], id));
}
static int dgf_fits(uint32_t k, uint32_t fx)
{
    const track_t *t = &trk[k];
    uint32_t i;
    switch (fx) {
    case DGF_ECHO: return dgf_free(k, P_DLY) && t->p[P_DLY] < 90;
    case DGF_CAVERN: return dgf_free(k, P_REV) && t->p[P_REV] < 90;
    case DGF_CHORUS: return dgf_free(k, P_CHOR) && t->p[P_CHOR] < 90;
    case DGF_DRIVE: return dgf_free(k, P_DIST) && t->p[P_DIST] < 40;
    case DGF_SLICER:
        for (i = P_SLCR; i <= P_SLDEPTH; i++)
            if (!dgf_free(k, i)) return 0;
        return t->p[P_SLCR] == SL_OFF;
    default:                                            /* the INSERT's */
        if (fx == DGF_PUMP && (k == 0u || seq_is_empty(&trk[0])))
            return 0;
        for (i = P_ITYPE; i <= P_IMIX; i++)
            if (!dgf_free(k, i)) return 0;
        return t->p[P_ITYPE] == IT_OFF;
    }
}
static void dgf_put(uint32_t id, int32_t v)
{
    track_t *t = &trk[dgf.trk];
    v = clamp(v, TP[id].min, TP[id].max);
    dgf.id[dgf.n] = (uint8_t)id;
    dgf.was[dgf.n] = t->p[id];
    dgf.set[dgf.n] = (int16_t)v;
    dgf.n++;
    t->p[id] = (int16_t)v;
}
static void dgf_lift(void)                              /* the floor's effect off: the values back (unless turned) */
{
    uint32_t i;
    if (!dgf.on)
        return;
    for (i = 0; i < dgf.n; i++)
        if (trk[dgf.trk].p[dgf.id[i]] == dgf.set[i])
            trk[dgf.trk].p[dgf.id[i]] = dgf.was[i];
    dgf.on = 0;
    dgf.n = 0;
}
static void dgf_forget(void) { dgf.on = 0; dgf.n = 0; }   /* (a project loaded meanwhile: its values are its own) */
#define DGF_FORGET 1                                     /* (project.c calls it: a build with the dungeon) */
/* a floor down: lift the last, pick a track and an effect, put it on; 0 = none fits anywhere */
static const char *dgf_apply(void)
{
    static const uint8_t INS[DGF_N][5] = {{0}, {0}, {0}, {0},
        {IT_CRUSH, 56, 88, 100, 100},                   /* CRUSH: 8 bits, 8.8 kHz, the LPF open-ish, MIX 79 % */
        {IT_PHASER, 40, 100, 70, 127}, {IT_FLANGER, 30, 90, 80, 110},
        {IT_FILTER, 55, 50, 0, 127},                    /* MUFFLE: a low-pass, a little resonance */
        {IT_DUCK, 100, 50, 40, 127}};                   /* PUMP: T1's kick */
    uint32_t pool[NTRK], np = 0, k, tries, fx, nfit, fits[DGF_N];
    dgf_lift();
    for (k = 0; k < NTRK; k++)
        if (dg.in[k])
            pool[np++] = k;
    if (!np)
        for (k = 0; k < NTRK; k++)
            pool[np++] = k;
    for (tries = 0; tries < np; tries++) {
        uint32_t t0 = dg_rand() % np;
        k = pool[(t0 + tries) % np];
        for (nfit = 0, fx = 0; fx < DGF_N; fx++)
            if (dgf_fits(k, fx) && !(k == dgf.last_trk && fx == dgf.last_fx))
                fits[nfit++] = fx;
        if (nfit)
            break;
    }
    if (tries == np)
        return 0;
    fx = fits[dg_rand() % nfit];
    dgf.trk = (uint8_t)k;
    dgf.fx = (uint8_t)fx;
    dgf.n = 0;
    dgf.on = 1;
    dgf.last_trk = (uint8_t)k;
    dgf.last_fx = (uint8_t)fx;
    switch (fx) {
    case DGF_ECHO: dgf_put(P_DLY, trk[k].p[P_DLY] + 50); break;
    case DGF_CAVERN: dgf_put(P_REV, trk[k].p[P_REV] + 55); break;
    case DGF_CHORUS: dgf_put(P_CHOR, trk[k].p[P_CHOR] + 60); break;
    case DGF_DRIVE: dgf_put(P_DIST, trk[k].p[P_DIST] + 35); break;
    case DGF_SLICER:
        dgf_put(P_SLPAT, 1 + (int32_t)(dg_rand() % 16u));
        dgf_put(P_SLRATE, 1);                           /* (1/16) */
        dgf_put(P_SLDEPTH, 100);
        dgf_put(P_SLCR, SL_GATE);
        break;
    default:
        dgf_put(P_IA, INS[fx][1]);
        dgf_put(P_IB, INS[fx][2]);
        dgf_put(P_IC, INS[fx][3]);
        dgf_put(P_IMIX, INS[fx][4]);
        dgf_put(P_ITYPE, INS[fx][0]);
        break;
    }
    str_cpy(dgf.msg, "STAIRS: T", sizeof dgf.msg);
    fmt_int(dgf.msg + str_len(dgf.msg), (int32_t)k + 1);
    str_cpy(dgf.msg + str_len(dgf.msg), " ", 2);
    str_cpy(dgf.msg + str_len(dgf.msg), DGF_NAME[fx], 10);
    return dgf.msg;
}

/* a monster's health: the depth, the party (its size, level, how busy), a boss x 3 (DIFFICULTY, at dg_avg10) */
static int32_t dg_mon_hp(uint32_t m, uint32_t boss)
{
    int32_t hp = (4 + 3 * (int32_t)dg.floor + 16 * dg_avg10() / 10) * (boss ? 3 : 1) + (m == DM_SLIME ? -2 : 0);
#if RONDO_LAYOUT
    hp = hp * (1 + (int32_t)dg_party_n()) / 5;           /* 1.0: a smaller party, a smaller monster; */
    hp = hp * (77 + 179 * (int32_t)dg_act_avg() / 256) / 256;   /* a quiet one, too; */
    hp = hp * 7 / 5;                                     /* (the turns: tuned, tools/combat_sim.py) */
#endif
    return hp > 1 ? hp : 1;
}
/* the stairs: one floor down, the fallen up at half health, the floor's count from 0, FLOOR FX */
static void dg_stairs(void)
{
    uint32_t k;
    dg.walked = 0;
    dg.fkills = 0;
    dg.boss_up = 0;
    dg.stairs = 0;
    for (k = 0; k < 4u; k++)
        if (dg_fallen(k))
            dg_raise(k, (int16_t)(dg_hpmax(k) / 2));
    dg.floor = (uint8_t)(dg.floor < 99u ? dg.floor + 1u : 99u);
    dg.best = dg.floor > dg.best ? dg.floor : dg.best;
    {
        const char *m = dgf_apply();                     /* FLOOR FX, one effect on one track for the floor */
        dg_say(m ? m : "STAIRS DOWN");
    }
}
/* a monster slain (the boss: everyone up a level, the stairs ahead) */
static char dg_lv_msg[16];                               /* "LEVEL UP: L2" */
static void dg_killed(uint32_t m)
{
    uint32_t k;
    dg.kills++;
    if (dg.fkills < 255u)
        dg.fkills++;
    if (m != DM_OGRE || dg.boss_up != 1u)
        return;
    dg.boss_up = 2;
    dg.stairs = DG_STAIRS;
    if (dg_plv() >= DG_LVMAX) {
        dg_say("OGRE SLAIN");
        return;
    }
    dg.plv = (uint8_t)(dg_plv() + 1u);
    for (k = 0; k < 4u; k++)
        if (dg.in[k]) {
            dg.lvl[k] = dg.plv;
            if (!dg_fallen(k))
                dg.hp[k] = (int16_t)(dg.hp[k] + 4);      /* (the level's four, at once) */
        }
    str_cpy(dg_lv_msg, "LEVEL UP: L", sizeof dg_lv_msg);
    dg_lv_msg[11] = (char)('0' + dg.plv);
    dg_lv_msg[12] = 0;
    dg_say(dg_lv_msg);
}
/* a beat while walking: one square on; a monster may come; the floor's boss; the stairs after it */
static void dg_walk(void)
{
    uint32_t k;
    dg.walked++;
    dg.cell++;
    dg_step_on();
    {   /* (the party's clerics: one standing, else whether there is one at all) */
        uint32_t up = 4u, any = 0, j;
        for (j = 0; j < 4u; j++)
            if (dg.in[j] && dg_cls(j) == DG_CLR) {
                any = 1;
                if (up == 4u && !dg_fallen(j))
                    up = j;
            }
        for (k = 0; k < 4u && (dg.walked & 1u); k++)  /* walking heals a little, the cleric more (every other square) */
            if (dg.in[k] && !dg_fallen(k) && dg.hp[k] < dg_hpmax(k))
                dg.hp[k] = (int16_t)(dg.hp[k] + (up < 4u ? 2 : !any ? 3 : 1) -
                                     (dg_rand() & 1u));  /* (1.0: no cleric in the party, a longer rest) */
        if (dg.dead && up < 4u && dg_roll(100u / DG_RAISE)) {   /* a cleric raises one */
            for (k = 0; k < 4u && !dg_fallen(k); k++)
                ;
            dg_raise(k, (int16_t)(dg_hpmax(k) / 2));
            dg.swing[up] = 10;
            dg_say(DG_RAISED[k]);
        }
    }
    if (dg.stairs && !--dg.stairs) {
        dg_stairs();
        return;
    }
    if (dg.boss_up == 2u)
        return;                                          /* (the boss slain: on to the stairs, nothing more comes) */
    if (dg.fkills >= DG_BOSS_AT && !dg.boss_up) {
        dg.boss_up = 1;
        dg.boss_due = 1;                                 /* (it waits if a monster is on its way) */
    }
    if (dg.boss_up && !dg.boss_due)
        return;                                          /* (the boss is here: nothing else comes) */
    if (dg.boss_due ? !dg.mon : dg_roll(12u + dg.floor)) {
        uint32_t boss = dg.boss_due, m = boss ? DM_OGRE : DM_GOBLIN + dg_rand() % 3u;
        int32_t hp = dg_mon_hp(m, boss);
        if (!dg.mon) {                                   /* a monster comes up the corridor */
            dg.boss_due = 0;
            dg.mon = (uint8_t)m;
            dg.mon_dist = 4;
            dg.mon2 = DM_NONE;
            dg.mon_max = dg.mon_hp = (int16_t)hp;
            dg_say(DM_NAME[dg.mon]);
        } else if (!dg.mon2 && dg.mon != DM_OGRE && dg.floor >= 2u && dg_roll(18u)) {   /* 1.1: now and then a second */
            dg.mon2 = (uint8_t)m;                        /* one joins it (else the one coming stays: 1.0 replaced it) */
            dg.mon2_max = dg.mon2_hp = (int16_t)(hp * 3 / 5 > 1 ? hp * 3 / 5 : 1);
            dg.mon_max = (int16_t)(dg.mon_max * 4 / 5 > 1 ? dg.mon_max * 4 / 5 : 1);   /* (each a little weaker) */
            dg.mon_hp = (int16_t)(dg.mon_hp < dg.mon_max ? dg.mon_hp : dg.mon_max);
            str_cpy(dg.pair_msg, DM_NAME[dg.mon], sizeof dg.pair_msg);
            str_cpy(dg.pair_msg + str_len(dg.pair_msg), " AND ", 6);
            str_cpy(dg.pair_msg + str_len(dg.pair_msg), DM_NAME[dg.mon2], 10);
            dg_say(dg.pair_msg);
        }
    }
}

#if !RONDO_LAYOUT
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
        d = lv + (int32_t)(dg_rand() % (uint32_t)(lv + 1)) + (k == DG_RGE ? 2 : k == DG_FTR ? 1 : k == DG_MAG ? lv : 0);
        dg.mon_hp = (int16_t)(dg.mon_hp - d);
        dg.dmg = (int16_t)d;
        dg.mon_hit = 6;
    }
    if (dg.mon_hp <= 0) {
        dg_say(dg.mon == DM_OGRE ? "OGRE SLAIN" : "SLAIN");
        dg_killed(dg.mon);
        dg.mon = DM_NONE;
        dg.mon_dist = 0;
        return;
    }
    if (!n)
        return;
    pick = alive[dg_rand() % n];
    if (dg_roll(60u)) {
        dg.hp[pick] = (int16_t)(dg.hp[pick] - (1 + dg.floor / 4 + 13 * dg_avg10() / 100 + (dg.mon == DM_OGRE ? 3 : 0)));
        dg.mon_atk = 5;
        if (dg.hp[pick] <= 0) {                          /* it falls: its track is cut (dg_tick) */
            dg.hp[pick] = 0;
            dg.dead |= (uint8_t)(1u << pick);
            dg_say(DG_FALL[dg_deadly()][pick]);
        }
    }
    if (!dg_up()) {                                      /* the party falls: a bar of silence, then the retreat */
        dg.wipe = DG_WIPE;
        dg.mon = dg.mon2 = DM_NONE;
        dg.mon_dist = 0;
        dg.boss_due = 0;
        dg_say("THE PARTY FALLS");
    }
}
#else
/* Dungeon Tracker: THE FIGHT IN TURNS. A round is five sixteenths of the song: the monster's, then T1 T2 T3 T4's,
 * one each, so it turns against the beat (five against four: every turn visits every place in the beat once in five
 * beats). What a hero does on its turn is what its track played since its last turn (seq.c dgq, what really sounded):
 *   - a note: the action, as strong as the strongest note (velocity, accent, the drum, a chord, a chance that played, a
 *     nudge; dgq_on) and a little more for the others (+15 % each, to +45 %), +100 % for each turn charged before it;
 *     the rogue and the fighter strike (a ratchet: as many hits, a slide through a skeleton's armour, an accent may
 *     crit), the mage bolts with the power it stored, the cleric heals one who is hurt (a chord: everyone) or strikes;
 *   - a tie, the note held: a weapon follows through, weaker each turn (1/2, 1/4, never under 1/4); the cleric heals a
 *     little, more each turn held; the mage stores power for its next bolt;
 *   - an explicit REST: it guards (the monster hits it half as often);
 *   - nothing: it charges (up to four turns).
 * The monster strikes one standing adventurer at random; the rogue dodges it when its own sixteenth has no drum.
 * Tuned on the power-on songs and a set of styles (tools/combat_sim.py): about what DIFFICULTY moderate was, the music
 * deciding more (busy and syncopated parts fall least, a lone simple loop most) */
#define DG_CHARGE_MAX 4u
static void dg_slain(void)
{
    dg_say(dg.mon == DM_OGRE ? "OGRE SLAIN" : "SLAIN");
    dg_killed(dg.mon);
    if (dg.mon2) {                                       /* 1.1: a pair: the other steps up, the fight goes on */
        dg.mon = dg.mon2;
        dg.mon_hp = dg.mon2_hp;
        dg.mon_max = dg.mon2_max;
        dg.mon_hit = dg.mon2_hit;
        dg.mon2 = DM_NONE;
        return;
    }
    dg.mon = DM_NONE;
    dg.mon_dist = 0;
}
static int32_t dg_base(uint32_t k)                       /* a blow's dice, by level and class */
{
    int32_t lv = dg.lvl[k];
    return lv + (int32_t)(dg_rand() % (uint32_t)(lv + 1)) + (dg_cls(k) == DG_RGE ? 2 : dg_cls(k) == DG_FTR ? 1 : dg_cls(k) == DG_MAG ? lv * 3 / 4 : 0);
}
static void dg_strike(uint32_t k, int32_t d256, int pierce)   /* damage x 256 */
{
    int32_t d = (d256 + 128) >> 8;
    if (dg.mon == DM_SKEL && !pierce)
        d -= 2;                                          /* (bones: a blade glances, a slide or a spell does not) */
    if (d <= 0 || !dg.mon)
        return;
    dg.mon_hp = (int16_t)(dg.mon_hp - d);
    dg.dmg = (int16_t)d;
    dg.mon_hit = 6;
    if (dg_cls(k) == DG_FTR && dg.mon2 && d >= 2) {              /* 1.1: the axe cleaves: the one beside it takes half */
        dg.mon2_hp = (int16_t)(dg.mon2_hp - d / 2);
        dg.mon2_hit = 6;
        if (dg.mon2_hp <= 0) {
            dg_killed(dg.mon2);
            dg.mon2 = DM_NONE;
            dg_say("CLEAVED");
        }
    }
}
static void dg_heal(uint32_t j, int32_t h)
{
    if (h > 0 && dg.in[j] && !dg_fallen(j))
        dg.hp[j] = (int16_t)clamp(dg.hp[j] + h, 0, dg_hpmax(j));
}
static uint32_t dg_hurt_most(int ratio)                  /* the standing one hurt most (by health, or its share), 4: none */
{
    uint32_t j, w = 4;
    for (j = 0; j < 4u; j++) {
        if (!dg.in[j] || dg_fallen(j) || dg.hp[j] >= dg_hpmax(j))
            continue;
        if (w == 4u || (ratio ? dg.hp[j] * dg_hpmax(w) < dg.hp[w] * dg_hpmax(j) : dg.hp[j] < dg.hp[w]))
            w = j;
    }
    return w;
}
static void dg_hero(uint32_t k)
{
    int32_t lv = dg.lvl[k], d256;
    uint32_t on = dg.pw[k].on, fl = dg.pw[k].fl, tie = dg.pw[k].tie, rest = dg.pw[k].rest, w = dg.pw[k].w, j;
    memset(&dg.pw[k], 0, sizeof dg.pw[k]);
    if (!dg.in[k] || dg_fallen(k))
        return;                                          /* (its turn passes: a round is always five) */
    dg.guard &= (uint8_t)~(1u << k);
    if (!on && !tie) {
        if (rest)
            dg.guard |= (uint8_t)(1u << k);              /* REST: on guard */
        else if (dg.charge[k] < DG_CHARGE_MAX)
            dg.charge[k]++;                              /* nothing: it gathers itself */
        return;
    }
    dg.swing[k] = 10;
    if (!on) {                                           /* a tie: the note held */
        if (dg_cls(k) == DG_CLR) {
            dg.chan[k] = (uint8_t)(dg.chan[k] < 4u ? dg.chan[k] + 1u : 4u);
            j = dg_hurt_most(1);
            if (j < 4u)
                dg_heal(j, ((2 + lv) * dg.chan[k] * 3 + 5) / 10);   /* (1 + level / 2) x turns held x 0.6 */
        } else if (dg_cls(k) == DG_MAG) {
            dg.chan[k] = (uint8_t)(dg.chan[k] < 4u ? dg.chan[k] + 1u : 4u);
        } else {
            int32_t f = tie < 2u ? 128 : 64;             /* 1/2, then 1/4 from the second tie on */
            d256 = dg_base(k) * f;
            if (d256 >= 128 && dg_roll((uint32_t)(50 + 5 * lv)))
                dg_strike(k, d256, 0);
        }
        return;
    }
    {   /* a note: the action */
        uint32_t extra = on - 1u, fx = extra * 15u, acc = fl & DGQ_ACC, r = ((fl >> DGQ_RAT_SH) & 3u) + 1u, i;
        w = w * (1u + dg.charge[k]);
        w = w * (100u + (fx > 45u ? 45u : fx)) / 100u;
        dg.charge[k] = 0;
        if (dg_cls(k) == DG_CLR) {
            dg.chan[k] = 1;
            for (j = 0; j < 4u && !(dg.in[j] && !dg_fallen(j) && dg.hp[j] * 10 < dg_hpmax(j) * 6); j++)
                ;
            if (j < 4u) {                                /* someone under 60 %: a heal */
                if ((fl >> DGQ_CH_SH) & 3u) {            /* a chord: everyone, half as much */
                    for (j = 0; j < 4u; j++)
                        dg_heal(j, (int32_t)(((uint32_t)(2 + lv) * w + 256u) / 512u));
                } else {
                    dg_heal(dg_hurt_most(0), (int32_t)(((uint32_t)(3 + lv) * w + 128u) / 256u));
                }
                return;
            }
            d256 = dg_base(k) * (int32_t)w * 154 / 256;  /* nobody hurt: the mace (x 0.6) */
        } else if (dg_cls(k) == DG_MAG) {
            d256 = dg_base(k) * (int32_t)w * (int32_t)(2u + dg.chan[k]) / 2;   /* + 50 % a turn stored */
            dg.chan[k] = 0;
        } else {
            d256 = dg_base(k) * (int32_t)w;
        }
        if (r > 1u)
            d256 = d256 / (int32_t)r * 13 / 10;          /* a ratchet: its hits, a little more in all */
        for (i = 0; i < r; i++)
            if (dg_roll((uint32_t)(50 + 5 * lv + (acc ? 10 : 0))))
                dg_strike(k, acc && dg_roll(15u) ? d256 * 2 : d256, (fl & DGQ_SLIDE) || dg_cls(k) == DG_MAG);
    }
}
static void dg_monster(const dgq_t *b)                   /* the monster's turn: b, this sixteenth's tracks */
{
    uint32_t k, n = 0, alive[4], p, hit;
    for (k = 0; k < 4u; k++)
        if (dg.in[k] && !dg_fallen(k))
            alive[n++] = k;
    if (!n)
        return;
    dg.mon_atk = 5;
    for (uint32_t m = 0; m < (dg.mon2 ? 2u : 1u); m++) {   /* (1.1: a pair, both strike, each a little lighter) */
        for (k = 0, n = 0; k < 4u; k++)
            if (dg.in[k] && !dg_fallen(k))
                alive[n++] = k;
        if (!n)
            break;
        p = alive[dg_rand() % n];
        hit = (dg.guard >> p) & 1u ? 30u : 60u;
        if (dg_cls(p) == DG_RGE && !b[p].on) {           /* no drum on its sixteenth: the rogue is not there */
            dg.dodged++;
            continue;
        }
        if (dg_roll(hit)) {
            int32_t d = 1 + dg.floor / 4 + 13 * dg_avg10() / 100 + ((m ? dg.mon2 : dg.mon) == DM_OGRE ? 3 : 0);
            d = (d * (1 + (int32_t)dg_party_n()) * 21 + 50) / 100;   /* (a smaller party, a smaller blow; x 1.05) */
            if (dg.mon2)
                d = (d * 2 + 1) / 3;                     /* (two-thirds each, rounded: a pair hits ~1.3x as hard) */
            dg.hp[p] = (int16_t)(dg.hp[p] - (d > 1 ? d : 1));
            if (dg.hp[p] <= 0) {                         /* it falls: its track is cut (dg_tick) */
                dg.hp[p] = 0;
                dg.dead |= (uint8_t)(1u << p);
                dg_say(dg_fall(p, dg_deadly()));
            }
        }
    }
    if (!dg_up()) {                                      /* the party falls: a bar of silence, then the retreat */
        dg.wipe = DG_WIPE;
        dg.mon = dg.mon2 = DM_NONE;
        dg.mon_dist = 0;
        dg.boss_due = 0;
        dg_say("THE PARTY FALLS");
    }
}
static void dg_engage(void)                              /* a fight begins: the monster's turn first */
{
    dg.tslot = 0;
    dg.guard = 0;
    memset(dg.charge, 0, sizeof dg.charge);
    memset(dg.chan, 0, sizeof dg.chan);
    memset(dg.pw, 0, sizeof dg.pw);
}
/* a sixteenth of the song played: what each track did goes to its hero; fighting, it is someone's turn */
static void dg_sixteenth(const dgq_t *b)
{
    uint32_t k, slot;
    for (k = 0; k < 4u; k++) {
        if (b[k].on) {
            dg.pw[k].on = (uint8_t)(dg.pw[k].on + b[k].on < 255u ? dg.pw[k].on + b[k].on : 255u);
            if (b[k].w > dg.pw[k].w) {
                dg.pw[k].w = b[k].w;
                dg.pw[k].fl = b[k].fl;
            }
        }
        if (b[k].tie > dg.pw[k].tie)
            dg.pw[k].tie = b[k].tie;
        dg.pw[k].rest |= b[k].rest;
    }
    if (dg.wipe || !dg.mon || dg.mon_dist > 1u)
        return;
    slot = dg.tslot;
    dg.tslot = (uint8_t)((dg.tslot + 1u) % 5u);
    if (!slot) {
        dg_monster(b);
        return;
    }
    dg_hero(slot - 1u);
    if (dg.mon && dg.mon_hp <= 0)
        dg_slain();
}
/* a whole round as if every track played a plain note on every sixteenth (tests/dungeon_test.c) */
static void dg_fight(void)
{
    dgq_t b[NTRK];
    uint32_t i, k;
    for (i = 0; i < 5u && dg.mon && dg.mon_dist <= 1u && !dg.wipe; i++) {
        for (k = 0; k < NTRK; k++) {
            b[k].on = 1;
            b[k].fl = 0;
            b[k].tie = b[k].rest = 0;
            b[k].w = 256;
        }
        dg_sixteenth(b);
    }
}
#endif
static void dg_retreat(void)                             /* after the fallen party's bar: one floor up, healed */
{
    uint32_t k;
    dgf_lift();                                          /* (1.1: the floor's effect goes with it) */
    dg.floor = (uint8_t)(dg.floor > 1u ? dg.floor - 1u : 1u);
    dg.walked = 0;
    dg.fkills = 0;                                       /* (that floor again, from its first monster) */
    dg.boss_up = 0;
    dg.stairs = 0;
    dg.wipe = 0;
    dg.dead = 0;
    for (k = 0; k < 4u; k++)
        dg.hp[k] = dg.in[k] ? dg_hpmax(k) : 0;
    dg_say("RETREAT");
}

/* MENU > GAME > DUNGEON: on, a new run from B1 (everyone at full); off, the run over, every cut lifted at once */
static void dg_game_set(uint32_t on)
{
    uint32_t r = dg.rng ? dg.rng : fm1_ms * 2654435761u + 1u, cell = dg.cell, seed = dg.seed, head = dg.heading;
    dgf_lift();                                          /* (1.1: the run over, or a new one: no floor effect) */
    dgf.last_trk = dgf.last_fx = 0xFF;
    memset(&dg, 0, sizeof dg);
    dg.floor = dg.best = 1;
    dg.rng = r;
    dg.cell = cell;                                      /* (the corridor goes on where it was, facing the same way) */
    dg.seed = seed;
    dg.heading = (uint8_t)head;
    dg.t_ms = fm1_ms;
    dg_game = (uint8_t)(on != 0u);
    dg_cut = 0;
    dg_party();                                          /* (all of them join at full) */
#if RONDO_LAYOUT
    dg.sstep = dgq.step;
#endif
    if (on)
        dg_say("INTO THE DUNGEON");
}

/* ------------------------------------------------------------------------ the crossroads --- */
/* the track whose loop is the phrase: the longest pattern (in time) of those with steps; none: T1 */
static uint32_t dg_ref(uint32_t *len)
{
    uint32_t k, r = 0, best = 0;
    for (k = 0; k < NTRK; k++) {
        uint32_t l;
        if (seq_is_empty(&trk[k]))
            continue;
        l = (uint32_t)clamp(trk[k].p[P_SLEN], 1, NSTEP) * div_samples((uint32_t)trk[k].p[P_SDIV]);
        if (l > best) {
            best = l;
            r = k;
        }
    }
    *len = best ? best : 16u * div_samples((uint32_t)trk[0].p[P_SDIV]);
    return r;
}
/* the crossroads reached: straight on, left or right, a third each (a monster ahead: straight on, to it) */
static void dg_cross(void)
{
    uint32_t w = dg.mon ? 0u : dg_rand() % 3u;
    dg.jlast = dg.junc;
    dg.junc = 0;
    dg.way[w]++;
    if (w) {
        dg.turn = (int8_t)(w == 1u ? -1 : 1);
        dg.tph = 0;
    }
}
/* a square walked (the game's or the scenery's): the crossroads there? */
static void dg_step_on(void)
{
    if (dg.junc && dg.cell >= dg.junc && !dg.turn)
        dg_cross();
}
/* each tick while playing: a phrase starts when the reference track's step goes back (its loop); every other one
 * places the crossroads its length in beats ahead */
static void dg_phrases(void)
{
    uint32_t len, r = dg_ref(&len), idx = trk[r].seq_idx;
    if (r != dg.ref) {
        dg.ref = (uint8_t)r;
        dg.last_idx = (uint16_t)idx;
        return;
    }
    if (idx < dg.last_idx) {
        dg.phrase++;
        if ((dg.phrase & 1u) && !dg.junc && !dg.turn) {
            uint32_t beat = beat_samples(), b = (len + beat / 2u) / (beat ? beat : 1u);
            dg.junc = dg.cell + (b ? b : 1u);
        }
    }
    dg.last_idx = (uint16_t)idx;
}
/* a turn under way: half a beat, then facing the new hallway */
static void dg_turning(uint32_t add)
{
    if (!dg.turn)
        return;
    dg.tph += add * DG_TURN;
    if (dg.tph >= 65536u) {
        dg.heading = (uint8_t)((dg.heading + (dg.turn < 0 ? 3u : 1u)) & 3u);
        dg.seed++;
        dg.jlast = 0;
        dg.turn = 0;
        dg.tph = 0;
    }
}

/* off: the scenery's walk only, a square a beat while playing */
static void dg_stroll(uint32_t dt)
{
    uint32_t bpm = (uint32_t)clamp(song.g[G_BPM], 20, 300), add = dt * bpm * 65536u / 60000u;
    dg_cut = 0;
    if (!song.playing)
        return;
    dg_phrases();
    dg_turning(add);
    dg.phase += add;
    while (dg.phase >= 65536u) {
        dg.phase -= 65536u;
        dg.cell++;
        dg_step_on();
    }
}

static void dg_tick(void)
{
    uint32_t now = fm1_ms, dt = dg.t_ms ? now - dg.t_ms : 0, k;
    dg.t_ms = now;
    if (dt > 200u)
        dt = 200u;
    if (song.playing)
        dg_played = 1;                                   /* (played on any page: HOME's PRESS PLAY goes) */
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
    if (dg.mon2_hit)
        dg.mon2_hit--;
    if (dg.msg_f)
        dg.msg_f--;
    if (!song.playing || !dg_party_n()) {                /* camping: no walking, the fallen raised, healing */
        if (!song.playing)
            dgf_lift();                                  /* (1.1: stopped, the floor's effect lifted: no save holds it) */
        if (dg.wipe)
            dg_retreat();
        for (k = 0; k < 4u; k++)
            if (dg_fallen(k))
                dg_raise(k, 1);
        if ((now >> 9) != ((now - dt) >> 9))
            for (k = 0; k < 4u; k++)
                if (dg.in[k] && dg.hp[k] < dg_hpmax(k))
                    dg.hp[k]++;
#if RONDO_LAYOUT
        dg.sstep = dgq.step;                             /* (nothing played meanwhile counts) */
#endif
        dg_cut = 0;
        return;
    }
    {
        uint32_t bpm = (uint32_t)clamp(song.g[G_BPM], 20, 300), add = dt * bpm * 65536u / 60000u;   /* a beat: 65536 */
        dg_phrases();
        dg_turning(add);
        if (!dg.wipe && !dg_up())                        /* (fallen, and nobody else: as a fallen party) */
            dg.wipe = DG_WIPE;
#if RONDO_LAYOUT
        {   /* 1.0: the sixteenths the song played since the last tick: the fight's turns (dg_sixteenth) */
            uint32_t now_s = dgq.step;
            if (now_s - dg.sstep > DGQ_N - 2u)
                dg.sstep = now_s - (DGQ_N - 2u);         /* (far behind: the latest only) */
            while (dg.sstep != now_s) {
                dg_sixteenth(dgq.b[dg.sstep % DGQ_N]);
                dg.sstep++;
            }
        }
#endif
        if (dg.wipe || (dg.mon && dg.mon_dist <= 1u)) {  /* fighting: the view standing still */
            dg.fphase += add;
            if (dg.fphase >= 65536u) {
                dg.fphase &= 0xFFFFu;
                dg.beat_odd ^= 1u;
                if (dg.wipe) {
                    if (!--dg.wipe)
                        dg_retreat();
                }
#if !RONDO_LAYOUT
                else
                    dg_fight();                          /* (a round a beat) */
#endif
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
#if RONDO_LAYOUT
                dg_engage();                             /* (the monster's turn on the next sixteenth) */
#endif
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
        if (open) {                                      /* a side passage: the dark beyond (its far wall), its floor */
            dg_vrect(x, top, 1, 2 * h, ux_mix(mortar, T_BG, 55));
            dg_vrect(x, cy - hb, 1, 2 * hb, ux_mix(mortar, T_BG, 80));
            dg_vrect(x, cy + hb, 1, h - hb, ux_mix(c, T_BG, 40));
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
    uint8_t sw[4] = {0, 0, 0, 0};                        /* (each class's swing: its tracks', the latest) */
    uint32_t k;
    for (k = 0; k < 4u; k++)
        if (dg.swing[k] > sw[dg_cls(k) & 3u])
            sw[dg_cls(k) & 3u] = dg.swing[k];
    if ((f = sw[DG_RGE]) != 0) {                         /* the sword: from the right, across */
        static const int8_t TIP[10][2] = {{-70, -40}, {-62, -48}, {-50, -54}, {-36, -56}, {-20, -54},
                                          {-6, -48}, {6, -38}, {14, -26}, {18, -14}, {20, -4}};
        int32_t hx = 196, hy = DG_Y0 + DG_H - 4, tx = hx + TIP[10 - f][0], ty = hy + TIP[10 - f][1];
        cv_line_t(hx, hy, tx, ty, T_TEXT, 3);
        cv_line_t(hx - 6, hy + 4, hx + 6, hy - 4, T_ACCENT, 3);   /* the crossguard */
    }
    if ((f = sw[DG_FTR]) != 0) {                         /* the axe: from the left, down */
        int32_t hx = 36, hy = DG_Y0 + DG_H - 2, a = 10 - f, tx = hx + 20 + a * 3, ty = hy - 52 + a * 4;
        cv_line_t(hx, hy, tx, ty, T_MID, 3);
        dg_vrect(tx - 6, ty - 4, 12, 9, T_TEXT);
    }
    if ((f = sw[DG_MAG]) != 0 && dg.mon) {               /* the bolt: zigzag to the monster */
        int32_t x0 = dg_cx(), y0 = DG_Y0 + DG_H - 2, k, px = x0, py = y0;
        for (k = 1; k <= 5; k++) {
            int32_t x = x0 + (mx - x0) * k / 5 + ((k & 1) ? 5 : -5) * (k < 5), y = y0 + (my - y0) * k / 5;
            cv_line_t(px, py, x, y, T_ACCENT, 2);
            px = x, py = y;
        }
    }
}

/* the corridor at centre cx: far to near, segment k from depth k to k + 1 (squares), moved on by the walk's phase;
 * seed its walls' pattern, cross: the crossroads shown (both sides open where it is) */
static void dg_corridor(int32_t cx, int32_t cy, uint32_t ph, uint32_t seed, int cross, uint16_t ceil, uint16_t flo,
                        uint16_t far)
{
    int32_t k;
    for (k = DG_NSEG; k >= 0; k--) {
        int32_t za = (k << 8) - (int32_t)(ph >> 8) + 160, zb;   /* depth in 1/256 square (+0.6: the eye) */
        zb = za + 256;
        za = za < 96 ? 96 : za;                          /* (the nearest segment, passing the eye: held at the frame) */
        int32_t wa = 70 * dg_hs / za, ha = 46 * dg_vs / za, wb = 70 * dg_hs / zb, hb = 46 * dg_vs / zb;
        uint32_t cell = dg.cell + (uint32_t)k, h = cell * 2654435761u ^ dg.floor * 40503u ^ seed * 2246822519u;
        int32_t shade = clamp(k * 14 + (int32_t)(ph >> 12), 0, 90);
        int x = cross && cell && (cell == dg.junc || cell == dg.jlast);
        uint16_t wc = ux_mix(T_RAISE, far, shade), mo = ux_mix(wc, far, 45);
        if (k == DG_NSEG) {                              /* the dark at the end of the corridor */
            dg_vrect(cx - wb, cy - hb, 2 * wb, 2 * hb, far);
            continue;
        }
        dg_wall(cx, cy, wa, ha, wb, hb, -1, wc, mo, x || (h >> 7) % 7u == 0u);
        dg_wall(cx, cy, wa, ha, wb, hb, 1, wc, mo, x || (h >> 11) % 7u == 1u);
        dg_vrect(cx - wa, cy + ha, 2 * wa, 1, ux_mix(flo, far, 30));   /* the floor's line, the ceiling's */
        dg_vrect(cx - wa, cy - ha, 2 * wa, 1, ux_mix(ceil, far, 30));
        if (!x && (h >> 15) % 5u == 0u && !((h >> 7) % 7u == 0u)) {   /* a torch on the left wall */
            int32_t tx = cx - (wa + wb) / 2, ty = cy - (ha + hb) / 3, fl = (int32_t)((fm1_ms >> 6) & 1u);
            dg_vrect(tx - 1, ty, 3, 6 * dg_vs / za + 2, T_MID);
            dg_vrect(tx - 2, ty - 4 - fl, 5, 4 + fl, T_ACCENT);
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
        bob = ((int32_t)((ph < 32768u ? ph : 65535u - ph) >> 13) - 2) * dg_vs / 256;
    cy = DG_Y0 + DG_H / 2 - 4 + bob;
    dg_vrect(DG_X0, DG_Y0, DG_W, cy - DG_Y0, ceil);
    dg_vrect(DG_X0, cy, DG_W, DG_Y0 + DG_H - cy, flo);
    if (dg.turn) {                                       /* turning: the hallway left slides out, the new one in */
        int32_t dx = (int32_t)(((uint64_t)dg.tph * DG_W) >> 16), s = dg.turn < 0 ? 1 : -1;
        dg_corridor(cx + s * dx, cy, ph, dg.seed, 1, ceil, flo, far);
        dg_corridor(cx + s * dx - s * DG_W, cy, ph, dg.seed + 1u, 0, ceil, flo, far);
        dg_vrect(cx + s * dx - s * DG_W / 2, DG_Y0, 2, DG_H, ux_mix(T_RAISE, far, 30));   /* the corner */
    } else {
        dg_corridor(cx, cy, ph, dg.seed, 1, ceil, flo, far);
    }
    b[0] = DG_HEAD[dg.heading & 3u];                     /* the compass */
    b[1] = 0;
    if (song.playing)
        dg_played = 1;
    if (!dg_game) {                                      /* off: the scenery, the compass and the party, nothing else */
        if (DG_TOP)
            cv_text_on(DG_X0 + 4, DG_Y0, &AF_S, b, T_MID, ceil);
        if (DG_TOP && !dg_played && dg_party_n())        /* (Dungeon Tracker: until the first PLAY, the way in) */
            dg_plate(cx, ceil, "PRESS PLAY");
        goto party;
    }
    /* the monster */
    if (dg.mon && dg.mon_dist) {
        int32_t z = ((int32_t)dg.mon_dist << 8) - (int32_t)(ph >> 8) + 60;
        int32_t foot = cy + 46 * dg_vs / (z > 64 ? z : 64), px = clamp(46 * 2 * dg_hs / (z > 64 ? z : 64) / 16, 1, dg_hs > 256 ? 9 : 7);
        uint16_t body = dg.mon_hit & 2u ? T_TEXT : T_ACCENT;
        int32_t gap = dg.mon2 ? 7 * px : 0;              /* (1.1: a pair, side by side, the front one left) */
        if (dg.mon == DM_OGRE)
            px = clamp(px * 3 / 2, 1, dg_hs > 256 ? 12 : 9);
        dg_sprite(dg.mon, cx - gap, foot, px, body, T_BG);
        if (dg.mon2)
            dg_sprite(dg.mon2, cx + gap, foot, px, dg.mon2_hit & 2u ? T_TEXT : T_ACCENT, T_BG);
        dg_weapons(cx - gap, foot - 6 * px);
        if (dg.mon_dist <= 1u && DG_TOP && dg.mon2) {    /* their health: two bars */
            int32_t w1 = 40 * clamp(dg.mon_hp, 0, dg.mon_max) / (dg.mon_max ? dg.mon_max : 1);
            int32_t w2 = 40 * clamp(dg.mon2_hp, 0, dg.mon2_max) / (dg.mon2_max ? dg.mon2_max : 1);
            dg_vrect(cx - 44, DG_Y0 + 3, 40, 4, ux_mix(T_BG, T_SURF, 50));
            dg_vrect(cx - 44, DG_Y0 + 3, w1, 4, T_ACCENT);
            dg_vrect(cx + 4, DG_Y0 + 3, 40, 4, ux_mix(T_BG, T_SURF, 50));
            dg_vrect(cx + 4, DG_Y0 + 3, w2, 4, T_ACCENT);
        }
        if (dg.mon_dist <= 1u && DG_TOP) {               /* its health */
            int32_t w = 60 * clamp(dg.mon_hp, 0, dg.mon_max) / (dg.mon_max ? dg.mon_max : 1);
            if (!dg.mon2) {
                dg_vrect(cx - 30, DG_Y0 + 3, 60, 4, ux_mix(T_BG, T_SURF, 50));
                dg_vrect(cx - 30, DG_Y0 + 3, w, 4, T_ACCENT);
            }
            if (dg.mon_hit) {
                b[0] = '-';
                fmt_int(b + 1, dg.dmg);
                cv_text_on(cx + 40, dg.msg_f && dg.msg ? DG_Y0 + 32 : DG_Y0 + 10, &AF_S, b, T_TEXT, ceil);   /* (a
                                                         * message's plate showing: under it, 1.1) */
            }
        }
    } else {
        dg_weapons(cx, cy);
    }
    if (dg.mon_atk)                                      /* the monster strikes: the view's frame */
        cv_frame(DG_X0, DG_Y0, DG_W, DG_H, T_ACCENT);
    /* the depth and the compass, the kills, a message */
    if (!DG_TOP)
        goto party;
    b[0] = 'B';
    fmt_int(b + 1, dg.floor);
    k = cv_text_on(DG_X0 + 4, DG_Y0, &AF_S, b, T_TEXT, ceil);
    b[0] = DG_HEAD[dg.heading & 3u];
    b[1] = 0;
    cv_text_on(k + 6, DG_Y0, &AF_S, b, T_MID, ceil);
    fmt_int(b, dg.kills);
    str_cpy(b + str_len(b), " KO", 8);
    cv_text_r(DG_X0 + DG_W - 4, DG_Y0, &AF_S, b, T_MID, ceil);
    {   /* the message on a plate of the view's far colour (over a monster too: it reads, whatever is behind) */
        const char *mm = !song.playing || !dg_party_n() ? (!dg_party_n() ? "NO PARTY: ADD STEPS" : "CAMP") :
                         dg.msg_f && dg.msg ? dg.msg : 0;
        if (mm)
            dg_plate(cx, ceil, mm);
    }
party:
    /* the party: four slots, class, level, health */
    for (k = 0; k < 4 && DG_LOW; k++) {
        int32_t x = 6 + k * 58, w = 54;
        uint16_t bg = dg.in[k] ? T_RAISE : T_SURF;
        cv_rrect(x, DG_HUD, w, 22, 4, bg, T_SURF);
        if (!dg.in[k]) {
            cv_text_on(x + 4, DG_HUD + 3, &AF_S, DG_CLASS[dg_cls(k) & 3u], T_DIM, bg);
            continue;
        }
        if (dg_fallen(k)) {                              /* fallen: its track cut (DUNGEON) or only down (the slot: the track) */
            cv_text_in(x, DG_HUD + 3, w, &AF_S, dg_deadly() ? "CUT" : "KO", dg_deadly() ? T_ACCENT : T_DIM, bg);
            continue;
        }
        {   /* (health hidden: a wounded one's name blinks, under a third) */
            uint32_t hurt = dg.hp[k] * 3 < dg_hpmax(k) && ((fm1_ms >> 8) & 1u);
            cv_text_on(x + 4, DG_HUD + 3, &AF_S, DG_CLASS[dg_cls(k) & 3u], dg.swing[k] || hurt ? T_ACCENT : T_TEXT, bg);
        }
        b[0] = 'L';
        b[1] = (char)('0' + dg.lvl[k] % 10u);
        b[2] = 0;
        cv_text_r(x + w - 4, DG_HUD + 3, &AF_S, b, T_MID, bg);
    }
}

/* 1.1, MENU > LARGE: HOME's dungeon on the whole screen under the header (ui_draw.c ui_draw_page, dg_full_on): the
 * panel 216 rows (y 24..239), the view 184 tall, the party's row at its foot; two bands of 108 rows */
#define DG_FULL_Y H_HEAD
#define DG_FULL_H (240 - H_HEAD)
static void dg_full_draw(void)
{
    uint32_t band;
    dg_vh = DG_FULL_H - 3 - 29;
    dg_hud = DG_FULL_H - 26;
    dg_vs = 256 * dg_vh / 91;
    dg_hs = 320;
    for (band = 0; band < 2u; band++) {
        cv_begin(240, DG_FULL_H / 2, T_BG);
        cv_oy = -(int32_t)band * (DG_FULL_H / 2);
        cv_rrect(3, 0, 234, DG_FULL_H, 5, T_SURF, T_BG);
        cv_bg = T_SURF;
        dg_band = (uint8_t)(band + 1u);
        graph_dungeon();
        cv_oy = 0;
        cv_blit(0, DG_FULL_Y + band * (DG_FULL_H / 2));
    }
    dg_band = 0;
    dg_vh = 91;
    dg_hud = 97;
    dg_vs = dg_hs = 256;
}
