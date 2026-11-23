# linuxTKeditor — level editor for Ultimate Tapan Kaikki in C + raylib

A port of the DOS level editor `EDITOR.EXE` (TK level editor v1.3), built the same
way as `../linuxTK`: a C89 core with static memory and one small platform file
(`src/plat_raylib.c`). The editor's source code was not released, so this port is
written from the editor's help texts, `../EDITOR.TXT` and the level format in
`../SRC/CLASSES.CPP`. It reads the original graphics (`EFPS/FLOOR1.EFP`,
`WALLS1.EFP`, `SHADOWS.EFP`, `CRATES.EFP`, `POINTER.EFP`, `PALETTE.TAB` and
`FNTS/8X8B.FNT`), and loads and saves the original `.LEV` files.

## Build and run

```sh
make                       # builds ./tked (needs raylib)
./tked                     # game data is looked up in .. (the TK directory), then .
./tked DEFAULT/LEVEL1.LEV  # open a level (relative to LEVS/)
./tked -d /path/to/tk      # data directory
make test                  # loads and saves all levels under ../LEVS and compares them
```

## Keys

The keys of the original editor, plus undo, a grid and scrolling:

| Key | |
|---|---|
| ESC | quit (asks Y), or cancel the current tool |
| F1 | help |
| F2 / F3 | save / load level (a list of every `.LEV` under `LEVS/`) |
| F4 | new level: x and y size (UP/DOWN ±1, LEFT/RIGHT ±10), filled with the right button's block |
| F5 | level map; click to jump there |
| F6 | automatic shadows (replaces all shadows) |
| F7 | general level variables: comment, time limit, enemies of each type |
| F8 / F9 | random crates for normal game / deathmatch |
| Q / W | place / delete spot light (UP/DOWN size) |
| A / S | place / delete steam (UP/DOWN range, LEFT/RIGHT angle) |
| Z / X | place a normal game (blue) / deathmatch (green) crate (UP/DOWN weapon, bullets or energy; LEFT/RIGHT which) |
| C | delete crate |
| 1 / 2 | place player 1 / player 2 start |
| F | fill a rectangle: click two corners |
| ENTER | switch between editing blocks and shadows |
| SPACE | choose blocks for the left and right button (or the shadow type) |
| PGUP / PGDN | walls / floor in the block selection |
| U | undo (16 steps) |
| G | grid on/off |
| arrows (SHIFT faster), middle button drag | scroll |

Mouse: the left and right buttons paint with their own block. A floor block on the
right button is handy for erasing walls. In shadow mode the left button paints the
chosen shadow and the right button removes shadows. Tools such as lights, steam and
crates stay on until you right-click or press ESC.

The status bar shows the blocks of both buttons, the tool, the block under the mouse,
the level size and the file name (`*` = not saved). Closing the window with unsaved
changes writes them to `LEVS/AUTOSAVE.LEV`.

## Level files

Levels are saved as version 5, the newest format, which the DOS game and linuxTK both
read. Older levels (versions 1–4) are converted when they are saved. Version 5 files
the editor did not change are saved byte for byte identical; `make test` checks this
for all 88 levels of the game. Sizes go from 2×2 to 100×100 blocks, the largest
level linuxTK accepts.

linuxTK finds levels only in the episode folders listed in `../linuxTK/src/data.c`
(`DEFAULT`, `ULTIMATE`, …), named `LEVEL1.LEV`, `LEVEL2.LEV`, … in upper case. Save as
for example `DEFAULT/LEVEL9.LEV` to play a new level there. linuxTK does not draw
shadows, spot lights or steam yet; the editor keeps them for the DOS game.

Automatic shadows use a rule measured from the original levels, with light from the
upper right. A floor block gets:
- shadow 3 under a wall,
- shadow 2 left of a wall,
- the corner shadow 1 when the block up-right of it is a wall, or the blocks above
  it and to its right both are.

## Layout

| File | Contents |
|---|---|
| `src/plat.h` | platform interface: frame, keys, mouse, level file list |
| `src/plat_raylib.c` | Linux backend (window, palette → RGBA texture, keys, mouse) |
| `src/plat_test.c` | headless backend with scripted input, writes PPM screenshots |
| `src/level.c` | `.LEV` load (versions 1–5) / save (version 5), automatic shadows |
| `src/data.c` | EFP decoder, tiles, shadows, crates, pointer, font, palette, shadow table |
| `src/gfx.c` | clipped drawing, shadows, the game font |
| `src/editor.c` | the editor: map view, tools, dialogs, selection screens |
| `tools/roundtrip.c` | the load/save test of `make test` |

## Testing without a window

```sh
make tked_test
ED_SCRIPT="2:kF1,10:s,12:kesc,14:q" ED_SHOTDIR=/tmp ./tked_test DEFAULT/LEVEL1.LEV
```

See the comment at the top of `src/plat_test.c` for the script format (keys, typed
text, mouse moves and clicks, screenshots).
