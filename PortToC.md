# PortToC — Ultimate Tapan Kaikki → C + raylib (Linux)

## 1. Goal

Port the DOS game (Watcom C++ / DOS4GW / MIDAS / VESA / IPX) to **plain C** with
**raylib** as the only platform dependency on Linux.

Design constraints, in order of priority:

1. **Gameplay first**: walk, rotate, shoot, enemies with AI, crates and pickups,
   level progression, time limit, death, **2-player split screen**.
2. **Gore stays**: blood splats and blood trails, flying and kickable **body parts**, and
   **footprints** are part of the game's feel and are kept.
3. **Fixed 320x200, 8-bit indexed framebuffer.** The game renders into
   `u8 fb[320*200]` plus a 256-entry palette. raylib only *presents* it.
4. **Minimal code.** Visual polish that isn't in the list above is removed, not ported.
5. **Retro-portable core, 32-bit only.** The game core must compile with an old C89 compiler
   (Open Watcom/DOS4GW, DJGPP, vbcc, SAS/C, GCC 2.x) on **32-bit flat-memory** machines,
   little- or big-endian (386+ DOS extender, 68020+/68000 Amiga & Atari, PowerPC, …).
   **16-bit / segmented targets are out of scope**: no `near`/`far`/`huge` pointers, no 64 KB
   array limit, every buffer is one continuous array.
   All OS/hardware code is kept inside one small platform file (~200 lines), so
   porting to an old machine means rewriting only that file.
6. **Global variables and static memory only.** Game state lives in global arrays with
   compile-time sizes; the core never calls `malloc`/`free`. The whole memory footprint is
   known at link time (see §4.5).

Non-goals **for now**: network play (dropped at first; see §8 Phase 6 for how to keep
the door open), music, 320x240 VESA, dynamic lighting, wall shadows, dark mode, fades,
editor, setup.

---

## 2. Current code inventory (SRC/, ~9.5k lines)

| File | Lines | What it is | Port decision |
|---|---:|---|---|
| `GAME.CPP` | 2616 | main, init, game loop, menus, **~1200 lines of IPX net code** | Split; drop net code for now |
| `CLASSES.CPP/.H` | 1436+262 | Player, Enemy, Bullet, Crate, Body_part, Effect, Level::load | **Core** – port to structs + functions |
| `DRAW.CPP/.H` | 976 | sprite/tile blitters, HUD, map, lights, shadows, dark mode | Keep blitters/HUD/effects/body parts; drop lights/shadows/dark |
| `OPTIONS.CPP` | 768 | options menus, episode/level file picker, cfg save | Replace with tiny menu (~150 lines) |
| `MISCFUNC.CPP` | 541 | key setup, player init, **bullet/weapon/effect tables**, `explo()`, dir listing | Keep tables + init; drop key-config UI, DOS calls |
| `GLOBVAR.CPP/.H` | 424 | all globals, weapon/enemy data tables, sprite buffers, `dust_blocks` | Keep data tables; shrink buffers |
| `SHOP.H` | 240 | between-level shop | Phase 5 (optional, simplified) |
| `I_FUNCS.H`, `FUNCS.H` | 220 | level info / stats screens (1 and 2 player), quit dialog | Simplified text screens |
| `WRITE.CPP` | 190 | bitmap font loader & text writer | Keep, one font |
| `NEWFUNCS.CPP` | 91 | spawn bullet / effect / body part | Keep |
| `ROUT.CPP` | 98 | dist, angle, sin/cos tables, time string | Keep (replace `int386`) |
| `FADE.CPP`, `RGBHSL.C` | 140 | palette fades | **Drop** |
| `EFP/EFP.CPP` | 48 | RLE image + palette loader | Keep, extended with load-time conversion (§5) |
| `IPX/`, `INT/`, `IPXDEFS.*`, `CHAT.CXX` | ~450 | IPX networking, DPMI low-mem asm | **Drop** |
| `MAKET/`, `SETUP/`, `TESTI/` | ~550 | palette-table generator, sound setup, tests | **Drop** |
| `MIDAS.LIB`, `<vesa*.h>`, `<keyb.hpp>`, `<image.hpp>` | – | 3rd-party DOS libs | Replaced by platform layer |

`BAK/` is an older copy of `SRC/` – ignore it.

### Assets used

| Asset | Format | Keep? |
|---|---|---|
| `EFPS/*.EFP` | "EF pic" header, u16 w, u16 h, RLE (byte >192 = run), 768-byte 6-bit palette at file end | Yes (subset, see §6) |
| `LEVS/<episode>/LEVELn.LEV` | little-endian int32 stream, see §5 | Yes |
| `FNTS/8X8.FNT` | byte xs, byte ys, glyph data starts at offset 512: 256 glyphs of xs*ys bytes | Yes (one font) |
| `WAVS/*.WAV` | 8-bit PCM | Optional (Phase 5) |
| `MUSIC/*.S3M` | modules | No |
| `PALETTE.TAB` | 256x256 translucency (64 KB) + shadow + light tables | **Read once at load** to build a small translucency table (§5.3), then freed |
| `KEYS.TAB`, `OPTIONS.CFG` | key names, saved options | No (hard-coded defaults) |

---

## 3. What is kept and what is removed

### 3.1 Kept

**Gameplay:** player movement (rotate/strafe/forward/back) with wall collision,
11 weapons + 9 bullet types (incl. explosive & incendiary/burning), enemy types 0–7
with seeing/rotating/shooting AI, crates (weapon/ammo/energy), push-back from hits,
explosions damaging area, energy & death, time limit + time bonus, level → next level
→ victory, cash, map overview (SPACE), HUD (energy, ammo, weapon), message board.

**Split screen (must have):** 2 local players, 160x200 view each, both cooperative
episode play and the shared stats screen. `two_to_one()` (continue with one player when
the other dies) is kept.

**Gore / effect system:** the whole `Effect` system (`new_effect`, `Effect::roll`,
`draw_effects`) is kept, minus its light calls. That covers:

| Effect | Draw mode | Spawned by |
|---|---|---|
| `BLOOD` (flying drop) | opaque | hits on players/enemies, bullets (`CLASSES.CPP:912,967,1110`) |
| `BLOOD2` (floor splat, long-lived) | **translucent** | blood drop landing (`Effect::roll`), body-part trail, wounded walking |
| `FOOTPRINT` (only on `dust_blocks` floor tiles) | **translucent** | `Player::footprint`, `Enemy::footprint` |
| `SMOKE`, `SMOKE2`, `SMOKE3` | opaque / translucent | gun smoke, burning |
| `METAL` (sparks) | opaque | `explo()` |
| `EXPLO` | **originally light-only** – see below | explosions |
| `TELEPORT` | opaque | spawn |

`Body_part` is kept fully: `new_body_part`, `Body_part::move` (slides, wall collision,
leaves `BLOOD2` trail), `draw_body_parts`, `Player::kick_body_parts`.

> Note: `EXPLO` is drawn **only** through `draw_light` in the original (`DRAW.CPP:626`), so
> dropping lights would make explosions invisible. The port draws the `EXPLO` frames from
> `EFFECTS.EFP` (25x25, 9 frames at offset `320*13`) as a normal sprite instead.

### 3.2 Removed

| Feature | Where | Why removing is safe |
|---|---|---|
| IPX network game (8 players), server/client | `GAME.CPP` 1031–2200, `IPX/`, `INT/` | Not needed for first version; largest single chunk of code |
| Deathmatch mode, crate wake-up, kill board | `chk_wake_ups`, `draw_kills`, `death_match_*` | Only meaningful with network/DM; co-op is kept (Phase 6 candidate) |
| MIDAS music + 4-channel SFX | `menu_biisi`, `peli_biisi`, `MIDASplaySample` | Replace with `plat_sound(id)` stub (no-op at first) |
| VESA 2 320x240 mode | `vbeOpen`, `scr_y_size` | Always 200; `scr_y_size` becomes `SCR_H` = 200 |
| Palette fade in/out | `FADE.CPP`, `RGBHSL.C` | Instant palette switch |
| Dynamic lights, spot lights, muzzle flash lights | `draw_light`, `draw_spots`, `fire_lights`, `Light`, `Light_table` | Pure visuals; only `EXPLO` needs a replacement (above) |
| Wall shadows & sprite shadows | `draw_shadows`, `draw_gae_shadows`, `BLOCK.shadow` | Visual |
| Dark mode / line-of-sight fog | `seeing`, `draw_black`, `see_level`, `floorspr2`, `make_floors` | Option, not core; saves 64 KB + a per-frame LOS pass |
| Steam vents | `do_steam`, `Steam` | Visual (the `STEAM` effect type stays defined but is never spawned) |
| Floating crate pickup texts | `Crate::move_text`, `draw_crate_texts` | Replaced with one message-board line |
| Screen capture (F12, PCX) | `screen_capture` | Not needed |
| FPS / waste-time counter | `frames_per_sec`, `OVER_POWER` | Debug only |
| Keyboard redefinition UI, name editing | `Keys::change`, `change_name`, `readline` | Hard-coded keys and names |
| Logo, credits, animated menu balls | `logo`, `credits`, `draw_ball` | Cosmetic |

Expected size: **~3500–4000 lines of C** for the core, **~200 lines** for the raylib backend.

---

## 4. Target architecture

```
port/
  Makefile               # gcc -std=c89 -pedantic -Wall ; links raylib only in plat_raylib.c
  src/
    plat.h               # THE ONLY interface to the outside world
    plat_raylib.c        # Linux/raylib backend (~200 lines)
    types.h              # u8/s16/s32 typedefs, fixed-point macros, constants (from DEFINES.H)
    globals.c / globals.h# ALL game state and asset buffers, statically sized (§4.5)
    data.c / data.h      # const tables: weapon[], bullet_type[], enemy_info[], effect_type[], dust_blocks[]
    assets.c             # EFP/font/level loaders + load-time pixel conversion (§5)
    gfx.c                # fb, palette, View, blit_tile, blit_sprite, blit_trans, blit_recolor, text, rect
    level.c              # level grid, collision queries, crate placement, enemy placement
    player.c             # player input → movement, shooting, damage, weapon switch, footprints, kicking
    enemy.c              # enemy AI: see, rotate, move, shoot, damage, death gore
    bullet.c             # bullet move, hit tests, explosions
    effect.c             # effects (blood, splats, footprints, smoke, sparks, explosion) + body parts
    render.c             # draws one View: floor, gore, crates, bullets, actors, walls, effects, HUD
    game.c               # state machine, fixed-timestep loop, screens, menu, split-screen setup
    main.c               # argv parsing, plat_init, run game
```

### 4.1 Platform layer (`plat.h`)

Keep it tiny so a DOS, Amiga, Atari ST or SDL1 backend is a one-file job:

```c
int   plat_init(void);                         /* window / video mode 320x200 */
void  plat_shutdown(void);
void  plat_set_palette(const u8 *pal768);      /* 6-bit VGA values 0..63 */
void  plat_present(const u8 *fb);              /* 320x200 indexed -> screen */
void  plat_poll(void);                         /* pump events */
u16   plat_buttons(int player);                /* player 0 or 1, bitmask below */
int   plat_key_pressed(void);                  /* for menus: last key or 0 */
u32   plat_ticks_ms(void);
int   plat_quit_requested(void);
void  plat_sound(int id);                      /* may be a no-op */
```

Input is an **action bitmask per player**, not scancodes:
`BTN_LEFT, BTN_RIGHT, BTN_UP, BTN_DOWN, BTN_FIRE, BTN_STRAFE, BTN_SLEFT, BTN_SRIGHT,
BTN_NEXTWPN`, plus global `BTN_MAP, BTN_ESC, BTN_ENTER` on player 0.
This replaces `KeyboardHandler k`, `k.state[]`, `KEYS.TAB` and all key-config code.
Split screen needs both players on one keyboard, so the backend has two fixed key sets.

### 4.2 raylib backend

- `InitWindow(960, 600, ...)`. Keep a 320x200 RGBA `Texture2D`.
- `plat_set_palette` builds `u32 rgba[256]` (`v<<2 | v>>4` for 6→8 bit).
  `plat_present` converts each index through it, `UpdateTexture`, `DrawTexturePro` with
  integer scale + letterbox.
- Default keys (same spirit as `OPTIONS.CFG` defaults):
  - Player 1: arrows rotate/move, Right Ctrl fire, Right Shift strafe, `,` `.` side-step, Enter next weapon.
  - Player 2: `W A S D`, Left Ctrl fire, Left Shift strafe, `Q` `E` side-step, Tab next weapon.
  - Space map, Esc quit.
- `plat_sound`: Phase 5, `LoadSound("WAVS/PISTOL.WAV")` + `PlaySound`.

### 4.3 Split screen: the `View` struct

The original passes `mode` 0/1/2 to every blitter and duplicates every draw loop for the
second player (e.g. `draw_effects` is three copies of the same code). The port replaces
that with a global view table, a global "current view" and **one** render function:

```c
typedef struct {
    s16 x0, x1;          /* screen columns this view may write: [0,320) or [0,160) / [161,320) */
    s32 cam_x, cam_y;    /* world position of the view's top-left corner (player.scr_x/scr_y) */
    s16 owner;           /* index into player[] for HUD and camera follow */
} View;

View view[2];            /* globals */
s16  num_views;          /* 1 or 2 */
s16  clip_x0, clip_x1;   /* copied from the current view, read by the blitters */
s32  cam_x, cam_y;       /* copied from the current view */

void render_view(int i); /* sets clip/cam globals, draws floor, splats, crates, ... HUD */
```

- Single player: one view `{0, 320}`. Split: `{0, 160}` and `{161, 320}`; column 160 is the divider.
- The camera clamp in `Player::move_scr` uses the view width (320 or 160) instead of
  hard-coded numbers.
- Blitters take only position + frame (`blit(x, y, &frames, i)`) and clip x to the global
  `[clip_x0, clip_x1)` and y to `[0, SCR_H)`. This is the only clipping code in the game, and
  it keeps blitter argument lists short (cheap calls on old CPUs).
- HUD positions (energy bar, ammo, weapon) are relative to `clip_x0`.
- The stats screen (`statics()`) draws the second column at `x + 160` when 2 players.

### 4.4 Coding rules for the core (retro portability)

- **C89**: declarations at block start, `/* */` comments, no VLAs, no `stdint.h`
  (own `types.h` with `u8 s16 s32` typedefs).
- **32-bit flat memory is assumed**: pointers and `int` are at least 32 bits, any array can be
  as large as needed and is indexed with plain `int`/`s32`. `types.h` has a compile-time
  check (`typedef char assert_int32[sizeof(int) >= 4 ? 1 : -1];`) so a 16-bit build fails
  immediately instead of misbehaving.
- **Fixed-width types where the size matters**: `s32` for anything read from files, `u8` for
  pixels; `s16` only to save space in large tables. Plain `int` is fine for loop counters
  and local math.
- **Continuous arrays only**: each asset set is one array (all player frames in one
  `player_pix[]`, all tiles in one `tile_pix[]`, framebuffer in one `fb[64000]`). Frame `i`
  is always `base + i*w*h`; no banking, splitting into chunks or segment juggling.
- **Pixels are `u8` from the moment they are loaded** (see §5.1). No `char` pixel buffers
  anywhere in the core.
- **No struct I/O.** Files are read with `read_le16()/read_le32()`; never `fread(&struct)`.
- **No dynamic memory at all** in the core — see §4.5.
- **Float → fixed point** (Phase 7): positions/speeds as 24.8 or 16.16 `s32`,
  `sini[]/cosi[]` as scaled `s16` tables. Keep float in earlier phases to match the original
  feel, but put all math behind macros (`FX_MUL`, `FX_FROM_INT`) so the switch is mechanical.
- **Exact-case paths in one table.** Disk files are UPPERCASE (`EFPS/COOL.EFP`) while the
  source uses `"efps\\cool.efp"`. All paths live in one table in `assets.c`.
- **No directory listing.** Replace `_dos_findfirst` with a fixed episode list
  (`DEFAULT, ULTIMATE, ...`) and probe `LEVEL1.LEV, LEVEL2.LEV …` with `fopen` until one is missing.

### 4.5 Globals and static memory

The original already keeps almost everything in globals (`GLOBVAR.CPP`: `player[]`,
`enemy[]`, `bullet[]`, `effect[]`, `body_part[]`, `crate[]`, sprite buffers). The port keeps
that style and removes the few heap allocations (`level` grid, `see_level`, font buffers).

Rules:

- **Every buffer is a global (or file-`static`) array with a `#define`d size**, declared in
  `globals.c`, `extern` in `globals.h`. No `malloc`, `calloc`, `realloc`, `free` in `src/`
  (the raylib backend may allocate internally; that is outside the core).
- **Entities are fixed pools with an `in_use` flag**, as in the original:
  `Player player[MAX_PLAYERS]`, `Enemy enemy[MAX_ENEMIES]`, `Bullet bullet[MAX_BULLETS]`,
  `Effect effect[MAX_EFFECTS]`, `Body_part body_part[MAX_BODY_PARTS]`, `Crate crate[MAX_CRATES]`.
  Spawn = find free slot (or oldest, for effects). All pool sizes live in `types.h` so a
  small machine can shrink them in one place.
- **Level grid is static at the maximum size**: `u8 level_tile[MAX_LEVEL_W * MAX_LEVEL_H]`
  with `MAX_LEVEL_W = MAX_LEVEL_H = 100` (largest shipped level is 99x99, `SUNRISE/LEVEL10`).
  The loader rejects bigger levels with an error message. 10 KB total.
- **Asset buffers are static, continuous and sized per sheet** (`u8 player_pix[PLAYER_FRAMES*28*28]`
  ≈ 721 KB as a single array, `u8 tile_pix[160*400]`, …). Sizes are macros derived from frame
  counts and `ANGLES`.
- **No temporary full-sheet buffer while loading**: the EFP decoder streams pixels straight
  into the final frame arrays (§5.2), and the translucency rows are read straight from
  `PALETTE.TAB` with `fseek` (§5.3). Only small locals (`u8 line[320]`) on the stack.
- **Enemy sprites:** `ENEMY_SLOTS` static slot buffers (8 on Linux = all types loaded once;
  e.g. 3 on a small machine, refilled per level from the level's enemy list).
- **Mutable game state in globals, constant data in `const` tables** (`data.c`) so a ROM/
  read-only segment can hold them on old targets.
- **Functions work on globals and take indices**, e.g. `player_move(int pi, int angle, fx spd)`
  uses `player[pi]`. Inside a function a local `Player *p = &player[pi];` is fine for brevity.
  Cross references between entities are indices (`s16 frying_player`), not pointers, so the
  whole state can be saved, compared or (later) synchronised over the network as plain data.
- **Footprint is printed at link time**: Makefile runs `size` on the core object files; the
  total of `.bss + .data` is the game's memory use. Keep it in the README of the port.

### 4.6 C++ → C conversion pattern

Usage of C++ is shallow (classes with methods, `cout`, no templates/inheritance/new/delete):

- `class Player { void move(int,float,int); }` → `typedef struct {...} Player;` +
  `void player_move(int pi, int angle, fx spd);` operating on global `player[pi]`.
- implicit members → `p->` via a local `Player *p = &player[pi];`.
- `Player *Frying_player`, `Bullet.P` → `s16` player index (`-1` = none).
- `cout << ...` → `printf` (only in init/error paths; most disappear).
- Remove every `GAME_MODE == NETWORK` / `NETWORK_MODE` branch and `sendipx(...)` call, keeping
  the local (server) path. Keep `GAME_MODE` with only `ONE_PLAYER` and `SPLIT_SCREEN`.

---

## 5. Load-time conversion (signed pixels → render-ready data)

The original keeps raw decoded EFP bytes in `char` arrays and does the interpretation at draw
time: `char colo; if (colo != 0) ...`, `if (colo >= 80 && colo <= 95) recolor`,
`trans_table[colo][*dest]`. With signed `char` (GCC/x86 default) colours ≥128 are negative,
so the recolor test and the table index break. The port fixes this **once, in the loader**,
and the blitters only see clean data.

### 5.1 Pixel format

- `load_efp()` decodes RLE straight into a `u8` buffer (`fgetc` already returns 0..255;
  store as `u8`, never via `char`/`short` casts).
- Palette stored as `u8 pal[768]` (6-bit values 0..63).
- Index `0` = transparent for all sprites (same as original). Tiles have no transparency.
- Nothing downstream ever needs to know the signedness of `char` on the platform.

### 5.2 Sprite sheets → packed frame arrays

Instead of keeping full 1160-wide sheets and computing `offs = (num*29+1)*1160 + angle*29 + 1`
at draw time, the loader **cuts each sheet into contiguous frames**:

```c
typedef struct { u8 w, h; u8 *pix; } Frames;   /* frame i = pix + i*w*h; pix points into a static array */

u8     player_pix[PLAYER_ANIMS * ANGLES * 28 * 28];   /* globals.c */
Frames player_frames = { 28, 28, player_pix };
```

**Streaming cut (no scratch buffer):** the EFP decoder emits pixels in order with a running
`(sx, sy)` sheet position. A per-sheet *router* turns that into
`cell = (sy / cell_h) * cols + sx / cell_w`, `(cx, cy)` inside the cell; border pixels and
unwanted angles are skipped, the rest is written to `pix[frame*w*h + cy*w + cx]`.
So the 774 KB `RAMBO2.EFP` never exists in memory as a sheet — only the packed frames do.
The same decoder with a "copy whole image" router loads full-screen pictures
(`COOL.EFP`, `TK.EFP`) into the single static `u8 picture[64000]` used by menus/screens.

| Sheet | Source layout | Packed as |
|---|---|---|
| `RAMBO2.EFP` 1160x667 | 29x29 cells (1-px border), 40 angles × 23 anim frames | `Frames` 28x28, index `anim*ANGLES + angle` |
| `ENEMY0..7.EFP` 1160x232 | same grid, 40 angles × 8 frames | same |
| `BODYP.EFP` | 21-px cells of 20x20 | 20x20 frames, index = part num |
| `EFFECTS.EFP` 320x82 | per effect: `offs`, `(xs+1)` stride (`define_effects`) | one `Frames` per effect type |
| `CRATES.EFP` | 14x14 cells | 14x14 frames |
| `FLOOR1.EFP` / `WALLS1.EFP` 320x200 | 16x10 tiles of 20x20 | 160 tiles × 400 bytes each |
| `TARGET/C4/MINE/BOMB.EFP` | single 7x7 / 8x8 | 1 frame each |

The loader also:

- **Marks player-recolor pixels.** Rambo pixels 80..95 are the shirt colours replaced by the
  player colour. At load, keep them as-is but the recolor blitter tests `p - 80 < 16u` on a
  `u8`, which is correct by construction. (Optionally remap to 240..255 so the test is `p >= 240`.)
- **Optional `ANGLES` reduction** (40 → 16/8) by picking every Nth column, for memory-tight
  targets (§6). Game logic still uses 0..359 degrees; the blitter maps `angle*ANGLES/360`.

Blitters become three tiny functions with no offset math:
`blit(View*, x, y, const Frames*, i)`, `blit_recolor(..., u8 col)`, `blit_trans(...)`.

### 5.3 Translucency without the 64 KB table

`BLOOD2`, `FOOTPRINT`, `SMOKE2`, `SMOKE3` are drawn with `dest = trans_table[src][dest]`.
Only a handful of distinct `src` colours occur in those sprites. At load:

1. Scan the translucent effect frames and collect the distinct non-zero colours (expect < 16).
2. Remap those frame pixels to small indices `1..K`.
3. Fill the static `u8 trans_lut[MAX_TRANS_COLORS][256]` (`MAX_TRANS_COLORS` = 16) by
   `fseek`-ing to row `src*256` of `PALETTE.TAB` and reading 256 bytes per used colour
   (or, if that file is dropped later, by computing nearest palette colour of the 50/50 mix).
   Too many distinct colours → map the extras to the nearest already-used one.
4. Cost: 4 KB static instead of 64 KB; the 64 KB table is never loaded.

`blit_trans` then does `*d = trans_lut[*s][*d]`.
Fallback for very slow/old targets: `#define TRANS_DITHER` draws translucent pixels only on
a checkerboard (`(x ^ y) & 1`) with the original colour — zero tables, same look at 320x200.

### 5.4 Level file (`.LEV`, versions 1–5)

All little-endian 32-bit, read with `read_le32`:

```
s32 version (<=5)
s32 x_size, y_size
x_size*y_size * { s32 type (0 floor / 1 wall), s32 num (tile index), s32 shadow }
s32 p1_x, p1_y, p2_x, p2_y                            -> both kept (split screen)
s32 spot_count,  spot_count  * {x, y, size}          -> read & discard
s32 steam_count, steam_count * {x, y, angle, speed}  -> read & discard
char comment[20]
s32 time_limit (seconds)
s32 enemies[8]   (version<4: only 7)
weapon/bullet counts by version: v1: 9/7, v2: 10/8, v3+: 11/9
s32 normal.weapon_crates[w], normal.bullet_crates[b], normal.energy_crates
s32 dm.weapon_crates[w],     dm.bullet_crates[b],     dm.energy_crates     -> discard
v5+: s32 n, n*{type1,type2,x,y}  (normal crate positions)
     s32 n, n*{type1,type2,x,y}  (deathmatch crate positions -> discard)
```

Stored in the static global `u8 level_tile[MAX_LEVEL_W*MAX_LEVEL_H]` (100x100), one byte
per cell: bit 7 = wall, bits 0–6 = tile num (160 tiles) — 1 byte instead of 12. `shadow` is
dropped. Row stride is the level's own `level_w`, so only `level_w*level_h` bytes are used.
Crate positions go into the static `crate_info[MAX_CRATE_INFO]`; counts are clamped to the
array size.

### 5.5 EFP header

`"EF pic"`, u16 w, u16 h (little-endian, read with `read_le16`), RLE body,
palette = last 768 bytes of the file.

---

## 6. Memory budget (important for old platforms)

Original static buffers ≈ 3.6 MB: `rambospr` 774 KB, 8 × enemy sheets 2.15 MB,
floor/wall/floor2/picture/ware 5 × 64 KB, `trans_table` 64 KB.

| Item | Original | Port (40 angles) | Port (8 angles) |
|---|---:|---:|---:|
| Player frames 23 × A × 784 B | 774 KB | 721 KB | 144 KB |
| Enemy frames per type 8 × A × 784 B | 269 KB | 251 KB | 50 KB |
| Floor + wall tiles | 128 KB | 128 KB | 128 KB |
| `floorspr2`, `trans_table`, shadow/light tables | ~140 KB | 0 | 0 |
| Translucency LUT | (64 KB) | ~4 KB | ~4 KB |
| Effects, body parts, crates, misc | ~40 KB | ~35 KB | ~35 KB |
| Framebuffer | 64 KB | 64 KB | 64 KB |

- Everything in this table is static (`.bss`); no heap, so the numbers are exact at link time.
- Linux: all 8 enemy types in 8 static slots. Small machine: `ENEMY_SLOTS` (e.g. 3) slots,
  refilled at level start with the types used by that level (`enemies[i] > 0`).
- Effects/body-part pools stay at 300 each (~10 KB of structs); can be lowered with a `#define`.
- Level grid 10 KB, `picture[64000]` for menu/stat screens (can share memory with the
  framebuffer on tight targets, since screens and gameplay never run at the same time).
- Target for a 1 MB machine with 8 angles and ≤ 3 enemy types per level: ≈ 550 KB.
- An offline converter (`tools/pack.c`, host-only) can save the §5.2 packed frames as raw
  `.bin` files so the old machine skips RLE decode and cutting. The in-game loader path stays
  for Linux.

---

## 7. Game loop & timing

Original: MIDAS timer callback sets `framecount` at 40 Hz (`TARGET_FRAMES`), loop runs
`game_shit()` (update + full redraw) once per tick.

```c
#define TICK_MS 25                      /* 40 Hz, all speeds in data tables assume this */
acc += now - last;
while (acc >= TICK_MS) { game_update(); acc -= TICK_MS; }
for (i = 0; i < num_views; i++) render_view(&view[i]);
if (split) draw_divider();
plat_present(fb);
```

`game_update()`: input for each active player, push, burning, bullets, body parts (move +
blood trail), enemies (AI), crates pickup, effects roll, timers, death/level checks.

`render_view()` order (from `game_shit`, minus removed passes):
`floor → order-0 effects (BLOOD2 splats, FOOTPRINT, SMOKE) → dead players → body parts →
crates → bullets → live enemies → live players → walls → order-1 effects (BLOOD, EXPLO,
METAL, smoke) → targets → HUD`. Then (once, full screen) map overlay and message board.

On a slow machine renders drop but game speed stays. `TICK_MS 50` with doubled speeds is the
fallback for very slow CPUs.

---

## 8. Phased work plan

Each phase ends with something runnable.

### Phase 0 — Setup (½ day)
- New `port/` dir; `SRC/` stays untouched as reference.
- Makefile: `cc -std=c89 -pedantic -Wall -Wextra`; core files compiled without raylib headers
  (proves the core has no platform dependency); backend links `-lraylib -lm`.
- `types.h`, `plat.h`, `plat_raylib.c` showing a test pattern with a palette.

### Phase 1 — Assets on screen (1–2 days)
- EFP loader to `u8` + palette; show `TK.EFP` (menu picture).
- Sheet cutting into `Frames` (§5.2) for all used sheets; a debug viewer that steps through
  player/enemy angles and animation frames to confirm the cuts are right.
- Translucency LUT build (§5.3).
- Font loader + text writer (`8X8.FNT`).
- Level loader (§5.4); scroll around `LEVEL1.LEV`.
- ✅ Verify: all levels in all `LEVS/*` episodes load; tiles and sprites match the DOS game.

### Phase 2 — Views, player & combat (2–3 days)
- `View` + `render_view` (§4.3) **with split screen from day one**: two players, two cameras,
  divider. Single player is just the one-view case.
- Port `Player` → `player.c`: `move` (wall collision), `rotate`, `move_scr` (camera),
  `shoot`, `load`, `change_weapon`, `get_damage`, `animate`, `footprint`, `kick_body_parts`,
  player-vs-player push and damage.
- Weapon/bullet tables from `MISCFUNC.CPP:define_bullet_types` and `GLOBVAR.CPP`.
- `Bullet::move`, `new_bullet`, `explo()`.
- Effects (`new_effect`, `roll`, `draw_effects`) with blood, splats, footprints, sparks, smoke,
  sprite-based `EXPLO`. Body parts (`new_body_part`, `Body_part::move`, draw).
- HUD per view: energy, ammo, weapon, target cursor.
- ✅ Verify: two players walk, collide, shoot all weapons; footprints appear on dust tiles;
  blood splats stay on the floor; explosions are visible and hurt.

### Phase 3 — Enemies, crates, level flow (2–3 days)
- Port `Enemy` → `enemy.c`: `try_see_players` (both players), `get_angles`, `rotate`,
  `move`, `chk_for_shoot`, `shoot`, `get_damage` (with body parts + blood bursts), `animate`,
  burning (`chk_burn`).
- `place_enemies`, `place_normal_game_crates`, crate pickup, message board.
- Level state machine: `level_info` → play → `statics` (one or two columns) → next level /
  game over / victory; `two_to_one()` when a split-screen player is dead at level end.
- Time limit (`kello`), map overlay (SPACE).
- ✅ Verify: complete `LEVS/DEFAULT` levels 1–10 in both 1-player and split-screen modes.

### Phase 4 — Menu (½ day)
- Main menu: *1 Player*, *2 Players (split)*, *Episode: < name >*, *Quit*.
- Quit-to-menu dialog on Esc. Menus poll per frame (no busy-wait loops).

### Phase 5 — Optional extras (independent, ~½ day each)
- Shop between levels (`SHOP.H`), per player in split mode.
- Sound effects through `plat_sound` (10 WAVs); in split screen no panning (as original).

### Phase 6 — Network later (not scheduled)
Dropped for now. To keep it possible without re-architecting:
- `game_update()` reads input only from the global `u16 input_buttons[MAX_PLAYERS]`, which
  the main loop fills from `plat_buttons()`; it never reads the keyboard directly.
- All state is already plain global data with index references (§4.5), so it can be sent or
  checksummed as-is.
- All randomness through one `game_rand()` with its own seed.
- With those two rules a later lockstep UDP mode (exchange button masks per tick) is a
  separate module; deathmatch could come back at the same time.

### Phase 7 — Retro hardening (1–2 days)
- Float → fixed point (`fx` type), integer `get_angle` (atan table), integer `get_dist`.
- `tools/pack.c` + `ANGLES` option (§5.2, §6), `TRANS_DITHER` option (§5.3).
- Build the core with an old/strict 32-bit compiler (Open Watcom `wcl386` for DOS4GW or DJGPP,
  `gcc -m32 -std=c89`, and a big-endian 32-bit target such as vbcc/m68k or GCC under qemu)
  to prove portability.
- Second backend as proof, e.g. `plat_dos.c` (mode 13h, int 9 keyboard, PIT timer, ~150 lines).

---

## 9. Risks & gotchas found in the source

- **Signed `char` pixels** (`DRAW.CPP` `char colo`, `GLOBVAR.CPP` `char rambospr[]` …):
  correct on Watcom, wrong on GCC. Solved at load time (§5.1); core uses only `u8` pixels.
- **`int` size**: Watcom `int` = 32 bit; level files store 4-byte ints. Use `s32`.
- **Struct reads**: `fread(level, sizeof(struct BLOCK)*n)`, `fread(crate_info, sizeof(Crate_info)*n)`.
  Replace with field-by-field `read_le32`.
- **Unbounded counts from files**: level size, spot/steam/crate counts are read from the file
  and used directly. With static arrays every count must be checked against its `MAX_*`.
- **Heap uses to remove**: `malloc` of the level grid (`CLASSES.CPP` `Level::load`), of
  `see_level` (dark mode, dropped) and of font data (`WRITE.CPP` `loadfonts`) → static arrays.
- **Large static `.bss` on old 32-bit toolchains**: arrays of several hundred KB are fine in
  flat memory, but some DOS extenders/linkers need the stack and heap options set
  (the original already used DOS4GW with ~3.6 MB of static arrays and `option stack=256000`).
  Check the linker map once per new target.
- **Explosions are light-only** in the original; must be given a sprite (§3.1).
- **Duplicated split-screen code** in every `draw_*` function: don't port it line by line;
  write the single `render_view` instead (§4.3). Check `chk4draw` margins per view width.
- **Effect pool overwrite**: `new_effect` reuses the oldest effect when full; long-lived
  `BLOOD2`/`FOOTPRINT` (delay 20/50) fill the pool fast. Keep that behaviour, it limits decals.
- **`itoa`, `strupr`, `clock()` busy loops, `int386`, `_dos_findfirst`, `putch`, `conio.h`**:
  replace with `sprintf`, own helpers, `plat_ticks_ms`.
- **Path case**: files are UPPERCASE on disk; Linux is case-sensitive.
- **Network branches threaded through gameplay** (`sendipx(...)` inside `chk_burn`,
  `get_damage`, etc.): remove, keep the local/server path.
- **`scr_y_size`** used everywhere for clipping: `#define SCR_H 200`.
- **Behavioural parity**: keep floats and 40 Hz until gameplay matches, then go fixed point.

---

## 10. Definition of done

- `make && ./tk` on Linux with raylib opens a scaled 320x200 window.
- **1-player and 2-player split screen** can start the DEFAULT episode, fight through all
  10 levels with all weapons, pick up crates, die or win, and return to the menu.
- Blood drops, floor splats, body-part trails, kickable body parts and footprints appear as
  in the original; explosions are visible.
- Core (`src/` minus `plat_raylib.c`) builds with `-std=c89 -pedantic` with zero warnings,
  contains no `#include` of raylib, OS or DOS headers, and has no `char` pixel buffers.
- `grep -E 'malloc|calloc|realloc|free\(' src/*.c` finds nothing outside `plat_raylib.c`;
  `size` on the core objects reports the full memory footprint.
- Total core size ≤ ~4000 lines.

---

## 11. Status (linuxTK/)

Phases 0–5 are implemented in `linuxTK/` (see its README):

- C89 core, ~3300 lines, builds with `-std=c89 -pedantic -Wall -Wextra` with no warnings;
  no heap use (`make check`); static footprint printed by `make` (≈ 3.25 MB bss, mostly
  the 40-angle sprite frames).
- raylib backend (`plat_raylib.c`, ~180 lines) and a headless test backend
  (`plat_test.c`) with scripted input that writes frames to PPM.
- Menu, episode selection, level info, single player, split screen, shop, stats,
  victory; all 85 shipped levels load.
- Gore kept: blood drops, translucent splats/trails/footprints (≤ 64-row LUT built from
  `PALETTE.TAB`), body parts with kicking; explosions drawn from `EFFECTS.EFP` frames.
- Sound effects through raylib (no music).

Deviations found while porting:

- The flame thrower's gas bullets were drawn only as a light in the original; the port
  draws them with the EXPLO frames, otherwise flames would be invisible.
- Floating crate pickup texts are replaced by a message board line.
- Level timer counts game ticks (40/s) instead of wall clock, so the quit dialog pauses it.

Not done yet: Phase 6 (network) and Phase 7 (fixed point, asset packer, reduced angles,
second retro backend). A full play-through of an episode has not been verified yet.

### Update — linuxTK 0.2

- Phase 7 fixed point done: no `float`/`double` in the core; 12.4 fixed point positions
  (`s16`), `u8` binary angles with a 256-entry sine table and integer atan2, energy and
  push in tenths, 16-bit xorshift random, distance by `max + 3/8 min`.
- 16 facing directions (changed from 8, which was too coarse): for each 22.5° direction
  the nearest column of the 40-angle sheets is used (≤ 4.5° off); frames are stored
  direction-major. The player turns one step every 2–3 ticks while held (average
  9°/tick = original turn rate); enemies aim with full angles and walk in their
  sprite direction.
- Minimal menus: black screen, 8×8 font only, text shop. Menu pictures, bloody fonts,
  shop icons and their palettes are no longer loaded.
- Core static memory 3.25 MB → 1.37 MB (0.82 MB with 8 directions).
