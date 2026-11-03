#!/usr/bin/env python3
"""Relabel nes-radar's scope (assets/ldv/ldv_bg.nam) for ships, in place.

Also its tiles (assets/ldv/ldv_bg.chr): the title bar says NES HARBOR,
with inverse H, B and O drawn where nes-radar's small letters for its
startup screen were (NES Harbor writes none), and the scope's rings lose
their 3, 6 and 9, which were nautical miles at nes-radar's one range.

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
RING_DIGITS = [0x6B, 0x7F, 0x96]  # the 9, 6 and 3 on the scope's line


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
    for tile in RING_DIGITS:
        px = pixels(data, tile)
        for y in range(2, 8):
            for x in range(1, 7):
                px[y][x] = 0
        put(data, tile, px)
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
    at = TITLE_ROW * 32 + 1
    title = TITLE_TEXT[1:].ljust(TITLE_END - 1)
    data[at:at + len(title)] = bytes(TITLE[c] for c in title)
    open(path, "wb").write(data)
    print(f"relabelled {path}")


if __name__ == "__main__":
    main(sys.argv[1] if len(sys.argv) > 1 else "nes_harbor/assets/ldv/ldv_bg.nam")
    patch_chr(sys.argv[2] if len(sys.argv) > 2 else CHR)
