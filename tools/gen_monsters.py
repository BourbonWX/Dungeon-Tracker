#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-only
"""Dungeon Tracker 1.5: the dungeon's monsters (firmware/src/ui_dungeon.c), written to firmware/src/dg_monsters.h, and a
preview sheet (build/monsters.png, when Pillow is there).

Sixteen by sixteen, four inks: b the body (the theme's ACCENT), d its shade, h a highlight (bone, teeth, steel: TEXT),
e the eyes (the background showing through; they glow when it strikes or rages). Twelve monsters in four tiers of depth,
three a tier (the goblin's, the skeleton's and the slime's ways of fighting, ui_dungeon.c), and five bosses (the ogre's).
    python3 tools/gen_monsters.py"""
import pathlib

MON = {
# ---- tier 1 (B0..2) --------------------------------------------------------------------------------------------
"GOBLIN": [
    "................",
    ".b............b.",
    ".bb..bbbbbb..bb.",
    "..bbbbbbbbbbbb..",
    "...bbebbbbebb...",
    "...bbbbddbbbb...",
    "....bhdhhdhb....",
    ".....bbbbbb.....",
    "...dbbbbbbbbd...",
    "..d.bbbbbbbb.dh.",
    "..b.bbbbbbbb..h.",
    "....bbddddbb..h.",
    "....bb....bb..d.",
    "....bb....bb....",
    "...bbb....bbb...",
    "................"],
"SKELETON": [
    ".....hhhhhh.....",
    "....hhhhhhhh....",
    "....hheehheeh...",
    "....hhhhhhhhh...",
    ".....hdhdhdh....",
    "......hhhhh.....",
    "....hhhdhhhh....",
    "...h.hhdhhh.h...",
    "..h..hhdhhh..h..",
    "..h...hhhh...h..",
    "......hddh......",
    ".....hh..hh.....",
    ".....h....h.....",
    ".....h....h.....",
    "....hh....hh....",
    "................"],
"SLIME": [
    "................",
    "................",
    "................",
    "................",
    "......bbbb......",
    "....bbbbbbbb....",
    "...bbhhbbbbbb...",
    "..bbhbbbbbbbbb..",
    "..bbbebbbbebbb..",
    ".bbbbbbbbbbbbbb.",
    ".bbbbbddddbbbbb.",
    "bbbbbbbbbbbbbbbb",
    "bbbbbbbbbbbbbbbb",
    ".bddbbbbbbbbddb.",
    "..dddddddddddd..",
    "................"],
# ---- tier 2 (B3..5) --------------------------------------------------------------------------------------------
"KOBOLD": [
    "......bb........",
    ".....bbbb.......",
    "....bbbbbbbb....",
    "...bbebbbbbbbbh.",
    "...bbbbbbbbhdhh.",
    "....bbbbbbbbbb..",
    ".....bbbbb......",
    "...bbbbbbbbb..h.",
    "..bb.bbbbbbb.hh.",
    "..b..bbddbbb..h.",
    ".....bbbbbbb..h.",
    "....bbb..bbb..d.",
    "....bb....bb....",
    "...bb......bb.dd",
    "..bbb......bbbd.",
    "................"],
"ARMOUR": [
    ".....hhhhhh.....",
    "....hhhhhhhh....",
    "....hdeeeedh....",
    "....hhhhhhhh....",
    "...hh.hddh.hh...",
    "..hhhhhhhhhhhh..",
    "..hdhhhddhhhdh..",
    "..hdhhhddhhhdh..",
    "..hh.hhhhhh.hh..",
    "..hh.hddddh.hh..",
    "..d..hhhhhh..d..",
    ".....hhddhh.....",
    ".....hh..hh.....",
    ".....hh..hh.....",
    "....hhh..hhh....",
    "................"],
"BAT": [
    "................",
    "................",
    "................",
    "b.............b.",
    "bb....b..b....bb",
    "bbb...bbbb...bbb",
    "bbbb.bebbeb.bbbb",
    "bbbbbbbbbbbbbbbb",
    ".bbbbbbhhbbbbbb.",
    "..bbb.bbbb.bbb..",
    "...b..bbbb..b...",
    "......bddb......",
    ".......dd.......",
    "................",
    "................",
    "................"],
# ---- tier 3 (B6..8) --------------------------------------------------------------------------------------------
"ORC": [
    "................",
    ".....bbbbbb.....",
    "....bbbbbbbb....",
    "...bbebbbbebb...",
    "...bbbbbbbbbb...",
    "...bhbbddbbhb...",
    "....bhbbbbhb....",
    "..dbbbbbbbbbbd..",
    ".dbbbbbbbbbbbbd.",
    ".bb.bbbbbbbb.bb.",
    ".bb.bbddddbb.hh.",
    ".hh.bbbbbbbb.hh.",
    "....bbb..bbb.hh.",
    "....bbb..bbb....",
    "...dbbb..bbbd...",
    "................"],
"GARGOYLE": [
    "................",
    "..d...bbbb...d..",
    ".dd..bbbbbb..dd.",
    ".ddd.bebbeb.ddd.",
    "dddd.bbbbbb.dddd",
    "ddddbbhhhhbbdddd",
    "dddbbbbbbbbbbddd",
    ".dd.bbbbbbbb.dd.",
    "..d.bbbddbbb.d..",
    "....bbbbbbbb....",
    "....bb.bb.bb....",
    "...bb..bb..bb...",
    "...b...bb...b...",
    "..hh..hhhh..hh..",
    "..dddddddddddd..",
    "................"],
"WISP": [
    "................",
    ".......h........",
    "......hhh.......",
    ".....hhbhh......",
    "....hhbbbhh.....",
    "...hhbbbbbhh....",
    "...hbbebbebh....",
    "...hbbbbbbbh....",
    "...hbbbddbbh....",
    "....hbbbbbh.....",
    ".....hbbbh......",
    "......hbh.......",
    ".......b........",
    "......b.........",
    ".......b........",
    "................"],
# ---- tier 4 (B9..) ---------------------------------------------------------------------------------------------
"DARK ELF": [
    "......dddd......",
    ".....dddddd.....",
    "....ddbbbbdd....",
    "h...dbebbebd...h",
    "hh..dbbbbbbd..hh",
    ".h...bbddbb...h.",
    ".h....bbbb....h.",
    "..h.ddbbbbdd.h..",
    "...hdbbbbbbdh...",
    "....dbbbbbbd....",
    "....dbbddbbd....",
    "....ddbbbbdd....",
    ".....bb..bb.....",
    ".....bb..bb.....",
    "....ddd..ddd....",
    "................"],
"BONE KNIGHT": [
    ".....hhhhhh..d..",
    "....hhhhhhhh.d..",
    "....hheehheh.d..",
    "....hhhhhhhh.d..",
    ".....hdhdhh..h..",
    "...ddhhhhhhddh..",
    "..dddhhdhhhdddh.",
    "..d.dhhdhhhd.hh.",
    "..d.dhhdhhhd....",
    "..hh.dhhhhd.....",
    "..hh.ddhhdd.....",
    ".....hh..hh.....",
    ".....h....h.....",
    ".....h....h.....",
    "....dd....dd....",
    "................"],
"CUBE": [
    "................",
    "..hhhhhhhhhhhh..",
    "..hbbbbbbbbbbh..",
    "..hbbbbbbbbbbh..",
    "..hbbebbbbebbh..",
    "..hbbbbbbbbbbh..",
    "..hbbbhbbbbbbh..",
    "..hbbbbbbbhbbh..",
    "..hbbddddddbbh..",
    "..hbbbbbbbbbbh..",
    "..hbhbbbbbbbbh..",
    "..hbbbbbbhbbbh..",
    "..hbbbbbbbbbbh..",
    "..hhhhhhhhhhhh..",
    "..dddddddddddd..",
    "................"],
# ---- bosses -----------------------------------------------------------------------------------------------------
"OGRE": [
    "..bb........bb..",
    "..bbbbbbbbbbbb..",
    ".bbbebbbbbbebbb.",
    ".bbbbbbbbbbbbbb.",
    ".bbhdhddddhdhbb.",
    "..bbbbbbbbbbbb..",
    "bbbbbbbbbbbbbbbb",
    "bbbbbbbbbbbbbbbb",
    "bb.bbbbddbbbb.bb",
    "bb.bbbbbbbbbb.bb",
    "hh.bbbbbbbbbb.hh",
    "...bbbddddbbb...",
    "...bbbb..bbbb...",
    "...bbb....bbb...",
    "..bbbb....bbbb..",
    "................"],
"MINOTAUR": [
    "hh............hh",
    ".hh..bbbbbb..hh.",
    "..hhbbbbbbbbhh..",
    "....bebbbbeb....",
    "....bbbbbbbb....",
    "....bbdhhdbb....",
    "..dbbbbbbbbbbd..",
    ".dbbbbbbbbbbbbd.",
    "bbb.bbbbbbbb.bbb",
    "bb..bbbddbbb..bb",
    "hh..bbbbbbbb..hh",
    "hh..ddbbbbdd..hh",
    "....bbb..bbb....",
    "....bbb..bbb....",
    "...ddd....ddd...",
    "................"],
"LICH": [
    "....h..hh..h....",
    "....hhhhhhhh....",
    ".....hhhhhh.....",
    ".....heehee.....",
    ".....hhhhhh.....",
    "...dddhdhdddd.h.",
    "..ddddhhhhddddh.",
    ".ddbbddddddbbdh.",
    ".d.bbbbbbbbbb.h.",
    "...bbbbddbbbb.h.",
    "...bbbbbbbbbb.h.",
    "...bbbddddbbb.h.",
    "..bbbbbbbbbbbbh.",
    "..bbbbbbbbbbbb..",
    ".dddddddddddddd.",
    "................"],
"DRAGON": [
    "........hh......",
    ".......bbbbb....",
    "......bbebbbb...",
    "......bbbbbhhh..",
    "d.....bbbbbbbb..",
    "dd...bbbbb......",
    "ddd.bbbbbb...d..",
    "dddbbbbbbbb.dd..",
    ".ddbbhbhbbbbdd..",
    "..dbbhbhbbbbbd..",
    "...bbhbhbbbbb...",
    "...bbbbbbbbbbb..",
    "...bbb...bbbbbb.",
    "..hbb.....bb.bbb",
    "..hh......hh...b",
    "................"],
"DEMON": [
    "hh............hh",
    ".h...bbbbbb...h.",
    ".hh.bbbbbbbb.hh.",
    "..hbbebbbbebbh..",
    "...bbbbbbbbbb...",
    "...bbhdhhdhbb...",
    "d...bbbbbbbb...d",
    "dd.bbbbbbbbbb.dd",
    "dddbbbbbbbbbbddd",
    "ddbb.bbddbb.bbdd",
    "d.hh.bbbbbb.hh.d",
    ".....bbbbbb.....",
    ".....bb..bb.....",
    "....bbb..bbb.bb.",
    "...hhh....hhhb..",
    "................"],
}
TIERS = [["GOBLIN", "SKELETON", "SLIME"], ["KOBOLD", "ARMOUR", "BAT"], ["ORC", "GARGOYLE", "WISP"],
         ["DARK ELF", "BONE KNIGHT", "CUBE"]]
BOSSES = ["OGRE", "MINOTAUR", "LICH", "DRAGON", "DEMON"]


def main():
    order = [n for t in TIERS for n in t] + BOSSES
    for n in order:
        rows = MON[n]
        assert len(rows) == 16, (n, len(rows))
        for r in rows:
            assert len(r) == 16 and set(r) <= set(".bdhe"), (n, r, len(r))
    out = ["/* SPDX-License-Identifier: GPL-3.0-only",
           " * Copyright (C) 2026 Dungeon Tracker (RONDO) contributors */",
           "/* generated by tools/gen_monsters.py: the dungeon's monsters (ui_dungeon.c): 16 x 16, two bits a pixel",
           " * (0 none, 1 body, 2 shade, 3 highlight; the eyes in DGM_EYE's bits: the background, glowing when it strikes) */",
           f"#define DGM_N {len(order)}                 /* twelve (four tiers of three), then the five bosses */",
           "#define DGM_TIERS 4",
           "#define DGM_BOSS0 12",
           "static const char *const DGM_NAME[DGM_N] = {" + ", ".join(f'"{n}"' for n in order) + "};",
           "static const uint32_t DGM_PIX[DGM_N][16] = {      /* a row: 16 pixels, 2 bits each, the first pixel lowest */"]
    eye = []
    for n in order:
        vals, ev = [], []
        for r in MON[n]:
            v, e = 0, 0
            for i, ch in enumerate(r):
                code = {".": 0, "b": 1, "d": 2, "h": 3, "e": 0}[ch]
                v |= code << (2 * i)
                e |= (ch == "e") << i
            vals.append(v)
            ev.append(e)
        out.append("    {" + ", ".join(f"0x{v:08X}u" for v in vals) + "},   /* " + n + " */")
        eye.append(ev)
    out.append("};")
    out.append("static const uint16_t DGM_EYE[DGM_N][16] = {")
    for n, ev in zip(order, eye):
        out.append("    {" + ", ".join(f"0x{v:04X}" for v in ev) + "},   /* " + n + " */")
    out.append("};")
    dest = pathlib.Path(__file__).resolve().parent.parent / "firmware" / "src" / "dg_monsters.h"
    dest.write_text("\n".join(out) + "\n")
    print(f"monsters: {len(order)} -> {dest}")
    try:
        from PIL import Image, ImageDraw
        S, pad = 8, 24
        img = Image.new("RGB", (len(order) // 2 * (16 * S + pad) + pad + 16 * S, 2 * (16 * S + pad + 16) + pad), (24, 20, 30))
        d = ImageDraw.Draw(img)
        col = {"b": (220, 90, 60), "d": (120, 50, 40), "h": (235, 230, 210), "e": (24, 20, 30)}
        for i, n in enumerate(order):
            ox, oy = pad + (i % 9) * (16 * S + pad), pad + (i // 9) * (16 * S + pad + 16)
            for y, r in enumerate(MON[n]):
                for x, ch in enumerate(r):
                    if ch != ".":
                        d.rectangle([ox + x * S, oy + y * S, ox + x * S + S - 1, oy + y * S + S - 1], fill=col[ch])
            d.text((ox, oy + 16 * S + 3), n, fill=(200, 200, 200))
        p = dest.parent.parent.parent / "build" / "monsters.png"
        img.save(p)
        print(f"preview -> {p}")
    except ImportError:
        pass


if __name__ == "__main__":
    main()
