/* gfx.c - framebuffer drawing: clipped blitters and the bitmap font.
 * Every draw is clipped to the global window [clip_x0, clip_x1) x [0, SCR_H). */
#include <string.h>
#include "globals.h"

void set_view(int i)
{
    const View *v = &view[i];
    clip_x0 = v->clip_x0;
    clip_x1 = v->clip_x1;
    view_ox = v->origin_x;
    cam_x = player[v->owner].scr_x;
    cam_y = player[v->owner].scr_y;
}

void set_full_clip(void)
{
    clip_x0 = 0;
    clip_x1 = SCR_W;
    view_ox = 0;
}

void clear_screen(void)
{
    memset(fb, 0, sizeof(fb));
    set_full_clip();
}

/* Clip a w*h rectangle at (x,y). Returns 0 if nothing is visible.
 * sx,sy = first visible source column/row, ex,ey = end (exclusive). */
static u8 clip_box(s16 x, s16 y, s16 w, s16 h, s16 *sx, s16 *sy, s16 *ex, s16 *ey)
{
    *sx = (s16)(x < clip_x0 ? clip_x0 - x : 0);
    *sy = (s16)(y < 0 ? -y : 0);
    *ex = (s16)(x + w > clip_x1 ? clip_x1 - x : w);
    *ey = (s16)(y + h > SCR_H ? SCR_H - y : h);
    return (u8)(*sx < *ex && *sy < *ey);
}

/* address helpers: fb is 64000 bytes, offsets need more than 16 bits */
#define FB_AT(x, y) (fb + (s32)(y) * SCR_W + (x))
#define FRAME(f, i) ((f)->pix + (s32)(i) * (f)->w * (f)->h)

void blit_tile(s16 x, s16 y, const u8 *tile)
{
    s16 sx, sy, ex, ey, a;
    const u8 *src;
    u8 *dst;
    if (!clip_box(x, y, TILE_SIZE, TILE_SIZE, &sx, &sy, &ex, &ey)) return;
    src = tile + sy * TILE_SIZE + sx;
    dst = FB_AT(x + sx, y + sy);
    for (a = sy; a < ey; a++, src += TILE_SIZE, dst += SCR_W)
        memcpy(dst, src, (size_t)(ex - sx));
}

void blit(s16 x, s16 y, const Frames *f, s16 i)
{
    s16 sx, sy, ex, ey, a, b;
    const u8 *src;
    u8 *dst;
    if (!clip_box(x, y, f->w, f->h, &sx, &sy, &ex, &ey)) return;
    for (a = sy; a < ey; a++) {
        src = FRAME(f, i) + a * f->w;
        dst = FB_AT(x, y + a);
        for (b = sx; b < ex; b++)
            if (src[b]) dst[b] = src[b];
    }
}

/* player sprite: colours 80..95 are replaced with the player's colour range */
void blit_recolor(s16 x, s16 y, const Frames *f, s16 i, u8 col)
{
    s16 sx, sy, ex, ey, a, b;
    const u8 *src;
    u8 *dst, c;
    if (!clip_box(x, y, f->w, f->h, &sx, &sy, &ex, &ey)) return;
    for (a = sy; a < ey; a++) {
        src = FRAME(f, i) + a * f->w;
        dst = FB_AT(x, y + a);
        for (b = sx; b < ex; b++) {
            c = src[b];
            if (c) {
                if ((u8)(c - 80) < 16) dst[b] = (u8)(col + (c - 80));
                else dst[b] = c;
            }
        }
    }
}

/* translucent sprite: pixels are small indices into trans_lut (see assets.c) */
void blit_trans(s16 x, s16 y, const Frames *f, s16 i)
{
    s16 sx, sy, ex, ey, a, b;
    const u8 *src;
    u8 *dst;
    if (!clip_box(x, y, f->w, f->h, &sx, &sy, &ex, &ey)) return;
    for (a = sy; a < ey; a++) {
        src = FRAME(f, i) + a * f->w;
        dst = FB_AT(x, y + a);
        for (b = sx; b < ex; b++)
            if (src[b]) dst[b] = trans_lut[src[b]][dst[b]];
    }
}

void put_pixel(s16 x, s16 y, u8 col)
{
    if (x >= clip_x0 && x < clip_x1 && y >= 0 && y < SCR_H)
        *FB_AT(x, y) = col;
}

void rectangle(s16 x1, s16 y1, s16 x2, s16 y2, u8 col)
{
    s16 a;
    for (a = x1; a <= x2; a++) {
        put_pixel(a, y1, col);
        put_pixel(a, y2, col);
    }
    for (a = (s16)(y1 + 1); a < y2; a++) {
        put_pixel(x1, a, col);
        put_pixel(x2, a, col);
    }
}

/* ------------------------------------------------------------------ */
/* font: proportional, value 1 = black outline, >1 = colour cq + row   */

static void glyph_span(u8 g, s16 *x1, s16 *x2)
{
    const u8 *gl = font.data + g * font.w * font.h;
    s16 c, d;
    u8 hit;
    for (c = 0; c < font.w; c++) {
        for (hit = 0, d = 0; d < font.h; d++)
            if (gl[d * font.w + c] > 1) hit = 1;
        if (hit) break;
    }
    *x1 = c;
    for (c = font.w; c > 0; c--) {
        for (hit = 0, d = 0; d < font.h; d++)
            if (gl[d * font.w + c - 1] > 1) hit = 1;
        if (hit) break;
    }
    *x2 = c;
    if (*x1 > *x2) {
        *x1 = 0;
        *x2 = (s16)(font.w / 2);
    }
}

void writefonts(s16 x, s16 y, const char *str, u8 cq)
{
    s16 a, c, d, xp1, xp2, l, r, cnt = 0;
    u8 g, col;
    const u8 *gl;
    for (a = 0; str[a]; a++) {
        g = (u8)str[a];
        gl = font.data + g * font.w * font.h;
        glyph_span(g, &xp1, &xp2);
        l = (s16)(xp1 - 1); if (l < 0) l = 0;
        r = (s16)(xp2 + 1); if (r > font.w) r = font.w;
        for (d = 0; d < font.h; d++)
            for (c = l; c < r; c++) {
                col = gl[d * font.w + c];
                if (col) {
                    if (cq != 0 && col != 1) put_pixel((s16)(c - xp1 + x + cnt), (s16)(d + y), (u8)(cq + d));
                    else put_pixel((s16)(c - xp1 + x + cnt), (s16)(d + y), 0);
                }
            }
        cnt += xp2 - xp1 + 1;
    }
}

s16 str_length(const char *str)
{
    s16 a, xp1, xp2, len = 0;
    for (a = 0; str[a]; a++) {
        glyph_span((u8)str[a], &xp1, &xp2);
        len += xp2 - xp1 + 1;
    }
    return len;
}
