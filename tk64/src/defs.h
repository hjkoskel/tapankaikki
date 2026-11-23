/* defs.h - shared definitions for the C64 port (cc65) */
#ifndef TK64_DEFS_H
#define TK64_DEFS_H

#include "gfxdata.h"

typedef unsigned char u8;
typedef signed char   s8;
typedef unsigned int  u16;   /* cc65: int is 16 bit */
typedef int           s16;

/* VIC bank 3 layout (c64portplan.md section 4) */
#define SCREEN_A   ((u8 *)0xC000)
#define SCREEN_B   ((u8 *)0xC400)
#define CHARSET    ((u8 *)0xC800)
#define SPRPTR_A   ((u8 *)0xC3F8)
#define SPRPTR_B   ((u8 *)0xC7F8)
#define D018_A     0x02
#define D018_B     0x12
#define COLORRAM   ((u8 *)0xD800)

#define PLAY_ROWS  22          /* map rows on screen, HUD below */
#define HUD_ROW    22

/* fixed point: 12.4 world pixels, 16 px per tile */
#define FX_SHIFT   4
#define PX(v)      ((v) >> FX_SHIFT)
#define FX(p)      ((s16)(p) << FX_SHIFT)
#define TILE_PX    16
#define ACTOR      22          /* actor box (28 px in the original * 0.8) */

/* joystick bits (active high after inversion) */
#define J_UP    1
#define J_DOWN  2
#define J_LEFT  4
#define J_RIGHT 8
#define J_FIRE  16
#define J_CHANGE 32     /* next weapon */

/* level file header (written by tools/conv64.c) */
#define LH_W 0
#define LH_H 1
#define LH_P1X 2
#define LH_P1Y 3
#define LH_P2X 4
#define LH_P2Y 5
#define LH_TIME 6
#define LH_ENEMIES 8
#define LH_WCRATES 16
#define LH_BCRATES 27
#define LH_ECRATES 36
#define LH_NCINFO 37
#define LH_COMMENT 38
#define LH_SIZE 64

/* ------------------------------------------------------------------ */
/* game data (data64.c)                                                */

#define NWEAPONS 12
#define NBULLETS 10
#define NENEMYTYPES 8

/* bullet kinds */
#define K_HIT   0      /* hitscan (fist, guns) */
#define K_PROJ  1      /* flying grenade */
#define K_PLACE 2      /* C4: lies still, explodes after time */
#define K_FLAME 3      /* short incendiary ray */
#define K_MINE  4      /* lies still, explodes on touch */

/* sound effects */
#define SFX_NONE 0
#define SFX_GUN 1
#define SFX_SHOTGUN 2
#define SFX_UZI 3
#define SFX_LAUNCH 4
#define SFX_HIT 5
#define SFX_EXPLO 6
#define SFX_PICKUP 7
#define SFX_DEATH 8
#define SFX_FLAME 9

typedef struct {
    const char *name;
    u16 cost;
    u8 load, amount, spread, gun, btype, sfx;
} W64;

typedef struct {
    const char *name;
    u8 kind, range;
    s16 power;          /* tenths */
    u8 explos, push;
    u16 time;           /* projectile life in ticks */
    u16 crate;
    u8 mul;
    u16 max;
    u8 cost;
} B64;

typedef struct {
    u8 speed_fx, weapon;
    s16 energy;
    u16 reward;
    u8 explo_death;
} E64;

extern const W64 weapon64[NWEAPONS];
extern const B64 bullet64[NBULLETS];
extern const E64 enemy64[NENEMYTYPES];
extern const u8 atan_tab[33];
extern const signed char sintab[256];

/* ------------------------------------------------------------------ */
/* shared state                                                        */

#define MAXE 72          /* enemies */
#define MAXP 8           /* projectiles */
#define MAXX 4           /* explosions */
#define MAXC 64          /* crates */

extern u8 levelbuf[];
extern u8 *map;
extern u8 map_w, map_h;
extern u16 rowoff[];

/* players (game.c): one array entry per player. game.c works on player cp
 * through short macros (px = pl_x[cp] ...), so the logic reads as before. */
#define MAXPL 2
extern u8 nplayers, cp, joy[MAXPL];
extern s16 pl_x[MAXPL], pl_y[MAXPL], pl_energy[MAXPL], pl_max_energy[MAXPL];
extern s16 pl_bullets[MAXPL][NBULLETS];
extern u8 pl_angle[MAXPL], pl_dead[MAXPL];
extern u8 pl_weapons[MAXPL][NWEAPONS], pl_curr[MAXPL], pl_shield[MAXPL];
extern u16 pl_cash[MAXPL], pl_kills[MAXPL];
extern char pl_msg[MAXPL][21];
extern u8 pl_msg_time[MAXPL], pl_msg_seq[MAXPL];
extern const char *const wshort[NWEAPONS];

/* difficulty, chosen on the title screen */
#define SKILL_EASY   0
#define SKILL_NORMAL 1
#define SKILL_HARD   2
#define SKILL_GOD    3     /* players take no damage at all */
#define NSKILLS      4
extern u8 skill;

/* enemies (structure of arrays: fast indexing in cc65) */
extern u8 ne, edead_count;
extern s16 ex[MAXE], ey[MAXE], eenergy[MAXE];
extern u8 eang[MAXE], etang[MAXE], etype[MAXE], ewalk[MAXE], eload[MAXE];
extern u8 efire[MAXE], eflags[MAXE], eburn[MAXE], epush[MAXE], epushang[MAXE];
#define EF_DEAD 1
#define EF_SEES 2
#define EF_SAW  4

/* projectiles and explosions */
extern u8 prj_on[MAXP], prj_type[MAXP], prj_owner[MAXP];
extern s16 prj_x[MAXP], prj_y[MAXP], prj_dx[MAXP], prj_dy[MAXP];
extern u16 prj_time[MAXP];
extern u8 exp_on[MAXX], exp_frame[MAXX];
extern s16 exp_x[MAXX], exp_y[MAXX];

/* views (main.c): camera in characters, sprite Y window in raster lines */
extern u8 view_cx[MAXPL], view_cy[MAXPL], view_rows[MAXPL], view_top[MAXPL], view_ymin[MAXPL], view_ymax[MAXPL];

/* irq.s: sprite shadow registers, set 0 = view 0, set 1 = view 1 */
extern u8 spr_x[16], spr_y[16], spr_ptr[16], spr_col[16], spr_ena[2], spr_msb[2], spr_mc[2];
extern volatile u8 irq_frame;
extern u8 irq_d018, irq_split;
void irq_init(void);

/* main.c */
void print(u8 x, u8 y, const char *s);
void print_num(u8 x, u8 y, u16 v, u8 width);
void sfx(u8 n);
u8 wall(s16 x, s16 y);
void stamp_tile(u8 tx, u8 ty, u8 id);

/* game.c */
void game_level_init(void);
void game_tick(void);
void game_sprites(u8 view);
u8 rnd(void);

/* map.s */
void draw_map(void);
extern u8 *dm_map, *dm_scr;
extern u8 dm_w, dm_px, dm_py, dm_rows;

#endif
