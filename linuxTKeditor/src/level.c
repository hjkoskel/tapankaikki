/* level.c - level files (LEVS/...LEV): load versions 1-5, save version 5.
 * All values are 32-bit little endian integers:
 *   version, x size, y size, blocks (type, num, shadow) row by row,
 *   player 1 start x y, player 2 start x y (in blocks),
 *   spot lights: count, (x, y, size)*, steams: count, (x, y, angle, speed)*,
 *   comment (20 bytes), time limit, enemies (7 before version 4, then 8),
 *   normal game crates: weapons, bullets, energy (fewer weapons and bullets
 *   in versions 1 and 2), the same for deathmatch,
 *   version 5: normal game crates: count, (type1, type2, x, y)*, deathmatch crates. */
#include <stdio.h>
#include <string.h>
#include "ed.h"
#include "plat.h"

Level lev;
static char err[120];

const char *level_error(void)
{
    return err;
}

static s32 rd(FILE *f)
{
    u32 v = 0;
    int i, c;
    for (i = 0; i < 4; i++) {
        c = fgetc(f);
        if (c < 0) c = 0;
        v |= (u32)c << (8 * i);
    }
    if (v & 0x80000000UL) return -(s32)((~v & 0x7FFFFFFFUL) + 1);
    return (s32)v;
}

static void wr(FILE *f, s32 v)
{
    u32 u = (u32)v;
    int i;
    for (i = 0; i < 4; i++) fputc((int)((u >> (8 * i)) & 255), f);
}

static s32 clampi(s32 v, s32 lo, s32 hi)
{
    return v < lo ? lo : v > hi ? hi : v;
}

static void defaults(void)
{
    s16 i;
    lev.start_x[0] = 1; lev.start_y[0] = 1;
    lev.start_x[1] = 2; lev.start_y[1] = 1;
    lev.spots = lev.steams = 0;
    memset(lev.comment, 0, sizeof(lev.comment));
    lev.time_limit = 0;
    for (i = 0; i < DIFF_ENEMIES; i++) lev.enemies[i] = 0;
    memset(lev.rnd, 0, sizeof(lev.rnd));
    lev.crates[0] = lev.crates[1] = 0;
}

void level_new(s16 w, s16 h, s32 type, s32 num)
{
    s32 i;
    lev.version = LEVEL_VERSION;
    lev.w = clampi(w, MIN_LEVEL_W, MAX_LEVEL_W);
    lev.h = clampi(h, MIN_LEVEL_H, MAX_LEVEL_H);
    for (i = 0; i < lev.w * lev.h; i++) {
        lev.blk[i].type = type;
        lev.blk[i].num = num;
        lev.blk[i].shadow = 0;
    }
    defaults();
}

static void read_counts(FILE *f, Crate_counts *c, s16 weapons, s16 bullets)
{
    s16 i;
    memset(c, 0, sizeof(*c));
    for (i = 0; i < weapons; i++) c->weapon[i] = rd(f);
    for (i = 0; i < bullets; i++) c->bullet[i] = rd(f);
    c->energy = rd(f);
}

int level_load(const char *path)
{
    FILE *f = fopen(path, "rb");
    s32 i, n, w, h, v;
    s16 k, weapons, bullets;
    if (!f) {
        sprintf(err, "file not found");
        return 0;
    }
    v = rd(f);
    w = rd(f);
    h = rd(f);
    if (v < 1 || v > 5) {
        sprintf(err, "unknown level version %ld", (long)v);
        fclose(f);
        return 0;
    }
    if (w < MIN_LEVEL_W || h < MIN_LEVEL_H || w > MAX_LEVEL_W || h > MAX_LEVEL_H) {
        sprintf(err, "level size %ldx%ld not supported", (long)w, (long)h);
        fclose(f);
        return 0;
    }
    defaults();
    lev.version = v;
    lev.w = w;
    lev.h = h;
    for (i = 0; i < w * h; i++) {
        lev.blk[i].type = rd(f) != 0;
        lev.blk[i].num = clampi(rd(f), 0, NUM_TILES - 1);
        lev.blk[i].shadow = clampi(rd(f), 0, SHADOW_TYPES);
    }
    for (k = 0; k < 2; k++) {
        lev.start_x[k] = clampi(rd(f), 0, w - 1);
        lev.start_y[k] = clampi(rd(f), 0, h - 1);
    }
    n = rd(f);
    for (i = 0; i < n; i++) {
        Spot s;
        s.x = rd(f); s.y = rd(f); s.size = clampi(rd(f), 0, LIGHTS - 1);
        if (lev.spots < MAX_SPOTS) lev.spot[lev.spots++] = s;
    }
    n = rd(f);
    for (i = 0; i < n; i++) {
        Steam s;
        s.x = rd(f); s.y = rd(f); s.angle = rd(f); s.speed = rd(f);
        if (lev.steams < MAX_STEAM) lev.steam[lev.steams++] = s;
    }
    if (fread(lev.comment, 1, 20, f) != 20) memset(lev.comment, 0, sizeof(lev.comment));
    lev.comment[20] = 0;
    lev.time_limit = rd(f);
    for (k = 0; k < (v >= 4 ? DIFF_ENEMIES : DIFF_ENEMIES - 1); k++) lev.enemies[k] = rd(f);
    if (v == 1) { weapons = DIFF_WEAPONS - 2; bullets = DIFF_BULLETS - 2; }
    else if (v == 2) { weapons = DIFF_WEAPONS - 1; bullets = DIFF_BULLETS - 1; }
    else { weapons = DIFF_WEAPONS; bullets = DIFF_BULLETS; }
    read_counts(f, &lev.rnd[0], weapons, bullets);
    read_counts(f, &lev.rnd[1], weapons, bullets);
    if (v >= 5)
        for (k = 0; k < 2; k++) {
            n = rd(f);
            for (i = 0; i < n; i++) {
                Crate_info c;
                c.type1 = clampi(rd(f), 0, 2); c.type2 = rd(f); c.x = rd(f); c.y = rd(f);
                if (lev.crates[k] < MAX_CRATE_INFO) lev.crate[k][lev.crates[k]++] = c;
            }
        }
    fclose(f);
    return 1;
}

int level_save(const char *path)
{
    FILE *f;
    s32 i;
    s16 k, j;
    char comment[20];

    plat_make_dirs(path);
    f = fopen(path, "wb");
    if (!f) {
        sprintf(err, "can't write the file");
        return 0;
    }
    lev.version = LEVEL_VERSION;
    wr(f, lev.version);
    wr(f, lev.w);
    wr(f, lev.h);
    for (i = 0; i < lev.w * lev.h; i++) {
        wr(f, lev.blk[i].type);
        wr(f, lev.blk[i].num);
        wr(f, lev.blk[i].shadow);
    }
    for (k = 0; k < 2; k++) {
        wr(f, lev.start_x[k]);
        wr(f, lev.start_y[k]);
    }
    wr(f, lev.spots);
    for (i = 0; i < lev.spots; i++) {
        wr(f, lev.spot[i].x); wr(f, lev.spot[i].y); wr(f, lev.spot[i].size);
    }
    wr(f, lev.steams);
    for (i = 0; i < lev.steams; i++) {
        wr(f, lev.steam[i].x); wr(f, lev.steam[i].y); wr(f, lev.steam[i].angle); wr(f, lev.steam[i].speed);
    }
    memcpy(comment, lev.comment, 20);     /* as loaded: bytes after the 0 are kept */
    fwrite(comment, 1, 20, f);
    wr(f, lev.time_limit);
    for (k = 0; k < DIFF_ENEMIES; k++) wr(f, lev.enemies[k]);
    for (k = 0; k < 2; k++) {
        for (j = 0; j < DIFF_WEAPONS; j++) wr(f, lev.rnd[k].weapon[j]);
        for (j = 0; j < DIFF_BULLETS; j++) wr(f, lev.rnd[k].bullet[j]);
        wr(f, lev.rnd[k].energy);
    }
    for (k = 0; k < 2; k++) {
        wr(f, lev.crates[k]);
        for (i = 0; i < lev.crates[k]; i++) {
            const Crate_info *c = &lev.crate[k][i];
            wr(f, c->type1); wr(f, c->type2); wr(f, c->x); wr(f, c->y);
        }
    }
    if (fclose(f) != 0) {
        sprintf(err, "write error");
        return 0;
    }
    return 1;
}

/* Automatic shadows, light from the upper right. The rule is read from the
 * original levels: a floor block gets shadow 3 below a wall, 2 left of a
 * wall and 1 (corner) when the wall above-right or both of them are walls.
 * Blocks outside the level count as walls. Wall blocks get no shadow. */
static int is_wall(s32 x, s32 y)
{
    if (x < 0 || y < 0 || x >= lev.w || y >= lev.h) return 1;
    return BLK(x, y).type != 0;
}

void level_auto_shadows(void)
{
    s32 x, y;
    int n, ne, e;
    for (y = 0; y < lev.h; y++)
        for (x = 0; x < lev.w; x++) {
            Block *b = &BLK(x, y);
            if (b->type) {
                b->shadow = 0;
                continue;
            }
            n = is_wall(x, y - 1);
            ne = is_wall(x + 1, y - 1);
            e = is_wall(x + 1, y);
            if (ne || (n && e)) b->shadow = 1;
            else if (n) b->shadow = 3;
            else if (e) b->shadow = 2;
            else b->shadow = 0;
        }
}
