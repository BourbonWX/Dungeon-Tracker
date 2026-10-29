# SPDX-License-Identifier: GPL-3.0-only
# Copyright (C) 2026 Dungeon Tracker (RONDO) contributors
"""Dungeon Tracker combat study: today's per-beat combat (ui_dungeon.c dg_fight) against the proposed turn system.
Turn system: a round is 5 sixteenth steps, M R C F W (monster first); a hero acts on its step only if its track does:
  new note -> attack / heal / bolt, shaped by the note (accent, velocity, chord, ratchet, slide, chance, nudge, lanes)
  held (tie) -> weapons follow through (x0.5, x0.25 ..); casters channel (cleric heals over time, mage stores power)
  nothing -> charge (+50% on the next action, up to 3); an explicit REST step -> defend (harder to hit)
Songs: the firmware's power-on songs (songs.json, tests/combat_export.c) and crafted archetypes."""
import json, random, sys, math
from collections import defaultdict

RGE, CLR, FTR, MAG = range(4)
NAMES = ["RGE", "CLR", "FTR", "MAG"]
ST_NOTE, ST_TIE, ST_REST = 0, 1, 2
WIPE, RAISE = 4, 5
BOSS_AT, STAIRS, LVMAX, EDGE = 10, 2, 9, 10                 # ui_dungeon.c: a floor's boss after ten slain; the party's level
LANE_W = [1.5, 1.2, 1.2, 0.6, 0.8, 1.2, 0.8, 0.8]          # KICK SNARE CLAP HATCL HATOP TOM RIM BELL

# step: [time, n, accent, slide, ratchet, vel, lanes, lane_acc, nudge, prob, note]
def step(time=ST_NOTE, n=1, acc=0, slide=0, rat=1, vel=100, lanes=0, lacc=0, nudge=0, prob=0, note=60):
    return [time, n, acc, slide, rat, vel, lanes, lacc, nudge, prob, note]
EMPTY = [ST_NOTE, 0, 0, 0, 1, 0, 0, 0, 0, 0, 0]


def track(steps, level, kit=False, div=2):
    return {"len": len(steps), "div": div, "gate": 64, "level": level, "kit": int(kit), "steps": steps}


# ------------------------------------------------------------------ the sequencer, per master 16th, per track
DIV16 = {0: 4, 1: 2, 2: 1, 3: 1, 6: 8, 7: 16}                # sixteenths a step (1/32 approximated as 1/16)

class Player:
    """what a track does at each master sixteenth: ('on', step), ('held', ties), ('rest',), ('none',)"""
    def __init__(self, t, rng):
        self.t, self.rng = t, rng
        self.per = DIV16.get(t["div"], 1)
        self.sounding = False                                 # a note playing whose ties may continue
        self.ties = 0
        self.loop = 0

    def at(self, g):
        t = self.t
        if g % self.per:                                      # inside a long step: held while its gate lasts
            return ("held", self.ties) if self.sounding and (g % self.per) < self.per * t["gate"] / 127 + 0.01 else ("none",)
        i = (g // self.per) % t["len"]
        if i == 0 and g:
            self.loop += 1
        s = t["steps"][i]
        if s[0] == ST_TIE:
            if self.sounding:
                self.ties += 1
                return ("held", self.ties)
            return ("none",)
        if s[0] == ST_REST:
            self.sounding = False
            return ("rest",)
        if not (s[1] or s[6]):
            self.sounding = False
            return ("none",)
        p = s[9]
        plays = True
        if 1 <= p <= 100:
            plays = self.rng.randrange(100) < p
        elif p == 101:
            plays = False
        elif p >= 102:                                         # a condition a:b: about 1 in b
            plays = self.rng.randrange(3) == 0
        if not plays:
            self.sounding = False
            return ("none",)
        self.sounding, self.ties = True, 0
        return ("on", s)


# ------------------------------------------------------------------ the shared world (walking, floors, monsters)
class World:
    def __init__(self, song, rng, cfg):
        self.song, self.rng, self.cfg = song, rng, cfg
        self.lv = [1] * 4                                     # the party starts at L1 (a level each boss slain)
        self.inn = [any((s[0] == ST_NOTE and (s[1] or s[6])) for s in t["steps"]) for t in song["tracks"]]
        self.hpmax = [6 + 4 * self.lv[k] + (4 if k == FTR else 0) for k in range(4)]
        self.hp = self.hpmax[:]
        self.dead = [False] * 4
        self.floor, self.best, self.walked = 1, 1, 0
        self.fkills, self.boss_up, self.stairs = 0, 0, 0
        self.mon = None
        self.mdist = 0
        self.wipe = 0
        self.st = defaultdict(float)
        self.first_fall = None
        self.fight_beats = []
        self.fb = 0

    def roll(self, p): return self.rng.randrange(100) < p
    def activity(self):
        """the share of its turns a hero would find something to do (new notes, ties), over the party: 0..1"""
        a, n = 0.0, 0
        for k, t in enumerate(self.song["tracks"]):
            if not self.inn[k]:
                continue
            st = t["steps"]
            per = DIV16.get(t["div"], 1)
            on = sum(1 for s in st if s[0] == ST_NOTE and (s[1] or s[6])) / (len(st) * per)
            held = sum(1 for s in st if s[0] == ST_TIE) / (len(st) * per)
            a += min(1.0, 1 - (1 - min(1.0, on + (0.5 if k in (CLR, MAG) else 0.15) * held)) ** 5)
            n += 1
        return a / max(n, 1)
    def avg10(self):
        n = sum(self.inn)
        return sum(self.lv[k] for k in range(4) if self.inn[k]) * 10 // max(n, 1) + EDGE
    def up(self): return [k for k in range(4) if self.inn[k] and not self.dead[k]]

    def spawn(self):
        boss = self.boss_up == 1
        kind = "OGRE" if boss else self.rng.choice(["GOBLIN", "SKELETON", "SLIME"])
        hp = (4 + 3 * self.floor + 16 * self.avg10() // 10) * (3 if boss else 1) + (-2 if kind == "SLIME" else 0)
        if self.cfg.get("party_scale"):                       # fewer heroes, a smaller monster
            hp = hp * (1 + sum(self.inn)) / 5
        if self.cfg.get("act_scale"):                         # a quiet party, a smaller monster
            hp = hp * (self.cfg.get("act_floor", 0.4) + (1 - self.cfg.get("act_floor", 0.4)) * self.activity())
        hp = int(hp * self.cfg.get("mon_hp", 1.0))
        self.mon = {"kind": kind, "hp": hp, "max": hp}
        self.mdist = 4

    def walk(self):
        self.walked += 1
        if self.walked & 1:
            for k in self.up():
                if self.hp[k] < self.hpmax[k]:
                    self.hp[k] = min(self.hpmax[k], self.hp[k] + (2 if not self.dead[CLR] and self.inn[CLR] else self.cfg.get("lone_heal", 1) if not self.inn[CLR] else 1) - self.rng.randrange(2))
        if any(self.dead) and self.inn[CLR] and not self.dead[CLR] and self.roll(100 // RAISE):
            k = self.dead.index(True)
            self.dead[k], self.hp[k] = False, self.hpmax[k] // 2
        if self.stairs:
            self.stairs -= 1
        if self.boss_up == 2 and not self.stairs:
            self.walked, self.fkills, self.boss_up = 0, 0, 0
            for k in range(4):
                if self.dead[k]:
                    self.dead[k], self.hp[k] = False, self.hpmax[k] // 2
            self.floor += 1
            self.best = max(self.best, self.floor)
            return
        if self.boss_up == 2:
            return
        if self.fkills >= BOSS_AT and not self.boss_up:
            self.boss_up = 1
            self.spawn()
        elif not self.boss_up and self.roll(12 + self.floor):
            self.spawn()

    def hurt(self, k, d, beat):
        self.hp[k] -= d
        self.st["taken%d" % k] += d
        if self.hp[k] <= 0:
            self.hp[k] = 0
            self.dead[k] = True
            self.st["falls"] += 1
            self.st["fall%d" % k] += 1
            if self.first_fall is None:
                self.first_fall = beat

    def end_fight(self, won):
        self.fight_beats.append(self.fb)
        self.fb = 0
        if won:
            self.st["kills"] += 1
            self.fkills += 1
            if self.mon and self.mon["kind"] == "OGRE" and self.boss_up == 1:
                self.boss_up, self.stairs = 2, STAIRS
                if self.lv[0] < LVMAX:                        # everyone up a level, its four health at once
                    self.lv = [l + 1 for l in self.lv]
                    for k in range(4):
                        self.hpmax[k] += 4
                        if not self.dead[k]:
                            self.hp[k] += 4
        self.mon = None
        self.mdist = 0

    def retreat(self):
        self.floor = max(1, self.floor - 1)
        self.walked, self.fkills, self.boss_up, self.stairs = 0, 0, 0, 0
        self.dead = [False] * 4
        self.hp = self.hpmax[:]


# ------------------------------------------------------------------ today's combat: a round a beat, every hero
def today(song, seconds, seed, cfg):
    w = World(song, random.Random(seed), cfg)
    beats = int(seconds * song["bpm"] / 60)
    odd = 0
    for b in range(beats):
        odd ^= 1
        if w.wipe:
            w.wipe -= 1
            if not w.wipe:
                w.retreat()
            continue
        if w.mon and w.mdist <= 1:
            w.fb += 1
            alive = []
            for k in range(4):
                if not w.inn[k] or w.dead[k]:
                    continue
                alive.append(k)
                L = w.lv[k]
                if k == CLR:
                    hurt = [j for j in w.up() if w.hp[j] < w.hpmax[j] // 2]
                    if hurt:
                        j = min(hurt, key=lambda j: w.hp[j])
                        h = min(w.hpmax[j] - w.hp[j], 3 + L)
                        w.hp[j] += h
                        w.st["heal"] += h
                        w.st["act%d" % k] += 1
                        continue
                if k == MAG and odd:
                    continue
                w.st["act%d" % k] += 1
                if not w.roll(50 + 5 * L):
                    continue
                d = L + w.rng.randrange(L + 1) + (2 if k == RGE else 1 if k == FTR else L if k == MAG else 0)
                w.mon["hp"] -= d
                w.st["dmg%d" % k] += d
            w.st["rounds"] += 1
            if w.mon["hp"] <= 0:
                w.end_fight(True)
                continue
            if alive and w.roll(60):
                p = w.rng.choice(alive)
                w.hurt(p, 1 + w.floor // 4 + 13 * w.avg10() // 100 + (3 if w.mon["kind"] == "OGRE" else 0), b)
            if not w.up():
                w.wipe = WIPE
                w.st["wipes"] += 1
                w.end_fight(False)
            continue
        if w.mon and w.mdist > 1:
            w.mdist -= 1
        if not w.mon or w.mdist > 1:
            w.walk()
    return w, beats


# ------------------------------------------------------------------ the turn system
def turns(song, seconds, seed, cfg):
    rng = random.Random(seed)
    w = World(song, rng, cfg)
    pl = [Player(t, rng) for t in song["tracks"]]
    total = int(seconds * song["bpm"] / 60) * 4               # master sixteenths
    charge = [0] * 4
    chan = [0] * 4                                            # casters: turns channelled
    defend = [False] * 4
    threat = [1.0] * 4
    slot = 0                                                  # 0 = the monster's turn, 1..4 the heroes'
    pend = [{"on": [], "held": 0, "rest": False} for _ in range(4)]
    in_fight = False
    beat_phase = 0
    C = cfg
    for g in range(total):
        ev = [p.at(g) for p in pl]                            # (every track plays on, fight or not)
        if C.get("window"):                                   # what each track did since its hero's last turn
            for k in range(4):
                e = ev[k]
                pw = pend[k]
                if e[0] == "on":
                    pw["on"].append(e[1])
                elif e[0] == "held":
                    pw["held"] = max(pw["held"], 1) if pw["held"] else e[1]
                elif e[0] == "rest":
                    pw["rest"] = True
        beat = g // 4
        if g % 4 == 0:                                        # the walk and the wipe stay on the beat
            if w.wipe:
                w.wipe -= 1
                if not w.wipe:
                    w.retreat()
                continue
            if not (w.mon and w.mdist <= 1):
                if w.mon and w.mdist > 1:
                    w.mdist -= 1
                if not w.mon or w.mdist > 1:
                    w.walk()
                if w.mon and w.mdist <= 1:                    # engaged: the first round starts on this step
                    in_fight, slot = True, 0
                    charge, chan, defend = [0] * 4, [0] * 4, [False] * 4
                    threat = [1.0] * 4
        if w.wipe or not (w.mon and w.mdist <= 1):
            continue
        if g % 4 == 0:
            w.fb += 1
        cur = slot
        slot = (slot + 1) % 5
        if cur == 0:                                          # the monster
            w.st["rounds"] += 1
            alive = w.up()
            if not alive:
                continue
            wts = [(threat[k] if C.get("threat", True) else 1.0) * (C.get("taunt", 1.0) if k == FTR else 1.0) for k in alive]
            p = rng.choices(alive, weights=wts)[0]
            hit = C.get("mon_hit", 60) - (C.get("defend", 30) if defend[p] else 0)
            if C.get("dodge") and p == RGE and ev[RGE][0] != "on":   # no drums on the monster's step: the rogue dodges
                w.st["dodge"] += 1
                hit = 0
            elif C.get("dodge") and p == RGE:
                w.st["nododge"] += 1
            if rng.randrange(100) < hit:
                d = 1 + w.floor // 4 + 13 * w.avg10() // 100 + (3 if w.mon["kind"] == "OGRE" else 0)
                if C.get("party_blow"):
                    d = d * (1 + sum(w.inn)) / 5
                d = max(1, int(round(d * C.get("mon_dmg", 1.0))))
                w.hurt(p, d, beat)
            for k in range(4):
                threat[k] = max(1.0, threat[k] * 0.6)
            if not w.up():
                w.wipe = WIPE
                w.st["wipes"] += 1
                w.end_fight(False)
                in_fight = False
            continue
        k = cur - 1
        if not w.inn[k] or w.dead[k]:
            continue                                          # (the slot passes: always five steps a round)
        defend[k] = False
        e = ev[k]
        extra = 0
        if C.get("window"):
            pw = pend[k]
            if pw["on"]:
                best = max(pw["on"], key=lambda s: (s[2] or s[7] != 0, s[1], s[4], s[5]))
                extra = len(pw["on"]) - 1
                e = ("on", best)
            elif pw["held"]:
                e = ("held", pw["held"])
            elif pw["rest"]:
                e = ("rest",)
            else:
                e = ("none",)
            pend[k] = {"on": [], "held": 0, "rest": False}
        L = w.lv[k]
        mult = 1 + C.get("charge", 0.5) * charge[k]
        if e[0] == "rest":
            defend[k] = True
            w.st["rest%d" % k] += 1
            continue
        if e[0] == "none":
            charge[k] = min(C.get("charge_max", 3), charge[k] + 1)
            w.st["charge%d" % k] += 1
            continue
        if e[0] == "held":
            w.st["held%d" % k] += 1
            if k == CLR:                                       # heal over time, growing while held
                chan[k] = min(4, chan[k] + 1)
                hurt = [j for j in w.up() if w.hp[j] < w.hpmax[j]]
                if hurt:
                    j = min(hurt, key=lambda j: w.hp[j] / w.hpmax[j])
                    h = min(w.hpmax[j] - w.hp[j], int(round((1 + L / 2) * chan[k] * C.get("chan_heal", 0.6))))
                    w.hp[j] += h
                    w.st["heal"] += h
                    w.st["heal_chan"] += h
                continue
            if k == MAG:                                       # power stored for the next bolt
                chan[k] = min(4, chan[k] + 1)
                continue
            # weapons: a follow-through, tapering
            f = max(C.get("follow", 0.5) ** e[1], C.get("follow_floor", 0.0))
            if f < C.get("guard_at", 0.0):                     # the swing spent: holding the note holds the line
                defend[k] = True
                w.st["guard%d" % k] += 1
                continue
            d = base(k, L, rng) * f
            if d >= 0.5 and rng.randrange(100) < 50 + 5 * L:
                strike(w, k, d, False, C)
            threat[k] += 0.5
            continue
        s = e[1]                                               # a new note: the action, shaped by it
        w.st["note%d" % k] += 1
        acc = s[2] or (s[7] != 0)
        vel = s[5] or 100
        wt = 0.5 + 0.5 * vel / 127
        if acc:
            wt *= C.get("accent", 1.75)
        if song["tracks"][k]["kit"] and s[6]:
            lw = [LANE_W[l] for l in range(8) if (s[6] >> l) & 1]
            wt *= max(lw) + 0.1 * (len(lw) - 1)
        if s[1] > 1:
            wt *= 1 + 0.25 * (s[1] - 1)
        p = s[9]
        if 1 <= p < 100:
            wt *= 1 + (100 - p) / 100
        if s[8]:
            wt *= 1.5 if k == RGE else 1.1
        wt *= mult
        wt *= 1 + min(C.get("flurry", 0.25) * extra, C.get("flurry_max", 0.75))
        charge[k] = 0
        threat[k] += 1 + (1 if acc else 0)
        if k == CLR:
            hurt = [j for j in w.up() if w.hp[j] < w.hpmax[j] * C.get("heal_at", 0.6)]
            chan[k] = 1
            if hurt:
                if s[1] > 1:                                   # a chord: the whole party, less each
                    for j in w.up():
                        h = min(w.hpmax[j] - w.hp[j], int(round((2 + L) * wt * 0.5)))
                        w.hp[j] += h
                        w.st["heal"] += h
                else:
                    j = min(hurt, key=lambda j: w.hp[j])
                    h = min(w.hpmax[j] - w.hp[j], int(round((3 + L) * wt)))
                    w.hp[j] += h
                    w.st["heal"] += h
                continue
            d = base(k, L, rng) * wt * 0.6
        elif k == MAG:
            d = base(k, L, rng) * wt * (1 + C.get("chan_bolt", 0.5) * chan[k])
            chan[k] = 0
        else:
            d = base(k, L, rng) * wt
        r = s[4]
        for i in range(r):
            if rng.randrange(100) < 50 + 5 * L + (10 if acc else 0):
                crit = acc and rng.randrange(100) < C.get("crit", 15)
                strike(w, k, d / r * (1.3 if r > 1 else 1) * (2 if crit else 1), s[3] or k == MAG, C)
                if crit:
                    w.st["crit%d" % k] += 1
        if w.mon and w.mon["hp"] <= 0:
            w.end_fight(True)
            in_fight = False
    return w, total // 4


MAG_BONUS = [1.0]
def base(k, L, rng):
    return L + rng.randrange(L + 1) + (2 if k == RGE else 1 if k == FTR else L * MAG_BONUS[0] if k == MAG else 0)


def strike(w, k, d, pierce, C):
    if not w.mon:
        return
    if w.mon["kind"] == "SKELETON" and not pierce:
        d -= C.get("armor", 2)
    d = max(0, int(round(d)))
    w.mon["hp"] -= d
    w.st["dmg%d" % k] += d


# ------------------------------------------------------------------ archetypes
def bars(pattern16, n=1):
    return pattern16 * n

def kit(hits):           # hits: list of 16 lane masks (accent lanes in bit 8+)
    out = []
    for h in hits:
        out.append(step(lanes=h & 255, lacc=h >> 8, n=0) if h & 255 else EMPTY[:])
    return out

def mel(spec, chord=1):  # spec: string of 'x' note, 'X' accent, '-' tie, '.' empty, 'r' rest
    out = []
    for c in spec:
        if c in "xX":
            out.append(step(n=chord, acc=int(c == "X")))
        elif c == "-":
            out.append([ST_TIE, 0, 0, 0, 1, 0, 0, 0, 0, 0, 0])
        elif c == "r":
            out.append([ST_REST, 0, 0, 0, 1, 0, 0, 0, 0, 0, 0])
        else:
            out.append(EMPTY[:])
    return out

K, S, CL, H, OH = 1, 2, 4, 8, 16
def A(x): return x | (x << 8)

ARCH = [
    {"name": "four on the floor", "bpm": 124, "tracks": [
        track(kit([A(K) | H, H, H | 0, H, K | S | H, H, H, H, A(K) | H, H, H, H, K | S | H, H, H, OH]), 4, True),
        track(mel("x" + "-" * 15, 3), 4),
        track(mel("x...x...x...x..."), 3),
        track(mel("x.x.x.x.x.x.x.x."), 5)]},
    {"name": "sparse", "bpm": 90, "tracks": [
        track(kit([K] + [0] * 15), 1, True),
        track(mel("x-------........" + "." * 16, 3), 2),
        track(mel("x..............."), 1),
        track(mel("x.....x.........x......." + "." * 8), 2)]},
    {"name": "busy", "bpm": 132, "tracks": [
        track(kit([A(K) | H, H, H, K | H, S | H, H, K | H, H, A(K) | H, H, H, H, S | H, H, K | H, H]), 7, True),
        track(mel("x.x.x.x.x.x.x.x.", 3), 7),
        track(mel("xxxxxxxxxxxxxxxx"), 7),
        track(mel("xxxxxxxxxxxxxxxx"), 8)]},
    {"name": "syncopated", "bpm": 112, "tracks": [
        track(kit([A(K), 0, H, A(K), S, 0, K | H, 0, 0, K, H, 0, S, 0, H, K]), 5, True),
        track(mel("..x...x...x...x.", 3), 5),
        track(mel("..x..x.x..x..x.x"), 5),
        track(mel(".x..x..x.x..x..x"), 6)]},
    {"name": "pads and drones", "bpm": 80, "tracks": [
        track(kit([K, 0, 0, 0, 0, 0, 0, 0, S, 0, 0, 0, 0, 0, 0, 0]), 2, True),
        track(mel("x" + "-" * 63, 4), 3),
        track(mel("x" + "-" * 31), 2),
        track(mel("x" + "-" * 15 + "x" + "-" * 15), 3)]},
    {"name": "accents and tricks", "bpm": 118, "tracks": [
        track(kit([A(K), H, H, H, A(S), H, H, H, A(K), H, A(K), H, A(S), H, H, OH]), 6, True),
        track(mel("X...x.r.X...x.r.", 3), 5),
        track(mel("X.x.X.x-X.x.X.x-"), 6),
        track(mel("X.xX.x.xX.xX.x.x"), 7)]},
    {"name": "drums and bass only", "bpm": 120, "tracks": [
        track(kit([A(K) | H, H, S | H, H] * 4), 4, True),
        track([EMPTY[:] for _ in range(16)], 1),
        track(mel("x..x..x.x..x..x."), 4),
        track([EMPTY[:] for _ in range(16)], 1)]},
]
# a few tricks on the trick song: ratchets, a 50 % chance, nudges, a slide
_t = ARCH[5]["tracks"]
_t[0]["steps"][15][4] = 3
_t[2]["steps"][3][3] = 1
_t[3]["steps"][2][9] = 50
_t[3]["steps"][5][8] = 3
_t[3]["steps"][9][4] = 2


def summarize(fn, songs, seconds, seeds, cfg):
    rows = []
    for song in songs:
        agg = defaultdict(float)
        ff, fl = [], []
        for sd in range(seeds):
            w, beats = fn(song, seconds, sd, cfg)
            for k, v in w.st.items():
                agg[k] += v / seeds
            agg["floor"] += w.best / seeds
            ff.append(w.first_fall / beats * seconds / 60 if w.first_fall is not None else seconds / 60)
            fl += w.fight_beats
        agg["first_fall_min"] = sorted(ff)[len(ff) // 2]
        agg["fight_beats"] = sum(fl) / max(len(fl), 1)
        rows.append((song, agg))
    return rows


# the settings built into the firmware (ui_dungeon.c 1.0): tuned by a random search over these (see the commit)
FINAL = {"act_floor": 0.3, "mon_hp": 1.4, "accent": 1.5, "flurry": 0.15, "flurry_max": 0.45, "charge": 1.0, "charge_max": 4,
         "threat": False, "lone_heal": 3, "follow_floor": 0.25, "window": 1, "party_scale": 1, "act_scale": 1,
         "party_blow": 1, "dodge": 1, "mon_dmg": 1.05}
MAG_BONUS[0] = 0.75
LANE_W[:] = [1.2, 0.96, 0.96, 0.6, 0.8, 0.96, 0.8, 0.8]   # (the kit's weights x 0.8 for the heavy drums)


if __name__ == "__main__":
    """python3 tools/combat_sim.py songs.json   (songs.json: build/host/combat_export 40 > songs.json)"""
    songs = json.load(open(sys.argv[1]))
    for lab, fn, cfg in (("today (0.1, a round a beat)", today, {}), ("1.0 turns", turns, FINAL)):
        rows = summarize(fn, songs, 600, 4, cfg)
        A = defaultdict(float)
        for s, a in rows:
            for k, v in a.items():
                A[k] += v / len(rows)
        print(f"{lab:28} kills {A['kills']:5.1f} falls {A['falls']:5.2f} wipes {A['wipes']:4.2f} first fall "
              f"{A['first_fall_min']:4.1f} min, fights {A['fight_beats']:4.1f} beats")
        for s, a in summarize(fn, ARCH, 600, 6, cfg):
            print(f"   {s['name']:24} kills {a['kills']:5.1f} falls {a['falls']:5.1f} fights {a['fight_beats']:4.1f} beats")
