/* globals.c - all mutable game state, statically allocated */
#include "globals.h"

u8 fb[SCR_W * SCR_H];
u8 pal_game[768];

s16 clip_x0 = 0, clip_x1 = SCR_W;
s16 cam_x = 0, cam_y = 0;
s16 view_ox = 0;

char data_dir[256] = "..";
u8  GAME_MODE = ONE_PLAYER;
u8  ACTIVE_PLAYERS = 1;
u8  active[2];
View view[2];
u8  num_views = 1;

Player player[MAX_PLAYERS];
Enemy enemy[MAX_ENEMIES];
Bullet bullet[MAX_BULLETS];
Effect effect[MAX_EFFECTS];
Body_part body_part[MAX_BODY_PARTS];
Crate crate[MAX_CRATES];

u8 level_tile[MAX_LEVEL_W * MAX_LEVEL_H];
u8 tile_num[256];
s16 level_x_size, level_y_size, level_version;
s16 pl_start_x[MAX_PLAYERS], pl_start_y[MAX_PLAYERS];
char level_comment[21];
s16 level_time_limit;
s16 level_enemies[DIFF_ENEMIES];
s16 lev_weapon_crates[DIFF_WEAPONS], lev_bullet_crates[DIFF_BULLETS], lev_energy_crates;
Crate_info crate_info[MAX_CRATE_INFO];
s16 crate_info_amount;

s16 ENEMIES, DEAD_ENEMIES, LEVEL, LEVELS, START_LEVEL, episode;
s16 episode_levels[16];
s16 num_episodes;
u8  quit, all_dead, back_to_menu, app_quit, MAP_ON, SHOW_ENEMIES;
s16 next_level, complete_time;
u16 effect_age_count, tick_count, level_ticks, plot_count;
s16 sell_price[DIFF_WEAPONS + 2];
char name1[10] = "player 1", name2[10] = "player 2";

u8 messages;
s16 message_time;
char message[MAX_MESSAGES][MAX_MESSAGE_LENGTH];
