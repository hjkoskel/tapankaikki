# linuxTK — Ultimate Tapan Kaikki in C + raylib

A port of the DOS game in `../SRC` following `../PortToC.md`:
a C89 game core with only static memory and only 8/16-bit integer math
(12.4 fixed point positions, 256-step binary angles, no floating point),
plus one small platform file. Actors use 16 facing directions; menus are plain text.

## Build and run

```sh
make            # builds ./tk (needs raylib) and prints the core memory footprint
./tk            # game data is looked up in .. (the original TK directory), then .
./tk -d /path/to/tk -e 2 -l 3   # data dir, episode 2, start at level 3
make check      # verifies the core has no malloc/free
```

## Keys

| | Player 1 / single player | Player 2 (left view in split screen) |
|---|---|---|
| turn (22.5° steps) | ← → | A D |
| walk | ↑ ↓ | W S |
| fire | Right Ctrl | Tab / Left Ctrl |
| next weapon | Right Shift / Enter | Caps Lock / Left Shift |
| strafe (hold) | AltGr | ` / Left Alt |
| side step | , . | 1 2 / Q E |
| select weapon | ` 1 … 0 - (single player only) | |

Space = map, Esc = quit dialog. In the shop: up/down with your own keys, FIRE buys,
CHANGE sells, Esc leaves.

## Layout

| File | Contents |
|---|---|
| `src/plat.h` | the whole platform interface |
| `src/plat_raylib.c` | Linux backend (window, palette → RGBA texture, keys, sounds) |
| `src/plat_test.c` | headless backend with scripted input, writes PPM frames |
| `src/types.h` | types, limits, constants (pool sizes live here) |
| `src/data.c` | weapon, bullet, enemy and effect tables (`const`) |
| `src/globals.c/.h` | all game state, all prototypes |
| `src/assets.c` | EFP decoder → packed u8 frames, fonts, translucency LUT, levels |
| `src/gfx.c` | clipped blitters and bitmap fonts |
| `src/player.c` `enemy.c` `bullet.c` `effect.c` `level.c` | game logic |
| `src/render.c` | one `render_view()` per view (split screen) |
| `src/screens.c` | menu, level info, stats, shop, quit dialog, victory |
| `src/game.c` | level flow and 40 Hz fixed time step loop |

## Testing without a window

```sh
make tk_test gore_test
TK_KEYS=5:enter,10:enter TK_HOLD0="20-200:up+fire" TK_SHOTS=100,200 TK_END=210 ./tk_test
TK_SHOTS=1,2,3,4 ./gore_test     # writes shot_0001..4.ppm: deaths, gore, footprints
```

See the comment at the top of `src/plat_test.c` for the script format.
