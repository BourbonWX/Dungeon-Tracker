# Dungeon Tracker — a tracker groovebox with a dungeon crawl, firmware for the M-VAVE FM-1

Dungeon Tracker (formerly RE-1 "Random Encounters", and RONDO before that) is an old-school tracker-style four-track
FM groovebox with an old-school first-person dungeon crawl on its HOME screen, played by the song. It is a fork of [Felucca](https://github.com/hugelton/Felucca) 1.6.1 (GPL-3.0) by Leo Kuroshita / Hügelton
Instruments. It keeps everything Felucca does (its sequencer, song mode, effects, the other engines, the web
installer's package format) and adds:

* **FMKIT** (engine 14) — four FM drum voices, Model-machine style: **KICK, SNARE, HAT, PERC**, each with its own
  PITCH, DECAY, COLOR, SWEEP, SHAPE and LEVEL.
* **FMTONE** (engine 15) — a two-operator FM voice with a sub and a detuned second carrier, for chords, pads, basses
  and leads, with a timbre envelope (COLOR / CNTR / MSUS) instead of an operator editor.
* **WAVES** (engine 16) — the digital oscillators of Mutable Instruments' Ambika / Shruthi-1 (Emilie Gillet):
  16 wavetables, WAVEQ, 8BIT, DPWM and QSAW, with a sub-oscillator and GRIT, through ANALOG's filter. 18 presets
  (*WAVES*, below).
* **SID** (engine 17) — a voice after the Commodore 64's sound chip: triangle, saw, pulse with PWM, noise and the
  chip's combined waveforms, hard sync and ring mod, its resonant filter, and a C64-style drum mode. 18 presets (*SID*,
  below).
* **The LOFI KIT** (SAMPLE's SET 3) — real acoustic drums through tape: kick, two snares, rim, hats, a pitched
  tom, a thick shaker and a wooden shell rattle, on the General MIDI drum notes (*LOFI KIT*, below).
* **The DUNGEON KIT** (SAMPLE's SET 1) — a drum kit of dungeon sounds in a stone room: a door's boom, an anvil, an
  iron cell door, chains, a torch, a tolling bell, bones, a drip and a gong (*DUNGEON KIT*, below).
* **96 more presets** across FMKIT, FMTONE, WAVES and SID, so every class has plenty to pick from.
* **CHIP**, Felucca's LOFI engine renamed for what it plays (its first knob, Felucca's CHIP, is now MODE), with
  twelve more presets (square and pulse leads, a staircase-triangle bass, plucks,
  arps, zaps, chip kick, noise hat and snare, a wave-RAM organ and choir).
* **DICE sound design**: on FMTONE, FMKIT and CHIP tracks, every part of the sound is a die. Roll, hold the dice you like,
  roll again, and the rest change around them, always inside ranges that sound musical.
* **A tracker sequencer** in place of Felucca's step grid and piano roll: every track has its own tracker screen,
  steps as rows top to bottom, and **SEQ held + SELECT turned** picks the track whose tracker opens when SEQ is let go.
* **THE PARTY**: every track is played by a class, and the class sets its voices and its sounds, so no track ever
  steals a voice from another and a drum track never lands on a bass preset mid-song (*The party*, below):

| Class | Plays | Voices | Default party |
|---|---|---|---|
| ROGUE | drums | 4 (one per drum) | T1 · FMKIT · CYCLE KIT (with the BEAT pattern) |
| CLERIC | chords / pads | 4 | T2 · FMTONE · FM PAD |
| FIGHTER | bass | 1 (mono, legato) | T3 · FMTONE · FM BASS |
| MAGE | lead | 3 | T4 · FMTONE · FM LEAD |

The party has **12 points** (one per voice; Felucca's budget is 8 shared). Run two rogues for two drum kits, two
fighters and two mages, whatever fits in 12.

> **Status: 1.0, the first release** (`dungeon-tracker-1.0.fwsc`). Earlier builds of it have run on an FM-1 (the
> dungeon, the tracker, the sounds, WAVES, FLOOR FX, song slots); the SID engine, the LOFI KIT, slots I–K and THE
> PARTY pass their tests on the host and are new to the hardware. Read *Flashing* below before you try.

## Powering on

The splash: DUNGEON over TRACKER in the firmware's own block lettering, in the palette's colours (MENU > COLOR;
NIGHT, black and green, until you pick another).
Then, unless there is music of yours to bring back (MENU > SYSTEM > RESTORE LAST, or a pattern stored in the PATTERN
BANK), Dungeon Tracker writes a **song of its own** (`firmware/src/boot_song.c`): a tempo, a key and mode, a four-bar
chord progression, an FMKIT beat on T1, chords on T2, a bass line on T3 and a lead on T4 (FMTONE or CHIP sounds),
different at every power-on. HOME shows the dungeon with the whole party and **PRESS PLAY**: press it and they walk.
Nothing of it is saved until you change something; then it is yours and comes back at the next power-on.

### Starting from scratch: NEW

SAVE > TOOLS (tap SAVE until TOOLS), **KNOB 1**: the first turn right picks CLEAR (the selected track's pattern, as
before), the next **NEW**, the next **NEW+S**; turning left steps back. OCT+ asks, OCT+ again does it.

- **NEW** — a blank song: all four tracks' steps and automation, the song order, the other patterns of the PATTERN
  BANK and the project's name are cleared. The sounds, the tempo and the other settings stay, so you start writing
  with the sounds you were just hearing.
- **NEW+S** (NEW + DEFAULT SOUNDS) — the same, and the four tracks go back to the default sounds (CYCLE KIT,
  FM PAD, FM BASS, FM LEAD) with their default mix; SOUND MORPH off.

**Hold SAVE** right afterwards to undo either one: the whole song comes back, its sounds, song order and patterns
too. The undo is there until you change something (a note, a sound, the tempo). A SONG playing has to be stopped
first; a pattern playing keeps playing, now empty.

### Keeping your work

Your music is kept in the **autosave**: the flash copy that comes back at the next power-on (MENU > SYSTEM > RESTORE
LAST, ON by default). Writing the flash stops the audio for ~40 ms, so the autosave **never writes while anything
plays**: it writes **3 seconds after you STOP**, once nothing sounds and no key is held (at most once every 20 s).

* **The header's mark.** While your latest changes are not in the autosave yet, an asterisk follows the pattern letter
  (`A*`, or `A>C*` with C queued). Switch off then and those changes are lost. STOP, wait for the mark to go, then
  switch off.
* **QUICK** (SAVE > PROJECT, the slot **Q** above A–K) is the autosave itself. **LOAD** brings it back (SAVE held
  undoes, as any load); **SAVE** writes it at once, no name, no dialog (stopped, as every save). It works with RESTORE
  LAST OFF too: nothing then comes back at power-on, but QUICK SAVE / LOAD still do. QUICK can't be erased or renamed.
* **Song slots A–K** are the place for music you want to keep for good: the autosave follows whatever you play next.

### Song slots A–K

SAVE > PROJECT keeps **eleven songs**, A–K (SLOT: Q, then A..K; the list scrolls). Each one is the **whole song**: the
sounds and the mix, the tempo and the settings, SONG's arrangement, and **all six patterns of the PATTERN BANK** (A–F)
with which one you were on. **LOAD** brings all of it back; the bank's six keys become the song's.

* **SAVE** (stopped) writes the song as it is now, the pattern you're on included. **NAME**, **ERASE** and the overwrite
  dialog work as before; a rename keeps the song's patterns.
* **Loading over other patterns asks first.** If the bank holds patterns besides the one you're on, LOAD asks
  `LOAD SONG B? REPLACES PATTERNS A-F` (OCT+ yes, OCT− no), since those keys get the song's patterns. Save what you're
  working on to a slot first if you want to keep it.
* **Power cuts are safe.** A save writes into a free part of the flash and switches over only once it's complete; switch
  off mid-save and the slot still holds what it held before.
* **Size.** A song takes up to 12 KB packed. Patterns are stored as their differences from each other, so a typical song
  uses 3–5 KB. Six completely different patterns full of random notes, chances and drum hits on every step can be too
  much: SAVE then says **SONG TOO BIG** and writes nothing (clear a key or two you don't need, and save again).
* **From Felucca:** your projects A–D move into song slots A–D at the first power-on. They have no patterns of their
  own, so they load the way Felucca's did (the steps into the current pattern, the other keys untouched) until you
  save over them.
* **The flash** came from Felucca's user sample slots: **USR2 and USR3 are retired**; USR1 keeps its 80 KiB. A sample
  that was in USR2 or USR3 is gone, and the editor refuses uploads there. Going back to Felucca after saving songs:
  it can't read the song slots (its projects A–D and USR2 come back empty); come back and they're all there.
* **The web editor** lists A–K, and its complete backup holds the projects of A–K (not the patterns: the PATTERN
  BANK was never in it).

(Felucca waits 10 s of quiet and says nothing: a song built while it played was lost when the FM-1 went off soon
after STOP. `tests/persistence_test.c` replays exactly that.)

## Playing it

**MIDI:** channels **1–4 play tracks 1–4** (Felucca's default `MENU > MIDI > MIDI IN: CH1-4`), over USB or the TRS input
(a **Type A** 3.5 mm adapter). With the Yamaha QY70: set the QY70 song's drum part to channel 1 (CYC
KIT uses the General MIDI drum map, so the QY70's drum notes play as they are), chords to 2, bass to 3, lead to 4.
For tempo sync, have the QY70 send MIDI clock and set the FM-1's CLOCK page (on HOME, tap HOME twice) to `CLK TRS`.
The per-track channels can also be set one by one: `MENU > MIDI > MIDI IN: TRACK`, then `T1..T4 CHANNEL`.

**Getting around.** Dungeon Tracker gives each knob of the left-hand trio one job everywhere:

| Knob | Does |
|---|---|
| **SELECT** | **navigates**: the pages of the family you are in (tap EDIT, then SELECT walks EDIT 1, EDIT 2, DICE, MORPH, VOICE…; on HOME: HOME, MIXER, CLOCK), and in the MENU every row, rolling from one tab into the next; in ABOUT it scrolls |
| **PRESETS** | sounds (on a page), rows (in the MENU) |
| **ALGORITHM** | the track (on a page), tabs (in the MENU) |
| **KNOB 1–4**, **OCT±** | change what is selected |

The **tempo** is **GLO + SELECT**, HOME > CLOCK's **KNOB 1**, or TAP (GLO + F4). If you prefer Felucca's way
(SELECT always the tempo), set `MENU > CONTROL > SELECT` to BPM; PAGE is Dungeon Tracker's default (a setting saved
by Felucca reads PAGE here unless it was set to PAGE, which reads BPM).

**The panel** works as in Felucca otherwise. On a FMKIT track, tap **EDIT** to walk its pages:

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

**Presets** — FMKIT: CYCLE KIT, 808ISH, 909ISH, GRIT KIT, METAL KIT, DUB KIT, BOOM BAP, TECHNO KIT, ELECTRO KIT,
DUSTY KIT, TRIBAL KIT, MINIMAL KIT, FORGE KIT, CHIP KIT 2, CAVE KIT (a stone room's reverb), DISCO KIT. FMTONE: FM PAD,
CHORD EP, STAB, GLASS PAD, ORGAN, FM BASS, SUB BASS, PLUCK BASS (the basses are mono), FM LEAD, SYNC LEAD, BRASS,
BELL; basses REESE, DUB BASS, SLAP BASS, GROWL FM, ROLLING, WOOD BASS, ZAP BASS, OCTAVE BASS; pads WARM PAD, CHOIR FM,
DARK PAD, AIR PAD, FM STRINGS; keys DX PIANO, VIBES, CLAV, ORGAN 2, MUSIC BOX; plucks FM HARP, KOTO, DROP PLUCK, FM
MARIMBA; leads FM SAW LEAD, HOLLOW FM, SOLO HORN, WHISTLE, SCREAMER, GLIDE LEAD, HERO LEAD; and SIREN.

**CHIP** (Felucca's LOFI engine: MODE WAVE DUTY CRSH / SWP VIB ARP TONE; Felucca calls MODE "CHIP") — Felucca's five (PULSE LD, WAVE BASS,
ARP 8BIT, WAVE LEAD, STEP LEAD) and Dungeon Tracker's chip set:

| Preset | What it is |
|---|---|
| SQUARE 50 | MODE STEP, a 50 % square lead with vibrato |
| TRI BASS | the 32-step staircase triangle, mono |
| PULSE BASS | 25 % pulse with a stepped decay (DCY D10) |
| CHIP PLUCK | 25 % pulse, a short stepped decay (D3), poly |
| FAST ARP | 8BIT pulse, ARP MIN: one note plays a minor chord |
| LASER ZAP | square swept down fast, short decay |
| CHIP KICK | the triangle swept down, a very short decay (play it low, about C2–C3) |
| NOISE HAT | 1-bit noise, a tick (higher notes are brighter) |
| NOISE SNR | 1-bit noise, held longer and crushed darker |
| WAVE ORGAN | wave RAM ORGN table |
| 8BIT SAW | an 8-bit saw lead |
| VOX CHOIR | wave RAM VOXA ("ah") with a slow attack and vibrato |

## The party (MENU > GAME)

Every track is played by one of four classes, and **the class is the track's job**: how many notes it can sound at
once, and which sounds it can load.

| Class | Plays | Voices (points) | Sounds in its list |
|---|---|---|---|
| ROGUE | drums | 4, one per drum | DRUM |
| CLERIC | chords, pads | 4 | PAD, KEYS, PLUCK, other |
| FIGHTER | bass | 1, mono and legato | BASS |
| MAGE | lead | 3 | LEAD, PLUCK, FX, other |

**Picking the party.** Hold **HOME** for the MENU and turn **ALGORITHM** to **GAME**. Below DUNGEON are **T1 .. T4**,
one row per track, and **PARTY**, the points it uses: `12/12`. Pick a row, turn KNOB 1 through ROGUE, CLERIC, FIGHTER
and MAGE. Any class on any track, as many of one as you like: two rogues are two drum kits (4 + 4), and with the
fighter (1) and the mage (3) that is still 12.

* **12 points.** A class costs its voices. A change that stays within 12 happens at once. One that goes over waits:
  PARTY reads e.g. `15/12 !` and nothing changes yet, so you can swap a mage for a fighter on the way to two clerics.
  The change that brings it back to 12 or under puts them all in together. Leave the MENU while it's still over and
  the waiting changes are dropped: `PARTY NOT CHANGED`.
* **A new class, a new job.** The track lets go of its notes. If its sound belongs to the new class (a PLUCK going from
  cleric to mage) it keeps it; otherwise it takes the class's default sound (CYCLE KIT, FM PAD, FM BASS, FM LEAD).
  Its steps stay.
* **Saved** with the song, in every slot and the autosave. NEW+S goes back to the default party (rogue, cleric,
  fighter, mage on T1..T4). The party is the song's, with the DUNGEON on or off.

**PRESETS shows the class's sounds.** On PRESETS, **LIST** (KNOB 4) starts at **CLASS**: the presets, across every
engine, that fit the track's class (a rogue's track lists the 19 drum kits and drum sounds, nothing else). Scrolling
while the song plays can't land a drum track on a bass patch. ENG skips the engines that have nothing for the class,
and picking one of those on the EDIT layer says e.g. `ROGUE: NOT ON THIS ENGINE`.

**The escape hatch: LIST ALL.** Turn LIST one step right, to **ALL**, and every sound is there again for any track,
a pad on the fighter if you want it (it plays with the fighter's one voice). FAV and the categories follow, as before.
The LIST you pick stays for every track until you change it.

DICE rolls a sound for the class (a rogue rolls a kit, a fighter a bass), and in the dungeon each track fights as
its class (*HOME: the dungeon*, below). The HOME slots show it: `RGE FTR RGE MAG`.

## WAVES

The digital oscillators of Mutable Instruments' Ambika and Shruthi-1 (Emilie Gillet), through ANALOG's filter: 8-bit
waves with a gritty, glassy edge. In PRESETS after FM6, and on the EDIT layer's fifth key.

| Page | KNOB 1 | KNOB 2 | KNOB 3 | KNOB 4 |
| --- | --- | --- | --- | --- |
| EDIT 1 (WAVE) | SHAPE | PRM | PENV (the envelope to PRM) | SUB (the sub's level) |
| EDIT 2 (FLTR) | CUT | RES | SUBT (the sub's shape) | GRIT |
| HOME | PRM | CUT | RES | PENV |

* **SHAPE** picks the oscillator, **PRM** what it does: **WT 1–16** the wavetables (PRM moves through each table's
  waves; WT 1, 12 and 15 open up as PRM rises, WT 13 peaks in the middle like a vowel, WT 7 closes to nearly a sine),
  **WAVEQ** one of the 80 waves the tables are made of, **8BIT** bit-twiddled saws, **DPWM** a raw pulse (PRM its
  width), **QSAW** four saws drifting apart.
* **SUB** adds a square, triangle or pulse an octave down (two with the "2" shapes); **GRIT** holds and roughens the
  output, like a lower sample rate.
* **Presets** (each suggests a pattern in SAVE > PHRASES): WT PAD, WT SWEEP, WT KEYS, WAVEQ BASS, 8BIT LEAD, DIRTY
  BASS, QUAD PAD, GRIT KEYS, and the dungeon set: **TORCHLIGHT** (a warm pad, a flicker of grit), **CRYSTAL CAVE**
  (a glass bell), **COIN GET** (a blip), **ACID WAVE** (a squelching pulse bass), **BOSS FIGHT** (the big lead),
  **GHOST CHOIR** (a vowel pad), **DEEP DELVE** (a sub bass), **LASER ZAP**, **ARP RUNES** (a pluck for the ARP
  pattern) and **MANA POOL** (a pad that opens slowly); basses WT BASS, 8BIT BASS, QSAW BASS, RUBBER, SUB WAVE, GRIT
  BASS, ACID PULSE; pads DUSK PAD, VOWEL PAD, GLASS WT, DRONE, CRYPT PAD, SHIMMER WT; keys WT PIANO, LOFI KEYS, TOY
  PIANO; plucks WT PLUCK, 8BIT PLUCK, HARP RUNE; leads WT LEAD, QSAW LEAD, PULSE HERO, CHIP SOLO, VOWEL LEAD, DRAGON;
  and PORTAL, TRAPDOOR, ROBOT TALK.
* It costs about what ANALOG does per voice (`tests/waves_test.c`). NOISE has left the EDIT layer's keys for it (the
  16 white keys hold 14 engines, INIT and CLEAN); NOISE is still in PRESETS. WAVES presets can't be starred, like
  FMKIT's and FMTONE's.

## SID

A voice after the MOS 6581, the Commodore 64's sound chip (written for Dungeon Tracker from how the chip behaves; no
emulator's code). In PRESETS after WAVES, and on the EDIT layer's sixth key.

| Page | KNOB 1 | KNOB 2 | KNOB 3 | KNOB 4 |
| --- | --- | --- | --- | --- |
| EDIT 1 (OSC) | WAVE | PW (pulse width) | PWM (its sweep) | MOD |
| EDIT 2 (MOD) | TUNE | SWEEP | CUT | RES |
| HOME | PW | CUT | RES | SWEEP |

* **WAVE**: **TRI**, **SAW**, **PLS** (pulse), **NSE** (the chip's noise: clocked by the note, so low notes rumble and
  high ones hiss), and the chip's **combined** waves **ST PT PS PST**: thin, buzzy, the growl of many C64 basses.
  **DRM** is a C64 drum: a burst of noise, then the pulse falling in pitch (SWEEP sets how far).
* **PW** is the pulse's width (50 % at the middle); **PWM** sweeps it slowly, the classic moving C64 lead and bass.
* **MOD** brings in a second oscillator playing the note: **SYNC** restarts the voice every cycle of it (with TUNE the
  voice above it: the sync lead), **RING** rings the triangle against it (bells), **S+R** both.
* **TUNE** (0..48 semitones): with MOD, the voice above the modulator; without, a transpose. **SWEEP** lets the
  envelope move TUNE: sync sweeps, zaps, drums.
* **CUT** / **RES**: the chip's 12 dB filter, driven a little first as the 6581's is (FILTER TYPE picks LP, BP, HP...).
* **Presets**: MONTY BASS (a PWM bass), GROWL BASS (combined waves), SAW BASS, SYNC LEAD, PWM LEAD, TRI FLUTE, RING
  BELL, SID STRINGS, C64 ARP (hold a chord with the ARP on), FILTER SWEEP, SID KICK, SID SNARE, SID HAT, ZAP, POWER
  UP, NOISE WIND, TITLE SCREEN and SPELLBOOK (sync and ring together); drums SID TOM, SID CLAP, SID COWBELL, SID
  BOOM; basses SID PULSE, SID TRI BASS, SYNC BASS, BUZZ BASS, FUNK BASS, DEEP PULSE, RING BASS; pads SID CHOIR, SAW
  PAD, DREAM PAD; keys SID ORGAN, SID KEYS; plucks SID PLUCK, HARPSI; leads SID SOLO, SAW SOLO, RING LEAD, SYNC
  SCREAM, HOLLOW LEAD, ARCADE LEAD; and EXPLOSION, LASER, TELEPORT, ALARM.
* It costs about half of ANALOG per voice (`tests/sid_test.c`). GRAIN has left the EDIT layer's keys for it (still in
  PRESETS, with NOISE).

## LOFI KIT

SAMPLE's SET 3 (in place of SAX): real acoustic drums from Versilian's free library (VCSL, CC0), run through a tape
treatment — pitched down a little, warmed, saturated, a touch of wow and flutter, the top end rolled off — and stored
at 16 kHz. Pick SAMPLE, then the **LOFI KIT** preset (it suggests the BEAT pattern), and play the General MIDI drum
notes, as the DRUM engine and a QY-70's drum track do:

| Note | Hit |
| --- | --- |
| 35, 36 (B0, C1) | KICK |
| 37 | RIM (cross-stick) |
| 38 | SNARE |
| 39, 40 | SNARE 2 (tighter) |
| 41, 43, 45, 47, 48, 50 | TOM (pitched: low on 41, high on 50) |
| 42, 44 | CLOSED HAT |
| 46 | OPEN HAT |
| 69 | SHELLS (a rattle of small wooden shells) |
| 70, 82 | SHAKER (thick, a hollow mid body) |

Other notes are silent. `tools/make_lofi_kit.py` makes the kit from VCSL's recordings (`assets/samples-cc0/LOFI`).

## DUNGEON KIT

SAMPLE's SET 1 (the number Felucca's retired TRANH set kept): a drum kit of dungeon sounds, each in a small stone
room with a light tape treatment, at 16 kHz. Most come from VCSL (CC0); the drip is made from scratch. Pick SAMPLE,
then the **DUNGEON KIT** preset (it suggests the BEAT pattern). It sits on the LOFI KIT's notes, so a beat written
for one plays on the other:

| Note | Hit | What it is |
| --- | --- | --- |
| 35, 36 | DOOM | a big bass drum and a log drum: a heavy door |
| 37 | BONE | claves, pitched down |
| 38 | CLANG | an anvil |
| 39, 40 | CELL | a brake drum and a rattle of bells: an iron cell door |
| 41, 43, 45, 47, 48, 50 | TOLL | a tubular bell, in tune on each tom note |
| 42, 44 | CHAIN | sleigh bells, pitched down |
| 46 | TORCH | an ocean drum's whoosh, a crackle |
| 49, 52, 55, 57 | GONG | a gong |
| 69 | DRIP | a drop into a pool |
| 70, 82 | RATTLE | a ratchet: bones |

A song that used SET 1 under Felucca (TRANH, which then played PIANO) now plays the DUNGEON KIT.
`tools/make_dungeon_kit.py` makes the kit (`assets/samples-cc0/DUNGEON`).

## DICE (EDIT > DICE)

On an FMTONE, FMKIT or CHIP track, tap **EDIT** until **DICE** (after EDIT 1 and EDIT 2). Each die is one part of the sound:

* FMTONE: RATIO, COLOR, CNTR, MSUS, FDBK, SWEEP, STACK, SUB and ENV (the amplitude envelope).
* CHIP: MODE, WAVE, DUTY, CRSH, SWEEP, VIB, ARP, TONE and ENV. DUTY follows the WAVE just rolled (a wave-RAM table
  for WRAM, a pulse width for PLS) and CRSH follows the MODE (on STEP it is the stepped decay, DCY). Chords and bass
  never roll NOISE or a chord ARP; only a lead with a short STEP decay rolls a SWEEP (a zap). MODE, WAVE, DUTY, CRSH,
  ARP and ENV are choices, so at a WILD below 100 % they change or not (by that chance) rather than part way.
* FMKIT: BD, SD, HH, PC (each voice's pitch, decay, color, sweep and shape together), MIX (the four levels) and FEEL
  (open-hat length, click, drive, variation).

| Control | Does |
|---|---|
| **OCT+** | ROLL: every die that is not held gets a new value |
| KNOB 1 DIE | move between the dice |
| KNOB 2 HOLD | right holds the die, left lets it go (held dice are filled in) |
| KNOB 3 WILD | 100 %: a fresh roll; lower: each die only moves part of the way, for small variations |
| KNOB 4 HIST / **OCT−** | step back through the last eight sounds before each roll (and forward again) |

Play the keys between rolls to hear each one. The dice are weighted by the track's role: T2 rolls pad and chord
sounds, T3 bass sounds (low ratios, often a sub, short envelopes), T4 leads. A rolled COLOR never goes past what its
RATIO keeps musical; if you hold a bright COLOR, RATIO rolls only among ratios that suit it. Successive rolls are one
undo (SAVE held); `tests/dice_test.c` listens to 900 rolls per engine (FMTONE, CHIP) and checks none clip, carry DC,
hang or turn harsh.

## Live effects

There are two layers of effects: one effect on each track, set up while you produce and ridden live with ARP held;
and four master effects on the whole mix, ridden with FX held.

### One effect per track (FX > INSERT, ridden with ARP held)

Every track has an INSERT: one effect, three settings (A B C) and a dry / wet MIX. Pick it on FX > INSERT (TYPE and
A B C); **INSERT 2** has the four you ride: A, B, C and MIX. Felucca's SOFT HARD FOLD FUZZ CRUSH PHASR FLANG CHOR are all
there, and Dungeon Tracker adds three:

| TYPE | A | B | C | |
|---|---|---|---|---|
| **FILTR** | CUT | RES | MODE (LP, LP+BP, BP, BP+HP, HP) | a resonant state-variable filter; MODE blends smoothly from low-pass to high-pass |
| **DUCK** | DEPTH | REL (release) | SHAPE | the track dips at every kick of T1 and comes back over REL; SHAPE holds it down longer (a pump) |
| **REPT** | RATE (1/8 1/8T 1/16 1/16T 1/32 1/64) | DECAY | PITCH (−4 … +4 semitones a pass) | a beat repeat of this track only |

* **DUCK**'s kick: on an FMKIT T1, the KICK voice; a DRUM kit, its kick; any other engine on T1, every note.
* **REPT** starts when MIX comes up from 0: at the next RATE boundary it grabs one RATE of the track and repeats it,
  each pass DECAY quieter and PITCH higher or lower. MIX back to 0 lets the live sound back in. Picking REPT sets MIX to
  0, so it waits for you. (It shares the SLICER's buffer: with the track's SLICER on STUT, or one of FX's buffer
  effects running, REPT stands aside.)
* Picking FILTR, DUCK or REPT loads their own starting values for A B C.

**Hold ARP** and KNOB 1–4 are the selected track's A, B, C and MIX, from any page. The map shows each track's effect.
While ARP is held, **black keys 1–4** switch the knobs to T1–T4; let go of ARP and you are back on the track you
were on. With the track armed (REC) and playing, the rides are recorded as automation, like any knob. A tap of ARP still
opens the ARP page.

### Four master effects (FX held, KNOB 1–4)

**Hold FX** and KNOB 1–4 are four master effects. Pick which four on **FX > MASTER** (KNOB k chooses knob k's effect;
saved with the project). The pool:

| Effect | Knob |
|---|---|
| FILTER | left a low-pass, right a high-pass (Felucca's) |
| CRUSH | bit crush (Felucca's) |
| THROW | the mix into the delay and reverb (Felucca's) |
| DEPTH | how much of FX's key effects you hear (Felucca's; SHIMR while OCT UP / DN plays) |
| ISOLATR | left takes the lows away, right the highs (a DJ kill) |
| WASH | the mix into the reverb, and the reverb bigger |
| PUMP | the whole mix ducks on every beat, in time |
| RIDE | every FMKIT / FMTONE track brighter (COLOR and FDBK up) |

**The knobs stay where you leave them** when you let go of FX (through the menu too), so you can build up and hold a
drop. The FX button stays lit while any master effect is on. **FX + OCT−** sets all of them back to rest.
A project from before keeps FILTER, CRUSH, THROW, DEPTH.

FX's white keys (repeat, reverse, tape stop, freeze, octaves, flanger, phaser, throws) and black-key mutes work as
before.

## HOME: the dungeon

HOME's view is a first-person dungeon corridor. While the transport runs, the party walks one square per beat at the
song's tempo, and the four slots along the bottom show which tracks are in the party and the party's level.

**Crossroads.** Every other phrase (a pass of the longest pattern playing), a crossroads comes up the corridor, placed
so the party reaches it as that phrase ends. There it goes straight on, turns left or turns right, a third each; a
turn pans the view into the new hallway over half a beat, and the compass in the corner (N E S W) follows. This
happens with the game on or off. With the game on, a fight holds the walk, so the crossroads comes when the party gets
there, and a party facing a monster goes straight on.

**The game is off by default.** With it off, the corridor is just scenery: no monsters, no dice rolls, nothing cut.
Dungeon Tracker is a plain four-track FM groovebox.

**To play:** hold **HOME** for the MENU, turn **ALGORITHM** to the **GAME** tab (one step left from DISPLAY), and
switch **DUNGEON** on with KNOB 1 or OCT+. A run starts at B1. Goblins, skeletons and slimes come up the corridor
and are fought in turns, played by your song (below); **ten slain on a floor and its boss comes**, the ogre. The game then runs on every page,
whatever HOME shows. Switch DUNGEON off to end the run: every cut track comes back at once. It is off again after
every power-on; it is not saved.

Each track fights as its class (*The party*, above; by default T1 rogue, T2 cleric, T3 fighter, T4 mage):

| Class | Fights with |
|---|---|
| RGE rogue | a sword; dodges when its drums leave a gap |
| CLR cleric | heals the party when it is hurt, otherwise a strike; heals over time while a note is held; raises the fallen |
| FTR fighter | an axe, more health |
| MAG mage | bolts that ignore armour; stores power while a note is held |

Two of a class fight side by side, each on its own track's notes.

* Every track that has steps and is not muted joins the party. With no steps anywhere the view reads NO PARTY.
* **Levels.** The whole party starts every run at **L1** (in the HUD). Every boss slain, **everyone levels up once**
  (`LEVEL UP: L2`), up to **L9**. A level is health and a harder hit.
* **Health is hidden.** It is 6 + 4 per level (the fighter gets 4 more): 10 at L1, 42 at L9. A level up adds its 4 at
  once. A wounded adventurer's name blinks (under a third of its health).
* **Floors and bosses.** Monsters come up the corridor until **ten are slain on the floor** (a pair counts two). Then
  nothing else comes but the **ogre**, the floor's boss: three times a monster's health and a heavier blow. Slay it and
  the party levels up and takes the stairs two squares on, one floor down (B1, B2, …), where the count starts again.
* Monsters grow with depth **and match your party**: their health and blows scale with the party's level, with how
  many there are, and with how much they play. Walking heals a little every other square (more when there is no cleric
  in the party). Expect push and pull: with a typical song, in ten minutes, about seven floors, around eight falls
  and one whole-party fall, the first about two minutes in. A lone simple loop has the floor's ten and its ogre to
  beat alone: it often stays near the top.

### Fighting in turns

A fight is played by the song, sixteenth by sixteenth. A **round is five sixteenths**: the monster's turn first,
then T1's, T2's, T3's and T4's, one each. A beat has four sixteenths, so the rounds turn against the beat
(five against four): each round starts a beat later and one sixteenth deeper, and every turn visits every place in
the beat once every five beats. Nothing on the screen shows whose turn it is; listen, watch who swings, and work it
out.

On its turn, each hero does what **its track played since its last turn**: only notes that actually sounded count (a
step whose chance skipped it is nothing), and a fallen hero's turn simply passes.

* **A note: the action.** The rogue and the fighter strike, the mage bolts, the cleric heals whoever is under 60 %
  (or strikes when nobody is). The strongest note since the last turn sets how strong:
  * velocity scales it, an **accent** adds half again and may land a critical hit;
  * on the kit, each drum weighs differently (the kick heaviest, the hats lightest);
  * a **chord** adds a quarter a note; on the cleric it heals the whole party;
  * a **ratchet** is a flurry of hits, a **slide** cuts through a skeleton's armour;
  * a **chance** below 100 % hits harder when it does play, a **nudge** off the grid is a sneak attack (the rogue's best);
  * more notes in the same window add a little (+15 % each, up to +45 %).
* **A held note (tie).** The rogue and the fighter follow through, weaker each turn (half, then a quarter). The
  cleric heals over time, more each turn the note is held; the mage stores power for its next bolt.
* **Nothing played: charge.** The hero gathers itself: +100 % on its next action for each turn it waited, up to four.
  Sparse parts hit rarely but hard; busy parts hit often.
* **An explicit REST step: guard.** The monster hits a guarding hero half as often.
* **Pairs.** From B2 down, now and then (about one encounter in eleven) a second monster joins the one coming
  up the corridor, and the two stand side by side (HOME says e.g. `SKELETON AND GOBLIN`, with a health bar each).
  Each is a little weaker than alone; both strike every round, each a little lighter, so a pair hits about a third
  harder than one. The party fights the front one (left); the **fighter's axe cleaves**, giving the other half of
  each blow (`CLEAVED` if it falls to it). When the front one falls, the other steps up and the fight goes on. Never
  beside the ogre.
* **The monster** strikes one standing hero at random each round. If it picks a rogue on a sixteenth where **its
  track plays no drum**, the rogue isn't there: a dodge. A busy drum part never dodges; a sparse or syncopated one often
  does.

### Falling cuts the track

When an adventurer's health reaches 0 it falls, and **its track is cut from the mix** until it is back on its feet.
The slot reads CUT and the view says e.g. FIGHTER FALLS: T3 CUT. The cut works like PERFORM's black-key mutes: a
click-free fade, with the sequencer still running underneath, so the track comes back in time. The reverb and delay
tails ring out. Nothing is saved, and the track's MUTE is not touched.

A fallen adventurer gets back up:

* when the cleric raises it: a 1 in 5 chance each beat the party walks, while a cleric stands. The view says
  T1 BACK (etc.).
* at the stairs: everyone fallen stands up on the next floor, at half health.
* when the whole party falls: the song is silent for one bar (4 beats), then the party retreats a floor up, healed.
  It keeps its level, and clears that floor again (its ten and its ogre, another level up).
* when you stop the transport: the party camps, and the fallen are raised.

Muting and unmuting a fallen track does not raise it.

**FLOOR FX.** Every time the party takes the stairs down, one track gets one effect for that floor, and HOME
says which (`STAIRS: T3 ECHO`). The track is one of the party's, at random; the effect is one of
ten, at random among those that fit the track:

| Effect | What it does |
|---|---|
| ECHO, CAVERN, CHORUS | the track's delay, reverb or chorus send raised |
| DRIVE | its DIST raised a little |
| CRUSH, PHASER, FLANGER, MUFFLE | its INSERT (8-bit crush; a phaser; a flanger; a resonant low-pass) |
| PUMP | its INSERT DUCK, pumping to T1's kick (not on T1) |
| SLICER | its SLICER gating a random rhythm on 1/16s |

It never takes an INSERT or SLICER you are using, nor a value that automation, a lock or SOUND MORPH moves, and never
the same effect on the same track twice running. The next floor, a retreat, STOP (the camp) or the game switched off
lifts it: each value goes back, unless you turned it meanwhile (then your turn stays). It only lives while the music
plays, so nothing you save ever holds it.

How often it happens (ten minutes at 120 BPM, 20 runs each): a lone kick loop on the beats reaches about B12 and is
silent about 1 % of the time (its rogue dodges three monster turns in four). A busy four-track song reaches about B31,
and its tracks are cut about 0.2 % of the time. The power-on songs fall about eight times in ten minutes at their own
tempos. `tools/combat_sim.py` models the fight; it was tuned on the power-on songs and a set of styles (sparse,
busy, syncopated, drones, drums and bass only) so the music decides more: syncopated and expressive parts fall least,
and nobody is left out.

MENU > DISPLAY > HOME still offers Felucca's TRACKS and LEVELS views in place of the corridor (DUNGEON). With the
game on, it keeps playing, and cutting, behind them.

The game never changes the saved project, and it starts over at power-on. It draws from the palette, so it follows
MENU > COLOR.

## The tracker (SEQ)

Tap **SEQ**: the selected track's **TRACKER** (tap again: AUTOMATION, then VARY). Steps run top to bottom, `00` to LEN − 1; the
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
| FX | a command (below), or a parameter lock: short name + hex value (`BDP41`, `DEC3A`) | set it on this step | which: SPEED, LOOP, PITCH, then the parameters (the engine's eight, a kit's voice values, level, pan, envelope, sends) |

**EDIT** clears the cursor's cell (on NOTE: the whole step and its locks, and the cursor moves down). Keys: with the
track armed (**REC**) and the transport stopped they write the cursor's step and move down; on a kit they **add** their
drum to what is already there, so a beat is built one drum at a time (the kicks, then back to the top for the hats);
otherwise they just play. SAVE held undoes, as everywhere in Felucca.

### Commands (the FX column)

The first three entries of KNOB 4's FX list are commands, in the spirit of a ProTracker effect column. A row with one
shows it in words, whichever FX entry is picked:

| Command | Shown | KNOB 3 | Does |
|---|---|---|---|
| SPEED | `HALF`, `DBL`, `SPx1` | x1, 1/2, x2 | from this row on, half time (each row holds two steps, its note twice as long) or double time (two rows a step); `SPx1` back to normal |
| LOOP | `L2x3` | rows back (1, 2, 4, 8) × times (1–8) | after this row, jump back that many rows, that many times: `L2x3` plays the last two rows three more times |
| PITCH | `P+2` | −12…+12 | on a LOOP row: each pass of the loop that many semitones up or down (notes only; a kit's drums stay) |

**The pattern keeps its length.** The track's loop always takes the time it did; when it comes round, it starts again
at row `00`, at normal speed, with no loop or pitch, so a track with commands never drifts from the others or from a
SONG's sections. A half-time section or a long roll simply leaves the rows after it unplayed for that pass, so put
them where you want the drop or the fill. The tracker's playing row follows the row actually played. While SPEED is
1/2 or x2, ratchets play once; a track with commands plays every step on its start (no nudge). Commands are saved
with the project, undone with SAVE held, and listed on AUTOMATION.

### VARY: sequence dice (SEQ > VARY)

DICE for patterns. **OCT+** rolls a variation of the selected track's pattern; dice you hold keep their choices, the
rest roll again.

| Die | Does |
|---|---|
| HITS | adds hits on empty steps (a melodic track repeats the note before; a kit picks one of its drums, hats likeliest), drops a few off-beat ones |
| NOTES | a melodic track: moves notes 1–3 semitones or an octave, onto the track's scale; a kit: swaps drums for their kin (closed / open hat, snare / clap, tom / rim / bell) |
| SHIFT | moves off-beat hits a step early or late |
| RATCH | adds ×2 / ×3 ratchets, more in the last quarter (fills) |
| ACC | accents on and off, velocities up and down |
| PROB | adds a chance (50–90 %) or a 1:2 / 2:2 condition on off-beat steps |

| Control | Does |
|---|---|
| **OCT+** | ROLL: every die not held rolls; the variation is written into the pattern (SAVE held undoes it) |
| KNOB 1 DIE | move between the dice |
| KNOB 2 HOLD | hold the die (filled in) or let it go |
| KNOB 3 WILD | how much each die does (50 % at first) |
| KNOB 4 HIST / **OCT−** | the last eight rolls, newest first, then ORIG: the original back |

Every roll starts from the **original** (the pattern as it was at the first roll), not from the last roll, so rolling
never drifts away. A die decides step by step from its own seed, so a held die makes the same choices on the same
steps whatever the others roll. Change the pattern by hand and the next roll takes it as the new original. The strip
under the dice shows the first 32 steps, the ones the dice changed in the accent colour. Downbeats are left alone by
HITS and SHIFT, a kit's kick never changes drum, and the steps' locks and commands stay where they are. Not while a
SONG plays.

## Songs: the PATTERN BANK (ENV held)

Six patterns live in the white keys **F3 G3 A3 B3 C4 D4**, as **A B C D E F**: an intro, a verse, a chorus, a
bridge, a fill and an outro, or whatever you like. A pattern is all four tracks' steps, their LEN DIV SWING GATE and
QUANTIZE, and their automation, locks and tracker commands. The sounds are shared: every pattern plays the tracks'
current instruments (as Felucca's songs always did).

Hold **ENV** for the PATTERNS map (a tap still opens the ENV page):

| With ENV held | What it does |
|---|---|
| a key with a pattern | Stopped: there at once. Playing: it's **next**, and starts when this pattern ends, every track from its step 1 together. |
| the next one's key again, or the current one's | Back to staying where you are. |
| an empty key | The current pattern is copied into it, and you carry on in the new key (finish the verse in A, press B, and B starts as a copy of A). |
| OCT+ held + a key | Copy the current pattern into that key (you stay where you are). |
| OCT− held + a key | Clear that key (not the one playing). |
| OCT− tapped alone | Forget the next pattern. |

**Edits stay in their key.** Whatever you change while a pattern is current (steps, LEN, automation, VARY rolls,
loading a project) belongs to that pattern. Switching away keeps it there, and switching back brings it back.

**The end of a pattern** is the end of its longest track's loop: a 2-bar bass under a 1-bar beat switches after
the 2 bars. The switch is on time to the sample, and nothing is skipped.

The map shows each key's four tracks in miniature. The current pattern is selected and the next one flashes, and
the cards show NOW, NEXT, how many are stored, and whether they're all in the flash yet. The header shows the letter
too: `A`, or `A>C` with C next. The LEDs: the current key lit, the next one flashing, the stored ones breathing.

**SONG (GLO > SONG)** now arranges the bank: a section's tracks each name a pattern A–F (or "-" for silent), and
F3..D4 set the selected cell, as with ENV. KNOB 3 turns A..F then "-". PLAY SONG stores the current pattern first and
refuses a section on an empty key ("PATTERN F EMPTY"). A section lasts its longest track's loop times REPS. While
a song plays, the ENV keys say SONG PLAYING.

**Record the arrangement.** On GLO > SONG, tap **REC**: the line says REC: PRESS PLAY. Press PLAY and jam: switch
patterns with ENV + the keys as you like, then STOP. Every pattern you played becomes a section (all four tracks on
it, REPS its whole loops, more sections past 16), replacing the song that was there. The last pattern counts at least
once. Tap REC again before PLAY to disarm. PLAY on SONG then plays what you recorded, and you can edit it as any song.

**MIDI.** A Program Change on **channel 16** picks a pattern: program 1–6 is A–F (0–5 counting from 0). Stopped, it
switches at once; playing, it queues, exactly as ENV + the key. Empty keys are ignored, as are program changes on
other channels. So the QY-70 (or any sequencer) can call the sections: send it program changes on channel 16 where
each part starts.

**Saving.** The six patterns have their own flash, written while stopped and idle for 3 s (an erase silences the
audio for ~40 ms, so never while anything sounds). The pattern you're on is also in the autosave, with which key it
is, so a power cycle puts you back where you were. Its key's own copy is refreshed at most once a minute.

**Where the flash came from.** Every sector of the FM-1 was already taken, so the bank uses the top 48 KiB of the
third user sample slot (the rest of USR3, with USR2, holds the song slots).

## SOUND MORPH (EDIT > MORPH)

Each track can blend its sound toward a **target**: another preset of the same engine. KNOB 1 picks the target
(OFF, then the engine's presets as P1, P2…), KNOB 2 the **AMOUNT** 0–100 %. At 0 % you hear the sound as it is; at
100 % the target's values; in between, everything that glides is part way: envelopes, filter, the engine's eight
EDIT values, the sends, a kit's pitches and decays. What doesn't glide stays the sound's own: the engine, its lists
and switches (waveforms, modes, the voice mode), an FM6 patch's operators, the mix (LEVEL, PAN, MUTE).

The page shows the target's name and a line per parameter that differs, from your sound to the target's, with a
marker where it is now.

* **Edits stay yours.** Turn a knob while the morph is part way and that value becomes the sound's own; back at 0 %
  you hear your edit.
* **Automation wins.** A parameter the pattern's automation or locks are moving is left to them.
* **Per pattern: the MORPH command.** In the TRACKER's FX column, after SPEED LOOP PITCH, MORPH sets the amount from
  that step on (shown M0 … M100). Put M100 on step 1 of the chorus pattern and M0 on the verse's, and switching
  patterns switches the lead from soft to bright, with the same four sounds.
* **Saved** with the project and the autosave: the amount, the target, and your sound as it is without the morph.

Switching a track to another engine lets go of the morph (the target named a preset of the old engine).

## The web editor (web/editor.html)

Felucca's desktop editor, brought up to Dungeon Tracker. Open `web/editor.html` in Chrome or Edge (it talks to the FM-1 over
USB MIDI; add `?mock=1` to try it without hardware). It knows FMKIT and FMTONE (their presets, an FMKIT track's
KICK / SNARE / HAT / PERC / SHAPE / LANES pages, its steps as drum hits), CHIP's chip presets, the three new track
effects (FILTR DUCK REPT), and lists the engines in Dungeon Tracker's order. Its Song panel arranges Dungeon Tracker's sections: a
pattern A–F or – (silent) per track, and the repeats. It groups every parameter by page on current firmware
(Felucca's editor fell back to one long list there). The four master effects (FX held) are set on the device.

## Differences from Felucca, in short

* SEQ's STEP and DETAIL pages (piano roll, drum grid, per-step detail lanes) are replaced by the TRACKER; AUTOMATION
  stays, and VARY joins it. The TRACKER's commands are step records on ids past the parameters (`CMD_*`, core.h). Felucca's own UI tests that drive the grid or the roll no longer apply.
* Engines 14 (FMKIT), 15 (FMTONE), 16 (WAVES) and 17 (SID) added. SLICE (13) is retired (its number kept, never
  offered): a project's SLICE track loads as SAMPLE — its BREAK as the LOFI KIT, USR1 and PIANO as themselves. SAMPLE's
  SAX set is the LOFI KIT (SET 3). The EDIT layer's 16 white keys hold the first 14 engines (NOISE and GRAIN are on
  PRESETS only).
* The four tracks' voices and sound lists come from their classes (*The party*); PRESETS' LIST starts at CLASS
  (Felucca's starts at ALL, and ALL is one step right).
* Presets of the CYC engines cannot be starred as favourites (Felucca's settings keep rows 14 and 15 of the
  favourites for MIDI LEARN and other settings).
* HOME shows the dungeon corridor instead of the scope (MENU > DISPLAY > HOME: DUNGEON, TRACKS, LEVELS). With MENU >
  LARGE ON the dungeon takes the whole screen under the header; turning a knob brings LARGE's tall cards back,
  and the dungeon returns 2 s after the last turn.
* A sixth MENU tab, GAME, holds DUNGEON OFF / ON: the dungeon's game, where a fallen adventurer cuts its track. Off at
  power-on, not saved. (Felucca's tests that count five tabs fail in a Dungeon Tracker build; they pass with `RONDO_LAYOUT=0`.)
* INSERT gains FILTR, DUCK and REPT; INSERT 2 is A B C MIX. ARP held is a layer (the track's effect to ride); a
  tap still opens ARP.
* FX's knobs are four master effects from a pool (FX > MASTER, saved in the project's otherwise unused `G_ENGGO`), and
  they stay put when FX is let go; FX + OCT− resets them (it no longer shifts the octave). Felucca's tests that expect
  the snap back, the octave shift or ARP's hold fail in a Dungeon Tracker build; with `RONDO_LAYOUT=0` the three INSERTs, the
  pool and the layer are left out and those tests pass.
* SONG plays the PATTERN BANK A–F instead of the four project slots A–D (a section's byte keeps 0–3 for A–D and 4
  for "-", and takes 5 and 6 for E and F), with SONG's keys on F3..D4. ENV held is the bank's layer. User sample slot 3
  keeps 32 KiB; its top 48 KiB are the bank's. SAVE > PROJECT has eleven slots, A–K, each a whole song with the bank's
  patterns (song_store.c, in user sample slots 2 and 3's flash and the four project objects' sectors). Felucca's tests of songs from project slots, of the header's slot
  letter and of slot 5 being refused fail in a Dungeon Tracker build. On SONG, REC records the arrangement (Felucca: "[SEQ] TO
  RECORD"). A Program Change on channel 16 picks a pattern; on the track channels it loads nothing (as before in Dungeon Tracker).
* EDIT > MORPH and the MORPH command; a project keeps the morph in 8 of FUN10's spare bytes (`PROJ_MORPH_OFF`) and the
  autosave which pattern is current in the header's reserved byte 65.
* The splash reads DUNGEON / TRACKER, the version and "a Felucca fork"; MENU's ABOUT shows the name with Felucca's
  credits. Inside the code the fork's additions are still marked `RONDO` and `RE-1` (its old names: `RONDO_LAYOUT`,
  `tests/run_rondo.sh`); the editor protocol still answers "FELUCCA" for the tools that look for it.
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
./build.sh --release 1.0                                # build/dungeon-tracker-1.0.fwsc
```

### Tests

```sh
sh tests/run_rondo.sh          # Dungeon Tracker's checks (engines, tracker, commands, vary, dice, dungeon, live effects,
                               # the pattern bank and its flash, the arrangement, MIDI
                               # program changes, sound morph, NEW, WAVES), demo WAVs
```

`tests/run_tests.sh` is Felucca's full suite; build it with `CC="cc -DRONDO_LAYOUT=0"` to check Felucca's behaviour
is unchanged outside the new engines. Felucca's `tests/ui_test.c` runs to the end in both layouts (and clean under
AddressSanitizer); its failures are Felucca behaviour Dungeon Tracker changes on purpose (TRACKER in place of STEP / DETAIL and
the DRUM grid, the two new engines in the lists, the header's pattern letter, step entry adding drums on T1's kit): 56 with `RONDO_LAYOUT=0`, 192 in the
Dungeon Tracker layout. A change should leave those lists exactly as they are.

## Flashing (at your own risk)

1. Update the FM-1 to M-VAVE's official **V15** first, and keep that `FM-1.fwsc` (m-vave.com/download).
2. Have a recovery path *before* you flash anything unofficial: the FM-1 has one firmware slot. While Dungeon Tracker runs,
   **OCT−** + **OCT+** held 5 s enters UPDATE MODE, and two failed boots in a row enter it by themselves (Felucca's
   boot guard). If a build cannot reach either, recovery needs
   [FM-1 Transporter](https://github.com/kurogedelic/FM-1-transporter) (a Seeed XIAO RP2040).
3. Install `dungeon-tracker-1.0.fwsc` with `python3 tools/fm1_install.py build/dungeon-tracker-1.0.fwsc` over USB, or a local copy of
   the web installer (`web/make_site.py`, see BUILDING.md).
4. To go back: install M-VAVE's `FM-1.fwsc` the same way.

## Changes

* **1.0**: first release. Over Felucca 1.6.1: the FMKIT, FMTONE, WAVES (Ambika's oscillators) and SID engines, the
  LOFI KIT, CHIP's presets and the dungeon set; DICE sound design; the TRACKER with its commands and VARY; THE PARTY
  (a class per track: its voices and its sounds, 12 points; PRESETS LIST CLASS, with ALL); the dungeon on HOME, its
  game (the party from L1, a boss every floor after ten slain, a level each boss), monster pairs and FLOOR FX, MENU > LARGE; the PATTERN BANK and SONG over it; eleven whole-song slots A–K with
  power-safe saves, and the QUICK slot; the autosave 3 s after STOP with the header's unsaved mark; SOUND MORPH; four
  track effects and the master FX pool; the web editor and its backup brought up to all of it. SLICE, the SAX
  samples and user sample slots USR2 and USR3 are retired for the room. The DUNGEON KIT (SAMPLE SET 1) and 96 more
  presets across FMKIT, FMTONE, WAVES and SID.

## Licence

GPL-3.0-only, as Felucca. Felucca-Engines (firmware/engines) is MIT. See LICENSE, LICENSING.md and CREDITS.md.
Dungeon Tracker is not affiliated with M-VAVE, Hügelton Instruments or Elektron.
