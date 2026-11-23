/* ed.h - editor state and prototypes */
#ifndef ED_H
#define ED_H

#include "types.h"

/* ---- level (level.c) ---- */
typedef struct { s32 type, num, shadow; } Block;   /* type != 0 = wall */
typedef struct { s32 x, y, size; } Spot;           /* centre in pixels, size 0..9 */
typedef struct { s32 x, y, angle, speed; } Steam;  /* angle in degrees, 0 = down, 90 = right */
typedef struct { s32 type1, type2, x, y; } Crate_info; /* type1 0 weapon 1 bullet 2 energy */
typedef struct {
    s32 weapon[DIFF_WEAPONS];
    s32 bullet[DIFF_BULLETS];
    s32 energy;
} Crate_counts;

typedef struct {
    s32 version, w, h;
    Block blk[MAX_LEVEL_W * MAX_LEVEL_H];
    s32 start_x[2], start_y[2];                    /* in blocks */
    s32 spots, steams;
    Spot spot[MAX_SPOTS];
    Steam steam[MAX_STEAM];
    char comment[21];
    s32 time_limit;                                /* seconds, 0 = none */
    s32 enemies[DIFF_ENEMIES];
    Crate_counts rnd[2];                           /* 0 = normal game, 1 = deathmatch */
    s32 crates[2];
    Crate_info crate[2][MAX_CRATE_INFO];
} Level;

extern Level lev;
#define BLK(x, y) (lev.blk[(s32)(y) * lev.w + (x)])

void level_new(s16 w, s16 h, s32 type, s32 num);
int  level_load(const char *path);
int  level_save(const char *path);
void level_auto_shadows(void);
const char *level_error(void);

/* ---- data files (data.c) ---- */
extern char data_dir[256];
extern u8 pal[768];
extern u8 floor_pix[NUM_TILES][TILE * TILE];
extern u8 wall_pix[NUM_TILES][TILE * TILE];
extern u8 shadow_pix[SHADOW_TYPES][TILE * TILE];
extern u8 crate_pix[CRATE_TYPES][CRATE_SIZE * CRATE_SIZE];
extern u8 pointer_pix[13 * 9];
extern u8 shadow_table[256][16];
extern u8 floor_avg[NUM_TILES], wall_avg[NUM_TILES];
extern u8 font_data[256 * 64];
extern const s16 light_size[LIGHTS];
extern const char *const weapon_name[DIFF_WEAPONS];
extern const char *const bullet_name[DIFF_BULLETS];
extern const char *const enemy_name[DIFF_ENEMIES];

const char *data_path(const char *rel);
int  data_load_all(void);
u8   nearest_colour(s16 r, s16 g, s16 b);   /* 0..255 RGB */

/* ---- drawing (gfx.c) ---- */
extern u8 fb[SCR_W * SCR_H];
extern s16 clip_x0, clip_y0, clip_x1, clip_y1;
void clip_set(s16 x0, s16 y0, s16 x1, s16 y1);
void clip_full(void);
void put_pixel(s16 x, s16 y, u8 c);
void fill_rect(s16 x, s16 y, s16 w, s16 h, u8 c);
void rect(s16 x, s16 y, s16 w, s16 h, u8 c);
void line(s16 x0, s16 y0, s16 x1, s16 y1, u8 c);
void circle(s16 cx, s16 cy, s16 r, u8 c);
void blit(s16 x, s16 y, s16 w, s16 h, const u8 *pix, int transparent);
void blit_shadow(s16 x, s16 y, const u8 *pix);
void darken_rect(s16 x, s16 y, s16 w, s16 h, s16 level);
void text(s16 x, s16 y, const char *s, u8 ramp);   /* proportional game font */
s16  text_width(const char *s);
void text_centre(s16 y, const char *s, u8 ramp);
void box(s16 x, s16 y, s16 w, s16 h);              /* dark panel with a frame */

/* UI colours, found from the palette at start (gfx.c) */
extern u8 col_black, col_white, col_grey, col_dgrey, col_red, col_green, col_blue, col_yellow, col_cyan;
void ui_colours(void);

/* ---- editor (editor.c) ---- */
void editor_run(const char *start_file);

#endif
