/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright (C) 2026 Dungeon Tracker (RONDO) contributors */
/* Dungeon Tracker 1.1: eight engines, matched to the dice (ui_dice.c); 1.5: four families of two, FM (GENESIS,
 * GEN KIT), WAVE (WAVES, WAVE 32), AMIGA'D (MOD, MOD KIT), SUB (ANALOG, back, and SID). The others are retired: PHASE
 * VOICE TRIO WHEEL DRUM NOISE FM6 (1.1), FMTONE CHIP SAMPLE GRAIN PHYS (1.5; and Felucca's DIGITAL, SLICE before them). Their numbers stay reserved (projects, user presets, favourites, the editor protocol and MIDI hold
 * engine numbers: engines.c ENGINES[] keeps each slot, as a silent stub, ENG_GONE) and a sound of one plays as its
 * substitute: ENG_SUBST's engine, its first factory preset of the old sound's category (category.c; the old engines'
 * category strings stay for this), so a bass stays a bass, a kit a kit. Included by ui.c after category.c */
#ifndef DT_RETIRE_C
#define DT_RETIRE_C 1
/* the substitute of a sound of retired engine e of category c: ENG_SUBST's, but SAMPLE's sounds other than its kits
 * (PIANO, FLUTE) GENESIS' (1.5: its kits MOD KIT's; PHYS's drums MOD KIT's too) */
static uint32_t eng_subst_c(uint32_t e, uint32_t c)
{
    if (c == CAT_NONE || c == CAT_OTHER)
        c = cat_of_engine(eng_idx(e));
    if (eng_idx(e) == ENGI_PHYS && c == CAT_DRUM)       /* (1.5: PHYS's HAND DRUM, TOMS: MOD KIT's) */
        return ENGI_MKIT;
    return eng_idx(e) == ENGI_SAMPLE && c != CAT_DRUM ? ENGI_GENESIS : eng_subst(e);
}
/* .. of its factory preset k */
static uint32_t eng_subst_k(uint32_t e, uint32_t k)
{
    return eng_subst_c(e, eng_idx(e) == ENGI_SAMPLE && k == SMP_SET_PERC ? CAT_DRUM : preset_cat(eng_idx(e), k));   /* (SAMPLE's
                                                           * once PERC, 4: a kit, as ever) */
}
/* the substitute's preset for a sound of retired engine e of category c: its first of that category (none: its first) */
static uint32_t eng_subst_cat(uint32_t e, uint32_t c)
{
    uint32_t ne = eng_subst_c(e, c), i;
    const engine_t *n = ENGINES[ne];
    if (c == CAT_NONE || c == CAT_OTHER)
        c = cat_of_engine(eng_idx(e));
    for (i = 0; i < n->npresets; i++)
        if (preset_cat(ne, i) == c)
            return i;
    return 0;
}
/* .. for factory preset k of retired engine e (1.5: SAMPLE's DUNGEON KIT and LOFI KIT, 1 and 3, MOD KIT's of the name,
 * rebuilt there; its FLUTE GENESIS' FLUTE) */
static uint32_t eng_subst_preset(uint32_t e, uint32_t k)
{
    if (eng_idx(e) == ENGI_SAMPLE && k >= 1u && k <= SMP_SET_PERC)   /* (and its FLUTE GENESIS' FLUTE, 16; PERC a kit) */
        return k == 1u ? 1u : k == 2u ? 16u : k == 3u ? 2u : 0u;
    return eng_subst_cat(e, preset_cat(eng_idx(e), k));
}
/* a sound's values (P_COUNT of them) of retired engine e, category c, as its substitute's preset pi: E1..E8, the ENV
 * and the filter envelope the preset's (the old values meant another engine's), a kit's voices and GENESIS' operators
 * too (1.5), the rest as they were. -> the substitute engine */
static uint32_t eng_retire_values(uint32_t e, uint32_t c, uint32_t pi, int16_t *p)
{
    uint32_t ne = eng_subst_c(e, c), i;
    const engine_t *n = ENGINES[ne];
    if (n->npresets) {
        const preset_t *pr = &n->presets[pi % n->npresets];
        for (i = 0; i < 8u; i++)
            p[P_E0 + i] = (int16_t)pr->e[i];
        p[P_ATK] = pr->env[0];
        p[P_DEC] = pr->env[1];
        p[P_SUS] = pr->env[2];
        p[P_REL] = pr->env[3];
        p[P_ED_FLT] = pr->fenv;
        eng_own_values(ne, pi, p);
    }
    return ne;
}
#endif
