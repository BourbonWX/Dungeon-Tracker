/* SPDX-License-Identifier: GPL-3.0-only
 * Dungeon Tracker: the combat study's songs (tools/combat_sim.py): N power-on songs (boot_song.c) as JSON, each track's
 * steps as the sequencer holds them and its dungeon level (1: the party starts at L1, ui_dungeon.c dg_plv).
 *   build/host/combat_export N > songs.json */
#define UI_TEST_NO_MAIN 1
#include "ui_test.c"
#include "../firmware/src/boot_song.c"

static void dump_track(const track_t *t)
{
    uint32_t i, len = (uint32_t)clamp(t->p[P_SLEN], 1, NSTEP);
    printf("{\"len\":%u,\"div\":%d,\"gate\":%d,\"level\":%u,\"kit\":%d,\"steps\":[", len, t->p[P_SDIV], t->p[P_SGATE],
           1u, t->eng_req == ENGI_CKIT);
    for (i = 0; i < len; i++) {
        const step_t *s = &t->step[i];
        printf("%s[%u,%u,%u,%u,%u,%u,%u,%u,%d,%u,%u]", i ? "," : "", s->time, s->n, s->flags & SF_ACCENT ? 1u : 0u,
               s->flags & SF_SLIDE ? 1u : 0u, step_ratchet(s), s->vel, step_lanes(s), step_accents(s), step_nudge(s),
               s->probability, s->n ? s->note[0] : 0u);
    }
    printf("]}");
}

int main(int argc, char **argv)
{
    uint32_t n = argc > 1 ? (uint32_t)atoi(argv[1]) : 40u, s, k;
    printf("[");
    for (s = 0; s < n; s++) {
        ui_power_on();
        boot_song(0x51ED5u + s * 2654435761u);
        boot_song_settle();
        printf("%s{\"name\":\"power-on %u\",\"bpm\":%d,\"tracks\":[", s ? ",\n" : "", s + 1u, song.g[G_BPM]);
        for (k = 0; k < NTRK; k++) {
            if (k) printf(",");
            dump_track(&trk[k]);
        }
        printf("]}");
    }
    printf("]\n");
    return 0;
}
