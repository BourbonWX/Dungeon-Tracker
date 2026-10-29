#!/bin/sh
# SPDX-License-Identifier: GPL-3.0-only
# RONDO's own host checks: the CYCLE engines and the voice layout (tests/rondo_test.c), with demo WAVs in
# build/rondo_demo; the TRACKER and the SEQ + SELECT track picker (tests/tracker_test.c);
# EDIT > DICE (tests/dice_test.c, with demos of eight rolls per role); the HOME dungeon (tests/dungeon_test.c);
# the live effects: FILTR DUCK REPT, ARP held, the master knobs (tests/livefx_test.c);
# the TRACKER's commands SPEED LOOP PITCH (tests/cmd_test.c); SEQ > VARY (tests/vary_test.c); the PATTERN BANK, SONG on it, the arrangement recorded, MIDI program changes (tests/bank_test.c); SOUND MORPH (tests/morph_test.c); 1.1's WAVES engine and its presets (tests/waves_test.c, demos in build/rondo_demo); 1.2's song slots (tests/songslot_test.c). Run from the repo root after the generate step (build/gen):
#   sh tests/run_rondo.sh            (NOCOST=1 skips the timing comparison)
set -e
cd "$(dirname "$0")/.."
[ -f build/gen/felucca_tables.h ] || { echo "run the generate step first (DUNGEON-TRACKER.md, Building)"; exit 1; }
mkdir -p build/host build/rondo_demo
${CC:-cc} -O2 -w -Ibuild/gen -Ifirmware/src -o build/host/rondo_test tests/rondo_test.c -lm
build/host/rondo_test build/rondo_demo
${CC:-cc} -O1 -w -Ibuild/gen -Ifirmware/src -Itests -o build/host/tracker_test tests/tracker_test.c -lm
build/host/tracker_test
${CC:-cc} -O1 -w -Ibuild/gen -Ifirmware/src -Itests -o build/host/dice_test tests/dice_test.c -lm
build/host/dice_test build/rondo_demo
${CC:-cc} -O1 -w -Ibuild/gen -Ifirmware/src -Itests -o build/host/dungeon_test tests/dungeon_test.c -lm
build/host/dungeon_test
${CC:-cc} -O1 -w -Ibuild/gen -Ifirmware/src -Itests -o build/host/livefx_test tests/livefx_test.c -lm
build/host/livefx_test
${CC:-cc} -O1 -w -Ibuild/gen -Ifirmware/src -Itests -o build/host/cmd_test tests/cmd_test.c -lm
build/host/cmd_test
${CC:-cc} -O1 -w -Ibuild/gen -Ifirmware/src -Itests -o build/host/vary_test tests/vary_test.c -lm
build/host/vary_test
${CC:-cc} -O1 -w -Ibuild/gen -Ifirmware/src -Itests -o build/host/bank_test tests/bank_test.c -lm
build/host/bank_test
${CC:-cc} -O1 -w -Ibuild/gen -Ifirmware/src -Itests -o build/host/morph_test tests/morph_test.c -lm
build/host/morph_test
${CC:-cc} -O2 -w -Ibuild/gen -Ifirmware/src -Itests -o build/host/waves_test tests/waves_test.c -lm
build/host/waves_test build/rondo_demo
# 1.3: the SID engine and the LOFI KIT (tests/sid_test.c; demos in build/rondo_demo)
${CC:-cc} -O2 -w -Ibuild/gen -Ifirmware/src -Itests -o build/host/sid_test tests/sid_test.c -lm
build/host/sid_test build/rondo_demo
${CC:-cc} -O1 -w -Ibuild/gen -Ifirmware/src -Itests -o build/host/persistence_test tests/persistence_test.c -lm
build/host/persistence_test > build/host/persistence.txt || { grep FAIL build/host/persistence.txt; exit 1; }
tail -1 build/host/persistence.txt                    # (the flash: the PATTERN BANK's pairs too)
# 1.2: the SONG SLOTS, A..H, whole songs (tests/songslot_test.c: round trips, power cuts, 1.1's projects moved in)
${CC:-cc} -O1 -w -Ibuild/gen -Ifirmware/src -Itests -o build/host/songslot_test tests/songslot_test.c -lm
build/host/songslot_test > build/host/songslot.txt || { grep FAIL build/host/songslot.txt; exit 1; }
tail -1 build/host/songslot.txt
# the audio tools build (tests/chip_demo.c, tests/chip_tour.c: CHIP's demos, run by hand)
${CC:-cc} -O1 -w -Ibuild/gen -Ifirmware/src -Itests -o build/host/chip_demo tests/chip_demo.c -lm
${CC:-cc} -O1 -w -Ibuild/gen -Ifirmware/src -Itests -o build/host/chip_tour tests/chip_tour.c -lm
# the editor: the firmware's side of the protocol (tests/editor_test.c, both layouts) and the web editor against the
# firmware's tables (tests/descdump.c -> build/host/desc.json; web/test_web.mjs, when node is there)
${CC:-cc} -O1 -w -Ibuild/gen -Ifirmware/src -Itests -o build/host/editor_test tests/editor_test.c -lm
build/host/editor_test > build/host/editor.txt || { grep FAIL build/host/editor.txt; exit 1; }
tail -1 build/host/editor.txt
${CC:-cc} -O1 -w -Ibuild/gen -Ifirmware/src -o build/host/descdump tests/descdump.c -lm
build/host/descdump > build/host/desc.json
if command -v node >/dev/null 2>&1; then node web/test_web.mjs > build/host/web.txt || { grep FAIL build/host/web.txt; exit 1; }; tail -1 build/host/web.txt; fi
# Dungeon Tracker's power-on song (tests/boot_song_test.c; WAVs of four power-ons in build/boot_song)
mkdir -p build/boot_song
${CC:-cc} -O1 -w -Ibuild/gen -Ifirmware/src -Itests -o build/host/boot_song_test tests/boot_song_test.c -lm
build/host/boot_song_test build/boot_song
# SELECT navigates (tests/nav_test.c: the pages, the MENU's rows, the tempo's other ways, MENU > SELECT BPM)
${CC:-cc} -O1 -w -Ibuild/gen -Ifirmware/src -Itests -o build/host/nav_test tests/nav_test.c -lm
build/host/nav_test
# SAVE > TOOLS' NEW, NEW + DEFAULT SOUNDS and their undo (tests/new_song_test.c)
${CC:-cc} -O1 -w -Ibuild/gen -Ifirmware/src -Itests -o build/host/new_song_test tests/new_song_test.c -lm
build/host/new_song_test
# drums written one at a time share a step (tests/kit_entry_test.c)
${CC:-cc} -O1 -w -Ibuild/gen -Ifirmware/src -Itests -o build/host/kit_entry_test tests/kit_entry_test.c -lm
build/host/kit_entry_test
