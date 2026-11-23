/* assets.c - loading of EFP sprite sheets, the font, tables and levels.
 *
 * All pixel data is converted to u8 while loading. Sprite sheets are never
 * kept whole in memory: the EFP decoder produces one row at a time and
 * "cut jobs" copy the wanted cells straight into packed static frame arrays.
 * Actor sheets have 40 angles (9 deg); the column nearest to each of the 16
 * directions (22.5 deg) is kept. Frames are stored direction-major:
 * frame = dir * anims + anim.
 */
#include <stdio.h>
#include <string.h>
#include "globals.h"

/* ------------------------------------------------------------------ */
/* static asset memory                                                 */

#define ACTOR_PIX (ACTOR_W * ACTOR_W)

static u8 player_pix[(s32)PLAYER_ANIMS * DIRS * ACTOR_PIX];
static u8 enemy_pix[DIFF_ENEMIES][(s32)ENEMY_ANIMS * DIRS * ACTOR_PIX];
static u8 floor_pix[NUM_TILES * TILE_SIZE * TILE_SIZE];
static u8 walls_pix[NUM_TILES * TILE_SIZE * TILE_SIZE];
static u8 bodypart_pix[BODY_PARTS_GFX * 20 * 20];
static u8 crate_pix[CRATE_TYPES * 14 * 14];
static u8 target_pix[7 * 7], c4_pix[8 * 8], mine_pix[7 * 7], bomb_pix[7 * 7];
static u8 effect_pix[EFFECT_POOL];
static u8 font_data[256 * 8 * 8];

Frames frm_player, frm_enemy[DIFF_ENEMIES], frm_floor, frm_walls;
Frames frm_bodypart, frm_crate, frm_target, frm_c4, frm_mine, frm_bomb;
Frames frm_effect[DIFF_EFFECTS];
u8 trans_lut[MAX_TRANS_COLORS][256];
u8 map_dark[256];
Font font;

/* ------------------------------------------------------------------ */
/* helpers                                                             */

static char path_buf[512];

const char *data_path(const char *rel)
{
    sprintf(path_buf, "%.255s/%.250s", data_dir, rel);
    return path_buf;
}

static FILE *open_data(const char *rel)
{
    FILE *f = fopen(data_path(rel), "rb");
    if (!f) printf("Missing file: %s\n", data_path(rel));
    return f;
}

static s16 read_le16(FILE *f)
{
    u16 a = (u16)(fgetc(f) & 255);
    u16 b = (u16)(fgetc(f) & 255);
    return (s16)(a | (b << 8));
}

/* level files store 32-bit values; all used values fit in 16 bits */
static s16 read_le32(FILE *f)
{
    s16 v = read_le16(f);
    (void)read_le16(f);
    return v;
}

/* ------------------------------------------------------------------ */
/* EFP streaming decoder + cut jobs                                    */

typedef struct {
    s16 x0, y0;         /* top-left of cell (0,0) inside the sheet */
    s16 sx, sy;         /* cell stride */
    s16 w, h;           /* copied size of each cell */
    s16 cols, rows;
    u8 *dest;           /* frame (r*cols + c) at dest + (r*cols+c)*w*h */
} Cut;

#define MAX_SHEET_W 1160

static int efp_cut(const char *rel, const Cut *cuts, int ncuts)
{
    static u8 line[MAX_SHEET_W];
    char header[6];
    FILE *f;
    s16 w, h, x, y, run = 0, val = 0, r, c, cy, sx, b;
    int i;

    f = open_data(rel);
    if (!f) return 0;
    if (fread(header, 1, 6, f) != 6 || memcmp(header, "EF pic", 6) != 0) {
        printf("Not an EF pic: %s\n", rel);
        fclose(f);
        return 0;
    }
    w = read_le16(f);
    h = read_le16(f);
    if (w > MAX_SHEET_W) {
        printf("Picture too wide: %s\n", rel);
        fclose(f);
        return 0;
    }
    for (y = 0; y < h; y++) {
        /* decode one row; RLE runs may continue across rows */
        for (x = 0; x < w; x++) {
            if (run == 0) {
                b = (s16)fgetc(f);
                if (b < 0) b = 0;
                if (b > 192) {
                    run = (s16)(b - 192);
                    val = (s16)(fgetc(f) & 255);
                } else {
                    run = 1;
                    val = b;
                }
            }
            line[x] = (u8)val;
            run--;
        }
        /* route row into the cut jobs */
        for (i = 0; i < ncuts; i++) {
            const Cut *k = &cuts[i];
            if (y < k->y0) continue;
            r = (s16)((y - k->y0) / k->sy);
            cy = (s16)((y - k->y0) % k->sy);
            if (r >= k->rows || cy >= k->h) continue;
            for (c = 0; c < k->cols; c++) {
                sx = (s16)(k->x0 + c * k->sx);
                if (sx + k->w > w) continue;
                memcpy(k->dest + ((s32)(r * k->cols + c) * k->w * k->h) + cy * k->w, line + sx, (size_t)k->w);
            }
        }
    }
    fclose(f);
    return 1;
}

static void set_cut(Cut *k, s16 x0, s16 y0, s16 sx, s16 sy, s16 w, s16 h, s16 cols, s16 rows, u8 *dest)
{
    k->x0 = x0; k->y0 = y0; k->sx = sx; k->sy = sy;
    k->w = w; k->h = h; k->cols = cols; k->rows = rows; k->dest = dest;
}

static void set_frames(Frames *f, s16 w, s16 h, u8 *pix)
{
    f->w = w; f->h = h; f->pix = pix;
}

static int cut_single(const char *rel, s16 x0, s16 y0, s16 sx, s16 sy, s16 w, s16 h,
                      s16 cols, s16 rows, u8 *dest, Frames *f)
{
    Cut k;
    set_cut(&k, x0, y0, sx, sy, w, h, cols, rows, dest);
    if (f) set_frames(f, w, h, dest);
    return efp_cut(rel, &k, 1);
}

static int load_palette(const char *efp, u8 *pal)
{
    FILE *f = open_data(efp);
    if (!f) return 0;
    fseek(f, -768L, SEEK_END);
    if (fread(pal, 1, 768, f) != 768) {
        fclose(f);
        return 0;
    }
    fclose(f);
    return 1;
}

/* ------------------------------------------------------------------ */
/* effects + translucency                                              */

static int load_effects(void)
{
    Cut cuts[DIFF_EFFECTS];
    s16 i, used = 0, size;
    for (i = 0; i < DIFF_EFFECTS; i++) {
        const Effect_type *t = &effect_type[i];
        size = (s16)(t->tcount * t->xs * t->ys);
        if (used + size > EFFECT_POOL) {
            printf("EFFECT_POOL too small\n");
            return 0;
        }
        set_cut(&cuts[i], (s16)((t->offs % 320) + 1), (s16)((t->offs / 320) + 1), (s16)(t->xs + 1), (s16)(t->ys + 1),
                t->xs, t->ys, t->tcount, 1, effect_pix + used);
        set_frames(&frm_effect[i], t->xs, t->ys, effect_pix + used);
        used += size;
    }
    return efp_cut("EFPS/EFFECTS.EFP", cuts, DIFF_EFFECTS);
}

/* Remap the colours of translucent effect frames to 1..K and build a
 * K x 256 lookup from the 64 KB table in PALETTE.TAB (only K rows are read). */
static int build_trans(void)
{
    static s16 remap[256];
    s16 k = 1, i, p, best, bd, d, n;
    u8 c;
    FILE *f;

    for (i = 0; i < 256; i++) remap[i] = 0;
    for (i = 0; i < DIFF_EFFECTS; i++) {
        const Frames *fr = &frm_effect[i];
        if (!effect_type[i].trans) continue;
        n = (s16)(effect_type[i].tcount * fr->w * fr->h);
        for (p = 0; p < n; p++) {
            c = fr->pix[p];
            if (c && !remap[c]) {
                if (k < MAX_TRANS_COLORS) {
                    remap[c] = k++;
                } else {
                    /* too many colours: use the nearest index already in use */
                    best = 1; bd = 1000;
                    for (d = 1; d < 256; d++)
                        if (remap[d] > 0 && (d > c ? d - c : c - d) < bd) {
                            bd = (s16)(d > c ? d - c : c - d);
                            best = remap[d];
                        }
                    remap[c] = (s16)-best;
                }
            }
        }
    }
    f = open_data("PALETTE.TAB");
    if (!f) return 0;
    for (i = 1; i < 256; i++)
        if (remap[i] > 0) {
            fseek(f, (long)i * 256L, SEEK_SET);
            if (fread(trans_lut[remap[i]], 1, 256, f) != 256) {
                fclose(f);
                return 0;
            }
        }
    /* map colour: shadow_table[c][15] follows the 64 KB table, 16 bytes per colour */
    for (i = 0; i < 256; i++) {
        fseek(f, 65536L + (long)i * 16L + 15L, SEEK_SET);
        map_dark[i] = (u8)fgetc(f);
    }
    fclose(f);
    for (i = 0; i < DIFF_EFFECTS; i++) {
        const Frames *fr = &frm_effect[i];
        if (!effect_type[i].trans) continue;
        n = (s16)(effect_type[i].tcount * fr->w * fr->h);
        for (p = 0; p < n; p++) {
            s16 m = remap[fr->pix[p]];
            fr->pix[p] = (u8)(m < 0 ? -m : m);
        }
    }
    return 1;
}

/* ------------------------------------------------------------------ */

static int load_font(void)
{
    FILE *f = open_data("FNTS/8X8B.FNT");
    if (!f) return 0;
    font.w = (s16)fgetc(f);
    font.h = (s16)fgetc(f);
    if (font.w != 8 || font.h != 8) {
        fclose(f);
        return 0;
    }
    fseek(f, 512L, SEEK_SET);
    if (fread(font_data, 1, sizeof(font_data), f) != sizeof(font_data)) {
        fclose(f);
        return 0;
    }
    fclose(f);
    font.data = font_data;
    return 1;
}

static const char *const enemy_files[DIFF_ENEMIES] = {
    "EFPS/ENEMY0.EFP", "EFPS/ENEMY1.EFP", "EFPS/ENEMY2.EFP", "EFPS/ENEMY3.EFP",
    "EFPS/ENEMY4.EFP", "EFPS/ENEMY5.EFP", "EFPS/ENEMY6.EFP", "EFPS/ENEMY7.EFP"
};

/* Cut one actor sheet: one cut job per direction, all done in a single
 * decoding pass. Direction d uses sheet column round(d * 40 / 16). */
static int cut_actor(const char *rel, s16 anims, u8 *dest, Frames *f)
{
    Cut cuts[DIRS];
    s16 d, col;
    for (d = 0; d < DIRS; d++) {
        col = (s16)(((d * SHEET_ANGLES * 2 + DIRS) / (DIRS * 2)) % SHEET_ANGLES);
        set_cut(&cuts[d], (s16)(1 + col * ACTOR_CELL), 1, ACTOR_CELL, ACTOR_CELL, ACTOR_W, ACTOR_W,
                1, anims, dest + (s32)d * anims * ACTOR_PIX);
    }
    set_frames(f, ACTOR_W, ACTOR_W, dest);
    return efp_cut(rel, cuts, DIRS);
}

int assets_load_all(void)
{
    int ok = 1;
    s16 i;

    ok &= load_font();
    /* tiles: 16x10 grid of 20x20 */
    ok &= cut_single("EFPS/FLOOR1.EFP", 0, 0, 20, 20, 20, 20, 16, 10, floor_pix, &frm_floor);
    ok &= cut_single("EFPS/WALLS1.EFP", 0, 0, 20, 20, 20, 20, 16, 10, walls_pix, &frm_walls);
    /* actors: 29x29 cells with a 1 px border */
    ok &= cut_actor("EFPS/RAMBO2.EFP", PLAYER_ANIMS, player_pix, &frm_player);
    for (i = 0; i < DIFF_ENEMIES; i++)
        ok &= cut_actor(enemy_files[i], ENEMY_ANIMS, enemy_pix[i], &frm_enemy[i]);
    ok &= cut_single("EFPS/BODYP.EFP", 0, 0, 21, 21, 20, 20, BODY_PARTS_GFX, 1, bodypart_pix, &frm_bodypart);
    ok &= cut_single("EFPS/CRATES.EFP", 0, 0, 14, 14, 14, 14, CRATE_TYPES, 1, crate_pix, &frm_crate);
    ok &= cut_single("EFPS/TARGET.EFP", 0, 0, 7, 7, 7, 7, 1, 1, target_pix, &frm_target);
    ok &= cut_single("EFPS/C4.EFP", 0, 0, 8, 8, 8, 8, 1, 1, c4_pix, &frm_c4);
    ok &= cut_single("EFPS/MINE.EFP", 0, 0, 7, 7, 7, 7, 1, 1, mine_pix, &frm_mine);
    ok &= cut_single("EFPS/BOMB.EFP", 0, 0, 7, 7, 7, 7, 1, 1, bomb_pix, &frm_bomb);
    ok &= load_effects();
    if (ok) ok &= build_trans();
    ok &= load_palette("EFPS/WALLS1.EFP", pal_game);
    return ok;
}

/* ------------------------------------------------------------------ */
/* levels                                                              */

int level_load(const char *rel)
{
    FILE *f = open_data(rel);
    s16 i, n, type, num, weapons, bullets, next_floor = 0, next_wall = 128, id;

    if (!f) return 0;
    level_version = read_le32(f);
    level_x_size = read_le32(f);
    level_y_size = read_le32(f);
    if (level_version < 1 || level_version > 5 ||
        level_x_size < 2 || level_y_size < 2 ||
        level_x_size > MAX_LEVEL_W || level_y_size > MAX_LEVEL_H) {
        printf("Bad or too large level: %s\n", rel);
        fclose(f);
        return 0;
    }
    n = (s16)(level_x_size * level_y_size);
    for (i = 0; i < n; i++) {
        type = read_le32(f);
        num = read_le32(f);
        (void)read_le32(f); /* shadow, not used */
        /* Tile numbers go up to 159, so they don't fit in 7 bits + a wall bit.
         * Each level gets its own ids: floors 0-127, walls 128-255. */
        if (num < 0 || num >= NUM_TILES) num = 0;
        if (type != 0) {
            for (id = 128; id < next_wall && tile_num[id] != num; id++) ;
            if (id == next_wall && next_wall < 256) tile_num[next_wall++] = (u8)num;
        } else {
            for (id = 0; id < next_floor && tile_num[id] != num; id++) ;
            if (id == next_floor && next_floor < 128) tile_num[next_floor++] = (u8)num;
        }
        level_tile[i] = (u8)id;
    }
    pl_start_x[0] = read_le32(f);
    pl_start_y[0] = read_le32(f);
    pl_start_x[1] = read_le32(f);
    pl_start_y[1] = read_le32(f);
    n = read_le32(f);                         /* spot lights: skipped */
    for (i = 0; i < n * 3; i++) (void)read_le32(f);
    n = read_le32(f);                         /* steam: skipped */
    for (i = 0; i < n * 4; i++) (void)read_le32(f);
    if (fread(level_comment, 1, 20, f) != 20) level_comment[0] = 0;
    level_comment[20] = 0;
    level_time_limit = read_le32(f);
    for (i = 0; i < DIFF_ENEMIES; i++) level_enemies[i] = 0;
    for (i = 0; i < (level_version >= 4 ? DIFF_ENEMIES : DIFF_ENEMIES - 1); i++)
        level_enemies[i] = read_le32(f);

    if (level_version == 1) { weapons = DIFF_WEAPONS - 2; bullets = DIFF_BULLETS - 2; }
    else if (level_version == 2) { weapons = DIFF_WEAPONS - 1; bullets = DIFF_BULLETS - 1; }
    else { weapons = DIFF_WEAPONS; bullets = DIFF_BULLETS; }

    for (i = 0; i < DIFF_WEAPONS; i++) lev_weapon_crates[i] = 0;
    for (i = 0; i < DIFF_BULLETS; i++) lev_bullet_crates[i] = 0;
    for (i = 0; i < weapons; i++) lev_weapon_crates[i] = read_le32(f);
    for (i = 0; i < bullets; i++) lev_bullet_crates[i] = read_le32(f);
    lev_energy_crates = read_le32(f);
    for (i = 0; i < weapons + bullets + 1; i++) (void)read_le32(f);   /* deathmatch info */

    crate_info_amount = 0;
    if (level_version >= 5) {
        n = read_le32(f);
        for (i = 0; i < n; i++) {
            Crate_info ci;
            ci.type1 = read_le32(f);
            ci.type2 = read_le32(f);
            ci.x = read_le32(f);
            ci.y = read_le32(f);
            if (crate_info_amount < MAX_CRATE_INFO) crate_info[crate_info_amount++] = ci;
        }
        /* deathmatch crate positions follow; not used */
    }
    fclose(f);
    return 1;
}

/* Count LEVELn.LEV files per episode by probing (no directory listing). */
void count_episodes(void)
{
    char rel[64];
    s16 e, n;
    FILE *f;
    num_episodes = 0;
    for (e = 0; e < num_episode_names && e < 16; e++) {
        for (n = 0; n < MAX_LEVELS; n++) {
            sprintf(rel, "LEVS/%s/LEVEL%d.LEV", episode_names[e], n + 1);
            f = fopen(data_path(rel), "rb");
            if (!f) break;
            fclose(f);
        }
        episode_levels[e] = n;
        num_episodes = (s16)(e + 1);
    }
}
