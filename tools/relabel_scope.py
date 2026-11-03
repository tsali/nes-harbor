#!/usr/bin/env python3
"""Relabel nes-radar's scope (assets/ldv/ldv_bg.nam) for ships, in place.

Also its tiles (assets/ldv/ldv_bg.chr): the title bar says NES HARBOR,
with inverse H, B and O drawn where nes-radar's small letters for its
startup screen were (NES Harbor writes none), and the scope's middle is
the map's (harbor_map.inc): sixteen tiles of land and water where its rings
were, and water to begin with.

Aircraft fields become AIS ones: REG is the call sign, SQWK the navigational
status, CAT the AIS ship type's digits, SPD and TRK speed and course over
ground. Ships have no altitude or vertical rate, so those labels go. Run it
again and nothing changes.
"""
import sys

FONT = " 0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZ+-./:"
# (row, column, width, old, new)
LABELS = [
    (10, 0, 4, "REG",  "CALL"),
    (13, 0, 4, "SQWK", "STAT"),
    (16, 0, 4, "CAT",  "AIS"),
    (4, 26, 4, "ALT",  ""),
    (7, 26, 4, "SPD",  "SOG"),
    (10, 26, 4, "TRK", "COG"),
    (13, 26, 4, "V/S", ""),
]


CHR = "nes_harbor/assets/ldv/ldv_bg.chr"
INV_SLOTS = {"H": 0xEF, "B": 0xF0, "O": 0xF1}   # STARTUP_CHAR_A, _B, _C
TITLE_ROW = 1
TITLE = {"N": 0x5D, "E": 0x5E, "S": 0x5F, "R": 0x60, "A": 0x61, "D": 0x62,
         " ": 0x5C, **INV_SLOTS}
TITLE_TEXT = " NES HARBOR"      # from column 1; the bar goes on after it
TITLE_END = 20                  # TGT begins here
MAP_TILE_BASE = 0x70            # harbor_map.inc's: sixteen tiles of the map
MAP_ROWS = range(3, 21)         # the scope's middle, which the map covers
MAP_COLS = range(7, 25)
WATER, LAND, FOAM = 1, 2, 3     # colours of BG0 and BG1 on the scope


def map_tile(code):
    """Tile `code` of the map: four 4x4 cells, bit 1 the north-west's land,
    2 the north-east's, 4 the south-west's, 8 the south-east's. Water has a
    wave or two, and foam where it meets the land."""
    land = [[bool(code & (1 << ((y // 4) * 2 + x // 4))) for x in range(8)] for y in range(8)]
    px = [[LAND if land[y][x] else WATER for x in range(8)] for y in range(8)]
    for y, x in [(2, 1), (2, 2), (6, 5), (6, 6)]:
        if not land[y][x]:
            px[y][x] = FOAM
    for y in range(8):
        for x in range(8):
            if land[y][x]:
                continue
            for dy, dx in [(-1, 0), (1, 0), (0, -1), (0, 1)]:
                if 0 <= y + dy < 8 and 0 <= x + dx < 8 and land[y + dy][x + dx]:
                    px[y][x] = FOAM
    return px


def pixels(chr_data, tile):
    b = chr_data[tile * 16:tile * 16 + 16]
    return [[((b[y] >> (7 - x)) & 1) | (((b[y + 8] >> (7 - x)) & 1) << 1)
             for x in range(8)] for y in range(8)]


def put(chr_data, tile, px):
    for y in range(8):
        lo = hi = 0
        for x in range(8):
            lo |= (px[y][x] & 1) << (7 - x)
            hi |= (px[y][x] >> 1) << (7 - x)
        chr_data[tile * 16 + y] = lo
        chr_data[tile * 16 + 8 + y] = hi


def inverse(chr_data, glyph):
    """A font glyph as the title bar's letters are: a top row of 3, the
    rest 1 behind and 3 for the letter (as N at $5D is of N at $18)."""
    px = pixels(chr_data, glyph)
    return [[3] * 8] + [[3 if px[y][x] else 1 for x in range(8)] for y in range(1, 8)]


def patch_chr(path):
    data = bytearray(open(path, "rb").read())
    for letter, slot in INV_SLOTS.items():
        put(data, slot, inverse(data, FONT.index(letter)))
    for code in range(16):
        put(data, MAP_TILE_BASE + code, map_tile(code))
    open(path, "wb").write(data)
    print(f"retiled {path}")


def text(tiles):
    return "".join(FONT[t] if t < len(FONT) else "#" for t in tiles)


def main(path):
    data = bytearray(open(path, "rb").read())
    for row, col, width, old, new in LABELS:
        at = row * 32 + col
        now = text(data[at:at + width]).rstrip()
        if now == new:
            continue
        if now != old:
            sys.exit(f"row {row} col {col}: {now!r}, not {old!r}")
        data[at:at + width] = bytes(FONT.index(c) for c in new.ljust(width))
    for row in MAP_ROWS:                     # the scope's middle: water
        for col in MAP_COLS:
            data[row * 32 + col] = MAP_TILE_BASE
    at = TITLE_ROW * 32 + 1
    title = TITLE_TEXT[1:].ljust(TITLE_END - 1)
    data[at:at + len(title)] = bytes(TITLE[c] for c in title)
    open(path, "wb").write(data)
    print(f"relabelled {path}")


if __name__ == "__main__":
    main(sys.argv[1] if len(sys.argv) > 1 else "nes_harbor/assets/ldv/ldv_bg.nam")
    patch_chr(sys.argv[2] if len(sys.argv) > 2 else CHR)
