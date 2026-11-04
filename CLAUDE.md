# Dungeon Tracker (FM-1 firmware) — notes for Claude

Dungeon Tracker is a fork of Felucca 1.6.1 (GPL-3.0) for the M-VAVE FM-1: a four-track tracker groovebox whose HOME
screen is a dungeon crawl played by the song. The owner is Ed. The full manual is `DUNGEON-TRACKER.md`: read it before
changing behaviour, and keep it up to date with every user-visible change (its Changes section too).

## Where things stand (1.5, Nov 2026)

1.5 is built (`build/release-1.5/dungeon-tracker-1.5.fwsc`, identity FM-1_915) and being tested on the hardware.
Not committed yet: everything since 1.0 is uncommitted work in this tree (Ed commits and pushes himself; never touch
the git index, commit or push unless he asks). The `firmware/engines` submodule (Hügelton's Felucca-Engines) has one
local change, `src/eng_analog.c` (13 ANALOG presets), which needs a commit in Ed's fork of that repo and
`.gitmodules` pointed at it.

What 1.5 is:
- **Eight engines, four families of two** (ENGINE_ORDER {18,14,16,20,21,19,0,17}; NENGINES 22, the rest retired and
  loading as substitutes, `firmware/src/retire.c`): FM — GENESIS (18, `eng_genesis.c`), GEN KIT (14, `eng_cycle.c`);
  WAVE — WAVES (16), WAVE 32 (20, `eng_wt32.c`, tables from `tools/gen_wt32.py`); AMIGA'D — MOD (21), MOD KIT (19, both
  `eng_mod.c`, samples from `tools/gen_mod.py`); SUB — ANALOG (0, in the submodule), SID (17, `eng_sid.c`).
- **DICE** (`ui_dice.c`, EDIT > DICE) is the sound editor on every engine: OCT+ rolls, K1 DIE, K2 HOLD, K3 WILD, K4 HIST.
  On a held die K3 is VALUE (set by hand, recorded as automation) and K4 PART.
- **SOUND MORPH** (`morph.c`, EDIT > MORPH): target = a factory preset (1..n) or a saved user sound of the same engine
  (code MO_USER + slot, 0x80+). Known quirk: blending envelopes value by value can dip in level mid-way.
- **THE PARTY**: eight classes, 12 voice points (`core.h` CLS_VOICES {4,4,1,3,4,2,3,2}; the bard 4 since 1.5).
- **The dungeon** (`ui_dungeon.c`): bestiary of 12 monsters + 5 bosses (`tools/gen_monsters.py` -> `dg_monsters.h`),
  per-class weapons. Visual only: the rules and balance are 1.1's (`tests/dungeon_test.c` section 17).

## Target constraints

JieLi AC79 (pi32v2): no float, and **no 64-bit division** (`__udivdi3` is missing: keep divisions 32-bit). The image
must stay under 581,564 B (1.5 is ~556 KB). The firmware is a unity build: `felucca.c` includes the other sources.

## Build, test, run

```sh
# generated tables (once, and after changing tools/gen_*.py)
python3 -c "import sys; sys.path.insert(0, 'tools'); import build; build.generate()"
# firmware (needs the JieLi toolchain and AC79 SDK, see BUILDING.md; tools/get_toolchain.sh)
JIELI_TOOLCHAIN=~/.jieli/toolchain AC79_SDK=~/fw-AC79_AIoT_SDK python3 tools/build.py --release 1.5
# Dungeon Tracker's host tests (plain cc, no toolchain needed)
sh tests/run_rondo.sh
# the UI suite: ui_test has 224 known FAILs (retired-engine checks, the 1.5 baseline); project_test 3
cc -O1 -w -Ibuild/gen -Ifirmware/src -Itests -o build/host/ui_test tests/ui_test.c -lm && build/host/ui_test | grep -c FAIL
# every screen in every palette rendered + layout lint (must report 0 lint findings)
cc -O1 -w -Ibuild/gen -Ifirmware/src -Itests -o build/host/ui_render tests/ui_render.c -lm && mkdir -p build/ui_new/ppm && build/host/ui_render build/ui_new
# web tests
(cd web && node test_web.mjs)
# browser emulator (clang + WASI sysroot in ~/wasi, or web/emu/build.sh with Emscripten), then serve it
sh tools/build_emu_clang.sh && python3 -m http.server -d build/emu 8790
# web editor (Chrome/Edge, USB MIDI; ?mock=1 without hardware)
python3 -m http.server -d web 8791      # http://localhost:8791/editor.html
```

Other single tests compile the same way (`tests/dice_test.c`, `dungeon_test.c`, `morph_test.c`, `boot_song_test.c`...).
Audio demos: `tests/dice_demo.c` (a rolled song; `DEMO_ENG` picks engines per section) and `tests/morph_demo.c`.
`tools/gen_wt32.py` and `tools/gen_mod.py` need numpy.

## Open items

- GLIDE was reworked on 1.5 (voice.c glide_set, params.c GLIDE_MS, tests/glide_test.c): Ed is testing it on the
  hardware. An earlier report of the GLIDE value jumping back (1.0 -> 1.2 -> 1.4 -> 1.2) could not be reproduced on
  the host and went away on its own; if it returns, ask which track / page / engine.
- The web editor still shows Felucca's "6-OP FM" tab (FM6 is retired): harmless, could be hidden.
- Smarter envelope blending in MORPH, so the middle of a blend keeps its level.
- Results of Ed's hardware testing of 1.5.

## Working with Ed

Casual and direct. He tests on the real FM-1 and likes audio demos and screenshots. Keep `DUNGEON-TRACKER.md` and the
tests in step with every change, and rebuild `--release 1.5` after firmware changes.
