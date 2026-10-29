/* SPDX-License-Identifier: GPL-3.0-only
 * Dungeon Tracker: SELECT navigates (ui.c sel_mode, ui_menu.c menu_walk) through ui_test.c's harness.
 *   build/host/nav_test             (tests/run_rondo.sh)
 * 1. power-on (every blank setting): MENU > SELECT reads PAGE.
 * 2. HOME: SELECT steps HOME, MIXER, CLOCK and round; the tempo stays.
 * 3. a family: EDIT tapped, SELECT steps its pages (EDIT 1, EDIT 2, DICE, MORPH .. as the track's engine shows them),
 *    right and back left to where it began; the tempo stays.
 * 4. the tempo: GLO + SELECT; HOME > CLOCK's KNOB 1.
 * 5. the MENU: SELECT walks every row, a tab's last row on to the next tab's first; MI_COUNT detents come round to
 *    the start, every tab visited; left walks back; KNOB 1 still sets the row walked to; PRESETS and ALGORITHM still
 *    move as before.
 * 6. MENU > SELECT BPM: SELECT is the tempo again (the stored bit set); PAGE back (cleared); saved with the settings.
 * 7. ABOUT: SELECT scrolls it. */
#define UI_TEST_NO_MAIN 1
#include "ui_test.c"

static int fails;
static void okn(const char *what, int ok) { fails += check(what, ok); }
static char what[200];
static const char *where(void) { return ui.home ? "HOME" : cur_page()->title; }

int main(void)
{
    int16_t bpm;
    uint32_t i, k;

    /* 1. */
    ui_power_on();
    ui_prefs2 = 0;
    frames(20);
    okn("power-on: MENU > SELECT reads PAGE (a blank setting)", sel_mode() == SEL_PAGE);
    {   /* Dungeon Tracker: blank settings come up in NIGHT */
        uint32_t keep = settings.palette;
        settings.magic = 0;
        settings_init();
        okn("  blank settings: the palette NIGHT", settings.palette == UI_NIGHT_INDEX && !strcmp(UI_PALETTES[settings.palette].name, "NIGHT"));
        settings.palette = keep;
        palette_apply();
    }

    /* 2. HOME's round */
    go_home();
    frames(5);
    bpm = song.g[G_BPM];
    {
        char seen[3][12];
        for (i = 0; i < 3u; i++) {
            turn(EN_SELECT, 1);
            frames(3);
            str_cpy(seen[i], where(), sizeof seen[i]);
        }
        snprintf(what, sizeof what, "HOME: SELECT steps %s, %s, %s; the tempo stays", seen[0], seen[1], seen[2]);
        okn(what, str_eq(seen[0], "MIXER") && str_eq(seen[1], "CLOCK") && str_eq(seen[2], "HOME") && song.g[G_BPM] == bpm);
    }

    /* 3. a family */
    track_select(3);
    press(B_EDIT);
    frames(5);
    {
        char first[12], list[120] = "";
        uint32_t n = 0, back = 0;
        str_cpy(first, where(), sizeof first);
        for (i = 0; i < 12u; i++) {
            turn(EN_SELECT, 1);
            frames(3);
            if (str_len(list) < 100u) { strcat(list, " "); strcat(list, where()); }
            n++;
            if (str_eq(where(), first))
                break;
        }
        for (k = 0; k < n; k++) {
            turn(EN_SELECT, -1);
            frames(3);
        }
        back = str_eq(where(), first);
        snprintf(what, sizeof what, "EDIT's family on T4 from %s:%s; left as far back: %s", first, list, back ? "the start" : where());
        okn(what, n >= 3u && i < 12u && back && song.g[G_BPM] == bpm && strstr(list, "DICE") && strstr(list, "MORPH"));
    }

    /* 4. the tempo */
    btn_down(B_GLO);
    frame();
    turn(EN_SELECT, 3);
    frame();
    btn_up(B_GLO);
    frames(400);
    okn("the tempo: GLO + SELECT", song.g[G_BPM] == bpm + 3);
    go_title("CLOCK");
    frames(5);
    bpm = song.g[G_BPM];
    turn(EN_K1, 2);
    frames(3);
    okn("  HOME > CLOCK's KNOB 1", song.g[G_BPM] == bpm + 2);

    /* 5. the MENU */
    go_home();
    frames(5);
    hold(B_HOME);
    frames(5);
    {
        uint32_t start = ui.menu_sel, tabs = 0, steps = 0, ok = ui.menu == 1u, prevtab = menu_tab();
        tabs |= 1u << prevtab;
        for (i = 0; i < MI_COUNT && ok; i++) {
            uint32_t was = ui.menu_sel, t0 = menu_tab();
            turn(EN_SELECT, 1);
            frame();
            steps++;
            if (menu_tab() != t0)                         /* a tab change: from its last row to the next's first */
                ok &= was == mtab_first(t0) + mtab_rows(t0) - 1u && ui.menu_sel == mtab_first(menu_tab()) &&
                      menu_tab() == (t0 + 1u) % MTAB_COUNT;
            else
                ok &= ui.menu_sel == was + 1u;
            tabs |= 1u << menu_tab();
            if (ui.menu_sel == start)
                break;
        }
        snprintf(what, sizeof what, "the MENU: SELECT walks every row, tab to tab, round in %u detents (%u rows), every tab", steps, (unsigned)MI_COUNT);
        okn(what, ok && ui.menu_sel == start && tabs == (1u << MTAB_COUNT) - 1u && ui.menu == 1u);
        turn(EN_SELECT, -1);
        frame();
        k = (start + MI_COUNT - 1u) % MI_COUNT;
        okn("  left walks back (from the first row to the last tab's last)", ui.menu_sel == k);
        turn(EN_SELECT, 1);
        frame();
    }
    {   /* KNOB 1 sets the row walked to: walk to HELP, turn it on */
        uint32_t guard = 0;
        while (ui.menu_sel != MI_HELP && guard++ < 2u * MI_COUNT) {
            turn(EN_SELECT, 1);
            frame();
        }
        turn(EN_K1, 1);
        frame();
        okn("  KNOB 1 sets the row SELECT walked to (HELP ON)", ui.menu_sel == MI_HELP && help_on());
        turn(EN_K1, -1);
        frame();
    }
    {   /* PRESETS, ALGORITHM as before */
        uint32_t t0 = menu_tab(), r0 = ui.menu_sel;
        turn(EN_ALGO, 1);
        frame();
        k = menu_tab() == (t0 + 1u) % MTAB_COUNT;
        turn(EN_PRESET, 1);
        frame();
        okn("  PRESETS and ALGORITHM still move rows and tabs", k && menu_tab() == (t0 + 1u) % MTAB_COUNT && ui.menu_sel != r0);
    }

    /* 6. BPM again */
    {
        uint32_t guard = 0;
        while (ui.menu_sel != MI_SELECT && guard++ < 2u * MI_COUNT) {
            turn(EN_SELECT, 1);
            frame();
        }
        turn(EN_K1, -1);                                  /* PAGE -> BPM */
        frame();
        k = sel_mode() == SEL_BPM && (ui_prefs2 & (PREF_SEL_PAGE >> 16));
        press(B_HOME);
        frames(5);
        go_home();
        frames(5);
        bpm = song.g[G_BPM];
        turn(EN_SELECT, 2);
        frame();
        okn("MENU > SELECT BPM: SELECT is the tempo again (stored: the bit set)", k && song.g[G_BPM] == bpm + 2 && ui.home);
        {
            persist_t p;
            settings_export(&p);
            ui_prefs2 = 0;
            okn("  saved with the settings", settings_import(&p, (int)sizeof p) == 1 && sel_mode() == SEL_BPM);
        }
        menu_put(MI_SELECT, SEL_PAGE);
        okn("  PAGE back: the bit clear (as a blank setting)", sel_mode() == SEL_PAGE && !(ui_prefs2 & (PREF_SEL_PAGE >> 16)));
    }

    /* 7. ABOUT scrolls with SELECT */
    hold(B_HOME);
    frames(3);
    ui.menu = 2;
    ui.menu_scroll = 0;
    ui.force = 1;
    frame();
    turn(EN_SELECT, 3);
    frame();
    okn("ABOUT: SELECT scrolls it", ui.menu == 2u && ui.menu_scroll > 0u);
    press(B_HOME);
    frames(3);

    printf("nav: %s\n", fails ? "FAILED" : "all checks ok");
    return fails != 0;
}
