/* globals.h - all game state (statically allocated) and function prototypes.
 * Positions are 12.4 fixed point (s16), angles u8 binary, energy in tenths. */
#ifndef TK_GLOBALS_H
#define TK_GLOBALS_H

#include "types.h"
#include "data.h"

/* ------------------------------------------------------------------ */
/* structures                                                          */

typedef struct {
    s16 w, h;
    u8 *pix;            /* frame i = pix + i*w*h, points into a static array */
} Frames;

typedef struct {
    s16 w, h;           /* glyph size */
    u8 *data;           /* 256 glyphs of w*h bytes */
} Font;

typedef struct {
    s16 x, y;           /* 12.4 fixed point */
    u8  enabled;
    u8  keyset;
    u8  speed;          /* pixels per tick */
    u8  angle;          /* always a multiple of DIR_STEP */
    u8  turn_cnt;
    s16 scr_x, scr_y;   /* camera, pixels */
    u8  weapons[DIFF_WEAPONS + 1];
    u8  curr_weapon, load_cnt;
    u16 shoot_cnt;
    s16 bullets[DIFF_BULLETS];
    s32 cash;           /* can exceed 32767 */
    s16 max_energy;     /* tenths */
    s16 energy;         /* tenths */
    u8  shield;
    u8  color;
    s16 burning;
    s8  frying_player;  /* player index or -1 */
    char name[10];
    u8  HIT, WALK, FIRE, DEAD, KICK, ANIM;
    u8  TARGET;
    s16 push;           /* push speed in tenths of a pixel per tick */
    u8  push_angle;
    s16 kills[DIFF_ENEMIES];
    s16 total_kills;
    u16 shooted, hitten;
    s16 player_kills;
    u32 buttons, prev_buttons;
} Player;

typedef struct {
    s16 x, y;           /* 12.4 fixed point */
    u8  angle, tangle;
    s16 walk_cnt;
    u8  load_cnt;
    s16 energy;         /* tenths */
    u16 shoot_cnt;
    u8  angle_pl[MAX_PLAYERS];
    u8  type;
    s16 burning;
    s8  frying_player;
    s16 push;           /* tenths of a pixel per tick */
    u8  push_angle;
    u8  WALK, FIRE, DEAD, ANIM;
    u8  SEE_PL[MAX_PLAYERS];
    u8  see_somebody;
} Enemy;

typedef struct {
    s16 x, y;           /* 12.4 */
    u8  type;           /* index into bullet_type[] */
    s8  owner;          /* player index or -1 */
    u8  angle;
    s16 time;
    s16 count;
    u8  in_use;
} Bullet;

typedef struct {
    s16 x, y;           /* 12.4 */
    u8  angle;
    u8  speed;          /* pixels per tick */
    u8  type;           /* index into effect_type[] */
    u8  count;
    u8  in_use;
    u16 age;
} Effect;

typedef struct {
    s16 x, y;           /* 12.4 */
    u8  angle;
    u8  num;
    s16 speed;          /* tenths of a pixel per tick */
    u8  in_use;
} Body_part;

typedef struct {
    u8  type;           /* 0 weapon, 1 bullets, 2 energy */
    u8  weapon;
    u8  bullet;
    s16 x, y;           /* pixels */
    u8  in_use;
} Crate;

typedef struct {
    s16 type1, type2, x, y;
} Crate_info;

typedef struct {
    s16 origin_x;       /* screen x of the view's left edge */
    s16 clip_x0, clip_x1;
    s16 width;          /* camera width in pixels: 320 or 160 */
    u8  owner;          /* player index */
} View;

/* ------------------------------------------------------------------ */
/* graphics and assets (assets.c / gfx.c)                              */

extern u8 fb[SCR_W * SCR_H];
extern u8 pal_game[768];

extern Frames frm_player, frm_enemy[DIFF_ENEMIES], frm_floor, frm_walls;
extern Frames frm_bodypart, frm_crate, frm_target, frm_c4, frm_mine, frm_bomb;
extern Frames frm_effect[DIFF_EFFECTS];
extern u8 trans_lut[MAX_TRANS_COLORS][256];
extern u8 map_dark[256];
extern Font font;

extern s16 clip_x0, clip_x1;   /* current clip window, set per view */
extern s16 cam_x, cam_y;       /* current camera (world coords of view origin) */
extern s16 view_ox;            /* screen x of current view origin */

/* ------------------------------------------------------------------ */
/* game state (globals.c)                                              */

extern char data_dir[256];
extern u8  GAME_MODE;
extern u8  ACTIVE_PLAYERS;
extern u8  active[2];          /* active player indices, view order */
extern View view[2];
extern u8  num_views;

extern Player player[MAX_PLAYERS];
extern Enemy enemy[MAX_ENEMIES];
extern Bullet bullet[MAX_BULLETS];
extern Effect effect[MAX_EFFECTS];
extern Body_part body_part[MAX_BODY_PARTS];
extern Crate crate[MAX_CRATES];

extern u8 level_tile[MAX_LEVEL_W * MAX_LEVEL_H];  /* tile id: bit7 = wall, ids are per level */
extern u8 tile_num[256];       /* tile id -> original tile number in FLOOR1/WALLS1 */
extern s16 level_x_size, level_y_size, level_version;
extern s16 pl_start_x[MAX_PLAYERS], pl_start_y[MAX_PLAYERS];
extern char level_comment[21];
extern s16 level_time_limit;
extern s16 level_enemies[DIFF_ENEMIES];
extern s16 lev_weapon_crates[DIFF_WEAPONS], lev_bullet_crates[DIFF_BULLETS], lev_energy_crates;
extern Crate_info crate_info[MAX_CRATE_INFO];
extern s16 crate_info_amount;

extern s16 ENEMIES, DEAD_ENEMIES, LEVEL, LEVELS, START_LEVEL, episode;
extern s16 episode_levels[16];
extern s16 num_episodes;
extern u8  quit, all_dead, back_to_menu, app_quit, MAP_ON, SHOW_ENEMIES;
extern s16 next_level, complete_time;
extern u16 effect_age_count, tick_count, level_ticks, plot_count;
extern s16 sell_price[DIFF_WEAPONS + 2];
extern char name1[10], name2[10];

extern u8 messages;
extern s16 message_time;
extern char message[MAX_MESSAGES][MAX_MESSAGE_LENGTH];

/* ------------------------------------------------------------------ */
/* functions                                                           */

/* assets.c */
int  assets_load_all(void);
int  level_load(const char *path);
void count_episodes(void);
const char *data_path(const char *rel);

/* gfx.c */
void set_view(int i);
void set_full_clip(void);
void clear_screen(void);
void blit_tile(s16 x, s16 y, const u8 *tile);
void blit(s16 x, s16 y, const Frames *f, s16 i);
void blit_recolor(s16 x, s16 y, const Frames *f, s16 i, u8 col);
void blit_trans(s16 x, s16 y, const Frames *f, s16 i);
void put_pixel(s16 x, s16 y, u8 col);
void rectangle(s16 x1, s16 y1, s16 x2, s16 y2, u8 col);
void writefonts(s16 x, s16 y, const char *str, u8 cq);
s16  str_length(const char *str);

/* rout.c */
s16  get_dist(s16 x1, s16 y1, s16 x2, s16 y2);
u8   get_angle(s16 dx, s16 dy);              /* direction of vector (dx, dy) */
s16  fx_sin(s16 v, u8 a);                    /* v * sin(a) */
s16  fx_cos(s16 v, u8 a);                    /* v * cos(a) */
void rnd_seed(u16 s);
s16  rnd(void);                              /* 0..32767 */
const char *c_2_c(s16 secs);
void play_sound(u8 id, s16 rate);
u8   wall_px(s16 x, s16 y);                  /* 1 if pixel position is wall/outside */
u8   tile_px(s16 x, s16 y);                  /* tile byte at pixel position, wall if outside */
void add_message(const char *msg);

/* effect.c */
void new_effect(u8 type, s16 x, s16 y, u8 angle, u8 speed);
void new_body_part(s16 x, s16 y, u8 angle, s16 speed, u8 num);
void roll_effects(void);
void move_body_parts(void);
void null_effects(void);
void null_body_parts(void);

/* bullet.c */
void new_bullet(u8 type, s16 x, s16 y, u8 angle, s16 time_add, s8 owner);
void move_bullets(void);
void null_bullets(void);
void explo(s16 x, s16 y, u8 size, s8 owner);

/* player.c */
void player_chk_keys(u8 pi);
void player_move(u8 pi, u8 angle, s16 spd_fx);
void player_move_scr(u8 pi);
void player_animate(u8 pi);
void player_load(u8 pi);
void player_get_damage(u8 pi, s16 damage, s8 from);
void alusta_players1(void);
void alusta_players2(void);
void two_to_one(void);

/* enemy.c */
void alusta_enemies(void);
void place_enemies(void);
void update_enemies(void);
void enemy_get_damage(s16 ei, s16 damage, s8 from);

/* level.c */
void null_crates(void);
void place_normal_game_crates(void);
void chk_burn(void);

/* render.c */
void render_game(void);

/* screens.c */
int  wait_key(void);
void main_menu(void);
void level_info(void);
void statics(void);
void shop(u8 pi);
void victory(void);
int  ingame_quit(void);

/* game.c */
void game(void);

#endif
