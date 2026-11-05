#!/usr/bin/env python3
"""Draw NES Harbor's overworld look: the map's tiles, the ships, the status
bar, the "NOW ENTERING" port card and the ship card.

All of it is original pixel art, drawn here from code so that it can be
read, changed and made again. Nothing is taken from any other game.

It writes, in nes_harbor/assets/overworld/:

  harbor_a.chr    pattern table $0000 of the scope's CHR bank: the scope
                  font (copied from nes-radar's ldv_bg.chr, tiles $00-$53
                  and $EF-$FF, which the picker uses as before), then the
                  card frames and the pictures of the two cards.
  harbor_b.chr    pattern table $1000: the boat sprites (8x16 pairs), the
                  map's tiles, and the status bar's font on black. The map
                  screen shows its background from this table.
  overworld.inc   the tile numbers, palettes, nametables and tables the ROM
                  draws with (ca65).

    tools/make_overworld.py [-p]      (-p: also build/overworld-preview.png)

The map is drawn one tile to a cell of HARBOR's land and water mask. A
land tile is chosen by which of its four sides have water (a rocky edge on
each), a water tile next to land by which sides have land (surf on each),
and open water is plain or has a small wave that the ROM twinkles by
cycling two palette colours.
"""
import math
import os
import sys

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
LDV_CHR = os.path.join(ROOT, "nes_harbor/assets/ldv/ldv_bg.chr")
OUT = os.path.join(ROOT, "nes_harbor/assets/overworld")
FONT = " 0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZ+-./:"
FONT_BASE, FONT_DIM_BASE = 0x00, 0x2A

BLACK = 0x0F

# ---------------------------------------------------------------- palettes
# The map screen. Colour 0 is the deep sea under everything.
SEA = 0x02
MAP_PALETTE = [
    SEA, 0x1A, 0x07, 0x30,      # BG0 land: grass, rock, surf
    SEA, 0x22, SEA, 0x30,       # BG1 open sea: two wave colours the ROM cycles
    SEA, BLACK, 0x2A, 0x30,     # BG2 panel: black, green (LIVE), white
    SEA, BLACK, 0x27, 0x28,     # BG3 panel: black, orange (bars), yellow
    SEA, BLACK, 0x16, 0x30,     # SP0 cargo and tankers: red; wakes
    SEA, BLACK, 0x21, 0x30,     # SP1 passenger and high speed: light blue
    SEA, BLACK, 0x28, 0x30,     # SP2 tugs, pilots, law, service: yellow; the cursor
    SEA, BLACK, 0x30, 0x10,     # SP3 sail, pleasure, fishing, other: white, grey cabin
]
SKY, GRAY, WHITE, CARD_SEA = 0x21, 0x10, 0x30, 0x12
HULL = 0x16                     # the cards' hull colour; the ROM sets it per group
CARD_PALETTE = [
    BLACK, SKY, GRAY, WHITE,    # BG0 picture, above the horizon
    BLACK, HULL, WHITE, CARD_SEA,  # BG1 picture, below it
    BLACK, 0x00, 0x10, 0x30,    # BG2 text: white
    BLACK, 0x27, 0x17, 0x28,    # BG3 text: orange (FONT_BASE), yellow (FONT_DIM)
    BLACK, BLACK, BLACK, BLACK,
    BLACK, BLACK, BLACK, BLACK,
    BLACK, BLACK, BLACK, BLACK,
    BLACK, BLACK, BLACK, BLACK,
]
# A ship group's colour: on the map (SP0-SP3 colour 2) and on its card (hull).
GROUP_CARD_HULL = [0x16, 0x11, 0x27, 0x30]

# ------------------------------------------------------------ tile numbers
# Pattern table B ($1000)
SPR_BOAT, SPR_WAKE, SPR_BOX = 0x00, 0x20, 0x30
OW_LAND, OW_SURF, OW_SEA, OW_FLECK_A, OW_FLECK_B = 0x40, 0x50, 0x60, 0x61, 0x62
PANEL_FONT = 0x70
PANEL_LIVE = PANEL_FONT + len(FONT)          # L I V E, green
PANEL_BAR_ON = PANEL_LIVE + 4
PANEL_BAR_OFF = PANEL_BAR_ON + 1
PANEL_BORDER = PANEL_BAR_OFF + 1             # TL T TR L R BL B BR
# Pattern table A ($0000)
CARD_BORDER = 0x54                           # TL T TR L R BL B BR, colour 3
PIC_FIRST = CARD_BORDER + 8
PIC_LAST = 0xEE                              # $EF up: the picker's own

# ------------------------------------------------------------------ layout
MAP_ROWS, MAP_COLS = 24, 32                  # the map: rows 0-23
CELL_ROW0, CELL_COL0 = 6, 2                  # the mask's cell at the map's corner
TAG_COL, TAG_ROW, TAG_ROWS = 14, 2, 4        # the name tag: rows 2-5, columns 14-31
                                             # (row 0 is under a TV's edge)
STAT = {                                     # (row, column)
    "port_label": (25, 1), "port": (25, 6),
    "ships_label": (26, 1), "count": (26, 7),
    "rng_label": (26, 9), "range": (26, 13),
    "link": (26, 18),
    "signal_label": (28, 1), "meter": (28, 12),
}
PORT_PIC = (8, 6, 16, 6)                     # column, row, width, height in tiles
PORT_TEXT = {"title": (4, "NOW ENTERING"), "name": 15, "ships": (18, 11), "range": (20, 11),
             "hint": (24, "A: SHIP  B: PORTS")}
SHIP_PIC = (2, 2, 10, 4)
SHIP_RIGHT_COL = 13                          # name, MMSI, type beside the picture
SHIP_LABEL_COL, SHIP_VALUE_COL = 3, 11
SHIP_FIRST_ROW = 7
SHIP_HINT = (25, 3, "B: BACK TO MAP")


def addr(row, col):
    return 0x2000 + row * 32 + col


# ------------------------------------------------------------------- tiles
def tile_bytes(px):
    lo, hi = bytearray(8), bytearray(8)
    for y in range(8):
        for x in range(8):
            v = px[y][x]
            lo[y] |= (v & 1) << (7 - x)
            hi[y] |= (v >> 1) << (7 - x)
    return bytes(lo + hi)


def tile_pixels(data, t):
    b = data[t * 16:t * 16 + 16]
    return [[((b[y] >> (7 - x)) & 1) | (((b[y + 8] >> (7 - x)) & 1) << 1)
             for x in range(8)] for y in range(8)]


def grid(w, h, v=0):
    return [[v] * w for _ in range(h)]


def cut(px, x0, y0):
    return [row[x0:x0 + 8] for row in px[y0:y0 + 8]]


# ---------------------------------------------------------------- map tiles
GRASS, ROCK, FOAM = 1, 2, 3
SPECKS = [(1, 5), (5, 2), (6, 6)]


def land_tile(water):
    """water: bit 0 N, 1 E, 2 S, 3 W have water. Grass with a dark rocky
    edge on each of those sides, ragged on its inner line."""
    px = grid(8, 8, GRASS)
    for y, x in SPECKS:
        px[y][x] = ROCK
    rag = [1, 2, 1, 1, 2, 1, 2, 2]           # depth of the rock along an edge
    for i in range(8):
        d = rag[i]
        if water & 1:
            for k in range(d):
                px[k][i] = ROCK
        if water & 4:
            for k in range(d):
                px[7 - k][7 - i] = ROCK
        if water & 8:
            for k in range(d):
                px[7 - i][k] = ROCK
        if water & 2:
            for k in range(d):
                px[i][7 - k] = ROCK
    return px


def surf_tile(land):
    """land: bit 0 N, 1 E, 2 S, 3 W have land; 0 is land only at a corner.
    Deep water, white surf along each such side and a little spray."""
    px = grid(8, 8, 0)
    line = [1, 1, 0, 1, 1, 1, 0, 1]
    spray = [(2, 1), (2, 5), (3, 3), (4, 6)]
    def side(fn):
        for i in range(8):
            if line[i]:
                fn(0, i)
        for d, i in spray:
            fn(d - 1 if d > 2 else d, i)
    if land == 0:
        for y, x in [(1, 1), (0, 6), (6, 0), (7, 6), (5, 7)]:
            px[y][x] = FOAM
        return px
    if land & 1:
        side(lambda d, i: px[d].__setitem__(i, FOAM))
    if land & 4:
        side(lambda d, i: px[7 - d].__setitem__(7 - i, FOAM))
    if land & 8:
        side(lambda d, i: px[7 - i].__setitem__(d, FOAM))
    if land & 2:
        side(lambda d, i: px[i].__setitem__(7 - d, FOAM))
    return px


def fleck_tile(colour, at):
    px = grid(8, 8, 0)
    x, y = at
    for dx, dy in [(0, 1), (1, 0), (2, 0), (3, 1)]:
        px[y + dy][x + dx] = colour
    return px


# ------------------------------------------------------------------ sprites
def poly_cover(points, x, y, n=4):
    """How much of pixel (x, y) the polygon covers, by n x n samples."""
    inside = 0
    for sy in range(n):
        for sx in range(n):
            px, py = x + (sx + .5) / n, y + (sy + .5) / n
            c = False
            j = len(points) - 1
            for i in range(len(points)):
                xi, yi = points[i]
                xj, yj = points[j]
                if (yi > py) != (yj > py) and px < (xj - xi) * (py - yi) / (yj - yi) + xi:
                    c = not c
                j = i
            inside += c
    return inside / (n * n)


def heading_vectors(h):
    a = math.radians(h * 45)
    return (math.sin(a), -math.cos(a)), (math.cos(a), math.sin(a))


def local(points, h, cx=8.0, cy=8.0):
    (fx, fy), (rx, ry) = heading_vectors(h)
    return [(cx + px * rx + py * fx, cy + px * ry + py * fy) for px, py in points]


HULL_SHAPE = [(0, 6.6), (2.9, 2.4), (2.9, -5.2), (-2.9, -5.2), (-2.9, 2.4)]
CABIN_SHAPE = [(1.4, 0.6), (1.4, -3.4), (-1.4, -3.4), (-1.4, 0.6)]


def boat(h):
    """A 16x16 boat with its bow toward heading h (0 north, 1 north-east...):
    hull in colour 2, a white wheelhouse, a black outline."""
    px = grid(16, 16, 0)
    hull = local(HULL_SHAPE, h)
    cabin = local(CABIN_SHAPE, h)
    for y in range(16):
        for x in range(16):
            if poly_cover(hull, x, y) >= .5:
                px[y][x] = 2
    for y in range(16):
        for x in range(16):
            if px[y][x] == 2 and poly_cover(cabin, x, y) >= .45:
                px[y][x] = 3
    out = [row[:] for row in px]
    for y in range(16):
        for x in range(16):
            if px[y][x]:
                continue
            for dx, dy in [(-1, 0), (1, 0), (0, -1), (0, 1)]:
                if 0 <= x + dx < 16 and 0 <= y + dy < 16 and px[y + dy][x + dx]:
                    out[y][x] = 1
    return out


def wake(h):
    """A short white wake astern, in one 8x16 sprite, and where that sprite
    goes from the boat's middle."""
    (fx, fy), (rx, ry) = heading_vectors(h)
    dots = set()
    for d in (8.0, 10.0, 12.0):
        lat = 1.2 + (d - 8) * 0.55
        for s in (-1, 1):
            x = 8 + s * lat * rx - d * fx
            y = 8 + s * lat * ry - d * fy
            dots.add((math.floor(x), math.floor(y)))
    xs = [x for x, _ in dots]
    ys = [y for _, y in dots]
    w, hgt = max(xs) - min(xs) + 1, max(ys) - min(ys) + 1
    assert w <= 8 and hgt <= 16, (h, w, hgt)
    ox = min(xs) - (8 - w) // 2
    oy = min(ys) - (16 - hgt) // 2
    px = grid(8, 16, 0)
    for x, y in dots:
        px[y - oy][x - ox] = 3
    return px, ox - 8, oy - 8                 # from the boat's middle


def box_half():
    px = grid(8, 16, 0)
    for i in range(4):
        px[0][i] = px[15][i] = 2
        px[i][0] = px[15 - i][0] = 2
    return px


# ------------------------------------------------------------------- fonts
def panel_glyph(font, glyph, colour):
    src = tile_pixels(font, glyph)
    return [[colour if v else 1 for v in row] for row in src]


def border_tiles(line, back):
    """Eight frame tiles, TL T TR L R BL B BR: a line of colour `line`
    through pixel 3 of the tile, on `back`."""
    tiles = []
    for kind in ["TL", "T", "TR", "L", "R", "BL", "B", "BR"]:
        px = grid(8, 8, back)
        top, bottom = kind in ("TL", "T", "TR"), kind in ("BL", "B", "BR")
        left, right = kind in ("TL", "L", "BL"), kind in ("TR", "R", "BR")
        if top or bottom:
            y = 3 if top else 4
            x0 = 3 if left else 0
            x1 = 4 if right else 7
            for x in range(x0, x1 + 1):
                px[y][x] = line
        if left or right:
            x = 3 if left else 4
            y0 = 3 if top else 0
            y1 = 4 if bottom else 7
            for y in range(y0, y1 + 1):
                px[y][x] = line
        tiles.append(px)
    return tiles


# ---------------------------------------------------------------- pictures
class Canvas:
    def __init__(self, w, h, c):
        self.w, self.h = w, h
        self.px = grid(w, h, c)

    def fill(self, x0, y0, x1, y1, c):
        for y in range(max(0, y0), min(self.h, y1)):
            for x in range(max(0, x0), min(self.w, x1)):
                self.px[y][x] = c

    def dot(self, x, y, c):
        if 0 <= x < self.w and 0 <= y < self.h:
            self.px[y][x] = c

    def poly(self, points, c):
        for y in range(self.h):
            for x in range(self.w):
                if poly_cover(points, x, y, 2) >= .5:
                    self.px[y][x] = c


def sky_and_sea(w, h, horizon, sparkle=True):
    cv = Canvas(w, h, SKY)
    cv.fill(0, horizon, w, h, CARD_SEA)
    if sparkle:                              # a pattern that repeats every 16
        for y in range(horizon + 3, h, 5):
            for x in range(((y - horizon) * 6) % 16, w, 16):
                cv.dot(x, y, WHITE)
                cv.dot(x + 1, y, WHITE)
    return cv


def cloud(cv, x, y):
    cv.fill(x + 2, y, x + 9, y + 1, WHITE)
    cv.fill(x, y + 1, x + 13, y + 3, WHITE)
    cv.fill(x + 5, y - 1, x + 8, y, WHITE)


def ship_cargo():
    cv = sky_and_sea(80, 32, 16)
    cloud(cv, 8, 3)
    cv.poly([(2, 16), (78, 16), (71, 25), (9, 25)], HULL)
    cv.fill(4, 17, 76, 18, WHITE)
    cv.fill(10, 25, 71, 26, WHITE)               # foam at the waterline
    colours = [GRAY, WHITE, GRAY, BLACK, GRAY, WHITE]
    for i in range(6):
        cv.fill(10 + i * 8, 10, 17 + i * 8, 16, colours[i])
        cv.fill(10 + i * 8, 6, 17 + i * 8, 10, colours[(i + 3) % 6])
    cv.fill(60, 5, 71, 16, WHITE)                # the bridge, aft
    cv.fill(61, 7, 70, 8, BLACK)
    cv.fill(64, 0, 68, 5, BLACK)                 # the funnel
    cv.fill(74, 8, 75, 16, BLACK)                # a mast at the bow
    return cv


def ship_passenger():
    cv = sky_and_sea(80, 32, 16)
    cloud(cv, 60, 2)
    cv.poly([(3, 16), (77, 16), (70, 25), (10, 25)], HULL)
    cv.fill(10, 25, 70, 26, WHITE)
    cv.fill(8, 11, 72, 16, WHITE)
    cv.fill(14, 7, 66, 11, WHITE)
    cv.fill(24, 4, 56, 7, WHITE)
    for x in range(10, 70, 3):
        cv.dot(x, 13, BLACK)
    for x in range(16, 64, 3):
        cv.dot(x, 9, BLACK)
    cv.fill(42, 0, 49, 4, GRAY)
    cv.fill(42, 0, 49, 1, BLACK)
    cv.fill(6, 18, 74, 19, WHITE)
    return cv


def ship_tug():
    cv = sky_and_sea(80, 32, 16)
    cloud(cv, 6, 4)
    cv.poly([(18, 16), (62, 16), (58, 25), (22, 25)], HULL)
    cv.fill(18, 16, 62, 17, BLACK)               # the fender
    cv.fill(22, 25, 58, 26, WHITE)
    cv.fill(30, 6, 44, 16, WHITE)                # the wheelhouse
    cv.fill(31, 8, 43, 10, BLACK)
    cv.fill(36, 0, 37, 6, BLACK)                 # its mast
    cv.fill(34, 2, 40, 3, BLACK)
    cv.fill(46, 8, 50, 16, BLACK)                # the funnel
    cv.fill(46, 10, 50, 11, GRAY)
    cv.fill(21, 12, 29, 16, GRAY)
    return cv


def ship_sail():
    cv = sky_and_sea(80, 32, 16)
    cloud(cv, 58, 4)
    cv.poly([(20, 16), (64, 16), (57, 23), (27, 23)], HULL)
    cv.fill(27, 23, 57, 24, WHITE)
    cv.fill(41, 0, 42, 16, BLACK)                # the mast
    cv.poly([(42.5, 1), (42.5, 15), (60, 15)], WHITE)    # the main
    cv.poly([(40, 3), (40, 15), (26, 15)], WHITE)        # the jib
    for y in (6, 10):
        cv.fill(43, y, 43 + (y - 1), y + 1, GRAY)
    return cv


def port_picture():
    w, h = 128, 48
    cv = sky_and_sea(w, h, 32)
    cloud(cv, 12, 6)
    cloud(cv, 84, 10)
    cv.fill(0, 26, w, 32, GRAY)                  # the pier's deck
    cv.fill(0, 25, w, 26, BLACK)                 # its rail
    for x in range(3, w, 10):
        cv.fill(x, 21, x + 1, 25, BLACK)         # rail posts
        cv.fill(x, 32, x + 2, 41, BLACK)         # pilings
    for x in range(30, w, 40):                   # lamps
        cv.fill(x, 12, x + 1, 25, BLACK)
        cv.fill(x - 1, 10, x + 2, 12, WHITE)
    cv.poly([(46, 38), (84, 38), (79, 45), (51, 45)], HULL)
    cv.fill(58, 33, 70, 38, WHITE)
    cv.fill(59, 34, 69, 35, BLACK)
    cv.fill(66, 30, 68, 33, BLACK)
    cv.fill(50, 45, 80, 46, WHITE)
    return cv


# ------------------------------------------------------------------- build
class Table:
    def __init__(self, data=None):
        self.data = bytearray(data or bytes(4096))
        self.index = {}

    def put(self, t, px):
        self.data[t * 16:t * 16 + 16] = tile_bytes(px)

    def known(self, first, last):
        for t in range(first, last + 1):
            self.index.setdefault(bytes(self.data[t * 16:t * 16 + 16]), t)


def picture_tiles(cv, table, palettes, rows_palette, state):
    """The canvas cut into tiles; rows_palette(tile_row) is the palette a
    tile row must take. New tiles go from state['next']."""
    out = []
    for ty in range(cv.h // 8):
        pal = palettes[rows_palette(ty)]
        for tx in range(cv.w // 8):
            px = []
            for y in range(8):
                row = []
                for x in range(8):
                    c = cv.px[ty * 8 + y][tx * 8 + x]
                    if c not in pal:
                        sys.exit(f"colour ${c:02X} at {tx * 8 + x},{ty * 8 + y} is not in palette {pal}")
                    row.append(pal.index(c))
                px.append(row)
            b = tile_bytes(px)
            if b not in table.index:
                t = state["next"]
                if t > PIC_LAST:
                    sys.exit("the pictures need more tiles than table A has")
                table.data[t * 16:t * 16 + 16] = b
                table.index[b] = t
                state["next"] += 1
            out.append(table.index[b])
    return out


def text_tiles(s, base):
    return [base + FONT.index(c) for c in s]


def attributes(pal_at):
    """64 attribute bytes from pal_at(block_col, block_row) -> 0..3."""
    att = bytearray(64)
    for ay in range(8):
        for ax in range(8):
            v = 0
            for q, (dx, dy) in enumerate([(0, 0), (1, 0), (0, 1), (1, 1)]):
                bx, by = ax * 2 + dx, ay * 2 + dy
                v |= (pal_at(bx, by) if by < 15 else 0) << (q * 2)
            att[ay * 8 + ax] = v
    return att


def frame(nam, col, row, w, h, base):
    """A frame round the w x h tiles at (col, row), from border tiles at base."""
    nam[(row - 1) * 32 + col - 1] = base + 0
    nam[(row - 1) * 32 + col + w] = base + 2
    nam[(row + h) * 32 + col - 1] = base + 5
    nam[(row + h) * 32 + col + w] = base + 7
    for x in range(col, col + w):
        nam[(row - 1) * 32 + x] = base + 1
        nam[(row + h) * 32 + x] = base + 6
    for y in range(row, row + h):
        nam[y * 32 + col - 1] = base + 3
        nam[y * 32 + col + w] = base + 4


def put_text(nam, row, col, s, base):
    for i, t in enumerate(text_tiles(s, base)):
        nam[row * 32 + col + i] = t


def asm_bytes(name, data, per=16):
    lines = [f"{name}:"]
    for i in range(0, len(data), per):
        lines.append("    .byte " + ",".join(f"${b:02X}" for b in data[i:i + per]))
    return "\n".join(lines)


def main():
    font = open(LDV_CHR, "rb").read()
    a = Table()
    b = Table()

    # Table A: the picker's font, kept where it was
    a.data[0:0x54 * 16] = font[0:0x54 * 16]
    a.data[0xEF * 16:0x100 * 16] = font[0xEF * 16:0x100 * 16]
    for i, px in enumerate(border_tiles(3, 0)):
        a.put(CARD_BORDER + i, px)
    a.known(0, CARD_BORDER + 7)
    state = {"next": PIC_FIRST}
    sky_row = lambda ty: 0 if ty < 2 else 1
    ships = [picture_tiles(f(), a, [CARD_PALETTE[0:4], CARD_PALETTE[4:8]], sky_row, state)
             for f in (ship_cargo, ship_passenger, ship_tug, ship_sail)]
    port = picture_tiles(port_picture(), a, [CARD_PALETTE[0:4], CARD_PALETTE[4:8]],
                         lambda ty: 0 if ty < 4 else 1, state)
    pic_tiles = state["next"] - PIC_FIRST

    # Table B: sprites
    wake_dx, wake_dy = [], []
    for h in range(8):
        px = boat(h)
        for half in range(2):
            for part in range(2):
                b.put(SPR_BOAT + h * 4 + half * 2 + part, cut(px, half * 8, part * 8))
        wpx, dx, dy = wake(h)
        b.put(SPR_WAKE + h * 2, cut(wpx, 0, 0))
        b.put(SPR_WAKE + h * 2 + 1, cut(wpx, 0, 8))
        wake_dx.append(dx & 0xFF)
        wake_dy.append(dy & 0xFF)
    bx = box_half()
    b.put(SPR_BOX, cut(bx, 0, 0))
    b.put(SPR_BOX + 1, cut(bx, 0, 8))
    # the map
    for m in range(16):
        b.put(OW_LAND + m, land_tile(m))
        b.put(OW_SURF + m, surf_tile(m))
    b.put(OW_SEA, grid(8, 8, 0))
    b.put(OW_FLECK_A, fleck_tile(1, (2, 2)))
    b.put(OW_FLECK_B, fleck_tile(2, (3, 4)))
    # the status bar's font, on black
    for g in range(len(FONT)):
        b.put(PANEL_FONT + g, panel_glyph(font, g, 3))
    for i, ch in enumerate("LIVE"):
        b.put(PANEL_LIVE + i, panel_glyph(font, FONT.index(ch), 2))
    on = grid(8, 8, 1)
    off = grid(8, 8, 1)
    for y in range(1, 7):
        for x in range(1, 6):
            on[y][x] = 2
            if y in (1, 6) or x in (1, 5):
                off[y][x] = 2
    b.put(PANEL_BAR_ON, on)
    b.put(PANEL_BAR_OFF, off)
    for i, px in enumerate(border_tiles(3, 1)):
        b.put(PANEL_BORDER + i, px)

    # ---- the map screen's fixed rows: the tag's frame and the status bar
    blank = PANEL_FONT
    status = [blank] * (6 * 32)              # rows 24-29
    def stat(key, s, base=PANEL_FONT):
        r, c = STAT[key]
        for i, t in enumerate(text_tiles(s, base)):
            status[(r - 24) * 32 + c + i] = t
    stat("port_label", "PORT")
    stat("ships_label", "SHIPS")
    stat("rng_label", "RNG")
    stat("signal_label", "AIS SIGNAL")
    w = 32 - TAG_COL
    tag = [blank] * (TAG_ROWS * w)
    tag[0], tag[w - 1] = PANEL_BORDER + 0, PANEL_BORDER + 2
    tag[3 * w], tag[3 * w + w - 1] = PANEL_BORDER + 5, PANEL_BORDER + 7
    for x in range(1, w - 1):
        tag[x] = PANEL_BORDER + 1
        tag[3 * w + x] = PANEL_BORDER + 6
    for y in (1, 2):
        tag[y * w] = PANEL_BORDER + 3
        tag[y * w + w - 1] = PANEL_BORDER + 4

    def status_pal(bxl, byl):           # blocks of rows 24-29
        if byl == 12:                   # rows 24-25: PORT
            return 3 if bxl < 3 else 2
        if byl == 13:                   # rows 26-27: SHIPS, RNG, the link
            return 3 if bxl < 3 or bxl in (4, 5) else 2
        return 3                        # rows 28-29: the signal
    map_att = attributes(lambda bx_, by_: status_pal(bx_, by_) if by_ >= 12
                         else (2 if by_ == TAG_ROW // 2 and bx_ >= TAG_COL // 2
                               else 3 if by_ == TAG_ROW // 2 + 1 and bx_ >= TAG_COL // 2 else 1))

    # ---- the port card
    pc, pr, pw, ph = PORT_PIC
    port_nam = [0] * 960
    for i, t in enumerate(port):
        port_nam[(pr + i // pw) * 32 + pc + i % pw] = t
    frame(port_nam, pc, pr, pw, ph, CARD_BORDER)
    r, s = PORT_TEXT["title"]
    put_text(port_nam, r, (32 - len(s)) // 2, s, FONT_DIM_BASE)
    put_text(port_nam, PORT_TEXT["ships"][0], PORT_TEXT["ships"][1], "SHIPS:", FONT_DIM_BASE)
    put_text(port_nam, PORT_TEXT["range"][0], PORT_TEXT["range"][1], "RANGE:", FONT_DIM_BASE)
    r, s = PORT_TEXT["hint"]
    put_text(port_nam, r, (32 - len(s)) // 2, s, FONT_BASE)
    def port_pal(bx_, by_):
        if pc // 2 <= bx_ < (pc + pw) // 2 and pr // 2 <= by_ < (pr + ph) // 2:
            return 0 if by_ * 2 < pr + 4 else 1
        if by_ * 2 in (PORT_TEXT["name"] - 1, PORT_TEXT["hint"][0]):
            return 3
        return 2
    port_att = attributes(port_pal)

    # ---- the ship card
    sc, sr, sw, sh = SHIP_PIC
    ship_nam = [0] * 960
    for i, t in enumerate(ships[0]):
        ship_nam[(sr + i // sw) * 32 + sc + i % sw] = t
    frame(ship_nam, 2, 2, 28, 26, CARD_BORDER)
    r, c, s = SHIP_HINT
    put_text(ship_nam, r, c, s, FONT_DIM_BASE)
    def ship_pal(bx_, by_):
        if bx_ in (0, 15) or by_ in (0, 14):
            return 3                    # the frame, yellow
        if sc // 2 <= bx_ < (sc + sw) // 2 and sr // 2 <= by_ < (sr + sh) // 2:
            return 0 if by_ * 2 < sr + 2 else 1
        if by_ == 2:                    # rows 4-5: the type, yellow
            return 3
        if by_ == SHIP_HINT[0] // 2:
            return 3
        if by_ >= SHIP_FIRST_ROW // 2 and bx_ < SHIP_VALUE_COL // 2 + 0:
            return 3                    # the labels, orange
        return 2
    ship_att = attributes(ship_pal)

    os.makedirs(OUT, exist_ok=True)
    open(os.path.join(OUT, "harbor_a.chr"), "wb").write(a.data)
    open(os.path.join(OUT, "harbor_b.chr"), "wb").write(b.data)

    inc = [
        "; Generated by tools/make_overworld.py. Do not hand-edit.",
        "",
        f"SPR_BOAT      = ${SPR_BOAT:02X}   ; heading * 4: left 8x16 pair, right pair",
        f"SPR_WAKE      = ${SPR_WAKE:02X}   ; heading * 2",
        f"SPR_BOX       = ${SPR_BOX:02X}   ; the cursor's left half; the right is it flipped",
        f"OW_LAND       = ${OW_LAND:02X}   ; + the sides with water: 1 N, 2 E, 4 S, 8 W",
        f"OW_SURF       = ${OW_SURF:02X}   ; + the sides with land; + 0: land at a corner only",
        f"OW_SEA        = ${OW_SEA:02X}",
        f"OW_FLECK_A    = ${OW_FLECK_A:02X}",
        f"OW_FLECK_B    = ${OW_FLECK_B:02X}",
        f"PANEL_FONT    = ${PANEL_FONT:02X}   ; the font on black: glyph n, colour 3",
        f"PANEL_LIVE    = ${PANEL_LIVE:02X}   ; L I V E in colour 2",
        f"PANEL_BAR_ON  = ${PANEL_BAR_ON:02X}",
        f"PANEL_BAR_OFF = ${PANEL_BAR_OFF:02X}",
        f"PANEL_BORDER  = ${PANEL_BORDER:02X}",
        f"CARD_BORDER   = ${CARD_BORDER:02X}",
        "",
        f"MAP_SCREEN_ROWS = {MAP_ROWS}",
        f"MAP_SCREEN_COLS = {MAP_COLS}",
        f"MAP_CELL_ROW0 = {CELL_ROW0}",
        f"MAP_CELL_COL0 = {CELL_COL0}",
        f"TAG_COL       = {TAG_COL}",
        f"TAG_WIDTH     = {32 - TAG_COL}",
        f"TAG_ROW       = {TAG_ROW}",
        f"TAG_NAME_ADDR = ${addr(TAG_ROW + 1, TAG_COL + 1):04X}",
        f"TAG_INFO_ADDR = ${addr(TAG_ROW + 2, TAG_COL + 1):04X}",
        f"TAG_TEXT      = {32 - TAG_COL - 2}",
    ]
    for k, (r, c) in STAT.items():
        inc.append(f"STAT_{k.upper():12} = ${addr(r, c):04X}")
    inc += [
        f"PORT_NAME_ROW = {PORT_TEXT['name']}",
        f"PORT_SHIPS_ADDR = ${addr(PORT_TEXT['ships'][0], PORT_TEXT['ships'][1] + 7):04X}",
        f"PORT_RANGE_ADDR = ${addr(PORT_TEXT['range'][0], PORT_TEXT['range'][1] + 7):04X}",
        f"SHIP_PIC_ADDR = ${addr(sr, sc):04X}",
        f"SHIP_PIC_W    = {sw}",
        f"SHIP_PIC_H    = {sh}",
        f"SHIP_RIGHT_COL = {SHIP_RIGHT_COL}",
        f"SHIP_LABEL_COL = {SHIP_LABEL_COL}",
        f"SHIP_VALUE_COL = {SHIP_VALUE_COL}",
        f"SHIP_FIRST_ROW = {SHIP_FIRST_ROW}",
        "",
        '.segment "RODATA"',
        asm_bytes("map_palette", MAP_PALETTE),
        asm_bytes("card_palette", CARD_PALETTE),
        asm_bytes("group_card_hull", GROUP_CARD_HULL),
        asm_bytes("wake_dx", wake_dx),
        asm_bytes("wake_dy", wake_dy),
        asm_bytes("map_attributes", map_att),
        asm_bytes("status_rows", status, 32),
        asm_bytes("tag_frame", tag, w),
        asm_bytes("port_card_nam", port_nam, 32),
        asm_bytes("port_card_att", port_att),
        asm_bytes("ship_card_nam", ship_nam, 32),
        asm_bytes("ship_card_att", ship_att),
        asm_bytes("ship_pictures", sum(ships, []), sw),
        "",
    ]
    open(os.path.join(OUT, "overworld.inc"), "w").write("\n".join(inc))
    print(f"table A: {pic_tiles} picture tiles (${PIC_FIRST:02X}-${state['next'] - 1:02X}); "
          f"table B: up to ${PANEL_BORDER + 7:02X}")

    if "-p" in sys.argv:
        preview(a.data, b.data, ships, port, port_nam, port_att, ship_nam, ship_att)


NES_RGB = {
    0x00: (102, 102, 102), 0x01: (0, 42, 136), 0x02: (20, 18, 167), 0x07: (92, 30, 0),
    0x0F: (0, 0, 0), 0x10: (173, 173, 173), 0x11: (21, 95, 217), 0x12: (66, 64, 255),
    0x16: (181, 49, 32), 0x17: (153, 78, 0), 0x1A: (0, 143, 50), 0x21: (100, 176, 255),
    0x22: (146, 144, 255), 0x27: (234, 158, 34), 0x28: (188, 190, 0), 0x2A: (76, 208, 32),
    0x30: (255, 254, 255), 0x38: (228, 229, 148),
}


def render(nam, att, table, pal, w=32, h=30):
    from PIL import Image
    im = Image.new("RGB", (w * 8, h * 8))
    for ty in range(h):
        for tx in range(w):
            q = att[(ty // 4) * 8 + tx // 4] >> ((((ty // 2) & 1) * 2 + ((tx // 2) & 1)) * 2) & 3
            px = tile_pixels(table, nam[ty * 32 + tx])
            for y in range(8):
                for x in range(8):
                    v = px[y][x]
                    c = pal[0] if v == 0 else pal[q * 4 + v]
                    im.putpixel((tx * 8 + x, ty * 8 + y), NES_RGB.get(c, (255, 0, 255)))
    return im


def preview(a, b, ships, port, port_nam, port_att, ship_nam, ship_att):
    from PIL import Image
    sheet = Image.new("RGB", (256 * 3 + 16, 240), (40, 40, 40))
    sheet.paste(render(port_nam, port_att, a, CARD_PALETTE), (0, 0))
    sheet.paste(render(ship_nam, ship_att, a, CARD_PALETTE), (264, 0))
    # table B with the map palette, as sprites and tiles
    tb = Image.new("RGB", (128, 128))
    for t in range(256):
        px = tile_pixels(b, t)
        pal = 0 if 0x40 <= t < 0x70 else 2 if t >= 0x70 else 4
        for y in range(8):
            for x in range(8):
                v = px[y][x]
                c = MAP_PALETTE[0] if v == 0 else MAP_PALETTE[pal * 4 + v]
                tb.putpixel(((t % 16) * 8 + x, (t // 16) * 8 + y), NES_RGB.get(c, (255, 0, 255)))
    sheet.paste(tb.resize((240, 240), Image.NEAREST), (528, 0))
    os.makedirs(os.path.join(ROOT, "build"), exist_ok=True)
    sheet.resize((sheet.width * 2, sheet.height * 2), Image.NEAREST).save(
        os.path.join(ROOT, "build/overworld-preview.png"))


if __name__ == "__main__":
    main()
