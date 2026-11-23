/* data.c - constant game data, ported from GLOBVAR.CPP and MISCFUNC.CPP.
 * Angles are binary (256 = 360 deg), damage and energy are in tenths. */
#include "data.h"

const Weapon weapon[DIFF_WEAPONS + 1] = {
    /* name, cost, loading_time, smoke, bullet_amount, angle_add, gun, btype, sound */
    { "fist",              0, 10, 0, 1,  1, 0, 0, HITWAV },
    { "Pistola",         400, 10, 1, 1,  7, 1, 1, MAGNUMWAV },
    { "Shotgun",        1000, 17, 1, 6, 14, 1, 3, SHOTGUNWAV },
    { "Uzi",            2000,  4, 1, 1,  7, 1, 1, PISTOLWAV },
    { "Auto rifle",     4000,  5, 1, 1,  7, 1, 2, PISTOLWAV },
    { "Grenade launcher",4000,30, 0, 1,  7, 1, 5, SINKOWAV },
    { "Auto grenadier", 6000, 10, 0, 1,  7, 1, 4, SINKOWAV },
    { "Heavy launcher", 6000, 40, 0, 1,  7, 1, 6, SINKOWAV },
    { "Auto shotgun",   6000,  5, 1, 6, 14, 1, 3, SHOTGUNWAV },
    { "C4-Activator",   1000, 10, 0, 1,  1, 0, 7, 0 },
    { "Flame thrower",  3000,  1, 0, 1,  7, 1, 8, FLAMEWAV },
    { "Mine dropper",   1000, 20, 0, 1,  1, 0, 9, 0 }
};

const Bullet_type bullet_type[DIFF_BULLETS + 2] = {
    /* name, type, stop, speed, power, time, explos, push, cost, in_crate, mul, sleep, max, sprite, effect */
    { "fist",            NORMAL,     1, 5,      30, 1,             0, 3, 0,   0,   1,  0,  0,   BSPR_NONE,    0 },
    { "9mm bullets",     NORMAL,     1, 6 * 18, 50, 1,             0, 2, 3,   50,  1,  0,  300, BSPR_NONE,    0 },
    { "12mm bullets",    NORMAL,     1, 7 * 18, 90, 1,             0, 2, 5,   50,  1,  0,  300, BSPR_NONE,    0 },
    { "shotgun shells",  NORMAL,     1, 6 * 18, 30, 1,             0, 3, 6,   20,  1,  0,  300, BSPR_NONE,    0 },
    { "light grenades",  EXPLOSIVE,  1, 8,      0,  (15 * 18) / 8, 1, 0, 70,  15,  1,  0,  150, BSPR_GRENADE, SMOKE2 },
    { "medium grenades", EXPLOSIVE,  1, 8,      0,  (15 * 18) / 8, 2, 0, 100, 10,  1,  0,  125, BSPR_GRENADE, SMOKE2 },
    { "heavy grenades",  EXPLOSIVE,  1, 8,      0,  (15 * 18) / 8, 4, 0, 200, 5,   1,  0,  100, BSPR_GRENADE, SMOKE2 },
    { "C4-explosives",   EXPLOSIVE,  0, 0,      0,  100,           4, 0, 150, 5,   1,  0,  100, BSPR_C4,      0 },
    { "Gas",             INCENDIARY, 1, 5,      4,  (5 * 18) / 5,  0, 0, 6,   500, 10, 0,  300, BSPR_FLAME,   0 },
    { "Mines",           EXPLOSIVE,  1, 0,      0,  2000,          1, 0, 150, 5,   1,  25, 100, BSPR_MINE,    0 },
    { "Explosive ray",   NORMAL,     1, 20,     300, 0,            0, 8, 0,   0,   1,  0,  0,   BSPR_NONE,    EXPLO }
};

const Enemy_info enemy_info[DIFF_ENEMIES] = {
    /* name, speed, curr_weapon, energy (tenths), reward, explo_death */
    { "pistol boy",     2, 1,  180,  200, 0 },
    { "shotgun maniac", 2, 2,  280,  350, 0 },
    { "uzi rebel",      2, 3,  400,  400, 0 },
    { "commando",       3, 4,  500,  600, 0 },
    { "grenade gay",    2, 5,  400,  700, 0 },
    { "civilian",       2, 0,  100,   50, 0 },
    { "punisher",       1, 8,  1500, 650, 0 },
    { "flamer",         2, 10, 1000, 600, 1 }
};

const Effect_type effect_type[DIFF_EFFECTS] = {
    /* index, tcount, order, delay, random, xs, ys, offs, trans */
    { BLOOD,     4,  1, 2,  0, 12, 12, 0,                   0 },
    { SMOKE,     4,  0, 2,  0, 12, 12, 13 * 4,              0 },
    { EXPLO,     9,  1, 2,  0, 25, 25, 320 * 13,            0 },
    { METAL,     8,  1, 2,  0, 12, 12, 13 * 8,              0 },
    { SMOKE2,    12, 1, 5,  1, 20, 20, 320 * 39,            1 },
    { BLOOD2,    8,  0, 20, 1, 6,  6,  13 * 20,             1 },
    { SMOKE3,    4,  1, 2,  0, 12, 12, 13 * 16,             1 },
    { STEAM,     12, 1, 2,  0, 20, 20, 320 * 39,            1 },
    { TELEPORT,  7,  1, 2,  0, 20, 20, 320 * 60,            0 },
    { FOOTPRINT, 4,  0, 50, 1, 6,  6,  (13 * 20) + (320 * 7), 1 }
};

const s16 sample_rates[DIFF_WEAPONS + 1] = {
    10000, 30000, 13000, 22000, 16000, 17000, 18000, 16000, 15000, 0, 11025, 0
};

const u8 dust_blocks[DUST_BLOCKS] = {
    0, 16 * 4, 16 * 5, 16 * 5 + 2, 16 * 6 + 1, 16 * 6 + 1, 16 * 6 + 2, 16 * 7 + 1, 16 * 7 + 2, 16 * 7 + 3
};

/* 64 * sin(i * 360 / 256) */
const s8 sin_tab[256] = {
      0,   2,   3,   5,   6,   8,   9,  11,  12,  14,  16,  17,  19,  20,  22,  23,
     24,  26,  27,  29,  30,  32,  33,  34,  36,  37,  38,  39,  41,  42,  43,  44,
     45,  46,  47,  48,  49,  50,  51,  52,  53,  54,  55,  56,  56,  57,  58,  59,
     59,  60,  60,  61,  61,  62,  62,  62,  63,  63,  63,  64,  64,  64,  64,  64,
     64,  64,  64,  64,  64,  64,  63,  63,  63,  62,  62,  62,  61,  61,  60,  60,
     59,  59,  58,  57,  56,  56,  55,  54,  53,  52,  51,  50,  49,  48,  47,  46,
     45,  44,  43,  42,  41,  39,  38,  37,  36,  34,  33,  32,  30,  29,  27,  26,
     24,  23,  22,  20,  19,  17,  16,  14,  12,  11,   9,   8,   6,   5,   3,   2,
      0,  -2,  -3,  -5,  -6,  -8,  -9, -11, -12, -14, -16, -17, -19, -20, -22, -23,
    -24, -26, -27, -29, -30, -32, -33, -34, -36, -37, -38, -39, -41, -42, -43, -44,
    -45, -46, -47, -48, -49, -50, -51, -52, -53, -54, -55, -56, -56, -57, -58, -59,
    -59, -60, -60, -61, -61, -62, -62, -62, -63, -63, -63, -64, -64, -64, -64, -64,
    -64, -64, -64, -64, -64, -64, -63, -63, -63, -62, -62, -62, -61, -61, -60, -60,
    -59, -59, -58, -57, -56, -56, -55, -54, -53, -52, -51, -50, -49, -48, -47, -46,
    -45, -44, -43, -42, -41, -39, -38, -37, -36, -34, -33, -32, -30, -29, -27, -26,
    -24, -23, -22, -20, -19, -17, -16, -14, -12, -11,  -9,  -8,  -6,  -5,  -3,  -2
};

/* atan(i / 32) in binary angle units, i = 0..32 */
const u8 atan_tab[33] = {
    0, 1, 3, 4, 5, 6, 8, 9, 10, 11, 12, 13, 15, 16, 17, 18, 19,
    20, 21, 22, 23, 24, 25, 25, 26, 27, 28, 29, 29, 30, 31, 31, 32
};

const s16 shield_cost = 160, shield_max = 30, target_cost = 500;

/* episode directories under LEVS/ (replaces DOS directory listing) */
const char *const episode_names[] = {
    "DEFAULT", "ULTIMATE", "SUNRISE", "BLANKET", "BLANKET2", "FIENDIS", "MOTHAFUX", "POKETI"
};
const s16 num_episode_names = sizeof(episode_names) / sizeof(episode_names[0]);
