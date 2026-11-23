# c64portplan — Ultimate Tapan Kaikki for the Commodore 64 (cc65)

Source of the port: `../linuxTK` (C89 core). Toolchain found on this machine:
cc65/cl65 **2.18**, ca65/ld65, VICE `x64sc` and `c1541`.

## 1. Feasibility verdict (short)

**Feasible as a "demake", not as a recompile.** The game rules, data tables, level
format, AI and level flow carry over. Almost everything that touches graphics, maths and
memory must be rewritten for the C64:

| linuxTK assumption | C64 reality | Consequence |
|---|---|---|
| 32-bit `int`, `float` everywhere | cc65: 16-bit `int`, **no floating point at all** | All maths → 12.4 fixed point and 8-bit angles |
| 320×200 8-bit framebuffer, software blitting | 1 MHz 6510; a full redraw costs ~10× the frame time | Character mode map + hardware sprites, nothing blitted |
| 1.4 MB static data (16 directions) | ~46 KB for code+data, 16 KB for the VIC bank | 16 directions with only 2–3 frames, shared enemy sprites, level loaded per level |
| 16 directions × 23 frames per actor | 64-byte hardware sprites, 8 on screen | 16 directions × 2–3 frames, sprite colour = player/enemy colour |
| up to 100 enemies, 300 bullets/effects | CPU budget ≈ 39,000 cycles per 25 Hz tick | Only on-screen-ish enemies think every tick; hitscan guns |
| translucent blood, 300 effect sprites | no blending | Gore as **character decals** stamped into the map |

Expected size: ~2,500 lines of C + ~500 lines of 6502 assembly (IRQ, sprites, scrolling,
SID) + a ~700-line Linux-side asset converter that reuses `linuxTK/src/assets.c`.

Main risks: cc65 code size (must stay ≤ ~24 KB) and CPU time (cc65 C is roughly
5–10× slower than hand-written assembly). Both are checked in Phase 0 before
porting gameplay (§9).

## 2. Facts from the shipped data (measured)

| Item | Value | C64 impact |
|---|---|---|
| Levels | 85 in 8 episodes | One level in RAM at a time, loaded from disk |
| Largest level | 99×99 tiles (SUNRISE 10), several 90×90 | 9,801 bytes at 1 byte per tile, the biggest single buffer |
| Median level | 32×32 tiles | 1 KB |
| Distinct tiles used | 91 floor + 99 wall = 190 | Must be reduced to ≈ 48 "metatiles" to fit the charset |
| Enemies per level | median 10, max 71 | Pool of 72, only nearby ones are updated every tick |
| Enemy types per level | max 7 | Enemy *looks* are shared (§5.3) |
| Crates per level | max 350 (random + placed) | Crates live in the tile map as special tiles, not in a pool |

## 3. Target and scope

- **Machine:** C64 PAL (also NTSC, minor timing differences), 64 KB, 1541 drive or
  emulator. No REU or fast loader required. JiffyDOS or VICE warp mode just load faster.
- **Kept:** single player, 2-player split screen (two joysticks), 11 weapons and 9 ammo
  types, 8 enemy types with see/turn/walk/shoot AI, crates, energy, burning, explosions,
  push-back, time limit and bonus, cash and kill rewards, episodes and levels, blood,
  gibs and footprints (as decals).
- **Minimal menus:** a single text screen: `1 PLAYER / 2 PLAYERS / EPISODE <name> / START`,
  a text level intro ("LEVEL 3 – PAY BACK TIME, 5 ENEMIES, TIME 1:06"), a text stats
  screen, and a one-screen text shop (list, FIRE buys, the weapon key sells).
- **Dropped:** music, map overlay (Phase 7 optional), crate wake-up/deathmatch,
  smoke/sparks/steam, translucency, the 40-angle look, shop graphics, the bloody fonts.

## 4. Memory map

```
$0000-$00FF  zero page: cc65 runtime + hot game variables (cc65 zp segment)
$0100-$01FF  6502 stack
$0200-$07FF  KERNAL/BASIC work area, default screen (unused after start)
$0801-$BFFF  PROGRAM: code, rodata, data, bss, cc65 C stack      ~ 46 KB
$C000-$C3FF  screen A (40x25)      \
$C400-$C7FF  screen B (double buf) |  VIC bank 3 ($C000-$FFFF)
$C800-$CFFF  charset (256 chars)   |
$D000-$DFFF  64 sprites  (RAM under I/O, written once at load)
$E000-$FFFF  128 sprites (RAM under KERNAL ROM; VIC reads RAM, CPU writes go to RAM)
```

- BASIC ROM is banked out ($01 = $36), KERNAL stays in for disk loading.
- The sprite area holds **192 sprites (12 KB)** that the CPU never has to read back.
- Colour RAM ($D800) is filled once with one colour (multicolour char mode) and never
  scrolled. Only screen RAM moves, which halves the scroll cost.

### Program area budget ($0801-$BFFF, 46,079 bytes)

| Item | Bytes | Notes |
|---|---:|---|
| Code (C + asm) | ≈ 22,000 | Target; checked at the end of every phase |
| Level tile map | 9,801 | Max 99×99, 1 byte/tile (bit 7 = wall, bits 0–6 = metatile) |
| Metatile table | 256 | 64 metatiles × 4 chars |
| Enemy pool | 72 × 16 = 1,152 | x, y (12.4), dir, type, energy, state, timers |
| Bullets / projectiles | 16 × 8 = 128 | Grenades, C4, mines, flame puffs (guns are hitscan) |
| Effects | 8 × 6 = 48 | Explosion animations (sprites) |
| Players | 2 × 64 = 128 | |
| Constant tables | ≈ 1,500 | Weapons, ammo, enemies, sine/atan tables, texts |
| Text strings | ≈ 1,500 | Menus, names |
| cc65 C stack + runtime | ≈ 2,000 | `--static-locals` keeps the C stack small |
| Disk I/O buffer, misc | ≈ 1,000 | |
| **Total** | **≈ 39,500** | **≈ 6.5 KB headroom** |

## 5. Graphics design

### 5.1 Map: multicolour character mode with 2×2-char metatiles

- One original 20×20 tile becomes a **16×16 metatile = 2×2 characters**. The view in
  single player is 40×22 chars = **20×11 tiles** (the original showed 16×10), so the
  playfield feels the same.
- Multicolour chars: 4×8 fat pixels per char, colours = background `$D021`, `$D022`,
  `$D023` + 1 per-char colour (colour RAM, fixed). Floors use the dark shared colours;
  walls use the light ones.
- **Charset (256 chars):**

| Use | Chars |
|---|---:|
| Metatiles (≤ 48 unique tiles after clustering, chars deduplicated) | ≤ 176 |
| Decals: 4 blood, 2 footprint, 4 gibs, 2 scorch, each on 2 floor bases | 24 |
| Crates (weapon / ammo / energy, 2×2) | 12 |
| HUD font: 0-9, A-Z, `: . - % !` | 40 |
| Spare | 4 |

### 5.2 Scrolling

- **Phase 2 (first version): coarse scrolling** in 8-pixel steps: the camera follows
  in whole characters, and the screen is redrawn into the hidden screen buffer, then
  flipped by changing `$D018`. Cost: 880 char writes per change ≈ 9,000 cycles in a
  tight assembly loop. Simple and robust.
- **Phase 6 (polish): smooth scrolling** with `$D016/$D011` fine scroll and a raster IRQ
  that resets the scroll for the HUD rows.
- **Split screen is top/bottom**, not left/right: the VIC fine scroll and sprite
  registers can change at a raster line but not at a column. Each player gets
  40×11 chars (20×5.5 tiles, wide like the original half-screens turned sideways). A
  raster IRQ at the split line switches screen pointer and sprite set.

### 5.3 Sprites: 16 directions, few frames

Hardware sprites, multicolour (12×21 fat pixels), drawn at ~24×21 screen pixels
(original actors were 28×28). 16 directions match `linuxTK` (22.5° steps).

**The limit is the VIC bank:** the VIC chip only sees one 16 KB bank. After the two
screens and the charset (4 KB) there are **192 sprite slots (12 KB)**. Simply doubling
the 8-direction set (3 frames for everything) would need ~283 sprites (18 KB), so
frames are spent where they show most:

| Sprite set | Frames | Sprites | Bytes |
|---|---|---:|---:|
| Player (shared by both players, colour = sprite colour register; fist and guns use the same look) | 16 dirs × (stand, walk, fire) + 2 dead | 50 | 3,200 |
| Enemy look A "gunman" (pistol boy, uzi rebel, commando) | 16 × (stand, walk) | 32 | 2,048 |
| Enemy look B "heavy" (shotgun maniac, punisher, grenade guy) | 16 × (stand, walk) | 32 | 2,048 |
| Enemy look C "flamer" | 16 × (stand, walk) | 32 | 2,048 |
| Enemy look D "civilian" | 16 × 1 (walking shown by a 1-px bob) | 16 | 1,024 |
| Explosion | 4 | 4 | 256 |
| Grenade / C4 / mine / flame puff / gib | 5 | 5 | 320 |
| Spare (of 192 slots) | | 21 | |
| **Total** | | **171** | **10,944** |

- **Enemy firing** has no own frame: the enemy sprite flashes white (its colour register)
  for 2 frames when it shoots, which costs no memory and reads well at this size.
- The 8 enemy types are told apart by **sprite colour** (each sprite has its own colour
  register), as the original told players apart by recolouring.
- Directions are 0–15 (22.5°). The game angle is kept as 0–255 internally for aiming
  and spread; the sprite uses `((angle + 8) >> 4) & 15` (`DIR_OF` in linuxTK). Sprite
  pointer = set base + `dir * frames + frame`, the same direction-major layout as
  linuxTK.
- **If more frames are wanted later** (e.g. fire frames for enemies): keep the sprite
  sets in program RAM and copy the 8 visible sprites' 64-byte images into slots each
  frame ("sprite streaming", ≈ 4,000 cycles per frame). This frees the VIC bank limit
  but costs program RAM, so it is a Phase 7 option.

### 5.4 Sprite count per screen

- 8 hardware sprites per screen half: 1–2 players, nearest 4 enemies, 1 projectile,
  1 explosion. Each tick a fixed-priority list picks which actors get a sprite
  (players → closest live enemies → newest projectile → explosion).
- Enemies beyond the 4 visible ones still exist and act, they are just not drawn,
  unless a **sprite multiplexer** is added (Phase 6). A simple sort-by-Y multiplexer in
  assembly gives ~16 sprites per screen.
- In split screen the raster IRQ at the split line reloads all 8 sprite positions,
  pointers and colours for the second half, so each half has its own 8.

### 5.5 Gore and effects as decals

The original kept blood, footprints and gibs as up to 600 translucent sprites. On the
C64 they are written **into the tile map** as decal characters:

- Blood splat / bloody footprint: the floor char under the spot is replaced by one of
  the blood-variant chars. Permanent for the level (no fading), costs 0 CPU afterwards.
- Gibs: 1–3 gib chars placed around the death spot (the "flying" phase is a short
  sprite flight in Phase 6).
- Footprints on dust tiles: footprint chars, same mechanism.
- Explosions: 4-frame sprite animation; scorch decal afterwards.
- Since decals live in the char map, the level buffer needs a second layer: decals are
  stored in a small ring buffer of 128 entries (`x, y, char` = 384 bytes) and re-stamped
  when a screen is redrawn. The oldest decal is dropped when full, like the original
  effect pool.

### 5.6 HUD

Bottom 3 text rows (not scrolled): weapon name, ammo, energy bar (chars with partial
fills), time; in split screen a 1-row HUD per player.

## 6. Game logic changes

| Area | linuxTK | C64 version |
|---|---|---|
| Numbers | 12.4 fixed point x, y, energy in tenths (linuxTK 0.2; float in the DOS original) | `x, y` as 16-bit **12.4 fixed point** (0–4095 px with 1/16 px precision; the largest level is 99×16 = 1,584 px), energy as 16-bit integer in tenths (player max 100 + 30 shields × 10 = 400 → 4,000; the 0.4 burn and gas damage become exact integers), push as 8-bit |
| Angles | 0–255 binary angle, 256-byte sine table (linuxTK 0.2) | Same 0–255 angle, sine table as quarter wave (65 bytes), movement for 16 facing directions via the table |
| Rotation | 16 dirs, one 22.5° step per 2–3 ticks at 40 Hz (linuxTK 0.2) | 16 dirs; at 25 Hz one step per 1–2 ticks (alternating, avg. 1.5 ticks ≈ original 360°/s); aim spread still uses 0–255 angles |
| Tick rate | 40 Hz | **25 Hz** (every 2nd PAL frame); all speeds/timers × 1.6 in the data tables |
| Guns | bullets moving 108 px/tick with sub-steps | **Hitscan**: one ray traced in 8-px steps through the tile map and actor boxes at the shot moment; looks the same at these speeds |
| Grenades, C4, mines, flame | slow bullets | Real projectiles (pool of 16), sprite drawn |
| Explosions | 5–20 rays as bullets + EXPLO effects | Direct area damage: every actor within radius `size × 12 px` takes damage and push; 1 explosion sprite + scorch decal |
| Enemy AI | every enemy, every tick, 32-step line of sight | Enemies within ~1.5 screens: move/turn every tick; **line of sight is checked for 2 enemies per tick** (round robin) using a tile DDA; far enemies sleep |
| Collision | 8 wall test points + box vs all actors | 4 wall test points (tile lookups via shift, no divide: 16-px tiles) + box test against nearby actors only |
| Crates | pool of 200 | Crates are map tiles (crate metatiles); taking one turns it back into floor |
| Random | LCG 32-bit | 16-bit xorshift |
| Time | ticks / 40 | ticks / 25 |

Gameplay numbers (costs, ammo, damage, energy, rewards, time limits) are copied from
`linuxTK/src/data.c` unchanged where they fit in 16 bits.

## 7. Input and sound

- **Joysticks:** port 2 = player 1, port 1 = player 2. Up/down walk, left/right turn,
  fire shoots. Strafe is dropped (16-direction movement with turning is enough).
- **Keyboard, both players** (implemented, read straight from the CIA1 matrix):
  player 1 `W S A D`, fire left SHIFT, weapon C=; player 2 `- @ P *`, fire `+`,
  weapon right SHIFT. Chosen by a search over the 8×8 matrix so that no three of the
  12 keys form a rectangle (no ghost keys in any combination) and player 2's keys are
  on lines PA5–PA7, which a port 2 joystick cannot disturb. SHIFT+C= case switching is
  locked ($0291). RUN/STOP only counts when pressed alone (it is a possible ghost of
  S + SHIFT + C=).
- `RUN/STOP` = quit dialog, `F1` = pause.

**Sound (SID, no music):** one small assembly routine with an effect table, voices 1–2
for effects (voice 3 for explosions so they are not cut off):

| Effect | SID setup |
|---|---|
| Pistol / uzi / rifle | noise, fast attack, short decay, pitch per weapon |
| Shotgun | noise, lower pitch, longer decay |
| Grenade launch | pulse, downward sweep |
| Explosion | noise, low pitch, long release, pitch falling |
| Flame | noise, high pitch, sustained while firing |
| Hit / punch | triangle blip |
| Death | sawtooth downward sweep |
| Pickup / menu click | pulse, 2-note arpeggio |

## 8. Asset converter (`tk64/tools/conv64.c`, runs on Linux)

Reuses `linuxTK/src/assets.c` (EFP loader, frame cutter, level loader) to produce
C64 binaries:

1. **Palette:** map the 256 EFP colours to the 16 C64 colours (nearest by perceived RGB).
2. **Tiles → metatiles:** downscale each used 20×20 tile to 8×16 fat pixels, quantise
   to the shared colours, **cluster the 190 used tiles to ≤ 48** (k-means on the
   pixel data, wall/floor kept separate), deduplicate chars, write `charset.bin`
   and `metatiles.bin`. A manual override table fixes bad matches.
3. **Sprites:** for each of the 16 directions take the nearest of the 40 original
   angles (column `round(d × 2.5)`, ≤ 4.5° off — same as linuxTK), pick the frames
   from the table in §5.3, downscale 28×28 → 12×21 multicolour, write `sprites.bin`
   (loaded to $D000/$E000).
4. **Levels:** each `LEVELn.LEV` → `Ennn.Lnn`: header (w, h, start positions, time,
   enemy counts, crate counts, comment), map remapped to metatiles, crates placed by
   the converter as crate tiles, **RLE-compressed** (typ. 1–3 KB).
5. **Disk image:** `c1541` builds `tk64.d64`: `TK64` (program), `GFX` (charset + sprites),
   level files. One 1541 disk holds 664 blocks ≈ 170 KB; 85 levels × ~2 KB = ~170 KB, so
   **all episodes may need two disk sides** — first release ships DEFAULT + 2 episodes.

## 9. Phases

Each phase ends with a `.prg`/`.d64` that runs in VICE (`x64sc -autostart`).

| Phase | Content | Exit check |
|---|---|---|
| **0 Spike (do first)** | cc65 project, custom linker config for the memory map above, char mode + VIC bank 3, one hand-made metatile map scrolling coarse, 8 sprites moving, 1 SID effect. Compile a **dummy of the gameplay loop** (player move + 10 enemies with LOS + 4 hitscan shots per tick) and measure raster time | Tick fits in 2 frames with ≥ 30 % spare; code size of the dummy known |
| 1 Converter | `conv64`: palette, metatiles, sprites, one level → disk image | DEFAULT level 1 visible on C64, recognisable |
| 2 Player | Map draw + coarse scroll, player movement/turning (16 dirs), wall collision, HUD | Walk around level 1 |
| 3 Combat | Weapons table, hitscan, projectiles, explosions, crates, decals (blood, footprints) | Shoot, pick up crates, gore visible |
| 4 Enemies | Enemy pool, AI (sleep/see/turn/walk/shoot), damage, death with gibs, sprite priority list | Clear DEFAULT level 1 |
| 5 Flow | Text menu, level intro, stats/cash, text shop, level loading, game over/victory, SID effects | Play DEFAULT episode start to end |
| 6 Two players | Top/bottom split with raster IRQ, two sprite sets, joystick port 1 | 2-player game through a level |
| 7 Polish (optional) | Smooth scrolling, sprite multiplexer, gib flight, minimap, more episodes on disk 2 | |

## 10. Risks and mitigations

| Risk | Likelihood | Mitigation |
|---|---|---|
| cc65 code too big (> 24 KB) | Medium | `-Os`, global variables (already the linuxTK style), `--static-locals`, `register` zp variables, move hot loops to asm, drop the shop to a fixed upgrade screen if needed |
| Too slow (AI, collisions) | Medium–high | 25 Hz tick, sleeping far enemies, round-robin LOS, hitscan, tile lookups by shifts, zero-page globals; measured in Phase 0 |
| Tile reduction looks bad | Medium | Manual override table; floors can collapse to few variants without hurting gameplay, walls keep shapes |
| Only 4 enemies visible | Certain without multiplexer | Accept in v1; multiplexer in Phase 7. Most fights involve ≤ 4 enemies |
| 99×99 level + code don't fit | Low | 9.8 KB is budgeted; fallback: store map at 4 bits per tile for levels > 64×64 |
| Disk loading slow on real 1541 | Certain | One load per level, RLE keeps it to a few seconds; fast loader out of scope |
| 16 directions use too much sprite memory | Medium | 171 of 192 slots planned; fallbacks in order: civilian shares look A, explosion 3 frames, then sprite streaming (§5.3) |
| Faithfulness | Certain | It is a demake: same rules and levels, simpler visuals |

## 11. Directory layout (planned)

```
tk64/
  c64portplan.md       this file
  Makefile             cl65 build + c1541 disk image + run in VICE
  c64.cfg              ld65 memory config (program $0801-$BFFF, VIC data $C000-$FFFF)
  src/
    main.c game.c player.c enemy.c weapon.c level.c screens.c   (C, cc65)
    data.c data.h types.h globals.h
    vic.s     raster IRQ, screen flip, map draw, sprite update   (ca65)
    sid.s     sound effect player
    joy.s     joystick/keyboard read
  tools/
    conv64.c  asset converter (Linux, links ../linuxTK/src/assets.c)
  build/      generated: charset.bin sprites.bin levels, tk64.prg, tk64.d64
```

## 12. Recommendation

Start with **Phase 0** (about one day). It answers the two open questions — whether
cc65 code is fast enough and whether it fits — with real measurements before
any gameplay is ported. If the spike shows the tick does not fit in two frames, the
fallback is a 16.7 Hz tick (every 3rd frame) or more assembly in collision and AI; if the
code does not fit, the shop and the level intro shrink first.

---

## 13. Status (implementation started 2026-11-18)

Built with `make` → `build/tk64.d64` (DEFAULT episode). Tested headless in VICE
(`make test`, `make test-2p`, `make test-kill`: scripted joystick, screenshot on exit).

| Phase | State |
|---|---|
| 0 Spike | Done. Memory map as planned (`c64.cfg`: program $0801-$BFFF, VIC bank 3), CIA2 timer measures cycles per tick |
| 1 Converter | Done (`tools/conv64.c`). 190 used tiles → 80 floor + 87 wall metatiles via character-level clustering (474 → 160 chars), 4 decal + 3 crate metatiles, 171 sprites in 16 directions, 10 levels (0.7–4.9 KB each, raw) |
| 2 Player | Done. 16-direction turning (1–2 ticks per step), wall collision, coarse scroll with double-buffered screens, HUD |
| 3 Combat | Mostly done. Hitscan guns, shotgun spread, grenades/C4/mines as projectiles, area explosions, flame + burning, crates, blood/gib/scorch decal tiles, SID effects (never listened to yet) |
| 4 Enemies | Mostly done. See/turn/walk/shoot AI, sleeping far enemies, kill rewards, gore. Verified: enemies shoot the player, the player kills an enemy (reward, decals) |
| 5 Flow | Partly. Title, level intro, mission completed → next level, game over, victory. **Shop not done** |
| 6 Two players | Done (2026-11-19). Title: `1`/`2`. Top/bottom split, 40×12 chars per view, shared HUD row 12; raster IRQ (`src/irq.s`) at line 251 (screen flip, sprite set 0) and line 147 (sprite set 1), so 8 sprites per view; joystick port 1 or keys for player 2; enemies chase the nearer living player; kills credited to the player who caused them; friendly fire; level lost when both are dead |
| 7 Polish | Not started |

**Measured (cycles per 25 Hz tick; budget ≈ 39,000 = 2 PAL frames):**

| Part | Cycles |
|---|---:|
| Map redraw (only when the camera crosses a char) | ≈ 21,000 |
| Player | 3,300–5,400 |
| Line of sight, 1 enemy per tick | 4,000–6,000 |
| Enemies: asleep / awake / awake and shooting | ≈ 100 / 1,000 / 6,000 each |
| Projectiles | 200–700 |
| Whole tick with 2 enemies fighting | 22,000–33,000 |
| Camera / sprite lists per view | ≈ 2,300 / 2,000–6,000 |
| Split screen tick (both players fighting) | ≈ 41,000–51,000 |

The first version spent 54,000 cycles per tick on the HUD (16-bit division when
printing numbers); the HUD now only redraws changed fields. Enemy positions are cached
in pixels/tiles, line of sight works in 1/16 px units where the tile is the high byte.

**Memory:** code 25.8 KB, constants 2.4 KB, BSS 13.3 KB (level buffer 10.3 KB) →
≈ 42 KB of the 46 KB program area. Two players added ~6 KB of code: player state is
now arrays indexed by the current player (`pl_x[cp]` via macros in `game.c`).

**Split screen notes.** The split interrupt runs in the HUD row (raster 147–154),
so view 0 sprites must end above it and view 1 sprites start at line 157 or later.
Sprites partly over the HUD row are not shown, the same as at the screen edges. Each
view redraws only when its own camera moves. The other view is copied from the
visible screen (≈ 5,000 cycles), not redrawn (≈ 11,500). CIA1 interrupts are off
(the game scans the keyboard itself), so the KERNAL keyboard scan cannot delay the
split interrupt.

**Point-blank hits (fixed 2026-11-19).** Hitscan shots used to start 10 px ahead of
the shooter and step 4 px before the first test. An enemy closer than ~7 px
(overlapping) was therefore never hit, not even by the fist. Shots now start at the
shooter's centre, test that point first, and reach range + 12 px. The shooter
itself is skipped (`make test-close`).

**Timing fix.** The old `wait_vblank` polled for raster line 251. It could return
twice within that line, so short loop iterations skipped the every-second-frame wait
and the game often ran at 50 Hz instead of 25. Waiting now uses the interrupt's frame
counter. The ROM font copy now banks with `$01 = $32`; `$33` also maps the BASIC ROM
over the BSS, which now reaches past $A000. That was also the real reason the earlier
`memcpy` font copy failed.

**Known gaps / next steps:**
- Text shop between levels: done (2026-11-23). The shop screen code and `level_intro`
  run from an overlay at $0334-$07FF (tape buffer + the unused default screen): its load
  image lies under the BSS and `src/ovl.s` copies it there as a constructor, before
  crt0 clears the BSS. So nothing may write $0400 after start (the loading text is
  shown on the game screen in bank 3). Free after this: ≈ 570 bytes main, ≈ 80 overlay.
- Ticks with a map redraw *and* several shooting enemies exceed 2 frames, and split
  screen exceeds them more often (≈ 15–20 Hz in fights) → move `wall()`, `move_box()`,
  `trace()` and the player array accesses to assembly, or spread redraws over two ticks.
- Map palette: tiles use 3 shared colours (default black / dark grey / light grey,
  `CONV64_PAL`), colour RAM is fixed red and only gore and crates use it, so blood is red
  at no run-time cost.
- Keyboard input implemented but not yet tested with key presses (VICE headless tests
  only drive the scripted joystick).
- Phase 7 (smooth scroll, multiplexer, more episodes).
- Not yet tried on a real joystick/keyboard or with sound audible. Loading levels with
  the raster interrupt running is only tested in VICE with device traps, not on a real
  1541.
