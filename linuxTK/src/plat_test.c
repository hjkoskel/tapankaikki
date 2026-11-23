/* plat_test.c - headless backend for automated testing.
 * Runs the real game core with scripted input and writes chosen frames as PPM.
 *
 * Environment:
 *   TK_KEYS  = "frame:key,..."        key presses (enter, esc, up, down, left, right, space, or a letter)
 *   TK_HOLD0 = "from-to:btn+btn,..."  held buttons for key set 0 (left right up down fire change strafe sleft sright w0..w11)
 *   TK_HOLD1 = same for key set 1
 *   TK_SHOTS = "frame,frame,..."      frames to save as shot_<frame>.ppm
 *   TK_OUT   = output directory (default ".")
 *   TK_END   = frame number at which a window close is simulated
 * One present() = one frame = 25 ms of game time, so runs are deterministic. */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "plat.h"

static u8 pal[768];
static long frame;
static long end_frame = 1000;

static const char *env(const char *n)
{
    const char *v = getenv(n);
    return v ? v : "";
}

int plat_init(void)
{
    if (*env("TK_END")) end_frame = atol(env("TK_END"));
    return 1;
}

void plat_shutdown(void)
{
    printf("test backend: %ld frames\n", frame);
}

void plat_set_palette(const u8 *p)
{
    memcpy(pal, p, 768);
}

static int listed(const char *list, long f)
{
    const char *s = list;
    while (*s) {
        if (atol(s) == f) return 1;
        s = strchr(s, ',');
        if (!s) break;
        s++;
    }
    return 0;
}

void plat_present(const u8 *fb)
{
    char name[512];
    FILE *f;
    int i;
    frame++;
    if (listed(env("TK_SHOTS"), frame)) {
        sprintf(name, "%s/shot_%04ld.ppm", *env("TK_OUT") ? env("TK_OUT") : ".", frame);
        f = fopen(name, "wb");
        if (f) {
            fprintf(f, "P6\n%d %d\n255\n", SCR_W, SCR_H);
            for (i = 0; i < SCR_W * SCR_H; i++) {
                int c;
                for (c = 0; c < 3; c++) fputc(((pal[fb[i] * 3 + c] & 63) << 2), f);
            }
            fclose(f);
        }
    }
}

static u32 btn_bit(const char *s, int n)
{
    static const char *names[] = { "left", "right", "up", "down", "fire", "change", "strafe", "sleft", "sright" };
    int i;
    for (i = 0; i < 9; i++)
        if ((int)strlen(names[i]) == n && !strncmp(s, names[i], n)) return 1u << i;
    if (s[0] == 'w' && n >= 2) return BTN_W0 << atoi(s + 1);
    return 0;
}

u32 plat_buttons(int keyset)
{
    const char *s = env(keyset ? "TK_HOLD1" : "TK_HOLD0");
    u32 b = 0;
    while (*s) {
        long from = atol(s), to;
        const char *p = strchr(s, '-'), *c, *e;
        if (!p) break;
        to = atol(p + 1);
        c = strchr(p, ':');
        if (!c) break;
        e = strchr(c, ',');
        if (!e) e = c + strlen(c);
        if (frame >= from && frame <= to) {
            const char *t = c + 1;
            while (t < e) {
                const char *plus = strchr(t, '+');
                if (!plus || plus > e) plus = e;
                b |= btn_bit(t, (int)(plus - t));
                t = plus + 1;
            }
        }
        s = *e ? e + 1 : e;
    }
    return b;
}

static long key_frame_done = -1;

int plat_key_pressed(void)
{
    const char *s = env("TK_KEYS");
    while (*s) {
        long f = atol(s);
        const char *c = strchr(s, ':'), *e;
        if (!c) break;
        e = strchr(c, ',');
        if (!e) e = c + strlen(c);
        if (f == frame && key_frame_done != frame) {
            int n = (int)(e - c - 1);
            key_frame_done = frame;
            if (!strncmp(c + 1, "enter", n)) return PK_ENTER;
            if (!strncmp(c + 1, "esc", n)) return PK_ESC;
            if (!strncmp(c + 1, "up", n)) return PK_UP;
            if (!strncmp(c + 1, "down", n)) return PK_DOWN;
            if (!strncmp(c + 1, "left", n)) return PK_LEFT;
            if (!strncmp(c + 1, "right", n)) return PK_RIGHT;
            if (!strncmp(c + 1, "space", n)) return PK_SPACE;
            return c[1];
        }
        s = *e ? e + 1 : e;
    }
    return PK_NONE;
}

u32 plat_ticks_ms(void)
{
    return (u32)(frame * TICK_MS);
}

int plat_quit_requested(void)
{
    return frame >= end_frame;
}

void plat_load_sound(int id, const char *path)
{
    (void)id;
    (void)path;
}

void plat_sound(int id, s32 rate)
{
    (void)id;
    (void)rate;
}
