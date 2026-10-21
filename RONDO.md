# RONDO — a four-track FM groovebox firmware for the M-VAVE FM-1

RONDO is a fork of [Felucca](https://github.com/hugelton/Felucca) 1.6.1 (GPL-3.0) by Leo Kuroshita / Hügelton
Instruments. It keeps everything Felucca does (its sequencer, song mode, effects, the other engines, the web
installer's package format) and adds:

* **FMKIT** (engine 14) — four FM drum voices, Model-machine style: **KICK, SNARE, HAT, PERC**, each with its own
  PITCH, DECAY, COLOR, SWEEP, SHAPE and LEVEL.
* **FMTONE** (engine 15) — a two-operator FM voice with a sub and a detuned second carrier, for chords, pads, basses
  and leads, with a timbre envelope (COLOR / CNTR / MSUS) instead of an operator editor.
* **A tracker sequencer** in place of Felucca's step grid and piano roll: every track has its own tracker screen,
  steps as rows top to bottom, and **SEQ held + SELECT turned** picks the track whose tracker opens when SEQ is let go.
* **A fixed voice layout**, so no track ever steals a voice from another:

| Track | Default sound | Voices | Role |
|---|---|---|---|
| T1 | FMKIT · CYCLE KIT (with the BEAT pattern) | 4 (one per drum) | drums |
| T2 | FMTONE · FM PAD | 4 | chords / pads |
| T3 | FMTONE · FM BASS (mono, legato) | 1 | bass |
| T4 | FMTONE · FM LEAD | 3 | lead |

Any track can still load any engine (ANALOG, FM6, DRUM, …); it just keeps its track's voice count. The budget is
12 voices at once (Felucca's is 8 shared), all of them light FM voices.

> **Status: untested on hardware.** RONDO builds and passes its tests in the browser emulator and on the host
> (the firmware's own C code), but no `.fwsc` has been built or flashed yet. Read *Flashing* below before you try.

## Playing it

**MIDI:** channels **1–4 play tracks 1–4** (Felucca's default `MENU > MIDI > MIDI IN: CH1-4`), over USB or the TRS input
(a **Type A** 3.5 mm adapter). With the Yamaha QY70: set the QY70 song's drum part to channel 1 (CYC
KIT uses the General MIDI drum map, so the QY70's drum notes play as they are), chords to 2, bass to 3, lead to 4.
For tempo sync, have the QY70 send MIDI clock and set the FM-1's CLOCK page (on HOME, tap HOME twice) to `CLK TRS`.
The per-track channels can also be set one by one: `MENU > MIDI > MIDI IN: TRACK`, then `T1..T4 CHANNEL`.

**The panel** works as in Felucca. On a FMKIT track, tap **EDIT** to walk its pages:

| Page | KNOB 1 | KNOB 2 | KNOB 3 | KNOB 4 |
|---|---|---|---|---|
| KIT (EDIT 1) | TUNE (all) | DECAY (all) | COLOR (all) | OPEN (open-hat length) |
| FEEL (EDIT 2) | CLICK | VELO (velocity sensitivity) | DRV | VARI (per-hit variation) |
| KICK / SNARE / HAT / PERC | PITCH | DECAY | COLOR | SWEEP |
| SHAPE | KICK drive | SNARE noise mix | HAT metal ratio | PERC FM ratio |
| LANES | KICK level | SNARE level | HAT level | PERC level |

Everything on those pages can be **parameter-locked per step** (hold a step in SEQ, turn a knob), as in Felucca.
Hold **EDIT**: black keys 1–4 mute KICK, SNARE, HAT, PERC.

MIDI notes follow the GM drum map: 35/36 KICK, 38/40 SNARE, 42/44 closed HAT, 46 open HAT (OPEN sets its length; a
closed hat chokes it), cymbals a long open HAT, toms, rim, clap, cowbell and the rest PERC at their own pitches.

**FMTONE** pages:

| Page | KNOB 1 | KNOB 2 | KNOB 3 | KNOB 4 |
|---|---|---|---|---|
| TONE (EDIT 1) | RATIO (modulator : carrier) | COLOR (FM index) | CNTR (how fast the index falls) | MSUS (where it rests) |
| BODY (EDIT 2) | FDBK (modulator feedback) | SWEEP (pitch blip, ±) | STACK (detuned 2nd carrier) | SUB (octave-down sine) |

ENV / LFO DEST *SHP* and the matrix's SHAPE move COLOR. STACK and SUB share an oscillator: with STACK above 0, SUB
is off.

**Presets** — FMKIT: CYCLE KIT, 808ISH, 909ISH, GRIT KIT, METAL KIT, DUB KIT. FMTONE: FM PAD, CHORD EP, STAB,
GLASS PAD, ORGAN, FM BASS, SUB BASS, PLUCK BASS (the three basses are mono), FM LEAD, SYNC LEAD, BRASS, BELL.

## The tracker (SEQ)

Tap **SEQ**: the selected track's **TRACKER** (tap again: AUTOMATION). Steps run top to bottom, `00` to LEN − 1; the
cursor's row sits in the middle and, while the transport plays, the view follows the playing row (marked on the left)
until you touch a knob. Every fourth step number is brighter, and a dot after a number means the step has a lock.

**Pick a track:** hold **SEQ** and turn **SELECT**: a list of T1–T4 with their sounds; let go of SEQ and that track's
tracker opens. Each track keeps its own cursor row and column. Holding SEQ *without* turning SELECT still opens SEQ
TOOLS (LEN, DIV, SWING, GATE, euclid, double, half, …).

| Knob | Does |
|---|---|
| KNOB 1 ROW | move the cursor row |
| KNOB 2 COL | move the cursor column |
| KNOB 3 | change the cell |
| KNOB 4 | the column's second value (below) |

| Column | Shows | KNOB 3 | KNOB 4 |
|---|---|---|---|
| NOTE (melodic tracks) | `C-4`, `---` empty, ` \| ` held (tie) | on an empty row writes the last note played, then transposes | LEN: how many steps the note holds (ties) |
| BD / SD (kits) | `o` hit, `O` accent, `.` none | hit on / off | accent |
| HH | `x` closed, `o` open | none → closed → open | accent |
| PC | `c` clap, `t` tom, `r` rim, `b` bell | none → c → t → r → b | accent |
| VL | velocity in hex (`60` = 96) | velocity | FLAG: – / ACC / SLD / A+S |
| CHN | chance %, or a condition `1:2` … `4:4`, `FIL`, `!FI` | chance, then conditions | – |
| R | ratchet 2–4 | ×1–×4 | – |
| N | nudge −8…+7 sixteenths | nudge | – |
| FX | a parameter lock: short name + hex value (`BDP41`, `DEC3A`) | lock the parameter on this step | which parameter (the engine's eight, a kit's voice values, level, pan, envelope, sends) |

**EDIT** clears the cursor's cell (on NOTE: the whole step and its locks, and the cursor moves down). Keys: with the
track armed (**REC**) and the transport stopped they write the cursor's step (on a kit, its drums) and move down;
otherwise they just play. SAVE held undoes, as everywhere in Felucca.

## Differences from Felucca, in short

* SEQ's STEP and DETAIL pages (piano roll, drum grid, per-step detail lanes) are replaced by the TRACKER; AUTOMATION
  stays. Felucca's own UI tests that drive the grid or the roll no longer apply.
* Engines 14 and 15 added; SLICE (13) is no longer offered in the engine lists (so all engines still fit the EDIT
  layer's 16 white keys) but still plays in projects and user presets that use it.
* Presets of the CYC engines cannot be starred as favourites (Felucca's settings keep rows 14 and 15 of the
  favourites for MIDI LEARN and other settings).
* The splash and version read `RONDO R0.1`.
* `RONDO_LAYOUT=0` builds Felucca's shared 8-voice budget and power-on sounds (used to run Felucca's own tests).
* The voice counts are `TRK_CAP` in `firmware/src/core.h`; change them there if you want another layout.

## Building

### The browser emulator (no JieLi tools needed)

```sh
python3 -c "import sys; sys.path.insert(0, 'tools'); import build; build.generate()"   # build/gen
sh tools/build_emu_clang.sh            # clang + WASI sysroot  (or web/emu/build.sh with Emscripten)
cd build/emu && python3 -m http.server 8000                                              # open http://localhost:8000
```

### The firmware package for the FM-1

Exactly as Felucca (see `BUILDING.md`): Linux x86-64 natively, or macOS with Docker.

```sh
tools/get_toolchain.sh                                  # JieLi pi32v2 toolchain
git clone --depth 1 --branch AC79NN_SDK_V1.2.1_2023-12-13 https://gitee.com/Jieli-Tech/fw-AC79_AIoT_SDK.git ~/fw-AC79_AIoT_SDK
git submodule update --init                             # firmware/engines (Felucca-Engines)
./build.sh --release 0.1                                # build/felucca-0.1.fwsc
```

### Tests

```sh
sh tests/run_rondo.sh          # RONDO's own checks (tests/rondo_test.c) and demo WAVs in build/rondo_demo
```

`tests/run_tests.sh` is Felucca's full suite; build it with `CC="cc -DRONDO_LAYOUT=0"` to check Felucca's behaviour
is unchanged outside the new engines.

## Flashing (at your own risk)

1. Update the FM-1 to M-VAVE's official **V15** first, and keep that `FM-1.fwsc` (m-vave.com/download).
2. Have a recovery path *before* you flash anything unofficial: the FM-1 has one firmware slot. While RONDO runs,
   **OCT−** + **OCT+** held 5 s enters UPDATE MODE, and two failed boots in a row enter it by themselves (Felucca's
   boot guard). If a build cannot reach either, recovery needs
   [FM-1 Transporter](https://github.com/kurogedelic/FM-1-transporter) (a Seeed XIAO RP2040).
3. Install `felucca-0.1.fwsc` with `python3 tools/fm1_install.py build/felucca-0.1.fwsc` over USB, or a local copy of
   the web installer (`web/make_site.py`, see BUILDING.md).
4. To go back: install M-VAVE's `FM-1.fwsc` the same way.

## Licence

GPL-3.0-only, as Felucca. Felucca-Engines (firmware/engines) is MIT. See LICENSE, LICENSING.md and CREDITS.md.
RONDO is not affiliated with M-VAVE, Hügelton Instruments or Elektron.
