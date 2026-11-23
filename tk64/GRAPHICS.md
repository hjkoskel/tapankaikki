# tk64 graphics — artist's guide

How the Commodore 64 port gets its graphics, what the hardware allows, and where to
make changes when fine tuning the look.

**Important first:** the graphics are **converted automatically** from the original
PC graphics (`../EFPS/*.EFP`) by `tools/conv64.c`, and every result is also saved as
**editable PNG sheets in `tk64/gfx/`**. To change the look, edit those PNGs and run
`make`. A valid PNG is used for the game; a missing or broken one is generated again
from the original files. See [section 7](#7-editing-graphics-the-gfx-png-sheets) for the
workflow. You can also tune the converter itself (colours, thresholds, frames), see
[section 6](#6-recipes-for-fine-tuning).

---

## 1. Hardware limits in short

| | Map (tiles) | Sprites (actors, effects) | HUD and text |
|---|---|---|---|
| VIC-II mode | multicolour character mode | multicolour or hi-res sprites | the same character mode, hi-res chars |
| Pixel size | 2×1 ("fat" pixels), 4 per char row | multicolour 2×1, hi-res 1×1 | 1×1 |
| Unit | char 8×8 px; **metatile = 2×2 chars = 16×16 px** | 24×21 px (12×21 fat pixels in multicolour) | 8×8 char |
| Colours | 4 per char, 3 of them shared by the whole map | 3 + transparent (multicolour); 1 + transparent (hi-res) | 1 per char (colours 0–7 only) |
| Count | 256 chars in total | 171 sprite images, 8 on screen per view | ROM font, 64 chars |

The original game uses 20×20 tiles and 28×28 actors. The C64 world is **0.8×** that
size: tiles become 16×16, and actors are drawn into the 24×21 sprite box.

### C64 palette (numbers used everywhere below)

| # | colour | # | colour | # | colour | # | colour |
|---|---|---|---|---|---|---|---|
| 0 | black | 4 | purple | 8 | orange | 12 | mid grey |
| 1 | white | 5 | green | 9 | brown | 13 | light green |
| 2 | red | 6 | blue | 10 | light red | 14 | light blue |
| 3 | cyan | 7 | yellow | 11 | dark grey | 15 | light grey |

The converter uses the "Pepto" RGB values for these colours (`c64rgb` in `conv64.c`).

---

## 2. The map: characters and metatiles

### 2.1 Colours of the map

Every map character uses the multicolour bit pairs like this:

| bits | colour source | default | used for |
|---|---|---|---|
| `00` | background `$D021` | 0 black | dark parts, shadows, outlines |
| `01` | shared `$D022` | 11 dark grey | floors and walls |
| `10` | shared `$D023` | 15 light grey | floors and walls |
| `11` | colour RAM | 2 red | **gore and crates only** |

The colour RAM of the map rows is filled once with red (plus the multicolour bit) and
never changed, so the map scrolls fast. For the artist this means:

- Floors and walls have **3 colours**: black + 2 greys (by default).
- Red (`11`) is available everywhere. The converter keeps it out of floors and walls,
  so blood, gibs and crates stand out. You may use it in a tile on purpose, but every
  tile shares the same red.
- Changing the three map colours: `make CONV64_PAL=0,11,15` (background, `$D022`,
  `$D023`). Without the variable the converter chooses the best triple itself:
  black + the two colours that match the episode's tiles best. The Makefile sets
  `0,11,15` by default.

### 2.2 How a tile is converted

Source: `EFPS/FLOOR1.EFP` and `EFPS/WALLS1.EFP`, a 16×10 grid of 20×20 tiles each.
The palette comes from `WALLS1.EFP`.

1. **Downscale** 20×20 → 8×16 fat pixels (16×16 screen pixels) by averaging the colours
   (`downscale_tile`).
2. **Quantise** each pixel to the nearest of the 3 map colours (`quantise_tiles`). The
   colour distance weights brightness 6× more than hue, so light and dark areas are
   kept even when the hue changes.
3. **Split** the tile into 4 chars (top-left, top-right, bottom-left, bottom-right).
4. **Merge similar chars** until at most **160** remain (`merge_tiles`). The less used
   char takes the look of a more used, similar one. This step loses detail on purpose:
   if a carefully drawn char looks almost like a common one, it gets replaced.
5. Tiles whose 4 chars are now the same become **one metatile**.

Only tiles that some level uses are converted. Usage is counted over **all**
episodes, so later episodes fit with the same charset.

### 2.3 Character set layout (`tkchr.prg`, at `$C800`)

| chars | contents |
|---|---|
| 0–63 | ROM upper case font, copied in at start (HUD and menus). Anything drawn here in `tkchr.prg` is overwritten. |
| 64–223 | map tiles (up to 160 chars) |
| 224–239 | decals: 4 metatiles × 4 chars (blood, more blood, gibs, scorch) |
| 240–251 | crates: 3 metatiles × 4 chars (weapon, ammo, energy) |
| 252–255 | free |

### 2.4 Metatile numbers (one map byte per 16×16 tile)

| id | meaning |
|---|---|
| 0–120 | floors (walkable) |
| 121 | decal: blood |
| 122 | decal: more blood |
| 123 | decal: gibs |
| 124 | decal: scorch |
| 125–127 | crates: weapon, ammo, energy |
| 128–255 | walls (bit 7 = solid) |

`build/gfxdata.c` holds four tables `mt_tl`, `mt_tr`, `mt_bl`, `mt_br`: for each
metatile id, the char in each corner.

### 2.5 Decals (blood, gibs, scorch)

There are no blood sprites. When something bleeds or explodes, the game **replaces
the floor tile** under it with a decal metatile (121–124). The converter draws the
decals over **the most used floor tile**:

- blood (121, 122): splats from the blood frames of `EFFECTS.EFP`, red with a little black
- gibs (123): two body parts from `BODYP.EFP`
- scorch (124): a random black blotch

All decals share that one floor underneath, so on other floors a decal shows as a
square of "the common floor". This is the most visible place to improve by hand:
edit `gfx/decals.png` (the generator is `build_decals` in `conv64.c`).

### 2.6 Crates

From `EFPS/CRATES.EFP` (three 14×14 crates) to 16×16 metatiles 125–127. They may use
all 4 map colours, including red.

---

## 3. Sprites

### 3.1 Files and numbering

171 sprite images of 64 bytes (63 data bytes + 1 unused):

| file | load address | sprite pointers | images |
|---|---|---|---|
| `tksprd.prg` | (loaded to a buffer, copied to `$D000`) | 64–127 | 0–63 |
| `tkspre.prg` | `$E000` | 128–234 | 64–170 |

Sprite pointer = 64 + image number. The constants are in `build/gfxdata.h`:

| pointer | images | contents |
|---|---|---|
| 64 (`SPR_PLAYER`) | 48 | player: 16 directions × 3 frames (stand, walk, fire) |
| 112 (`SPR_PDEAD`) | 2 | player dying, dead |
| 114 (`SPR_LOOK_A`) | 32 | enemy look A: 16 directions × (stand, walk) |
| 146 (`SPR_LOOK_B`) | 32 | enemy look B |
| 178 (`SPR_LOOK_C`) | 32 | enemy look C |
| 210 (`SPR_LOOK_D`) | 16 | enemy look D: 16 directions, stand only |
| 226 (`SPR_EXPLO`) | 4 | explosion animation (hi-res) |
| 230 | 1 | grenade (hi-res) |
| 231 | 1 | C4 (hi-res) |
| 232 | 1 | mine (hi-res) |
| 233 | 1 | flame puff (hi-res) |
| 234 | 1 | gib (hi-res, not used by the game at the moment) |

Directions: 0 = facing down, 4 = right, 8 = up, 12 = left (counter-clockwise on
screen, 22.5° per step).

### 3.2 Multicolour actors (player and enemies)

12×21 fat pixels. The colours:

| bits | register | colour | meaning |
|---|---|---|---|
| `00` | – | transparent | |
| `01` | `$D025` (shared) | 0 black | dark: outlines, hair, guns |
| `10` | `$D027+n` (per sprite) | **own colour** | clothes: who this is |
| `11` | `$D026` (shared) | 15 light grey | bright: skin, highlights |

The shared colours are `MC0` and `MC1` in `conv64.c`. The conversion (`mc_sprite`)
works like this:

- Each fat pixel samples a 4×4 area of the 28×28 source frame. It stays transparent
  if less than half of the area is covered.
- Brightness < 60 → black. > 150 → light grey. In between → own colour.
- Player only: source colours 80–95 (the clothes in the original palette) always
  become the own colour.

**Own colours:**

| who | colour | set in |
|---|---|---|
| player 1 | 10 light red | `player_col` in `src/game.c` |
| player 2 | 14 light blue | `player_col` in `src/game.c` |
| player hit | flashes 1 white | `game.c` |
| enemy firing | flashes 1 white | `game.c` |
| enemy types 0–7 | 5, 9, 14, 12, 8, 3, 4, 7 | `enemy_col` in `conv64.c` |

**Source frames** (`RAMBO2.EFP` for the player, `ENEMYn.EFP` for enemies; 16 of the
40 directions in the sheets are used):

| | stand | walk | fire | dead |
|---|---|---|---|---|
| player (`RAMBO2.EFP`) | anim 4 | anim 3 | anim 6 | anims 9, 10 |
| enemies | anim 1 | anim 0 | – | – |

**Enemy looks:** there is sprite memory for only 4 enemy looks, so types share them
and differ by colour:

| look | drawn from | used by enemy types |
|---|---|---|
| A | `ENEMY0.EFP` | 0 pistol boy (green), 2 uzi rebel (light blue), 3 commando (grey) |
| B | `ENEMY1.EFP` | 1 shotgun maniac (brown), 4 grenade guy (orange), 6 punisher (purple) |
| C | `ENEMY7.EFP` | 7 flamer (yellow) |
| D | `ENEMY5.EFP` | 5 civilian (cyan), standing frame only |

The look mapping is `look_type` / `enemy_look` in `conv64.c`.

### 3.3 Hi-res effect sprites

One colour + transparent, 24×21 full resolution (`hr_sprite`). Source pixels darker
than the given minimum brightness are left out.

| sprite | source | colour in the game |
|---|---|---|
| explosion | `EFFECTS.EFP` explosion frames 0, 2, 4, 6 | 7 yellow |
| flame puff | explosion frame 3, smaller | 8 orange |
| grenade, C4, mine | `BOMB.EFP`, `C4.EFP`, `MINE.EFP` | 1 white |

The colours used in the game are set in `game_sprites()` in `src/game.c`.

### 3.4 Sprites on screen

Each view has the 8 hardware sprites: own player, the other player, up to 4 enemies,
one grenade/mine/C4, one explosion. (In one-player mode, 5 enemies.) Sprites 0–5 are
multicolour and 6–7 are hi-res. A ninth thing is not shown, which is why effects
are decals.

---

## 4. HUD and menus

Text uses the C64 ROM upper case font (chars 0–63) in hi-res, one colour per char from
colour RAM. Only colours 0–7 work there. The HUD colours are the `HC_*` defines
in `src/main.c` (yellow weapon, cyan ammo, red warnings, green energy, blue for
dimmed). The map tiles cannot be used in the HUD rows, and there is no custom font.

---

## 5. Looking at the result

- `build/preview.ppm`: written by every conversion. At the top it shows all metatiles
  (20 per row, fat pixels doubled), and below them the sprites (13 per row) in the
  real C64 colours. Grey 11 is the transparent background. It is 320×400, so only the
  first ~130 sprites fit. Convert it to PNG with
  `magick build/preview.ppm preview.png`.
- `make run` plays the game in VICE. `make test` saves a screenshot `build/test.png`
  (scripted play), and `make test-shop` shows the shop screen.

---

## 6. Recipes for fine tuning

`make` converts again when the converter, the linuxTK loaders or any `gfx/*.png`
changed. Converter changes only show in the sheets that are generated, so for a
sheet you have not edited, delete it to see the new conversion.

| I want to… | do this |
|---|---|
| change the map colours | `make CONV64_PAL=bg,d022,d023` (e.g. `0,9,8` brown and orange) |
| change the player colours | `player_col` in `src/game.c` |
| change an enemy type's colour | `enemy_col` in `write_gfxdata`, `conv64.c` |
| change the shared sprite black/grey | `MC0`, `MC1` in `conv64.c` (also changes `$D025/$D026` in the game) |
| show more or less own colour on actors | the brightness limits 60 and 150 in `mc_sprite` |
| pick other animation frames | the anim numbers in `build_sprites` |
| make other enemy types share a look | `look_type` and the `enemy_look` mapping in `conv64.c` |
| redraw blood, gibs or scorch | edit `gfx/decals.png` |
| redraw anything else | edit its sheet in `gfx/` (section 7) |
| keep more tile detail | edit the tile in `gfx/tiles_*.png`: hand edited tiles keep their chars in the 160 char merge |
| change explosion and effect colours | `game_sprites()` in `src/game.c` |

---

## 7. Editing graphics: the gfx/ PNG sheets

Every `make` that converts the graphics goes through the sheets in `tk64/gfx/`
(directory: `make CONV64_GFX=otherdir`):

- **The PNG is valid** (it reads and has the right size): it is **used** for the game.
- **It is missing:** it is generated from the original files and saved.
- **It is broken or the wrong size:** it is renamed to `<name>.png.bad` (your work is
  not lost) and a new one is generated.

The converter prints a `gfx:` line for each sheet saying which happened. To return
one sheet to the automatic conversion, delete it and run `make`.

### 7.1 The sheets

All sheets are in **real C64 colours at screen resolution**: what you see is what the
C64 shows. Cells are packed with no gaps.

| file | size | contents | type |
|---|---|---|---|
| `tiles_floor.png` | 256×160 | 16×10 floor tiles of 16×16, in the order of the original `FLOOR1.EFP` | map |
| `tiles_walls.png` | 256×160 | 16×10 wall tiles, order of `WALLS1.EFP` | map |
| `decals.png` | 64×16 | blood, more blood, gibs, scorch | map |
| `crates.png` | 48×16 | weapon, ammo, energy crate | map |
| `player.png` | 384×63 | 16 directions (columns) × stand, walk, fire (rows) | multicolour sprite |
| `player_dead.png` | 48×21 | dying, dead | multicolour sprite |
| `enemy_a.png` … `enemy_c.png` | 384×42 | 16 directions × stand, walk | multicolour sprite |
| `enemy_d.png` | 384×21 | 16 directions, stand (civilian) | multicolour sprite |
| `explosion.png` | 96×21 | 4 frames | hi-res sprite |
| `items.png` | 120×21 | grenade, C4, mine, flame puff, gib | hi-res sprite |

Directions in columns: 0 = facing down, then counter-clockwise (4 = right, 8 = up,
12 = left).

### 7.2 Drawing rules

The PNG may contain any colours. Each pixel is snapped to the **nearest allowed C64
colour**, so draw with the C64 palette (table in section 1) to get what you expect.

- **Map sheets** (tiles, decals, crates): pixels are 2 wide. Only the **left pixel of
  each pair** is read, so draw in pairs. Allowed colours are the 4 map colours
  (default black, dark grey 11, light grey 15, red 2). Transparent counts as background.
- **Multicolour sprites:** also 2 pixel wide pixels, left one read. Transparent
  (alpha < 50 %) = see-through. Allowed: black (`$D025`), light grey (`$D026`), and
  the **own colour**. The own colour is drawn as player = light red, enemy A = green,
  B = brown, C = yellow, D = cyan. In the game, it becomes the colour of each player
  or enemy type (section 3.2), so any colour that is nearest to the own colour means
  "own colour".
- **Hi-res sprites:** single pixels, any non-transparent pixel is drawn. The colour
  is set by the game (section 3.3).
- Each sprite is 24×21. Keep actors inside the cell the same way as the existing
  frames: the game places the cell, not the figure.

### 7.3 Tiles: hand edited vs. converted

`gfx/generated/` holds the tile sheets as the converter made them last time
(do not edit these). A tile in `tiles_*.png` that **differs from that reference
is hand edited**:

- It is used exactly as drawn, and its chars are **protected** in the 160 char merge
  (section 2.2). Other tiles may be merged into its chars, but not the other way
  around. If hand edited tiles alone need more than 160 different chars, conversion
  stops with an error.
- Tiles you did not touch follow the converter. If the conversion changes (another
  `CONV64_PAL`, converter improvements), they update and the sheet is re-saved to
  show what the game uses.

Only tiles that the levels use reach the game. Unused tiles in the sheet are shown
for reference.

### 7.4 Changing the map colours

`gfx/map_colours.txt` records which map colours the map sheets were saved with.
After `make CONV64_PAL=...` the sheets are read by their old colours (a pixel that
was light grey stays in the light grey slot) and re-saved in the new colours. So
your edits keep their meaning: slot `$D023` stays `$D023`, whatever colour it now
has.

### 7.5 Raw C64 files (for C64 tools)

The game files in `build/` are plain C64 data with a 2-byte load address:

- **`tkchr.prg`**: `$C800`, 256 chars × 8 bytes; each byte = 4 fat pixels, 2 bits
  each, leftmost in bits 7–6 (0 = background, 1 = `$D022`, 2 = `$D023`, 3 = red).
- **`tksprd.prg`** (`$2000`, images 0–63) and **`tkspre.prg`** (`$E000`, images
  64–170): 64 bytes per sprite, 21 rows × 3 bytes + 1 unused. Multicolour 2 bits per
  pixel (`01` black, `10` own, `11` light grey), hi-res 1 bit (bit 7 = leftmost).

They are overwritten on every conversion, so make lasting changes in `gfx/`. The raw
files are useful to look at the result in tools like CharPad or SpritePad.
