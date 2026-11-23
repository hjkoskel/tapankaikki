/* data64.c - game tables for the C64, from linuxTK/src/data.c.
 * Scaled to the C64 world: 25 Hz ticks (x0.625 time, x1.6 speed per tick)
 * and 16 px tiles (x0.8 distance). Energy/damage in tenths as in linuxTK. */
#include "defs.h"

const W64 weapon64[NWEAPONS] = {
    /* name             cost  load amt spread gun bt  sfx */
    { "FIST",              0,  6,  1,  1,    0,  0, SFX_HIT },
    { "PISTOLA",         400,  6,  1,  7,    1,  1, SFX_GUN },
    { "SHOTGUN",        1000, 11,  6, 14,    1,  3, SFX_SHOTGUN },
    { "UZI",            2000,  3,  1,  7,    1,  1, SFX_UZI },
    { "AUTO RIFLE",     4000,  3,  1,  7,    1,  2, SFX_UZI },
    { "GRENADE LAUNCH", 4000, 19,  1,  7,    1,  5, SFX_LAUNCH },
    { "AUTO GRENADIER", 6000,  6,  1,  7,    1,  4, SFX_LAUNCH },
    { "HEAVY LAUNCHER", 6000, 25,  1,  7,    1,  6, SFX_LAUNCH },
    { "AUTO SHOTGUN",   6000,  3,  6, 14,    1,  3, SFX_SHOTGUN },
    { "C4-ACTIVATOR",   1000,  6,  1,  1,    0,  7, SFX_NONE },
    { "FLAME THROWER",  3000,  1,  1,  7,    1,  8, SFX_FLAME },
    { "MINE DROPPER",   1000, 12,  1,  1,    0,  9, SFX_NONE }
};

/* short names for the split screen HUD (8 chars) */
const char *const wshort[NWEAPONS] = {
    "FIST", "PISTOL", "SHOTGUN", "UZI", "RIFLE", "LAUNCHER",
    "AUTOGREN", "HEAVY GL", "AUTOSHOT", "C4", "FLAMER", "MINES"
};

const B64 bullet64[NBULLETS] = {
    /* name              kind     range power exp push time crate mul max cost */
    { "FIST",            K_HIT,    12,   30, 0, 3,   0,   0,  1,   0,   0 },
    { "9MM BULLETS",     K_HIT,    86,   50, 0, 2,   0,  50,  1, 300,   3 },
    { "12MM BULLETS",    K_HIT,   100,   90, 0, 2,   0,  50,  1, 300,   5 },
    { "SHOTGUN SHELLS",  K_HIT,    86,   30, 0, 3,   0,  20,  1, 300,   6 },
    { "LIGHT GRENADES",  K_PROJ,    0,    0, 1, 0,  21,  15,  1, 150,  70 },
    { "MEDIUM GRENADES", K_PROJ,    0,    0, 2, 0,  21,  10,  1, 125, 100 },
    { "HEAVY GRENADES",  K_PROJ,    0,    0, 4, 0,  21,   5,  1, 100, 200 },
    { "C4-EXPLOSIVES",   K_PLACE,   0,    0, 4, 0,  62,   5,  1, 100, 150 },
    { "GAS",             K_FLAME,  70,    4, 0, 0,   0, 500, 10, 300,   6 },
    { "MINES",           K_MINE,    0,    0, 1, 0, 1250,  5,  1, 100, 150 }
};

const E64 enemy64[NENEMYTYPES] = {
    /* speed_fx weapon energy reward explo */
    { 41, 1,  180, 200, 0 },   /* pistol boy */
    { 41, 2,  280, 350, 0 },   /* shotgun maniac */
    { 41, 3,  400, 400, 0 },   /* uzi rebel */
    { 61, 4,  500, 600, 0 },   /* commando */
    { 41, 5,  400, 700, 0 },   /* grenade guy */
    { 41, 0,  100,  50, 0 },   /* civilian */
    { 20, 8, 1500, 650, 0 },   /* punisher */
    { 41, 10, 1000, 600, 1 }   /* flamer */
};

/* atan(i / 32) in binary angle units, i = 0..32 (from linuxTK) */
const u8 atan_tab[33] = {
    0, 1, 3, 4, 5, 6, 8, 9, 10, 11, 12, 13, 15, 16, 17, 18, 19,
    20, 21, 22, 23, 24, 25, 25, 26, 27, 28, 29, 29, 30, 31, 31, 32
};
