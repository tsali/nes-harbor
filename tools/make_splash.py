#!/usr/bin/env python3
"""Draw NES Harbor's title screen: assets/splash/splash_bg.{chr,nam,palette.bin}.

A sunset harbor: a striped sun on the horizon, a container ship's
silhouette, a lighthouse on its rock, and the sea. It is drawn in NES colours
straight onto a 256x240 picture, then cut into tiles: each 16x16 block takes
one of the four palettes below, and every colour in a block must be in its
palette, or this stops. Tiles $E6 up are the version stamp's (the source
puts them there), so the picture may have 230 tiles at most, tile $00 blank.

    tools/make_splash.py [assets/splash]   (and a preview, splash.png, with -p)
"""
import math
import os
import sys

W, H = 256, 240
BLACK = 0x0F
PALETTES = [
    [BLACK, 0x2A, 0x1A, 0x3A],  # 0: green, the stamp's text
    [BLACK, 0x01, 0x11, 0x21],  # 1: the sea
    [BLACK, 0x16, 0x27, 0x38],  # 2: the sunset
    [BLACK, 0x00, 0x16, 0x30],  # 3: the lighthouse
]
MAX_TILES = 0xE6
HORIZON = 112
SEA_END = 184
LIGHTHOUSE_X = (192, 224)       # its blocks' columns, in pixels
LIGHTHOUSE_Y = (32, 112)


def block_palette(bx, by):
    x, y = bx * 16, by * 16
    if LIGHTHOUSE_X[0] <= x < LIGHTHOUSE_X[1] and LIGHTHOUSE_Y[0] <= y < LIGHTHOUSE_Y[1]:
        return 3
    if 16 <= y < HORIZON:
        return 2
    if HORIZON <= y < SEA_END:
        return 1
    return 0


def draw():
    px = [[BLACK] * W for _ in range(H)]

    # Sky: black, a dithered band, red to the horizon.
    for y in range(16, HORIZON):
        for x in range(W):
            if y >= 84:
                c = 0x16
            elif y >= 56:
                # ordered dither: more red lower down
                level = (y - 56) / 28
                bayer = [[0, 8, 2, 10], [12, 4, 14, 6], [3, 11, 1, 9], [15, 7, 13, 5]]
                c = 0x16 if bayer[y % 4][x % 4] / 16 < level else BLACK
            else:
                c = BLACK
            px[y][x] = c

    # The sun, half risen, with the stripes of a retro sunset across it.
    cx, cy, r = 128, HORIZON, 30
    for y in range(cy - r, cy):
        for x in range(cx - r, cx + r):
            d = math.hypot(x - cx + 0.5, y - cy + 0.5)
            if d < r:
                band = cy - y
                if band in (4, 5, 10, 11, 17):
                    continue
                px[y][x] = 0x38 if d < r - 7 else 0x27

    # The sea: dark, with wave lines that repeat every 32 pixels, and the
    # sun's light on it.
    for y in range(HORIZON, SEA_END):
        for x in range(W):
            c = 0x01
            row = (y - HORIZON)
            if row % 6 == 0 or row % 6 == 3:
                phase = (x + row * 5) % 32
                if phase < 10:
                    c = 0x11
                elif phase < 12 and row < 40:
                    c = 0x21
            half = max(2, 26 - row // 2)
            if abs(x - 128) < half and row % 3 == 0 and (x // 2 + row) % 3:
                c = 0x21
            px[y][x] = c

    # A container ship, in silhouette, on the horizon.
    def fill(x0, y0, x1, y1, c=BLACK):
        for y in range(y0, y1):
            for x in range(x0, x1):
                if 0 <= x < W and 0 <= y < H:
                    px[y][x] = c
    for y in range(100, 116):                     # the hull, its bow raked
        t = (y - 100) / 16
        fill(int(26 + 10 * t), y, 112, y + 1)
    for i, (h0) in enumerate([92, 88, 92, 90, 88, 92]):   # containers
        fill(34 + i * 9, h0, 42 + i * 9, 100)
    fill(90, 82, 108, 100)                        # the bridge
    fill(86, 82, 108, 84)
    fill(97, 72, 103, 82)                         # the stack
    for x in range(92, 106, 4):                   # its windows lit
        px[86][x] = px[86][x + 1] = 0x16 if (x // 4) % 2 else 0x27
    fill(28, 96, 31, 100)                         # the bow's mast
    fill(29, 90, 30, 96)

    # The lighthouse on its rock.
    for y in range(HORIZON, HORIZON + 14):         # its rock, in the sea
        half = 8 + (y - HORIZON) * 3 // 2
        for x in range(208 - half, 208 + half):
            if (x * 7 + y * 3) % 11 or abs(x - 208) < half - 2:
                px[y][x] = BLACK
    for y in range(50, HORIZON):                  # tower, tapering
        half = 4 + (y - 50) * 4 // 66
        stripe = 0x16 if ((y - 50) // 8) % 2 == 0 else 0x30
        for x in range(208 - half, 208 + half):
            px[y][x] = stripe
    fill(201, 42, 215, 50, 0x30)                  # the lantern, lit
    for x in range(201, 215, 3):
        fill(x, 42, x + 1, 50)
    fill(199, 40, 217, 42)                        # gallery and cap
    fill(199, 50, 217, 52)
    fill(202, 36, 214, 40)
    fill(206, 33, 210, 36)
    return px


def tiles_of(px):
    pal_of = [[block_palette(bx, by) for bx in range(16)] for by in range(15)]
    tiles = [bytes(16)]                            # $00, blank
    index = {tiles[0]: 0}
    nam = []
    for ty in range(30):
        for tx in range(32):
            pal = PALETTES[pal_of[ty // 2][tx // 2]]
            lo = bytearray(8)
            hi = bytearray(8)
            for y in range(8):
                for x in range(8):
                    c = px[ty * 8 + y][tx * 8 + x]
                    if c not in pal:
                        sys.exit(f"colour ${c:02X} at {tx * 8 + x},{ty * 8 + y} "
                                 f"is not in palette {pal_of[ty // 2][tx // 2]}")
                    i = pal.index(c)
                    lo[y] |= (i & 1) << (7 - x)
                    hi[y] |= (i >> 1) << (7 - x)
            tile = bytes(lo + hi)
            if tile not in index:
                index[tile] = len(tiles)
                tiles.append(tile)
            nam.append(index[tile])
    if len(tiles) > MAX_TILES:
        sys.exit(f"{len(tiles)} tiles; the picture may have {MAX_TILES}")
    att = bytearray(64)
    for ay in range(8):
        for ax in range(8):
            v = 0
            for q, (dx, dy) in enumerate([(0, 0), (1, 0), (0, 1), (1, 1)]):
                bx, by = ax * 2 + dx, ay * 2 + dy
                p = pal_of[by][bx] if by < 15 else 0
                v |= p << (q * 2)
            att[ay * 8 + ax] = v
    chr_data = b"".join(tiles).ljust(4096, b"\0")
    return chr_data, bytes(nam) + bytes(att), bytes(c for p in PALETTES for c in p), len(tiles)


def main():
    args = [a for a in sys.argv[1:] if a != "-p"]
    out = args[0] if args else "nes_harbor/assets/splash"
    px = draw()
    chr_data, nam, pal, count = tiles_of(px)
    open(os.path.join(out, "splash_bg.chr"), "wb").write(chr_data)
    open(os.path.join(out, "splash_bg.nam"), "wb").write(nam)
    open(os.path.join(out, "splash_bg.palette.bin"), "wb").write(pal)
    print(f"{count} tiles -> {out}")
    if "-p" in sys.argv:
        from PIL import Image
        nes = {0x0F: (0, 0, 0), 0x2A: (76, 220, 72), 0x1A: (0, 168, 0), 0x3A: (184, 248, 184),
               0x01: (0, 42, 136), 0x11: (21, 95, 217), 0x21: (100, 176, 255),
               0x16: (181, 49, 32), 0x27: (234, 158, 34), 0x38: (228, 229, 148),
               0x00: (102, 102, 102), 0x30: (255, 254, 255)}
        im = Image.new("RGB", (W, H))
        im.putdata([nes[c] for row in px for c in row])
        im.resize((W * 2, H * 2), Image.NEAREST).save("splash.png")


if __name__ == "__main__":
    main()
