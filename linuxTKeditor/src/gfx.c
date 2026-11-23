/* gfx.c - drawing into the 320x200 indexed frame buffer, clipped to
 * [clip_x0, clip_x1) x [clip_y0, clip_y1). */
#include <string.h>
#include "ed.h"

u8 fb[SCR_W * SCR_H];
s16 clip_x0 = 0, clip_y0 = 0, clip_x1 = SCR_W, clip_y1 = SCR_H;
u8 col_black, col_white, col_grey, col_dgrey, col_red, col_green, col_blue, col_yellow, col_cyan;

void ui_colours(void)
{
    col_black = nearest_colour(0, 0, 0);
    col_white = nearest_colour(255, 255, 255);
    col_grey = nearest_colour(150, 150, 150);
    col_dgrey = nearest_colour(50, 50, 55);
    col_red = nearest_colour(255, 40, 40);
    col_green = nearest_colour(40, 230, 40);
    col_blue = nearest_colour(70, 110, 255);
    col_yellow = nearest_colour(255, 240, 40);
    col_cyan = nearest_colour(40, 230, 230);
}

void clip_set(s16 x0, s16 y0, s16 x1, s16 y1)
{
    clip_x0 = x0; clip_y0 = y0; clip_x1 = x1; clip_y1 = y1;
}

void clip_full(void)
{
    clip_set(0, 0, SCR_W, SCR_H);
}

#define VISIBLE(x, y) ((x) >= clip_x0 && (x) < clip_x1 && (y) >= clip_y0 && (y) < clip_y1)

void put_pixel(s16 x, s16 y, u8 c)
{
    if (VISIBLE(x, y)) fb[(s32)y * SCR_W + x] = c;
}

void fill_rect(s16 x, s16 y, s16 w, s16 h, u8 c)
{
    s16 x1 = (s16)(x + w), y1 = (s16)(y + h);
    if (x < clip_x0) x = clip_x0;
    if (y < clip_y0) y = clip_y0;
    if (x1 > clip_x1) x1 = clip_x1;
    if (y1 > clip_y1) y1 = clip_y1;
    for (; y < y1; y++)
        if (x < x1) memset(fb + (s32)y * SCR_W + x, c, (size_t)(x1 - x));
}

void rect(s16 x, s16 y, s16 w, s16 h, u8 c)
{
    fill_rect(x, y, w, 1, c);
    fill_rect(x, (s16)(y + h - 1), w, 1, c);
    fill_rect(x, y, 1, h, c);
    fill_rect((s16)(x + w - 1), y, 1, h, c);
}

void line(s16 x0, s16 y0, s16 x1, s16 y1, u8 c)
{
    s16 dx = (s16)(x1 > x0 ? x1 - x0 : x0 - x1), sx = (s16)(x0 < x1 ? 1 : -1);
    s16 dy = (s16)(y1 > y0 ? y0 - y1 : y1 - y0), sy = (s16)(y0 < y1 ? 1 : -1);
    s16 e = (s16)(dx + dy), e2;
    for (;;) {
        put_pixel(x0, y0, c);
        if (x0 == x1 && y0 == y1) break;
        e2 = (s16)(2 * e);
        if (e2 >= dy) { e = (s16)(e + dy); x0 = (s16)(x0 + sx); }
        if (e2 <= dx) { e = (s16)(e + dx); y0 = (s16)(y0 + sy); }
    }
}

void circle(s16 cx, s16 cy, s16 r, u8 c)
{
    s16 x = r, y = 0, e = (s16)(1 - r);
    while (x >= y) {
        put_pixel((s16)(cx + x), (s16)(cy + y), c); put_pixel((s16)(cx - x), (s16)(cy + y), c);
        put_pixel((s16)(cx + x), (s16)(cy - y), c); put_pixel((s16)(cx - x), (s16)(cy - y), c);
        put_pixel((s16)(cx + y), (s16)(cy + x), c); put_pixel((s16)(cx - y), (s16)(cy + x), c);
        put_pixel((s16)(cx + y), (s16)(cy - x), c); put_pixel((s16)(cx - y), (s16)(cy - x), c);
        y++;
        if (e < 0) e = (s16)(e + 2 * y + 1);
        else {
            x--;
            e = (s16)(e + 2 * (y - x) + 1);
        }
    }
}

void blit(s16 x, s16 y, s16 w, s16 h, const u8 *pix, int transparent)
{
    s16 a, b;
    u8 c;
    for (a = 0; a < h; a++) {
        s16 py = (s16)(y + a);
        if (py < clip_y0 || py >= clip_y1) continue;
        for (b = 0; b < w; b++) {
            s16 px = (s16)(x + b);
            c = pix[a * w + b];
            if (px < clip_x0 || px >= clip_x1 || (transparent && !c)) continue;
            fb[(s32)py * SCR_W + px] = c;
        }
    }
}

/* shadow sprite: pixel value v darkens the pixel below with shadow_table */
void blit_shadow(s16 x, s16 y, const u8 *pix)
{
    s16 a, b;
    u8 v, *d;
    for (a = 0; a < TILE; a++)
        for (b = 0; b < TILE; b++) {
            v = pix[a * TILE + b];
            if (!v || !VISIBLE(x + b, y + a)) continue;
            d = fb + (s32)(y + a) * SCR_W + x + b;
            *d = shadow_table[*d][v & 15];
        }
}

void darken_rect(s16 x, s16 y, s16 w, s16 h, s16 level)
{
    s16 a, b;
    u8 *d;
    for (a = 0; a < h; a++)
        for (b = 0; b < w; b++)
            if (VISIBLE(x + b, y + a)) {
                d = fb + (s32)(y + a) * SCR_W + x + b;
                *d = shadow_table[*d][level & 15];
            }
}

/* ------------------------------------------------------------------ */
/* game font: proportional, glyph value 1 = black outline, >1 = colour ramp + row */

static void glyph_span(u8 g, s16 *x1, s16 *x2)
{
    const u8 *gl = font_data + g * 64;
    s16 c, d;
    for (*x1 = 0; *x1 < 8; (*x1)++) {
        for (d = 0; d < 8 && gl[d * 8 + *x1] <= 1; d++) ;
        if (d < 8) break;
    }
    for (*x2 = 8; *x2 > 0; (*x2)--) {
        c = (s16)(*x2 - 1);
        for (d = 0; d < 8 && gl[d * 8 + c] <= 1; d++) ;
        if (d < 8) break;
    }
    if (*x1 >= *x2) {                     /* space */
        *x1 = 0;
        *x2 = 4;
    }
}

void text(s16 x, s16 y, const char *s, u8 ramp)
{
    s16 c, d, x1, x2, l, r;
    u8 g, v;
    const u8 *gl;
    for (; *s; s++) {
        g = (u8)*s;
        gl = font_data + g * 64;
        glyph_span(g, &x1, &x2);
        l = (s16)(x1 > 0 ? x1 - 1 : 0);
        r = (s16)(x2 < 8 ? x2 + 1 : 8);
        for (d = 0; d < 8; d++)
            for (c = l; c < r; c++) {
                v = gl[d * 8 + c];
                if (v) put_pixel((s16)(x + c - x1), (s16)(y + d), v == 1 ? col_black : (u8)(ramp + d));
            }
        x = (s16)(x + x2 - x1 + 1);
    }
}

s16 text_width(const char *s)
{
    s16 w = 0, x1, x2;
    for (; *s; s++) {
        glyph_span((u8)*s, &x1, &x2);
        w = (s16)(w + x2 - x1 + 1);
    }
    return w;
}

void text_centre(s16 y, const char *s, u8 ramp)
{
    text((s16)((SCR_W - text_width(s)) / 2), y, s, ramp);
}

void box(s16 x, s16 y, s16 w, s16 h)
{
    darken_rect((s16)(x + 1), (s16)(y + 1), (s16)(w - 2), (s16)(h - 2), 12);
    rect(x, y, w, h, col_grey);
}
