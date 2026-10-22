#!/bin/sh
# SPDX-License-Identifier: GPL-3.0-only
# RONDO's own host checks: the CYCLE engines and the voice layout (tests/rondo_test.c), with demo WAVs in
# build/rondo_demo; the TRACKER and the SEQ + SELECT track picker (tests/tracker_test.c);
# EDIT > DICE (tests/dice_test.c, with demos of eight rolls per role); the HOME dungeon (tests/dungeon_test.c);
# the live effects: FILTR DUCK REPT, ARP held, the master knobs (tests/livefx_test.c);
# the TRACKER's commands SPEED LOOP PITCH (tests/cmd_test.c); SEQ > VARY (tests/vary_test.c). Run from the repo root after the generate step (build/gen):
#   sh tests/run_rondo.sh            (NOCOST=1 skips the timing comparison)
set -e
cd "$(dirname "$0")/.."
[ -f build/gen/felucca_tables.h ] || { echo "run the generate step first (RONDO.md, Building)"; exit 1; }
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
