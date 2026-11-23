/* data.c - loading the game's graphics: tiles, shadows, crates, pointer,
 * font palette and the shadow colour table. */
#include <stdio.h>
#include <string.h>
#include "ed.h"

char data_dir[256] = "..";
u8 pal[768];
u8 floor_pix[NUM_TILES][TILE * TILE];
u8 wall_pix[NUM_TILES][TILE * TILE];
u8 shadow_pix[SHADOW_TYPES][TILE * TILE];
u8 crate_pix[CRATE_TYPES][CRATE_SIZE * CRATE_SIZE];
u8 pointer_pix[13 * 9];
u8 shadow_table[256][16];
u8 floor_avg[NUM_TILES], wall_avg[NUM_TILES];
u8 font_data[256 * 64];

/* diameter of the spot lights in pixels (GLOBVAR.CPP Light_sizes) */
const s16 light_size[LIGHTS] = { 15, 20, 25, 30, 35, 40, 45, 50, 55, 60 };

/* crate contents in level files: index 0 = first real weapon / ammo */
const char *const weapon_name[DIFF_WEAPONS] = {
    "Pistola", "Shotgun", "Uzi", "Auto rifle", "Grenade launcher", "Auto grenadier",
    "Heavy launcher", "Auto shotgun", "C4-Activator", "Flame thrower", "Mine dropper"
};
const char *const bullet_name[DIFF_BULLETS] = {
    "9mm bullets", "12mm bullets", "Shotgun shells", "Light grenades", "Medium grenades",
    "Heavy grenades", "C4-explosives", "Gas", "Mines"
};
const char *const enemy_name[DIFF_ENEMIES] = {
    "Pistol boys", "Shotgun maniacs", "Uzi rebels", "Commandos", "Grenade boys",
    "Civilians", "Punishers", "Flamers"
};

static char path_buf[600];

const char *data_path(const char *rel)
{
    sprintf(path_buf, "%.255s/%.300s", data_dir, rel);
    return path_buf;
}

/* ------------------------------------------------------------------ */
/* EFP: "EF pic", u16 w, u16 h, RLE data (byte > 192: run of byte-192,
 * runs may cross rows), 768 byte palette at the end of the file */

#define MAX_PIC (320 * 200)
static u8 pic[MAX_PIC];
static s16 pic_w, pic_h;

static int load_efp(const char *rel)
{
    FILE *f = fopen(data_path(rel), "rb");
    char hdr[6];
    s32 i, n;
    int b, run, val;
    if (!f) {
        printf("Missing file: %s\n", data_path(rel));
        return 0;
    }
    if (fread(hdr, 1, 6, f) != 6 || memcmp(hdr, "EF pic", 6) != 0) {
        printf("Not an EF pic: %s\n", data_path(rel));
        fclose(f);
        return 0;
    }
    pic_w = (s16)(fgetc(f) & 255);
    pic_w = (s16)(pic_w | ((fgetc(f) & 255) << 8));
    pic_h = (s16)(fgetc(f) & 255);
    pic_h = (s16)(pic_h | ((fgetc(f) & 255) << 8));
    n = (s32)pic_w * pic_h;
    if (pic_w <= 0 || pic_h <= 0 || n > MAX_PIC) {
        printf("Bad picture size: %s\n", data_path(rel));
        fclose(f);
        return 0;
    }
    for (i = 0; i < n; ) {
        b = fgetc(f);
        if (b < 0) break;
        if (b > 192) {
            run = b - 192;
            val = fgetc(f) & 255;
        } else {
            run = 1;
            val = b;
        }
        while (run-- > 0 && i < n) pic[i++] = (u8)val;
    }
    while (i < n) pic[i++] = 0;
    fclose(f);
    return 1;
}

/* copy a w*h cell at (x,y) of the last picture */
static void cut(s16 x, s16 y, s16 w, s16 h, u8 *dest)
{
    s16 r;
    for (r = 0; r < h; r++) {
        if (y + r >= pic_h || x + w > pic_w) memset(dest + r * w, 0, (size_t)w);
        else memcpy(dest + r * w, pic + (s32)(y + r) * pic_w + x, (size_t)w);
    }
}

static int load_tiles(const char *rel, u8 tiles[NUM_TILES][TILE * TILE])
{
    s16 i;
    if (!load_efp(rel)) return 0;
    for (i = 0; i < NUM_TILES; i++)
        cut((s16)((i % 16) * TILE), (s16)((i / 16) * TILE), TILE, TILE, tiles[i]);
    return 1;
}

static int load_palette(const char *rel)
{
    FILE *f = fopen(data_path(rel), "rb");
    if (!f) return 0;
    fseek(f, -768L, SEEK_END);
    if (fread(pal, 1, 768, f) != 768) {
        fclose(f);
        return 0;
    }
    fclose(f);
    return 1;
}

/* PALETTE.TAB: 64 KB translucency table, then shadow_table[256][16] */
static int load_shadow_table(void)
{
    FILE *f = fopen(data_path("PALETTE.TAB"), "rb");
    if (!f) {
        printf("Missing file: %s\n", data_path("PALETTE.TAB"));
        return 0;
    }
    fseek(f, 65536L, SEEK_SET);
    if (fread(shadow_table, 1, sizeof(shadow_table), f) != sizeof(shadow_table)) {
        fclose(f);
        return 0;
    }
    fclose(f);
    return 1;
}

u8 nearest_colour(s16 r, s16 g, s16 b)
{
    s32 best = 0x7FFFFFFFL, d;
    s16 i, dr, dg, db;
    u8 bi = 0;
    for (i = 0; i < 256; i++) {
        dr = (s16)(pal[i * 3] * 4 - r);
        dg = (s16)(pal[i * 3 + 1] * 4 - g);
        db = (s16)(pal[i * 3 + 2] * 4 - b);
        d = (s32)dr * dr + (s32)dg * dg + (s32)db * db;
        if (d < best) {
            best = d;
            bi = (u8)i;
        }
    }
    return bi;
}

static u8 average(const u8 *pix)
{
    s32 r = 0, g = 0, b = 0;
    s16 i;
    for (i = 0; i < TILE * TILE; i++) {
        r += pal[pix[i] * 3];
        g += pal[pix[i] * 3 + 1];
        b += pal[pix[i] * 3 + 2];
    }
    return nearest_colour((s16)(r * 4 / (TILE * TILE)), (s16)(g * 4 / (TILE * TILE)), (s16)(b * 4 / (TILE * TILE)));
}

/* FNTS/8X8B.FNT: w, h, then 256 glyphs of 8x8 bytes at offset 512 */
static int load_font(void)
{
    FILE *f = fopen(data_path("FNTS/8X8B.FNT"), "rb");
    int ok;
    if (!f) {
        printf("Missing file: %s\n", data_path("FNTS/8X8B.FNT"));
        return 0;
    }
    ok = fgetc(f) == 8 && fgetc(f) == 8;
    fseek(f, 512L, SEEK_SET);
    if (ok) ok = fread(font_data, 1, sizeof(font_data), f) == sizeof(font_data);
    fclose(f);
    return ok;
}

int data_load_all(void)
{
    int ok = 1;
    s16 i;
    ok &= load_font();
    ok &= load_tiles("EFPS/FLOOR1.EFP", floor_pix);
    ok &= load_tiles("EFPS/WALLS1.EFP", wall_pix);
    if (load_efp("EFPS/SHADOWS.EFP")) {
        for (i = 0; i < SHADOW_TYPES; i++) cut((s16)(i * TILE), 0, TILE, TILE, shadow_pix[i]);
    } else ok = 0;
    if (load_efp("EFPS/CRATES.EFP")) {
        for (i = 0; i < CRATE_TYPES; i++) cut((s16)(i * CRATE_SIZE), 0, CRATE_SIZE, CRATE_SIZE, crate_pix[i]);
    } else ok = 0;
    if (load_efp("EFPS/POINTER.EFP")) cut(0, 0, 9, 13, pointer_pix);
    else ok = 0;
    ok &= load_palette("EFPS/WALLS1.EFP");
    ok &= load_shadow_table();
    if (!ok) return 0;
    for (i = 0; i < NUM_TILES; i++) {
        floor_avg[i] = average(floor_pix[i]);
        wall_avg[i] = average(wall_pix[i]);
    }
    return 1;
}
