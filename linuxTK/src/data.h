/* data.h - structures and constant game data tables */
#ifndef TK_DATA_H
#define TK_DATA_H

#include "types.h"

/* bullet sprite ids */
#define BSPR_NONE    0
#define BSPR_GRENADE 1
#define BSPR_C4      2
#define BSPR_MINE    3
#define BSPR_FLAME   4   /* original drew flames only as a light; port uses EXPLO frames */

typedef struct {
    const char *name;
    s16 cost;
    u8  loading_time;
    u8  smoke;
    u8  bullet_amount;
    u8  angle_add;       /* spread, binary angle units */
    u8  gun;
    u8  btype;
    u8  sound;
} Weapon;

typedef struct {
    const char *name;
    u8  type;
    u8  stop;
    u8  speed;           /* pixels per tick */
    s16 power;           /* damage in tenths */
    s16 time;
    u8  explos;
    u8  push_power;      /* pixels per tick */
    s16 cost;
    s16 amount_in_crate;
    u8  mul;
    u8  sleep_time;
    s16 max;
    u8  sprite;          /* BSPR_* */
    u8  effect;
} Bullet_type;

typedef struct {
    const char *name;
    u8  speed;
    u8  curr_weapon;
    s16 energy;          /* tenths */
    s16 reward;
    u8  explo_death;
} Enemy_info;

typedef struct {
    u8 index;
    u8 tcount;
    u8 order;
    u8 delay;
    u8 random;
    u8 xs, ys;
    s16 offs;
    u8 trans;            /* drawn translucent */
} Effect_type;

extern const Weapon      weapon[DIFF_WEAPONS + 1];
extern const Bullet_type bullet_type[DIFF_BULLETS + 2];
extern const Enemy_info  enemy_info[DIFF_ENEMIES];
extern const Effect_type effect_type[DIFF_EFFECTS];
extern const s16         sample_rates[DIFF_WEAPONS + 1];
extern const u8          dust_blocks[DUST_BLOCKS];
extern const s8          sin_tab[256];
extern const u8          atan_tab[33];
extern const s16         shield_cost, shield_max, target_cost;
extern const char *const episode_names[];
extern const s16         num_episode_names;

#endif
