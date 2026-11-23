/* types.h - basic types, fixed point, limits and constants for the TK port.
 * Game logic uses only 8- and 16-bit integer math (no floating point), so the
 * core can move to 8/16-bit machines. Target build: C89. */
#ifndef TK_TYPES_H
#define TK_TYPES_H

typedef unsigned char  u8;
typedef signed char    s8;
typedef unsigned short u16;
typedef short          s16;
typedef unsigned long  u32;
typedef long           s32;

typedef char tk_assert_short16[(sizeof(short) == 2) ? 1 : -1];
typedef char tk_assert_long32[(sizeof(long) >= 4) ? 1 : -1];

#define TK_VERSION "linuxTK 0.2"

/* ------------------------------------------------------------------ */
/* fixed point and angles                                              */

/* positions: s16 12.4 fixed point = 0..2047 pixels with 1/16 px precision
 * (largest level is 99 tiles * 20 px = 1980 px) */
#define FX_SHIFT 4
#define FX_ONE   16
#define FX(px)   ((s16)((px) * FX_ONE))         /* pixels -> fixed */
#define PX(fx)   ((s16)((fx) >> FX_SHIFT))      /* fixed -> pixels (positions are >= 0) */

/* angles: u8 binary angle, 256 = full turn, 0 = down (+y), 64 = right (+x).
 * Movement: dx = v * SIN(a) / 64, dy = v * COS(a) / 64 */
#define SIN(a)   ((s16)sin_tab[(u8)(a)])
#define COS(a)   ((s16)sin_tab[(u8)((a) + 64)])
#define TRIG_ONE 64
#define ANG_DEG(d) ((u8)(((d) * 256L) / 360))   /* only for constants */

/* 16 facing directions, 22.5 degrees apart (sprites and player turning) */
#define DIRS      16
#define DIR_STEP  16                              /* 256 / DIRS */
#define DIR_OF(a) ((u8)((((a) & 255) + DIR_STEP / 2) >> 4) & (DIRS - 1))
/* player turns one direction per 2.5 ticks (alternating 3 and 2) = 9 deg/tick as original */
#define TURN_DELAY(angle) (((angle) & DIR_STEP) ? 3 : 2)

/* energy, push and body part speed are in tenths (0.4 damage = 4) */
#define TENTHS 10

/* ------------------------------------------------------------------ */
/* screen and timing                                                   */

#define SCR_W 320
#define SCR_H 200
#define TARGET_FRAMES 40
#define TICK_MS 25

/* pools and limits (shrink these for small machines) */
#define MAX_PLAYERS      2
#define MAX_ENEMIES      100
#define MAX_BODY_PARTS   300
#define MAX_EFFECTS      300
#define MAX_BULLETS      300
#define MAX_CRATES       200
#define MAX_CRATE_INFO   100
#define MAX_LEVEL_W      100
#define MAX_LEVEL_H      100
#define MAX_LEVELS       99
#define MAX_MESSAGES     6
#define MAX_MESSAGE_LENGTH 40

/* game data sizes */
#define DIFF_BULLETS 9
#define DIFF_WEAPONS 11
#define DIFF_ENEMIES 8
#define DIFF_EFFECTS 10

/* game modes */
#define ONE_PLAYER   0
#define SPLIT_SCREEN 1

/* bullet types */
#define NORMAL     0
#define EXPLOSIVE  1
#define INCENDIARY 2

/* effects */
#define BLOOD     0
#define SMOKE     1
#define EXPLO     2
#define METAL     3
#define SMOKE2    4
#define BLOOD2    5
#define SMOKE3    6
#define STEAM     7
#define TELEPORT  8
#define FOOTPRINT 9
#define EXPLO_RAY 10 /* bullet type index */

/* sounds */
#define DIFF_WAVS  10
#define MAGNUMWAV  1
#define SHOTGUNWAV 2
#define PISTOLWAV  3
#define SINKOWAV   4
#define HITWAV     5
#define EXPLOWAV   6
#define KLIKWAV    7
#define DEATHWAV   8
#define WOOSHWAV   9
#define FLAMEWAV   10

/* misc gameplay constants (from DEFINES.H) */
#define MESSAGE_TIME_ON_SCREEN 200
#define WALK_DELAY 20
#define DEAD_DELAY 15
#define C_DELAY 300
#define ROT_SPEED 6          /* enemy turn speed: 9 degrees in binary angle units */
#define TIME_LIMIT(t) ((s16)((t) * 11 / 10))   /* original TIME_ADD 1.1 */
#define DUST_BLOCKS 10

/* palette colours (game palette) used by text */
#define C_WHITE  1
#define C_RED    48
#define C_YELLOW 112
#define C_BLUE   144
#define C_GREY   32

/* sprite sheet geometry */
#define SHEET_ANGLES  40           /* angles in the original sheets */
#define ACTOR_W       28
#define ACTOR_CELL    29
#define PLAYER_ANIMS  23
#define ENEMY_ANIMS   8
#define TILE_SIZE     20
#define NUM_TILES     160
#define BODY_PARTS_GFX 15
#define CRATE_TYPES   3
#define EFFECT_POOL   21504        /* 21337 bytes needed */
#define MAX_TRANS_COLORS 32        /* 28 used */

#endif
