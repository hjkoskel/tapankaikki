/* conv64.c - converts the original TK data into Commodore 64 files.
 * Runs on Linux, reuses the linuxTK loaders (../linuxTK/src).
 *
 * Output (into the directory given as argv[2], default "build"):
 *   tkchr.prg    charset for $C800 (chars 0-63 are filled from ROM at run time)
 *   tksprd.prg   first 64 sprites, copied to $D000 by the game
 *   tkspre.prg   remaining sprites, loaded straight to $E000
 *   gfxdata.c/h  metatile tables, colours, sprite pointer bases
 *   l<e><nn>.prg levels (e = episode letter a.., nn = level)
 *   preview.ppm  tiles and sprites drawn with the C64 palette (for checking)
 *
 * Graphics overrides (GRAPHICS.md): directory $CONV64_GFX (default "gfx") holds
 * one PNG sheet per graphics group. A valid PNG is used instead of the converted
 * graphics; a missing, wrong sized or unreadable one is (re)generated from the
 * original files and written there (an unreadable/wrong sized file is first
 * renamed to <name>.png.bad).
 *
 * Usage: conv64 <tk data dir> <out dir> [episodes]   (episodes: e.g. "1" or "12")
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <png.h>
#include <sys/stat.h>
#include "../../linuxTK/src/globals.h"

/* ------------------------------------------------------------------ */
/* C64 palette (Pepto)                                                 */

static const int c64rgb[16][3] = {
    {0, 0, 0}, {255, 255, 255}, {104, 55, 43}, {112, 164, 178}, {111, 61, 134}, {88, 141, 67},
    {53, 40, 121}, {184, 199, 111}, {111, 79, 37}, {67, 57, 0}, {154, 103, 89}, {68, 68, 68},
    {108, 108, 108}, {154, 210, 132}, {108, 94, 181}, {149, 149, 149}
};

static int pal_r(int i) { return pal_game[i * 3] * 4; }
static int pal_g(int i) { return pal_game[i * 3 + 1] * 4; }
static int pal_b(int i) { return pal_game[i * 3 + 2] * 4; }

/* colour distance: brightness matters most (keeps walls/floors readable) */
static int luma(int r, int g, int b);
static long cdist(int r, int g, int b, int c)
{
    int l1 = luma(r, g, b), l2 = luma(c64rgb[c][0], c64rgb[c][1], c64rgb[c][2]);
    long dl = l1 - l2;
    long dr = (r - l1) - (c64rgb[c][0] - l2), dg = (g - l1) - (c64rgb[c][1] - l2), db = (b - l1) - (c64rgb[c][2] - l2);
    return dl * dl * 6 + dr * dr + dg * dg + db * db;
}

static int luma(int r, int g, int b) { return (r * 3 + g * 6 + b) / 10; }

static char outdir[512];

static FILE *out_file(const char *name, int load_addr)
{
    char p[1024];
    FILE *f;
    sprintf(p, "%s/%s", outdir, name);
    f = fopen(p, "wb");
    if (!f) { printf("cannot write %s\n", p); exit(1); }
    if (load_addr >= 0) { fputc(load_addr & 255, f); fputc(load_addr >> 8, f); }
    return f;
}

/* ------------------------------------------------------------------ */
/* tiles                                                               */

/* key = type * 160 + num: 0..159 floors, 160..319 walls */
#define NKEYS 320
static long usage[NKEYS];
static long pal_usage[NKEYS];              /* usage in the converted episodes only */
static int tile_rgb[NKEYS][16][8][3];      /* tile downscaled to 8 fat px x 16 rows */
static unsigned char tq[NKEYS][16][8];     /* quantised 0..3 */
static int rep[NKEYS];                     /* merged representative */
static int gid[NKEYS];                     /* global metatile id (bit 7 = wall) */
static int pal4[4];                        /* bg, d022, d023, colour ram */

#define FLOOR_IDS_MAX 121                  /* 121..124 = decals, 125..127 = crate tiles */
#define DECAL_TILE    121
#define CHAR_DECALS   224                  /* 4 decal metatiles = 16 chars */
#define WALL_IDS_MAX  128
#define CHAR_FIRST    64
#define CHAR_TILES_MAX 160                 /* chars 64..223 */
#define CHAR_CRATES   240                  /* 3 crate metatiles = 12 chars */

static void downscale_tile(int key)
{
    const u8 *src = (key < 160 ? frm_floor.pix : frm_walls.pix) + (key % 160) * 400;
    int fx, ry, sx, sy, r, g, b, n, px, py, c;
    for (ry = 0; ry < 16; ry++)
        for (fx = 0; fx < 8; fx++) {
            r = g = b = n = 0;
            for (sy = 0; sy < 5; sy++)
                for (sx = 0; sx < 5; sx++) {
                    px = ((fx * 5 + sx) * 20) / 40;
                    py = ((ry * 5 + sy) * 20) / 80;
                    c = src[py * 20 + px];
                    r += pal_r(c); g += pal_g(c); b += pal_b(c); n++;
                }
            tile_rgb[key][ry][fx][0] = r / n;
            tile_rgb[key][ry][fx][1] = g / n;
            tile_rgb[key][ry][fx][2] = b / n;
        }
}

static long weight(long u)
{
    long r = 1;
    while (r * r < u) r++;      /* sqrt: big floors count, but don't drown everything */
    return r;
}

/* Map colours: tiles use only the three shared colours (background, $D022,
 * $D023). The colour RAM colour (char bits 11) is fixed to red and kept for
 * gore and crates only, so blood is red without any colour RAM updates. */
#define TILE_SLOTS 3
#define GORE_RED   2

static void choose_palette(void)
{
    long best = -1, err, e, m;
    int a, b, c, k, y, x, i, cols[4];
    for (a = 0; a < 16; a++)
        for (b = a + 1; b < 16; b++)
            for (c = b + 1; c < 16; c++) {
                if (a == GORE_RED || b == GORE_RED || c == GORE_RED) continue;
                if (a != 0) continue;                 /* black is needed for shapes */
                cols[0] = a; cols[1] = b; cols[2] = c; cols[3] = GORE_RED;
                err = 0;
                for (k = 0; k < NKEYS && (best < 0 || err < best); k++) {
                    if (!pal_usage[k]) continue;
                    for (y = 0; y < 16; y++)
                        for (x = 0; x < 8; x++) {
                            m = -1;
                            for (i = 0; i < TILE_SLOTS; i++) {
                                e = cdist(tile_rgb[k][y][x][0], tile_rgb[k][y][x][1], tile_rgb[k][y][x][2], cols[i]);
                                if (m < 0 || e < m) m = e;
                            }
                            err += m * weight(pal_usage[k]);
                        }
                }
                if (best < 0 || err < best) {
                    best = err;
                    memcpy(pal4, cols, sizeof(pal4));
                }
            }
    /* manual override, e.g. CONV64_PAL=0,9,8 (background, $D022, $D023) */
    if (getenv("CONV64_PAL") && sscanf(getenv("CONV64_PAL"), "%d,%d,%d", &a, &b, &c) == 3) {
        pal4[0] = a; pal4[1] = b; pal4[2] = c; pal4[3] = GORE_RED;
    }
    printf("map colours: bg %d, d022 %d, d023 %d, colour ram (gore) %d\n", pal4[0], pal4[1], pal4[2], pal4[3]);
}

static void quantise_tiles(void)
{
    int k, y, x, i, bi;
    long e, m;
    for (k = 0; k < NKEYS; k++) {
        for (y = 0; y < 16; y++)
            for (x = 0; x < 8; x++) {
                m = -1; bi = 0;
                for (i = 0; i < TILE_SLOTS; i++) {
                    e = cdist(tile_rgb[k][y][x][0], tile_rgb[k][y][x][1], tile_rgb[k][y][x][2], pal4[i]);
                    if (m < 0 || e < m) { m = e; bi = i; }
                }
                tq[k][y][x] = (unsigned char)bi;
            }
    }
}

static void tile_char(int k, int part, unsigned char *out)
{
    int r, x, oy = (part >> 1) * 8, ox = (part & 1) * 4;
    for (r = 0; r < 8; r++) {
        out[r] = 0;
        for (x = 0; x < 4; x++) out[r] = (unsigned char)(out[r] | (tq[k][oy + r][ox + x] << (6 - x * 2)));
    }
}

static unsigned char charset[256][8];
static int nchars;

/* ---- character level clustering ---------------------------------- */

#define MAXU (NKEYS * 4)
static unsigned char uchar[MAXU][8];
static long uw[MAXU];
static int urep[MAXU], nu;
static unsigned char ulock[MAXU];  /* char of a hand edited tile: never replaced */
static unsigned char tlock[NKEYS]; /* tile was hand edited in the override PNG */
static int tcidx[NKEYS][4];        /* tile part -> unique char index */
static int dm[MAXU][MAXU];
static long pdist[4][4];

static int char_rep(int i) { while (urep[i] != i) i = urep[i]; return i; }

static int char_dist(int a, int b)
{
    int r, x, d = 0;
    for (r = 0; r < 8; r++)
        for (x = 0; x < 4; x++)
            d += (int)pdist[(uchar[a][r] >> (6 - x * 2)) & 3][(uchar[b][r] >> (6 - x * 2)) & 3];
    return d;
}

static int count_reps(int wall)
{
    int k, n = 0;
    for (k = wall ? 160 : 0; k < (wall ? 320 : 160); k++)
        if (usage[k] && rep[k] == k) n++;
    return n;
}

static int same_tile(int a, int b)
{
    int p;
    if ((a < 160) != (b < 160)) return 0;
    for (p = 0; p < 4; p++)
        if (char_rep(tcidx[a][p]) != char_rep(tcidx[b][p])) return 0;
    return 1;
}

static void merge_tiles(void)
{
    int k, p, i, j, a, b, active, bi = -1, bj = -1;
    long cost, bcost, wi, wj;
    unsigned char c[8];
    for (i = 0; i < 4; i++)
        for (j = 0; j < 4; j++) {
            pdist[i][j] = cdist(c64rgb[pal4[i]][0], c64rgb[pal4[i]][1], c64rgb[pal4[i]][2], pal4[j]) / 256;
            if (i != j && pdist[i][j] < 1) pdist[i][j] = 1;
        }
    for (k = 0; k < NKEYS; k++) rep[k] = k;
    nu = 0;
    for (k = 0; k < NKEYS; k++) {
        if (!usage[k]) continue;
        for (p = 0; p < 4; p++) {
            tile_char(k, p, c);
            for (i = 0; i < nu && memcmp(uchar[i], c, 8); i++) ;
            if (i == nu) { memcpy(uchar[nu], c, 8); uw[nu] = 0; urep[nu] = nu; nu++; }
            uw[i] += weight(usage[k]);
            tcidx[k][p] = i;
        }
    }
    memset(ulock, 0, sizeof(ulock));
    for (k = 0; k < NKEYS; k++)
        if (usage[k] && tlock[k])
            for (p = 0; p < 4; p++) ulock[tcidx[k][p]] = 1;
    for (i = 0; i < nu; i++)
        for (j = i + 1; j < nu; j++) dm[i][j] = dm[j][i] = char_dist(i, j);
    for (active = nu; active > CHAR_TILES_MAX; active--) {
        bcost = -1;
        for (i = 0; i < nu; i++) {
            if (urep[i] != i) continue;
            for (j = i + 1; j < nu; j++) {
                if (urep[j] != j || (ulock[i] && ulock[j])) continue;
                wi = uw[i]; wj = uw[j];
                cost = (long)dm[i][j] * (wi < wj ? wi : wj);
                if (bcost < 0 || cost < bcost) { bcost = cost; bi = i; bj = j; }
            }
        }
        if (bcost < 0) {
            printf("too many different chars in hand edited tiles (max %d)\n", CHAR_TILES_MAX);
            exit(1);
        }
        /* the less used char takes the look of the more used; a hand edited
         * char always keeps its look */
        if (ulock[bj] || (!ulock[bi] && uw[bi] < uw[bj])) { i = bi; bi = bj; bj = i; }
        urep[bj] = bi;
        uw[bi] += uw[bj];
    }
    /* tiles that now look the same become one metatile */
    for (a = 0; a < NKEYS; a++)
        for (b = a + 1; b < NKEYS; b++)
            if (usage[a] && usage[b] && rep[a] == a && rep[b] == b && same_tile(a, b)) {
                rep[b] = a;
                usage[a] += usage[b];
            }
    printf("unique chars %d -> %d, floors %d, walls %d\n", nu, active, count_reps(0), count_reps(1));
    if (count_reps(0) > FLOOR_IDS_MAX || count_reps(1) > WALL_IDS_MAX) { printf("too many metatiles\n"); exit(1); }
}

static unsigned char mt[4][256];   /* metatile -> char, by part */
static int char_code[MAXU];

static void build_metatiles(void)
{
    int k, p, nf = 0, nw = 0, id, u;
    for (u = 0; u < MAXU; u++) char_code[u] = -1;
    for (k = 0; k < NKEYS; k++) gid[k] = -1;
    for (k = 0; k < NKEYS; k++) {
        if (!usage[k] || rep[k] != k) continue;
        id = k < 160 ? nf++ : 128 + nw++;
        gid[k] = id;
        for (p = 0; p < 4; p++) {
            u = char_rep(tcidx[k][p]);
            if (char_code[u] < 0) {
                char_code[u] = CHAR_FIRST + nchars++;
                memcpy(charset[char_code[u]], uchar[u], 8);
            }
            mt[p][id] = (unsigned char)char_code[u];
        }
    }
    for (k = 0; k < NKEYS; k++)
        if (usage[k] && rep[k] != k) gid[k] = gid[rep[k]];
    /* unused ids: draw as the first metatile of the class */
    for (id = 0; id < 256; id++)
        if (!mt[0][id] && !(id >= DECAL_TILE && id <= 127))
            for (p = 0; p < 4; p++) mt[p][id] = mt[p][id < 128 ? 0 : 128];
}
/* crate metatiles 125..127: floor colour with a crate drawn in colour RAM colour */
/* decal metatiles 121..124 (blood, more blood, gibs, scorch), drawn over the
 * most used floor tile; the game swaps a floor tile to one of these */
static int nearest_slot(int r, int g, int b)
{
    int i, bi = 0;
    long m = -1, e;
    for (i = 0; i < 4; i++) {
        e = (long)(r - c64rgb[pal4[i]][0]) * (r - c64rgb[pal4[i]][0]) + (long)(g - c64rgb[pal4[i]][1]) * (g - c64rgb[pal4[i]][1]) +
            (long)(b - c64rgb[pal4[i]][2]) * (b - c64rgb[pal4[i]][2]);
        if (m < 0 || e < m) { m = e; bi = i; }
    }
    return bi;
}

static void build_decals(void)
{
    int base = -1, k, d, y, x, p, r, i, c, sx, sy;
    long bu = -1;
    unsigned char q[16][8];
    int dark = nearest_slot(0, 0, 0), red = 3;                 /* slot 3 = colour RAM = red */
    for (k = 0; k < 160; k++) if (usage[k] && rep[k] == k && usage[k] > bu) { bu = usage[k]; base = k; }
    for (d = 0; d < 4; d++) {
        memcpy(q, tq[base], sizeof(q));
        srand(d * 7 + 3);
        if (d < 2) {                        /* blood: splat frames scattered */
            for (i = 0; i < (d ? 7 : 4); i++) {
                int ox = rand() % 6, oy = rand() % 12, f = rand() % 8;
                for (y = 0; y < 6; y++)
                    for (x = 0; x < 6; x += 2) {
                        c = frm_effect[BLOOD2].pix[f * 36 + y * 6 + x];
                        if (c && ox + x / 2 < 8 && oy + y < 16) q[oy + y][ox + x / 2] = (unsigned char)(rand() % 6 ? red : dark);
                    }
            }
        } else if (d == 2) {                /* gibs: two body part sprites */
            for (i = 0; i < 2; i++) {
                int part = i ? 3 : 0, ox = i * 3, oy = i * 6;
                for (y = 0; y < 10; y++)
                    for (x = 0; x < 5; x++) {
                        sx = x * 4; sy = y * 2;
                        c = frm_bodypart.pix[part * 400 + sy * 20 + sx];
                        if (c && ox + x < 8 && oy + y < 16)
                            q[oy + y][ox + x] = (unsigned char)nearest_slot(pal_r(c), pal_g(c), pal_b(c));
                    }
            }
        } else {                            /* scorch: dark blotch */
            for (y = 0; y < 16; y++)
                for (x = 0; x < 8; x++) {
                    int dx = x * 2 - 7, dy = y - 8;
                    if (dx * dx + dy * dy < 40 + rand() % 25) q[y][x] = (unsigned char)dark;
                }
        }
        for (p = 0; p < 4; p++) {
            unsigned char ch[8];
            for (r = 0; r < 8; r++) {
                ch[r] = 0;
                for (x = 0; x < 4; x++) ch[r] = (unsigned char)(ch[r] | (q[(p >> 1) * 8 + r][(p & 1) * 4 + x] << (6 - x * 2)));
            }
            memcpy(charset[CHAR_DECALS + d * 4 + p], ch, 8);
            mt[p][DECAL_TILE + d] = (unsigned char)(CHAR_DECALS + d * 4 + p);
        }
    }
}

static void build_crates(void)
{
    int t, p, r, x, y, sx, sy, ch;
    unsigned char q[16][8];
    for (t = 0; t < CRATE_TYPES; t++) {
        for (y = 0; y < 16; y++)
            for (x = 0; x < 8; x++) {
                int rr = 0, gg = 0, bb = 0, n = 0, i, bi = 0;
                long m = -1, e;
                for (sy = 0; sy < 2; sy++)
                    for (sx = 0; sx < 2; sx++) {
                        int px = (x * 2 + sx) * 14 / 16, py = (y + sy / 2) * 14 / 16;
                        int c = frm_crate.pix[t * 196 + py * 14 + px];
                        if (c) { rr += pal_r(c); gg += pal_g(c); bb += pal_b(c); n++; }
                    }
                if (n < 2) { q[y][x] = 0; continue; }
                for (i = 0; i < 4; i++) {
                    e = cdist(rr / n, gg / n, bb / n, pal4[i]);
                    if (m < 0 || e < m) { m = e; bi = i; }
                }
                q[y][x] = (unsigned char)bi;
            }
        for (p = 0; p < 4; p++) {
            unsigned char c[8];
            for (r = 0; r < 8; r++) {
                c[r] = 0;
                for (x = 0; x < 4; x++) c[r] = (unsigned char)(c[r] | (q[(p >> 1) * 8 + r][(p & 1) * 4 + x] << (6 - x * 2)));
            }
            ch = CHAR_CRATES + t * 4 + p;
            memcpy(charset[ch], c, 8);
            mt[p][125 + t] = (unsigned char)ch;
        }
    }
}

/* ------------------------------------------------------------------ */
/* sprites                                                             */

#define NSPR 171
static unsigned char spr[NSPR][64];
static int spr_mc[NSPR];          /* 1 = multicolour */
static int spr_col[NSPR];         /* colour used for preview */
static const int MC0 = 0, MC1 = 15;

/* 28x28 frame -> 12x21 multicolour. recolor = treat colours 80..95 as own colour */
static void mc_sprite(const u8 *src, int idx, int own, int recolor)
{
    int x, y, sx, sy, r, g, b, n, tot, own_votes, l, bits, px, py, c;
    memset(spr[idx], 0, 64);
    for (y = 0; y < 21; y++)
        for (x = 0; x < 12; x++) {
            r = g = b = n = tot = own_votes = 0;
            for (sy = 0; sy < 4; sy++)
                for (sx = 0; sx < 4; sx++) {
                    px = ((x * 4 + sx) * 28) / 48;
                    py = ((y * 4 + sy) * 28) / 84;
                    c = src[py * 28 + px];
                    tot++;
                    if (!c) continue;
                    n++;
                    r += pal_r(c); g += pal_g(c); b += pal_b(c);
                    if (recolor && c >= 80 && c <= 95) own_votes++;
                }
            if (n * 2 < tot) continue;
            l = luma(r / n, g / n, b / n);
            if (own_votes * 2 >= n) bits = 2;
            else if (l < 60) bits = 1;           /* MC0: dark */
            else if (l > 150) bits = 3;          /* MC1: bright */
            else bits = 2;                        /* own colour */
            spr[idx][y * 3 + x / 4] = (unsigned char)(spr[idx][y * 3 + x / 4] | (bits << (6 - (x & 3) * 2)));
        }
    spr_mc[idx] = 1;
    spr_col[idx] = own;
}

/* any frame -> hi-res 24x21, scaled by sw/sh and centred */
static void hr_sprite(const u8 *src, int w, int h, int tw, int th, int idx, int col, int minluma)
{
    int x, y, c, ox = (24 - tw) / 2, oy = (21 - th) / 2;
    memset(spr[idx], 0, 64);
    for (y = 0; y < th; y++)
        for (x = 0; x < tw; x++) {
            c = src[(y * h / th) * w + x * w / tw];
            if (c && luma(pal_r(c), pal_g(c), pal_b(c)) >= minluma)
                spr[idx][(y + oy) * 3 + (x + ox) / 8] |= (unsigned char)(0x80 >> ((x + ox) & 7));
        }
    spr_mc[idx] = 0;
    spr_col[idx] = col;
}

#define ACTOR_FRAME(f, dir, anims, anim) ((f).pix + ((dir) * (anims) + (anim)) * 784)

static const int look_type[4] = { 0, 1, 7, 5 };   /* enemy sheets used for looks A..D */
static const int look_col[4] = { 5, 9, 7, 3 };    /* own colour shown in the PNG sheets */

static void build_sprites(void)
{
    int d, i = 0, l, e;
    /* player: 16 x (stand, walk, fire) + 2 dead */
    for (d = 0; d < 16; d++) {
        mc_sprite(ACTOR_FRAME(frm_player, d, PLAYER_ANIMS, 4), i++, 10, 1);
        mc_sprite(ACTOR_FRAME(frm_player, d, PLAYER_ANIMS, 3), i++, 10, 1);
        mc_sprite(ACTOR_FRAME(frm_player, d, PLAYER_ANIMS, 6), i++, 10, 1);
    }
    mc_sprite(ACTOR_FRAME(frm_player, 0, PLAYER_ANIMS, 9), i++, 10, 1);
    mc_sprite(ACTOR_FRAME(frm_player, 0, PLAYER_ANIMS, 10), i++, 10, 1);
    /* enemy looks A-C: 16 x (stand, walk); D: 16 x stand */
    for (l = 0; l < 4; l++) {
        e = look_type[l];
        for (d = 0; d < 16; d++) {
            mc_sprite(ACTOR_FRAME(frm_enemy[e], d, ENEMY_ANIMS, 1), i++, look_col[l], 0);
            if (l < 3) mc_sprite(ACTOR_FRAME(frm_enemy[e], d, ENEMY_ANIMS, 0), i++, look_col[l], 0);
        }
    }
    /* explosion: 4 of the 9 frames, hi-res yellow */
    for (d = 0; d < 4; d++)
        hr_sprite(frm_effect[EXPLO].pix + (d * 2) * 625, 25, 25, 22, 19, i++, 7, 40);
    hr_sprite(frm_bomb.pix, 7, 7, 6, 6, i++, 1, 1);                  /* grenade */
    hr_sprite(frm_c4.pix, 8, 8, 6, 6, i++, 8, 1);                    /* C4 */
    hr_sprite(frm_mine.pix, 7, 7, 6, 6, i++, 12, 1);                 /* mine */
    hr_sprite(frm_effect[EXPLO].pix + 3 * 625, 25, 25, 12, 10, i++, 8, 40); /* flame puff */
    hr_sprite(frm_bodypart.pix, 20, 20, 14, 12, i++, 2, 1);          /* gib */
    if (i != NSPR) { printf("sprite count %d != %d\n", i, NSPR); exit(1); }
}

/* ------------------------------------------------------------------ */
/* graphics overrides: PNG sheets in $CONV64_GFX (see GRAPHICS.md)     */

static char gfxdir[512];
static int pal_png[4];                    /* map colours the sheets were saved with */
static int recolour;                      /* they differ from pal4: re-save the sheets */

/* map_colours.txt remembers the map colours of the saved sheets, so a sheet
 * made with other colours (CONV64_PAL) is read by its own colours */
static void map_colours_load(void)
{
    char path[1024];
    FILE *f;
    sprintf(path, "%s/map_colours.txt", gfxdir);
    memcpy(pal_png, pal4, sizeof(pal_png));
    f = fopen(path, "r");
    if (f) {
        if (fscanf(f, "%d %d %d %d", &pal_png[0], &pal_png[1], &pal_png[2], &pal_png[3]) != 4)
            memcpy(pal_png, pal4, sizeof(pal_png));
        fclose(f);
    }
    recolour = memcmp(pal_png, pal4, sizeof(pal_png)) != 0;
    if (recolour) printf("gfx: map colours changed, map sheets are re-saved with the new colours\n");
}

static void map_colours_save(void)
{
    char path[1024];
    FILE *f;
    sprintf(path, "%s/map_colours.txt", gfxdir);
    f = fopen(path, "w");
    if (!f) return;
    fprintf(f, "%d %d %d %d\n# background $D022 $D023 colour-RAM: colours of the map PNG sheets\n",
            pal4[0], pal4[1], pal4[2], pal4[3]);
    fclose(f);
}
static unsigned char *sheet;              /* RGBA of the sheet being handled */
static int sheet_w;

static void px_set(int x, int y, int c)   /* c = C64 colour, -1 = transparent */
{
    unsigned char *o = sheet + ((long)y * sheet_w + x) * 4;
    if (c < 0) { o[0] = o[1] = o[2] = o[3] = 0; return; }
    o[0] = (unsigned char)c64rgb[c][0]; o[1] = (unsigned char)c64rgb[c][1];
    o[2] = (unsigned char)c64rgb[c][2]; o[3] = 255;
}

/* nearest of n allowed C64 colours (index into cols), -1 = transparent */
static int px_get(int x, int y, const int *cols, int n)
{
    const unsigned char *o = sheet + ((long)y * sheet_w + x) * 4;
    long m = -1, e, dr, dg, db;
    int i, bi = 0;
    if (o[3] < 128) return -1;
    for (i = 0; i < n; i++) {
        dr = o[0] - c64rgb[cols[i]][0]; dg = o[1] - c64rgb[cols[i]][1]; db = o[2] - c64rgb[cols[i]][2];
        e = dr * dr + dg * dg + db * db;
        if (m < 0 || e < m) { m = e; bi = i; }
    }
    return bi;
}

/* PNG of exactly w x h into buf (RGBA); 1 = ok, else why says what is wrong */
static int png_read(const char *path, int w, int h, unsigned char *buf, char *why)
{
    png_image im;
    FILE *f = fopen(path, "rb");
    if (!f) { strcpy(why, "missing"); return 0; }
    fclose(f);
    memset(&im, 0, sizeof(im));
    im.version = PNG_IMAGE_VERSION;
    if (!png_image_begin_read_from_file(&im, path)) {
        sprintf(why, "unreadable (%s)", im.message);
        return 0;
    }
    if ((int)im.width != w || (int)im.height != h) {
        sprintf(why, "wrong size %ux%u, must be %dx%d", im.width, im.height, w, h);
        png_image_free(&im);
        return 0;
    }
    im.format = PNG_FORMAT_RGBA;
    if (!png_image_finish_read(&im, NULL, buf, 0, NULL)) {
        sprintf(why, "unreadable (%s)", im.message);
        return 0;
    }
    return 1;
}

static void sheet_new(int w, int h)
{
    free(sheet);
    sheet = calloc((size_t)w * h, 4);
    sheet_w = w;
}

/* 1 = the PNG is valid and loaded into sheet (use it); 0 = sheet must be
 * filled with the generated graphics and then saved with sheet_save() */
static int sheet_open(const char *name, int w, int h)
{
    char path[1024], bad[1100], why[300];
    sprintf(path, "%s/%s", gfxdir, name);
    sheet_new(w, h);
    if (png_read(path, w, h, sheet, why)) {
        printf("gfx: %-18s used\n", name);
        return 1;
    }
    if (!strcmp(why, "missing")) printf("gfx: %-18s missing, generated\n", name);
    else {
        sprintf(bad, "%s.bad", path);
        rename(path, bad);
        printf("gfx: %-18s %s -> kept as %s.bad, generated\n", name, why, name);
    }
    memset(sheet, 0, (size_t)w * h * 4);
    return 0;
}

static void sheet_save(const char *name, int w, int h)
{
    char path[1024];
    png_image im;
    sprintf(path, "%s/%s", gfxdir, name);
    memset(&im, 0, sizeof(im));
    im.version = PNG_IMAGE_VERSION;
    im.width = (png_uint_32)w;
    im.height = (png_uint_32)h;
    im.format = PNG_FORMAT_RGBA;
    if (!png_image_write_to_file(&im, path, 0, sheet, 0, NULL)) {
        printf("gfx: cannot write %s (%s)\n", path, im.message);
        exit(1);
    }
}

/* map graphics: 16x16 px metatile = 8x16 fat pixels, slots 0..3 of pal4 */
static void meta_draw(int ox, int oy, unsigned char q[16][8])
{
    int x, y;
    for (y = 0; y < 16; y++)
        for (x = 0; x < 8; x++) {
            px_set(ox + x * 2, oy + y, pal4[q[y][x]]);
            px_set(ox + x * 2 + 1, oy + y, pal4[q[y][x]]);
        }
}

static void meta_read(int ox, int oy, unsigned char q[16][8])
{
    int x, y, c;
    for (y = 0; y < 16; y++)
        for (x = 0; x < 8; x++) {
            c = px_get(ox + x * 2, oy + y, pal_png, 4);  /* left pixel of the pair */
            q[y][x] = (unsigned char)(c < 0 ? 0 : c);    /* transparent = background */
        }
}

/* metatile id <-> fat pixels through the charset */
static void meta_get(int id, unsigned char q[16][8])
{
    int p, r, x;
    for (p = 0; p < 4; p++)
        for (r = 0; r < 8; r++)
            for (x = 0; x < 4; x++)
                q[(p >> 1) * 8 + r][(p & 1) * 4 + x] = (unsigned char)((charset[mt[p][id]][r] >> (6 - x * 2)) & 3);
}

static void meta_put(int id, unsigned char q[16][8])
{
    int p, r, x;
    unsigned char *c;
    for (p = 0; p < 4; p++)
        for (r = 0; r < 8; r++) {
            c = &charset[mt[p][id]][r];
            *c = 0;
            for (x = 0; x < 4; x++) *c = (unsigned char)(*c | (q[(p >> 1) * 8 + r][(p & 1) * 4 + x] << (6 - x * 2)));
        }
}

/* tiles: tiles_floor.png / tiles_walls.png, 16 x 10 tiles of 16x16 in the
 * order of the original sheets. Runs before merge_tiles.
 * gfx/generated/ keeps the sheets as the converter last made them: a tile that
 * differs from that is hand edited, is used as drawn and keeps its chars in
 * the merge. Other tiles follow the converter (new colours, better conversion),
 * and the sheet is re-saved to show what the game uses. */
static unsigned char qpng[NKEYS][16][8];

static void override_tiles(void)
{
    int w, k, n, use, have_ref, edited = 0, resave;
    unsigned char q[16][8];
    char path[1024], why[300];
    sprintf(path, "%s/generated", gfxdir);
    mkdir(path, 0777);
    for (w = 0; w < 2; w++) {
        const char *name = w ? "tiles_walls.png" : "tiles_floor.png";
        use = sheet_open(name, 256, 160);
        if (use)
            for (n = 0; n < 160; n++) meta_read((n % 16) * 16, (n / 16) * 16, qpng[w * 160 + n]);
        /* reference: the last generated sheet */
        sprintf(path, "%s/generated/%s", gfxdir, name);
        have_ref = png_read(path, 256, 160, sheet, why);
        resave = !use;
        for (n = 0; n < 160; n++) {
            k = w * 160 + n;
            if (have_ref) meta_read((n % 16) * 16, (n / 16) * 16, q);
            else memcpy(q, tq[k], sizeof(q));
            if (use && memcmp(qpng[k], q, sizeof(q))) {
                memcpy(tq[k], qpng[k], sizeof(q));     /* hand edited */
                tlock[k] = 1;
                if (usage[k]) edited++;
            } else if (use && memcmp(qpng[k], tq[k], sizeof(q))) resave = 1;
        }
        /* new reference: the converter's own tiles (edited ones from tile_rgb again) */
        memset(sheet, 0, (size_t)256 * 160 * 4);
        for (n = 0; n < 160; n++) {
            k = w * 160 + n;
            if (tlock[k]) {
                int y, x, i, bi;
                long e, m;
                for (y = 0; y < 16; y++)
                    for (x = 0; x < 8; x++) {
                        m = -1; bi = 0;
                        for (i = 0; i < TILE_SLOTS; i++) {
                            e = cdist(tile_rgb[k][y][x][0], tile_rgb[k][y][x][1], tile_rgb[k][y][x][2], pal4[i]);
                            if (m < 0 || e < m) { m = e; bi = i; }
                        }
                        q[y][x] = (unsigned char)bi;
                    }
                meta_draw((n % 16) * 16, (n / 16) * 16, q);
            } else meta_draw((n % 16) * 16, (n / 16) * 16, tq[k]);
        }
        sprintf(path, "generated/%s", name);
        sheet_save(path, 256, 160);
        if (resave || recolour) {
            for (n = 0; n < 160; n++) meta_draw((n % 16) * 16, (n / 16) * 16, tq[w * 160 + n]);
            sheet_save(name, 256, 160);
        }
    }
    if (edited) printf("gfx: %d hand edited tiles in use\n", edited);
}

/* decals.png (blood, blood 2, gibs, scorch) and crates.png (weapon, ammo,
 * energy): rows of 16x16 metatiles */
static void override_metas(const char *name, int first, int n)
{
    int i;
    unsigned char q[16][8];
    if (sheet_open(name, n * 16, 16)) {
        for (i = 0; i < n; i++) {
            meta_read(i * 16, 0, q);
            meta_put(first + i, q);
            if (recolour) meta_draw(i * 16, 0, q);
        }
        if (recolour) sheet_save(name, n * 16, 16);
    } else {
        for (i = 0; i < n; i++) {
            meta_get(first + i, q);
            meta_draw(i * 16, 0, q);
        }
        sheet_save(name, n * 16, 16);
    }
}

/* sprites: cells of 24x21 px. Multicolour: pixel pairs; black = MC0,
 * light grey = MC1, any other colour = the sprite's own colour (drawn as
 * `own`). Hi-res: any opaque pixel is set (drawn as `col`). */
static void spr_draw(int idx, int ox, int oy, int mc, int own)
{
    int x, y, b, c;
    for (y = 0; y < 21; y++)
        for (x = 0; x < 24; x++) {
            if (mc) {
                b = (spr[idx][y * 3 + x / 8] >> (6 - ((x / 2) & 3) * 2)) & 3;
                c = b == 1 ? MC0 : b == 2 ? own : b == 3 ? MC1 : -1;
            } else c = (spr[idx][y * 3 + x / 8] & (0x80 >> (x & 7))) ? own : -1;
            px_set(ox + x, oy + y, c);
        }
}

static void spr_read(int idx, int ox, int oy, int mc, int own)
{
    int x, y, c, cols[3];
    memset(spr[idx], 0, 64);
    cols[0] = MC0; cols[1] = own; cols[2] = MC1;
    for (y = 0; y < 21; y++)
        for (x = 0; x < 24; x += mc ? 2 : 1) {
            c = px_get(ox + x, oy + y, cols, mc ? 3 : 1);
            if (c < 0) continue;
            if (mc) spr[idx][y * 3 + x / 8] = (unsigned char)(spr[idx][y * 3 + x / 8] | ((c + 1) << (6 - ((x / 2) & 3) * 2)));
            else spr[idx][y * 3 + x / 8] = (unsigned char)(spr[idx][y * 3 + x / 8] | (0x80 >> (x & 7)));
        }
}

/* a sheet of sprites: cols x rows cells, cell (c, r) = sprite first + c * cstep + r * rstep */
static void override_sprites(const char *name, int first, int cols, int rows, int cstep, int rstep, int mc)
{
    int c, r, idx, use = sheet_open(name, cols * 24, rows * 21);
    for (r = 0; r < rows; r++)
        for (c = 0; c < cols; c++) {
            idx = first + c * cstep + r * rstep;
            if (use) spr_read(idx, c * 24, r * 21, mc, spr_col[idx]);
            else spr_draw(idx, c * 24, r * 21, mc, spr_col[idx]);
        }
    if (!use) sheet_save(name, cols * 24, rows * 21);
}

static void override_all_sprites(void)
{
    override_sprites("player.png", 0, 16, 3, 3, 1, 1);        /* dirs x (stand, walk, fire) */
    override_sprites("player_dead.png", 48, 2, 1, 1, 0, 1);
    override_sprites("enemy_a.png", 50, 16, 2, 2, 1, 1);      /* dirs x (stand, walk) */
    override_sprites("enemy_b.png", 82, 16, 2, 2, 1, 1);
    override_sprites("enemy_c.png", 114, 16, 2, 2, 1, 1);
    override_sprites("enemy_d.png", 146, 16, 1, 1, 0, 1);     /* dirs, stand only */
    override_sprites("explosion.png", 162, 4, 1, 1, 0, 0);
    override_sprites("items.png", 166, 5, 1, 1, 0, 0);        /* grenade, C4, mine, flame, gib */
}

/* ------------------------------------------------------------------ */
/* preview                                                             */

static unsigned char prev[400][320];

static void preview(void)
{
    int k, id, n = 0, x, y, p, bits, s, ox, oy, col;
    FILE *f;
    memset(prev, 0, sizeof(prev));
    /* all metatiles, 16x16 each at 2x horizontal (fat px) */
    for (id = 0; id < 256; id++) {
        int used = 0;
        for (k = 0; k < NKEYS; k++) if (gid[k] == id && rep[k] == k) used = 1;
        if (!used && !(id >= DECAL_TILE && id <= 127)) continue;
        ox = (n % 20) * 16; oy = (n / 20) * 16; n++;
        for (p = 0; p < 4; p++)
            for (y = 0; y < 8; y++)
                for (x = 0; x < 4; x++) {
                    bits = (charset[mt[p][id]][y] >> (6 - x * 2)) & 3;
                    col = pal4[bits];
                    prev[oy + (p >> 1) * 8 + y][ox + (p & 1) * 8 + x * 2] = (unsigned char)col;
                    prev[oy + (p >> 1) * 8 + y][ox + (p & 1) * 8 + x * 2 + 1] = (unsigned char)col;
                }
    }
    /* sprites 13 per row, 24x21 */
    for (s = 0; s < NSPR; s++) {
        ox = (s % 13) * 24; oy = 180 + (s / 13) * 21;
        if (oy + 21 > 400) break;
        for (y = 0; y < 21; y++)
            for (x = 0; x < 24; x++) {
                if (spr_mc[s]) {
                    bits = (spr[s][y * 3 + x / 8] >> (6 - ((x / 2) & 3) * 2)) & 3;
                    col = bits == 1 ? MC0 : bits == 2 ? spr_col[s] : bits == 3 ? MC1 : -1;
                } else col = (spr[s][y * 3 + x / 8] & (0x80 >> (x & 7))) ? spr_col[s] : -1;
                prev[oy + y][ox + x] = (unsigned char)(col < 0 ? 11 : col);
            }
    }
    f = out_file("preview.ppm", -1);
    fprintf(f, "P6\n320 400\n255\n");
    for (y = 0; y < 400; y++)
        for (x = 0; x < 320; x++)
            for (p = 0; p < 3; p++) fputc(c64rgb[prev[y][x]][p], f);
    fclose(f);
}

/* ------------------------------------------------------------------ */
/* levels                                                              */

#define C64_LEVEL_HDR 64
#define MIN_W 21
#define MIN_H 12

static int write_level(int ep, int lev, int letter)
{
    char name[32], rel[64];
    unsigned char hdr[C64_LEVEL_HDR];
    int w, h, x, y, t, i, n, wall_pad;
    FILE *f;
    sprintf(rel, "LEVS/%s/LEVEL%d.LEV", episode_names[ep], lev + 1);
    if (!level_load(rel)) return 0;
    w = level_x_size < MIN_W ? MIN_W : level_x_size;
    h = level_y_size < MIN_H ? MIN_H : level_y_size;
    wall_pad = gid[160 + 19] >= 0 ? gid[160 + 19] : 128;
    memset(hdr, 0, sizeof(hdr));
    hdr[0] = (unsigned char)w; hdr[1] = (unsigned char)h;
    hdr[2] = (unsigned char)pl_start_x[0]; hdr[3] = (unsigned char)pl_start_y[0];
    hdr[4] = (unsigned char)pl_start_x[1]; hdr[5] = (unsigned char)pl_start_y[1];
    hdr[6] = (unsigned char)(level_time_limit & 255); hdr[7] = (unsigned char)(level_time_limit >> 8);
    for (i = 0; i < 8; i++) hdr[8 + i] = (unsigned char)(level_enemies[i] > 255 ? 255 : level_enemies[i]);
    for (i = 0; i < 11; i++) hdr[16 + i] = (unsigned char)lev_weapon_crates[i];
    for (i = 0; i < 9; i++) hdr[27 + i] = (unsigned char)lev_bullet_crates[i];
    hdr[36] = (unsigned char)lev_energy_crates;
    n = crate_info_amount > 64 ? 64 : crate_info_amount;
    hdr[37] = (unsigned char)n;
    memcpy(hdr + 38, level_comment, 20);
    for (i = 38; i < 58; i++) {
        unsigned char c = hdr[i];
        if (c >= 'a' && c <= 'z') c = (unsigned char)(c - 'a' + 'A');   /* PETSCII upper */
        hdr[i] = c;
    }
    sprintf(name, "l%c%02d.prg", 'a' + letter, lev + 1);
    f = out_file(name, 0x2000);
    fwrite(hdr, 1, sizeof(hdr), f);
    for (i = 0; i < n; i++) {
        fputc(crate_info[i].x / 20, f); fputc(crate_info[i].y / 20, f);
        fputc(crate_info[i].type1, f); fputc(crate_info[i].type2, f);
    }
    for (y = 0; y < h; y++)
        for (x = 0; x < w; x++) {
            if (x >= level_x_size || y >= level_y_size) { fputc(wall_pad, f); continue; }
            t = level_tile[y * level_x_size + x];
            t = (t & 0x80 ? 160 : 0) + tile_num[t];
            fputc(gid[t] < 0 ? (t >= 160 ? 128 : 0) : gid[t], f);
        }
    fclose(f);
    return 1;
}

static void write_gfxdata(int nep, const int *eps)
{
    FILE *f;
    int p, i, e, look;
    static const int enemy_col[8] = { 5, 9, 14, 12, 8, 3, 4, 7 };
    f = out_file("gfxdata.h", -1);
    fprintf(f, "/* generated by conv64 - do not edit */\n#ifndef GFXDATA_H\n#define GFXDATA_H\n");
    fprintf(f, "#define COL_BG %d\n#define COL_D022 %d\n#define COL_D023 %d\n#define COL_CRAM %d\n",
            pal4[0], pal4[1], pal4[2], pal4[3]);
    fprintf(f, "#define SPR_MC0 %d\n#define SPR_MC1 %d\n", MC0, MC1);
    fprintf(f, "#define SPR_FIRST 64\n");
    fprintf(f, "#define SPR_PLAYER 64      /* + dir*3 + frame (stand, walk, fire) */\n");
    fprintf(f, "#define SPR_PDEAD 112\n");
    fprintf(f, "#define SPR_LOOK_A 114     /* + dir*2 + frame */\n#define SPR_LOOK_B 146\n#define SPR_LOOK_C 178\n");
    fprintf(f, "#define SPR_LOOK_D 210     /* + dir */\n#define SPR_EXPLO 226\n#define SPR_GRENADE 230\n");
    fprintf(f, "#define SPR_C4 231\n#define SPR_MINE 232\n#define SPR_FLAME 233\n#define SPR_GIB 234\n");
    fprintf(f, "#define CRATE_TILE 125\n#define DECAL_BLOOD 121\n#define DECAL_BLOOD2 122\n#define DECAL_GIBS 123\n#define DECAL_SCORCH 124\n#define NUM_EPISODES %d\n", nep);
    fprintf(f, "extern const unsigned char mt_tl[256], mt_tr[256], mt_bl[256], mt_br[256];\n");
    fprintf(f, "extern const unsigned char enemy_look[8], enemy_col[8], episode_levels[%d];\n", nep);
    fprintf(f, "extern const char *const episode_name[%d];\n#endif\n", nep);
    fclose(f);
    f = out_file("gfxdata.c", -1);
    fprintf(f, "/* generated by conv64 - do not edit */\n#include \"gfxdata.h\"\n");
    for (p = 0; p < 4; p++) {
        static const char *nm[4] = { "mt_tl", "mt_tr", "mt_bl", "mt_br" };
        fprintf(f, "const unsigned char %s[256] = {", nm[p]);
        for (i = 0; i < 256; i++) fprintf(f, "%s%d", i == 0 ? "\n  " : i % 16 ? "," : ",\n  ", mt[p][i]);
        fprintf(f, "\n};\n");
    }
    fprintf(f, "const unsigned char enemy_look[8] = {");
    for (e = 0; e < 8; e++) {
        look = (e == 0 || e == 2 || e == 3) ? 0 : (e == 7 ? 2 : (e == 5 ? 3 : 1));
        fprintf(f, "%s%d", e ? "," : "", look);
    }
    fprintf(f, "};\nconst unsigned char enemy_col[8] = {");
    for (e = 0; e < 8; e++) fprintf(f, "%s%d", e ? "," : "", enemy_col[e]);
    fprintf(f, "};\nconst unsigned char episode_levels[%d] = {", nep);
    for (e = 0; e < nep; e++) fprintf(f, "%s%d", e ? "," : "", episode_levels[eps[e]]);
    fprintf(f, "};\nconst char *const episode_name[%d] = {", nep);
    for (e = 0; e < nep; e++) fprintf(f, "%s\"%s\"", e ? "," : "", episode_names[eps[e]]);
    fprintf(f, "};\n");
    fclose(f);
}

int main(int argc, char **argv)
{
    int e, l, i, k, x, y, nep = 0, eps[16];
    FILE *f;
    static unsigned char sprbuf[NSPR * 64];
    strcpy(data_dir, argc > 1 ? argv[1] : "..");
    strcpy(outdir, argc > 2 ? argv[2] : "build");
    strcpy(gfxdir, getenv("CONV64_GFX") ? getenv("CONV64_GFX") : "gfx");
    if (!assets_load_all()) return 1;
    count_episodes();
    if (argc > 3) for (i = 0; argv[3][i]; i++) eps[nep++] = argv[3][i] - '1';
    else eps[nep++] = 0;

    /* tile usage over all levels of all episodes (so later episodes fit too) */
    for (e = 0; e < num_episodes; e++)
        for (l = 0; l < episode_levels[e]; l++) {
            char rel[64];
            sprintf(rel, "LEVS/%s/LEVEL%d.LEV", episode_names[e], l + 1);
            if (!level_load(rel)) continue;
            for (i = 0; i < level_x_size * level_y_size; i++) {
                k = level_tile[i];
                k = (k & 0x80 ? 160 : 0) + tile_num[k];
                usage[k]++;
                for (x = 0; x < nep; x++) if (eps[x] == e) pal_usage[k]++;
            }
        }
    usage[160 + 19]++;   /* padding wall */
    for (k = 0; k < NKEYS; k++) downscale_tile(k);   /* all: the override sheets show every tile */
    choose_palette();
    quantise_tiles();
    map_colours_load();
    override_tiles();
    merge_tiles();
    build_metatiles();
    build_crates();
    build_decals();
    override_metas("decals.png", DECAL_TILE, 4);
    override_metas("crates.png", 125, 3);
    map_colours_save();
    build_sprites();
    override_all_sprites();
    preview();

    f = out_file("tkchr.prg", 0xC800);
    for (i = 0; i < 256; i++) fwrite(charset[i], 1, 8, f);
    fclose(f);
    for (i = 0; i < NSPR; i++) memcpy(sprbuf + i * 64, spr[i], 64);
    f = out_file("tksprd.prg", 0x2000);
    fwrite(sprbuf, 1, 64 * 64, f);
    fclose(f);
    f = out_file("tkspre.prg", 0xE000);
    fwrite(sprbuf + 64 * 64, 1, (NSPR - 64) * 64, f);
    fclose(f);
    write_gfxdata(nep, eps);
    for (e = 0; e < nep; e++)
        for (l = 0; l < episode_levels[eps[e]]; l++)
            if (!write_level(eps[e], l, e)) printf("level %d/%d failed\n", e, l);
    printf("chars used: %d tiles + 12 crates, sprites %d\n", nchars, NSPR);
    (void)x; (void)y;
    return 0;
}
