/* rout.c - small helpers: integer math, random, level queries, messages, sound */
#include <stdio.h>
#include <string.h>
#include "globals.h"
#include "plat.h"

static s16 iabs(s16 v)
{
    return v < 0 ? (s16)-v : v;
}

/* distance approximation: max + 3/8 min (error < 7%), no multiply or sqrt */
s16 get_dist(s16 x1, s16 y1, s16 x2, s16 y2)
{
    s16 dx = iabs((s16)(x1 - x2)), dy = iabs((s16)(y1 - y2));
    if (dx < dy) {
        s16 t = dx;
        dx = dy;
        dy = t;
    }
    return (s16)(dx + ((dy * 3) >> 3));
}

/* angle of the vector (dx, dy) in the game's convention: dx = sin, dy = cos */
u8 get_angle(s16 dx, s16 dy)
{
    u16 ax = (u16)iabs(dx), ay = (u16)iabs(dy);
    u8 a;
    if (ax == 0 && ay == 0) return 0;
    if (ax <= ay) a = atan_tab[(u16)(ax * 32u) / ay];
    else a = (u8)(64 - atan_tab[(u16)(ay * 32u) / ax]);
    if (dy < 0) a = (u8)(128 - a);
    if (dx < 0) a = (u8)(0 - a);
    return a;
}

/* v * sin(a), truncated towards zero like the original float -> int casts */
s16 fx_sin(s16 v, u8 a)
{
    return (s16)((v * SIN(a)) / TRIG_ONE);
}

s16 fx_cos(s16 v, u8 a)
{
    return (s16)((v * COS(a)) / TRIG_ONE);
}

/* 16-bit xorshift: same sequence on every platform */
static u16 rnd_state = 1;

void rnd_seed(u16 s)
{
    rnd_state = s ? s : 1;
}

s16 rnd(void)
{
    rnd_state ^= (u16)(rnd_state << 7);
    rnd_state ^= (u16)(rnd_state >> 9);
    rnd_state ^= (u16)(rnd_state << 8);
    return (s16)(rnd_state & 0x7fff);
}

const char *c_2_c(s16 secs)
{
    static char text[16];
    if (secs < 0) secs = 0;
    sprintf(text, "%d:%02d", secs / 60, secs % 60);
    return text;
}

void play_sound(u8 id, s16 rate)
{
    if (id > 0) plat_sound(id, rate);
}

u8 tile_px(s16 x, s16 y)
{
    if (x < 0 || y < 0) return 0x80;
    x /= TILE_SIZE;
    y /= TILE_SIZE;
    if (x >= level_x_size || y >= level_y_size) return 0x80;
    return level_tile[y * level_x_size + x];
}

u8 wall_px(s16 x, s16 y)
{
    return (u8)((tile_px(x, y) & 0x80) != 0);
}

void add_message(const char *msg)
{
    u8 a;
    if (messages < MAX_MESSAGES) {
        strncpy(message[messages], msg, MAX_MESSAGE_LENGTH - 1);
        message[messages][MAX_MESSAGE_LENGTH - 1] = 0;
        messages++;
    } else {
        for (a = 0; a < MAX_MESSAGES - 1; a++) strcpy(message[a], message[a + 1]);
        strncpy(message[MAX_MESSAGES - 1], msg, MAX_MESSAGE_LENGTH - 1);
        message[MAX_MESSAGES - 1][MAX_MESSAGE_LENGTH - 1] = 0;
    }
    message_time = MESSAGE_TIME_ON_SCREEN;
}
