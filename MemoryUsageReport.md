# Memory Usage Report — linuxTK (C + raylib port)

> **Update — linuxTK 0.2 (fixed point, 16 directions, text menus).** The sections below
> describe version 0.1. After the 0.2 changes the game core needs **1.37 MB** instead of
> 3.25 MB:
>
> | Item | 0.1 (40 dirs) | 0.2 (16 dirs) | Change |
> |---|---:|---:|---|
> | Enemy frames (8 types) | 2,007,040 | 802,816 | 40 → 16 directions |
> | Player frames | 721,280 | 288,512 | 40 → 16 directions |
> | Menu picture `picture[]` + 2 extra palettes | 65,536 | 0 | text menus on black |
> | Fonts | 135,424 | 16,384 | only the 8×8 font |
> | Shop icons | 13,056 | 0 | text shop |
> | Effect frames pool | 32,768 | 21,504 | sized to need |
> | Translucency LUT | 16,384 | 8,192 | 32 rows (28 used) |
> | Object pools | 40,000 | ~17,000 | 16/8-bit struct fields |
> | sin/cos float tables | 2,880 | 289 | 256-byte `s8` sine + 33-byte atan table (const) |
> | **Core `.bss` total** | **3,250,000** | **1,368,152** | **−58 %** |
>
> Sprites scale linearly with the direction count: 8 directions would need 0.82 MB in
> total (actor frames 546 KB), 16 directions need 1.37 MB (actor frames 1.09 MB).
> The core no longer uses floating point at all (checked with `objdump`): positions are
> 12.4 fixed point in `s16`, angles are `u8` (256 per turn), energy/push/speed are
> integers in tenths. The raylib backend is unchanged (259 KB, mostly the RGBA buffer).

Measured on 2026-11-18 from the build in `linuxTK/` (GCC, x86-64, `-O2`).
Sources: `size` and `nm -S` on the object files, `/proc/<pid>/status` and `pmap` of a
running `./tk`, peak RSS of the headless `./tk_test`, and a small probe that loads all
assets and reports how much of each sized buffer is actually used.

## 1. Summary

| Part | Bytes | Share | Notes |
|---|---:|---:|---|
| Game core, static (`.bss` + `.data`) | **3,252,032** | — | All game state and assets; fixed at link time, no heap |
| Game core, code (`.text`) | 56,984 | — | ~3,300 lines of C89 |
| raylib backend, static | 258,964 | — | 250 KB of that is the RGBA conversion buffer |
| raylib internal heap (sounds, GL objects) | ≈ 2–3 MB | — | Estimated, see §4 |
| **Whole `tk` process, resident** | **≈ 177 MB** | — | 150 MB is shared libraries (Mesa/LLVM GPU driver), 27 MB private |
| Headless `tk_test` process, peak RSS | 15.7 MB | — | Same game core, no graphics stack |

**Key point:** the game itself needs about **3.3 MB**, and almost all of it (**2.73 MB, 84 %**)
is pre-rotated actor sprites. The large size of the running Linux process comes from the
OpenGL driver stack loaded by raylib, not from the game.

```
game core 3.25 MB
├── sprite frames  2.91 MB  ████████████████████████████████████████  89 %
│   ├── enemies    2.01 MB  ███████████████████████████▋
│   └── player     0.72 MB  █████████▉
├── screens        0.13 MB  █▊   (framebuffer + picture)
├── fonts          0.13 MB  █▊
├── object pools   0.04 MB  ▌
└── everything else 0.04 MB ▌
```

## 2. Game core static memory (where and why)

All of this is `.bss` (zero-initialised, so it costs nothing in the executable file, which
is only 88 KB). It is touched while loading, so it is all resident at run time.

### 2.1 Sprite and tile frames — 2,908,943 bytes (`assets.c`)

| Buffer | Bytes | Layout | Why it is this big |
|---|---:|---|---|
| `enemy_pix[8][…]` | 2,007,040 | 8 types × 8 anims × 40 angles × 28×28 | Every rotation is a separate pre-drawn image (no runtime rotation). All 8 enemy types are loaded at start-up, though a level uses 1–8 of them |
| `player_pix` | 721,280 | 23 anims × 40 angles × 28×28 | Player has more animations (walk, fire, gun/no gun, wounded, kick, dead) |
| `floor_pix` | 64,000 | 160 tiles × 20×20 | Full floor tile sheet |
| `walls_pix` | 64,000 | 160 tiles × 20×20 | Full wall tile sheet |
| `effect_pix` | 32,768 | pool for 10 effect types | Only **21,337** bytes used; pool is rounded up |
| `ware_pix` | 13,056 | 3 rows × 17 × 16×16 | Shop icons |
| `bodypart_pix` | 6,000 | 15 × 20×20 | Gibs |
| `crate_pix` | 588 | 3 × 14×14 | Crates |
| target / C4 / mine / bomb | 211 | tiny | Projectile sprites |

Why per-angle sprites: rotation is done by picking one of 40 pre-rendered frames
(9° steps), exactly as the DOS game did. This costs memory but no CPU, which is the right
trade on old machines **if** the memory is there. The packed format already removes the
1-pixel cell borders of the original sheets (original buffers were 1160×667 and
8 × 1160×232 = 2.93 MB; packed = 2.73 MB).

### 2.2 Screens — 130,304 bytes (`globals.c`)

| Buffer | Bytes | Why |
|---|---:|---|
| `fb` | 64,000 | The 320×200 8-bit framebuffer every frame is drawn into |
| `picture` | 64,000 | Background picture for menu, level info, stats, shop screens |
| `pal_game`, `pal_ware`, `pal_tmp` | 2,304 | 256-colour palettes (in-game, menus, victory screen) |

`picture` is only used when the game is *not* running a level, so on a tight machine it
could share memory with another buffer.

### 2.3 Fonts — 135,424 bytes (`assets.c`)

| Font | Bytes | Used for |
|---|---:|---|
| `font0_data` BLOODY.FNT 15×15 | 57,600 | Menu titles, selected menu item |
| `font2_data` BLOODY2.FNT 15×15 | 57,600 | Unselected menu items |
| `font1_data` 8X8B.FNT 8×8 | 16,384 | All in-game and info text |
| `font3_data` SNUMBERS.FNT | 3,840 | Shop counters |

All 256 glyphs are stored although only ~40 (lowercase letters, digits and some
punctuation) appear in the game. The two "bloody" menu fonts are 85 % of the font memory.

### 2.4 Colour tables — 16,640 bytes

| Buffer | Bytes | Why |
|---|---:|---|
| `trans_lut[64][256]` | 16,384 | Translucency for blood splats, footprints, smoke. Built from 28 rows of the original 64 KB `PALETTE.TAB` table. **Only 28 of the 64 rows are used** (7,168 bytes) |
| `map_dark[256]` | 256 | Darkened floor colours for the map overlay |

This replaces the original 64 KB `trans_table` plus 4 KB shadow table and 4 × 4 KB light tables (≈ 86 KB).

### 2.5 Game objects (pools) — 40,000 bytes (`globals.c`)

| Pool | Count × size | Bytes | Why the count |
|---|---|---:|---|
| `effect[]` | 300 × 32 | 9,600 | Blood, splats, footprints, smoke; when full the oldest is reused, which caps decals |
| `bullet[]` | 300 × 32 | 9,600 | Explosions alone spawn 5–20 rays at a time |
| `enemy[]` | 100 × 92 | 9,200 | Original limit |
| `body_part[]` | 300 × 24 | 7,200 | Gibs stay on the floor for the whole level |
| `crate[]` | 200 × 24 | 4,800 | Original limit |
| `crate_info[]` | 100 × 16 | 1,600 | Editor-placed crates from v5 levels |
| `player[]` | 2 × 272 | 544 | Two local players |

Sizes are the original DOS limits. They are all `#define`s in `src/types.h`.

### 2.6 Level and the rest — ≈ 15 KB

| Buffer | Bytes | Why |
|---|---:|---|
| `level_tile` | 10,000 | 100×100 tiles, 1 byte each (wall bit + tile number). Biggest shipped level is 99×99. The original used 12 bytes per tile |
| `sini`, `cosi` | 2,880 | 360-entry float sine/cosine tables |
| EFP row buffer `line` | 1,160 | One decoded row of the widest sheet (1160 px) while loading |
| constant tables (`data.c`) | 1,536 | Weapons, bullets, enemies, effects |
| messages, paths, counters | ~1 KB | |

### 2.7 Heap and stack

- The core makes **no** `malloc`/`free` calls (`make check`). The only hidden heap use
  is the C library's `FILE` buffers (≈ 4 KB each, only one file open at a time, freed on
  `fclose`).
- Stack use is small: the biggest locals are ~64-byte text buffers and a 10-entry `Cut`
  array during loading.
- Loading never holds a whole sprite sheet: the RLE decoder writes each row straight
  into the packed frame arrays.

## 3. raylib backend static memory — 258,964 bytes (`plat_raylib.c`)

| Buffer | Bytes | Why |
|---|---:|---|
| `rgba_buf` | 256,000 | Each frame the 8-bit framebuffer is expanded through the palette to 32-bit RGBA before being uploaded to the GPU texture |
| `sounds` | 1,760 | Sound handles (10 sounds × 4 aliases for overlapping playback) |
| `rgba_pal` | 1,024 | Palette as RGBA |

The RGBA buffer exists only because modern GPUs have no 8-bit palette mode. On a retro
backend (VGA mode 13h, Amiga, …) the framebuffer is copied as-is and this cost disappears.

## 4. raylib internal memory (estimated, not exactly measured)

| Item | Estimate | Why |
|---|---:|---|
| Sound effects | ≈ 2.0–2.2 MB | The 10 WAV files are 81 KB in total (8-bit mono, 11–22 kHz, ≈ 5.7 s). raylib converts every sound at load to the device format (32-bit float, stereo, ≈ 44.1–48 kHz) — about **27× larger**. Aliases share the data |
| Screen texture | 256,000 (in GPU/driver memory) | 320×200 RGBA texture |
| Window, GL context, audio device buffers | ~1 MB+ | raylib/GLFW/miniaudio internals |

## 5. Whole process (measured)

`./tk` showing the menu:

| Measure | Value |
|---|---:|
| VmRSS (resident) | 176,748 kB |
| — file-backed (shared libraries) | 149,864 kB |
| — anonymous (private memory) | 26,872 kB |
| VmData | 109,012 kB |

The biggest mappings are Mesa's GPU driver: `libLLVM.so` (~96 MB resident), `libgallium`
(~35 MB), plus ICU data and locale archives. These are shared, read-only libraries that the
OpenGL stack loads; they would be the same for any OpenGL program and are not part of the
game. Of the 27 MB private memory, the game core accounts for ~3.3 MB; the rest is
driver heaps, raylib/GLFW/audio, and the C runtime.

The headless test build (same core, no graphics/audio libraries) peaks at **15.7 MB RSS**,
most of which is the C runtime and the 3.3 MB of game data.

## 6. Unused capacity (easy wins)

| Item | Allocated | Used | Waste |
|---|---:|---:|---:|
| `effect_pix` pool | 32,768 | 21,337 | 11,431 |
| `trans_lut` rows | 64 rows (16,384) | 28 rows (7,168) | 9,216 |
| Enemy sprite types | 8 loaded | 1–8 per level (level 1 uses 2) | up to 1.75 MB |
| Font glyphs | 256 per font | ~40 used | ~110 KB |
| `level_tile` | 100×100 | level size (e.g. 40×40) | varies, max 10 KB |

## 7. How to shrink it for old machines

Ordered by savings. Each item is already a planned option (see `PortToC.md` §5–§6, Phase 7).

| Change | Saves | Result | Cost |
|---|---:|---:|---|
| Rotation angles 40 → 8 (`ANGLES`) | ≈ 2.18 MB | sprites 2.73 MB → 0.55 MB | Coarser turning visuals; gameplay angles unchanged |
| Rotation angles 40 → 16 | ≈ 1.64 MB | sprites → 1.09 MB | Smoother than 8 |
| Load only the level's enemy types into 3 slots (`ENEMY_SLOTS`) | 1.25 MB (40 angles) / 0.25 MB (8 angles) | | Reload sprites at level start |
| Drop BLOODY2 font (use BLOODY with a different colour) | 57,600 | | Slightly different menu look |
| Keep only glyphs 32–127 in fonts | ≈ 70 KB | | Index remap |
| Share `picture` with `fb` (screens draw straight to `fb`) | 64,000 | | Menus redraw the background each frame from the file or keep a copy |
| `trans_lut` to 32 rows, `effect_pix` to 22 KB | ≈ 19 KB | | None |
| Smaller pools (e.g. 150 effects/bullets/parts) | ≈ 13 KB | | Fewer decals at once |
| Dithered translucency (`TRANS_DITHER`) | 7 KB | | Checkerboard instead of true blend |

**Estimated footprint with 8 angles and 3 enemy slots:**
sprites ≈ 0.47 MB (enemies 3 × 50 KB = 0.15 MB, player 0.14 MB, tiles 0.13 MB,
other gfx 0.05 MB); plus screens 0.13 MB, fonts 0.08 MB, tables/pools/level 0.07 MB
→ **≈ 0.75 MB total**, which fits a 1 MB 32-bit machine (e.g. Amiga 1200 with fast RAM,
386 with DOS extender).

## 8. How to reproduce these numbers

```sh
cd linuxTK
make                                   # prints "core memory: text, data, bss"
size build/*.o                         # per module
nm -S --size-sort -t d build/*.o       # per buffer
./tk & grep -E 'VmRSS|RssAnon|RssFile' /proc/$!/status; pmap -x $!
```
