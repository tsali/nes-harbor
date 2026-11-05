# NES Harbor

Status: works end to end in the NESP32 build of Mesen against live AISHub
data. Not yet tried on a real console.

Play it in a browser: https://harbor.cultofjames.org

Ships on your NES. NES Harbor shows the vessels around a harbor on an
NES-pixel map of the coast, live, through a [NESP32](https://nesp32.retrobits.tv/) Wi-Fi
board in controller port 2. Pick a port from the list (or type a US zip
code), pick a range, and the nearest eight ships show up with their name,
call sign, type, status, speed, course and distance.

It is a fork of [NES Radar](https://github.com/k6lcm/nes-radar) by
levimaaia, which does the same for aircraft. Much of the code (the link,
the picker's editor, the records) is NES Radar's; the overworld map, the
boats, the port card and the ship card are NES Harbor's own, drawn in the
style of an NES top-down sea adventure.

## Playing it

1. A NESP32 with firmware that has the HARBOR service (service `$16`), on
   Wi-Fi. HARBOR is not in upstream nesp32 yet.
2. An [AISHub](https://www.aishub.net) username. AISHub gives one to
   members, and membership is free if you share an AIS receiver's data
   with them. The username lives on the NESP32 as the setting
   `aishub_key`, never on the console.
3. Load `nes_harbor.nes`.

On the picker:

| Button | List | Zip code |
|---|---|---|
| Up / Down | port | digit |
| Left / Right | range (3, 5, 10, 20, 40, 60 nm) | which digit |
| Start or A | go | go |
| B | music on/off | back to the list |

With a Bluetooth keyboard on the NESP32, type a zip code's digits, or a
letter to jump to the next port starting with it.

On the map:

| Button | Does |
|---|---|
| Up / Down | pick a ship |
| A (tap) | the ship's card: name, MMSI, IMO, call sign, flag, type, status, destination, ETA, size, draft, speed, course, heading, position |
| Left / Right | range, at once |
| A held + D-pad | move the map a quarter of the range |
| B (on a card) | back to the map |
| B | back to the picker |

After a port is picked, a NOW ENTERING card shows the place, its range
and, once they come, how many ships there are; the map follows when the
first ships and the map have come (A or Start goes there at once, B back
to the picker).

The map is OpenStreetMap's, turned by the NESP32 into land and water
cells (HARBOR's MAP op), one 8x8 tile a cell: grass, a rocky edge where
land meets water, surf on the water beside it, and open sea with waves
that twinkle. The first map for a place takes a few seconds. The map
shows the middle 32 by 24 of the NESP32's 36 by 36 cells, so ships near
the edge of the range (beyond about 60 percent of it north or south, or
85 percent east or west) are counted and can be picked but are not
drawn; pan, or zoom out, to see them.

Each ship is a little boat pointing the way it is going (eight headings,
from its course, or its heading when AIS has no course), with a wake
when it is moving. Its colour is its AIS type's group:

| Colour | Group |
|---|---|
| red | cargo and tankers (AIS 70-89) |
| light blue | passenger and high speed craft (40-49, 60-69) |
| yellow | tugs, pilots, search and rescue, law, tenders, medical, towing, dredging, military (31-33, 35, 50-59) |
| white | sail, pleasure, fishing, diving and the rest |

The selected ship has a yellow box, and its name tag at the top of the
map says its name, AIS type and speed. The NES draws only eight sprites
on a line; where more ships than that share one, they take turns
(flicker) rather than one of them vanishing, and the selected ship always
stays.

The status bar under the map:

| Field | What it shows |
|---|---|
| PORT | the place |
| SHIPS | how many ships are in the scene (eight at most) |
| RNG | the range, nautical miles |
| link | LIVE (dots while data comes), WAITING (the data is old or late), AIS DOWN, NO ANSWER (no NESP32), BAD DATA |
| AIS SIGNAL | how fresh the AIS feed is: ten bars while the NESP32 answers and AISHub's data is current, one bar fewer for each second without a good reply, at most four while the data is old (WAITING), none when AIS is down, the NESP32 does not answer or its data is bad |

The music is Drunken Sailor, a traditional shanty.

If the screen says SET AISHUB KEY, the NESP32 has no `aishub_key`.

### In Mesen

The NESP32 build of MesenCE (github.com/tsali/MesenCE, branch `nesp32`)
emulates the board. Give it the key either way:

- environment variable `NESP32_AISHUB_KEY=yourname`, or
- a file `Nesp32/settings.txt` in Mesen's home folder with the line
  `aishub_key=yourname`

## Building

Needs cc65 and Python 3 with Pillow, and a checkout of
[nesp32](https://github.com/Armitage64/nesp32) next to this one.

    make proto    # HARBOR's constants from the nesp32 checkout
    make          # build/nes_harbor.nes

`tools/gen_ports.py` writes the port list (`nes_harbor/nes/ports.inc`):
edit `PORTS` there to change it. `tools/make_overworld.py` draws the
map's tiles, the boats, the status bar's font, and the port and ship
cards (`nes_harbor/assets/overworld/`; `-p` writes a preview to
`build/`), `tools/make_splash.py` draws the title screen, and
`tools/gen_music.py` writes the music. All the art is drawn by these
scripts, so it can be read and made again. `tools/relabel_scope.py` is
from the earlier radar scope and is no longer part of the build.

## Credits

- NES Harbor: Rai
  - Bluesky [@cultofjames.org](https://bsky.app/profile/cultofjames.org)
  - X [@pandavieranash](https://x.com/pandavieranash)
  - GitHub [tsali](https://github.com/tsali)
- NES Radar, the scope this is built on: levimaaia
  ([levimaaia.com](https://levimaaia.com),
  [youtube.com/@levimaaia](https://youtube.com/@levimaaia)),
  [k6lcm/nes-radar](https://github.com/k6lcm/nes-radar)
- NESP32: [Armitage64/nesp32](https://github.com/Armitage64/nesp32),
  [retrobits](https://nesp32.retrobits.tv/)
- Ship positions: [AISHub](https://www.aishub.net), from volunteers' AIS
  receivers
- Map data © [OpenStreetMap](https://www.openstreetmap.org/copyright)
  contributors, ODbL

GPL-3.0, as NES Radar is. See LICENSE.
