# NES Harbor

Status: works end to end in the NESP32 build of Mesen against live AISHub
data. Not yet tried on a real console.

Ships on your NES. NES Harbor shows the vessels around a harbor on a
radar scope, live, through a [NESP32](https://nesp32.retrobits.tv/) Wi-Fi
board in controller port 2. Pick a port from the list (or type a US zip
code), pick a range, and the nearest eight ships show up with their name,
call sign, type, status, speed, course and distance.

It is a fork of [NES Radar](https://github.com/k6lcm/nes-radar) by
levimaaia, which does the same for aircraft. The scope, the screen and
most of the code are NES Radar's; NES Harbor changes what goes in it.

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
| B | | back to the list |

With a Bluetooth keyboard on the NESP32, type a zip code's digits, or a
letter to jump to the next port starting with it.

On the scope, as in NES Radar: Up/Down picks a ship, B goes back to the
picker.

| Field | What it shows |
|---|---|
| TGT | the selected ship's slot |
| TYPE | AIS ship type as a word: CRGO, TNKR, TUG, PASS, FISH, SAIL, PLSR... |
| CALL | radio call sign |
| STAT | navigational status: UWAY, ANCH, MOOR, FISH, SAIL... |
| AIS | AIS ship type code |
| SOG / COG | speed (knots) and course over ground |
| DIST | distance, nautical miles (shows 9.9 for anything farther) |

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
edit `PORTS` there to change it. `tools/relabel_scope.py` relabels NES
Radar's scope for ships.

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

GPL-3.0, as NES Radar is. See LICENSE.
