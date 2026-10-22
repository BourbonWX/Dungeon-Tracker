# Dungeon Tracker — a tracker groovebox with a dungeon crawl, firmware for the M-VAVE FM-1

Dungeon Tracker (formerly RE-1 "Random Encounters", and RONDO before that) is an old-school tracker-style four-track
FM groovebox with an old-school first-person dungeon crawl on its HOME screen, played by the song. It is a fork of [Felucca](https://github.com/hugelton/Felucca) 1.6.1 (GPL-3.0) by Leo Kuroshita / Hügelton
Instruments. It keeps everything Felucca does (its sequencer, song mode, effects, the other engines, the web
installer's package format) and adds:

* **FMKIT** (engine 14) — four FM drum voices, Model-machine style: **KICK, SNARE, HAT, PERC**, each with its own
  PITCH, DECAY, COLOR, SWEEP, SHAPE and LEVEL.
* **FMTONE** (engine 15) — a two-operator FM voice with a sub and a detuned second carrier, for chords, pads, basses
  and leads, with a timbre envelope (COLOR / CNTR / MSUS) instead of an operator editor.
* **CHIP**, Felucca's LOFI engine renamed for what it plays (its first knob, Felucca's CHIP, is now MODE), with
  twelve more presets (square and pulse leads, a staircase-triangle bass, plucks,
  arps, zaps, chip kick, noise hat and snare, a wave-RAM organ and choir).
* **DICE sound design**: on FMTONE, FMKIT and CHIP tracks, every part of the sound is a die. Roll, hold the dice you like,
  roll again, and the rest change around them, always inside ranges that sound musical.
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

> **Status: untested on hardware.** Dungeon Tracker builds (`dungeon-tracker-0.1.fwsc`) and passes its tests in the
> browser emulator and on the host (the firmware's own C code), but it has not been flashed to an FM-1 yet. Read
> *Flashing* below before you try.

## Powering on

The splash: DUNGEON over TRACKER in the firmware's own block lettering, in the palette's colours (MENU > COLOR).
Then, unless there is music of yours to bring back (MENU > SYSTEM > RESTORE LAST, or a pattern stored in the PATTERN
BANK), Dungeon Tracker writes a **song of its own** (`firmware/src/boot_song.c`): a tempo, a key and mode, a four-bar
chord progression, an FMKIT beat on T1, chords on T2, a bass line on T3 and a lead on T4 (FMTONE or CHIP sounds),
different at every power-on. HOME shows the dungeon with the whole party and **PRESS PLAY**: press it and they walk.
Nothing of it is saved until you change something; then it is yours and comes back at the next power-on.

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
song's tempo, and the four slots along the bottom show which tracks are in the party and each one's level.

**Crossroads.** Every other phrase (a pass of the longest pattern playing), a crossroads comes up the corridor, placed
so the party reaches it as that phrase ends. There it goes straight on, turns left or turns right, a third each; a
turn pans the view into the new hallway over half a beat, and the compass in the corner (N E S W) follows. This
happens with the game on or off. With the game on, a fight holds the walk, so the crossroads comes when the party gets
there, and a party facing a monster goes straight on.

**The game is off by default.** With it off, the corridor is just scenery: no monsters, no dice rolls, nothing cut.
Dungeon Tracker is a plain four-track FM groovebox.

**To play:** hold **HOME** for the MENU, turn **ALGORITHM** to the **GAME** tab (one step left from DISPLAY), and
switch **DUNGEON** on with KNOB 1 or OCT+. A run starts at B1. Goblins, skeletons and slimes come up the corridor
and are fought one round per beat, and every fifth floor has an ogre in the middle. The game then runs on every page,
whatever HOME shows. Switch DUNGEON off to end the run: every cut track comes back at once. It is off again after
every power-on; it is not saved.

| Track | Adventurer | Fights with |
|---|---|---|
| T1 | RGE rogue | a sword |
| T2 | CLR cleric | heals the party when it is hurt, otherwise a strike; raises the fallen |
| T3 | FTR fighter | an axe, more health |
| T4 | MAG mage | a bolt every other beat, hits hard |

* Every track that has steps and is not muted joins the party. With no steps anywhere the view reads NO PARTY.
* Each adventurer's level (L1–L9, in the HUD) is how much its track does. Levels rise with:
  * notes on, and distinct pitches (on a kit, distinct drums);
  * chords and parameter locks;
  * chances, conditions, ratchets and nudges.
* **Health is hidden.** It is 6 + 4 per level (the fighter gets 4 more), so a simple kick loop has 10 and a busy track
  42. A wounded adventurer's name blinks (under a third of its health). Make a track busier mid-fight and it gains the
  extra health at once.
* After 24 squares the party takes the stairs one floor down (B1, B2, …). Monsters grow with depth.

### Falling cuts the track

When an adventurer's health reaches 0 it falls, and **its track is cut from the mix** until it is back on its feet.
The slot reads CUT and the view says e.g. FIGHTER FALLS: T3 CUT. The cut works like PERFORM's black-key mutes: a
click-free fade, with the sequencer still running underneath, so the track comes back in time. The reverb and delay
tails ring out. Nothing is saved, and the track's MUTE is not touched.

A fallen adventurer gets back up:

* when the cleric raises it: a 1 in 5 chance each beat the party walks, while T2's cleric stands. The view says
  T1 BACK (etc.).
* at the stairs: everyone fallen stands up on the next floor, at half health.
* when the whole party falls: the song is silent for one bar (4 beats), then the party retreats a floor up, healed.
* when you stop the transport: the party camps, and the fallen are raised.

Muting and unmuting a fallen track does not raise it.

How often it happens (ten minutes at 120 BPM, 20 runs each): a lone kick loop reaches about B4 and is silent about
6 % of the time (it falls roughly twice a minute). A kit, chords and bass reach about B21, and each track drops out
for a bar or two roughly every 30 seconds between them. A busy four-track song reaches about B36, and its tracks are
cut less than 0.1 % of the time.

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
track armed (**REC**) and the transport stopped they write the cursor's step (on a kit, its drums) and move down;
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
third user sample slot. **USR3 now holds samples up to 32 KiB** (USR1 and USR2 stay at 80 KiB). A larger upload to
USR3 is refused, and a USR3 sample over 32 KiB from before shows as empty (re-upload a shorter one).

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
* Engines 14 and 15 added; SLICE (13) is no longer offered in the engine lists (so all engines still fit the EDIT
  layer's 16 white keys) but still plays in projects and user presets that use it.
* Presets of the CYC engines cannot be starred as favourites (Felucca's settings keep rows 14 and 15 of the
  favourites for MIDI LEARN and other settings).
* HOME shows the dungeon corridor instead of the scope (MENU > DISPLAY > HOME: DUNGEON, TRACKS, LEVELS).
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
  keeps 32 KiB; its top 48 KiB are the bank's. Felucca's tests of songs from project slots, of the header's slot
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
./build.sh --release 0.1                                # build/dungeon-tracker-0.1.fwsc
```

### Tests

```sh
sh tests/run_rondo.sh          # Dungeon Tracker's checks (engines, tracker, commands, vary, dice, dungeon, live effects,
                               # the pattern bank and its flash, the arrangement, MIDI
                               # program changes, sound morph), demo WAVs
```

`tests/run_tests.sh` is Felucca's full suite; build it with `CC="cc -DRONDO_LAYOUT=0"` to check Felucca's behaviour
is unchanged outside the new engines. Felucca's `tests/ui_test.c` runs to the end in both layouts (and clean under
AddressSanitizer); its failures are Felucca behaviour Dungeon Tracker changes on purpose (TRACKER in place of STEP / DETAIL and
the DRUM grid, the two new engines in the lists, the header's pattern letter): 56 with `RONDO_LAYOUT=0`, 190 in the
Dungeon Tracker layout. A change should leave those lists exactly as they are.

## Flashing (at your own risk)

1. Update the FM-1 to M-VAVE's official **V15** first, and keep that `FM-1.fwsc` (m-vave.com/download).
2. Have a recovery path *before* you flash anything unofficial: the FM-1 has one firmware slot. While Dungeon Tracker runs,
   **OCT−** + **OCT+** held 5 s enters UPDATE MODE, and two failed boots in a row enter it by themselves (Felucca's
   boot guard). If a build cannot reach either, recovery needs
   [FM-1 Transporter](https://github.com/kurogedelic/FM-1-transporter) (a Seeed XIAO RP2040).
3. Install `dungeon-tracker-0.1.fwsc` with `python3 tools/fm1_install.py build/dungeon-tracker-0.1.fwsc` over USB, or a local copy of
   the web installer (`web/make_site.py`, see BUILDING.md).
4. To go back: install M-VAVE's `FM-1.fwsc` the same way.

## Licence

GPL-3.0-only, as Felucca. Felucca-Engines (firmware/engines) is MIT. See LICENSE, LICENSING.md and CREDITS.md.
Dungeon Tracker is not affiliated with M-VAVE, Hügelton Instruments or Elektron.
