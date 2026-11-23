/* render.c - draws the game world: one render_view() per split screen view.
 * Replaces the duplicated mode 0/1/2 draw loops of DRAW.CPP. */
#include <stdio.h>
#include "globals.h"

#define SX(px) ((s16)((px) - cam_x + view_ox))
#define SY(py) ((s16)((py) - cam_y))

static void draw_tiles(u8 walls)
{
    s16 tx0 = (s16)(cam_x / TILE_SIZE), ty0 = (s16)(cam_y / TILE_SIZE), tx, ty;
    s16 tw = (s16)((clip_x1 - clip_x0) / TILE_SIZE + 1), th = SCR_H / TILE_SIZE + 1;
    u8 t;
    for (ty = ty0; ty <= ty0 + th; ty++)
        for (tx = tx0; tx <= tx0 + tw; tx++) {
            if (tx < 0 || ty < 0 || tx >= level_x_size || ty >= level_y_size) {
                if (walls) blit_tile(SX(tx * 20), SY(ty * 20), frm_walls.pix + 19 * 400);
                continue;
            }
            t = level_tile[ty * level_x_size + tx];
            if (walls && (t & 0x80))
                blit_tile(SX(tx * 20), SY(ty * 20), frm_walls.pix + tile_num[t] * 400);
            else if (!walls && !(t & 0x80))
                blit_tile(SX(tx * 20), SY(ty * 20), frm_floor.pix + tile_num[t] * 400);
        }
}

static void draw_players(u8 live)
{
    u8 a;
    for (a = 0; a < MAX_PLAYERS; a++) {
        Player *p = &player[a];
        if (p->enabled && (live ? !p->DEAD : p->DEAD))
            blit_recolor(SX(PX(p->x)), SY(PX(p->y)), &frm_player, (s16)(DIR_OF(p->angle) * PLAYER_ANIMS + p->ANIM), p->color);
    }
}

static void draw_effects(u8 order)
{
    s16 a, fr;
    for (a = 0; a < MAX_EFFECTS; a++) {
        Effect *e = &effect[a];
        const Effect_type *t = &effect_type[e->type];
        if (!e->in_use || t->order != order) continue;
        fr = (s16)(e->count / t->delay);
        if (fr >= t->tcount) fr = (s16)(t->tcount - 1);
        if (t->trans) blit_trans((s16)(SX(PX(e->x)) - t->xs / 2), (s16)(SY(PX(e->y)) - t->ys / 2), &frm_effect[e->type], fr);
        else blit((s16)(SX(PX(e->x)) - t->xs / 2), (s16)(SY(PX(e->y)) - t->ys / 2), &frm_effect[e->type], fr);
    }
}

static void draw_body_parts(void)
{
    s16 a;
    for (a = 0; a < MAX_BODY_PARTS; a++)
        if (body_part[a].in_use)
            blit(SX(PX(body_part[a].x)), SY(PX(body_part[a].y)), &frm_bodypart, body_part[a].num);
}

static void draw_crates(void)
{
    s16 a;
    for (a = 0; a < MAX_CRATES; a++)
        if (crate[a].in_use) blit(SX(crate[a].x), SY(crate[a].y), &frm_crate, crate[a].type);
}

static void draw_bullets(void)
{
    s16 a, fr, x, y;
    const Frames *f;
    for (a = 0; a < MAX_BULLETS; a++) {
        Bullet *b = &bullet[a];
        if (!b->in_use) continue;
        x = SX(PX(b->x));
        y = SY(PX(b->y));
        switch (bullet_type[b->type].sprite) {
        case BSPR_GRENADE: f = &frm_bomb; break;
        case BSPR_C4:      f = &frm_c4; break;
        case BSPR_MINE:    f = &frm_mine; break;
        case BSPR_FLAME:
            /* the original showed flames only as a light; use explosion frames */
            fr = (s16)(b->count / 2);
            if (fr > 8) fr = 8;
            blit((s16)(x - 12), (s16)(y - 12), &frm_effect[EXPLO], fr);
            continue;
        default: continue;
        }
        blit((s16)(x - (f->w >> 1)), (s16)(y - (f->h >> 1)), f, 0);
    }
}

static void draw_live_enemies(void)
{
    s16 a;
    for (a = 0; a < ENEMIES; a++) {
        Enemy *e = &enemy[a];
        if (!e->DEAD)
            blit(SX(PX(e->x)), SY(PX(e->y)), &frm_enemy[e->type], (s16)(DIR_OF(e->angle) * ENEMY_ANIMS + e->ANIM));
    }
}

/* laser sight (bought "Target system") */
static void draw_target(u8 pi)
{
    Player *p = &player[pi];
    const Bullet_type *bt;
    s16 x, y, a, dist, sx, sy, dx, dy;
    if (p->DEAD || !weapon[p->curr_weapon].gun || !p->TARGET) return;
    bt = &bullet_type[weapon[p->curr_weapon].btype];
    x = (s16)(p->x + FX(14) + fx_sin(FX(10), p->angle));
    y = (s16)(p->y + FX(14) + fx_cos(FX(10), p->angle));
    dx = fx_sin(FX_ONE, p->angle);
    dy = fx_cos(FX_ONE, p->angle);
    dist = (s16)(bt->time * bt->speed);
    for (a = 0; a < dist; a++) {
        sx = SX(PX(x));
        sy = SY(PX(y));
        if (sx < clip_x0 || sx >= clip_x1 || sy < 0 || sy >= SCR_H || wall_px(PX(x), PX(y))) break;
        if ((a - plot_count / 2) % 6 == 0) put_pixel(sx, sy, 9 * 16);
        x += dx;
        y += dy;
    }
    if (a == dist || wall_px(PX(x), PX(y))) blit((s16)(SX(PX(x)) - 3), (s16)(SY(PX(y)) - 3), &frm_target, 0);
}

static void draw_hud(u8 pi)
{
    Player *p = &player[pi];
    char text[20];
    s16 a, n, len, total;
    u8 bt = weapon[p->curr_weapon].btype;
    if (p->HIT > 0) rectangle(clip_x0, 0, (s16)(clip_x1 - 1), SCR_H - 1, (u8)((4 * 16 - 1) - p->HIT));
    writefonts((s16)(view_ox + 3), SCR_H - 15, weapon[p->curr_weapon].name, C_WHITE);
    if (p->curr_weapon > 0) {
        n = (s16)(p->bullets[bt - 1] / bullet_type[bt].mul);
        if (n < 0) n = 1;
        sprintf(text, "%d", n);
        writefonts((s16)(view_ox + 3), SCR_H - 23, text, C_YELLOW);
    }
    /* energy bar: 1 pixel per 5 energy, colour fades with the bar length */
    total = (s16)((p->max_energy + p->shield * 10 * TENTHS) / (5 * TENTHS));
    len = (s16)(p->energy / (5 * TENTHS));
    for (a = 0; a < len; a++) {
        put_pixel((s16)(view_ox + 3 + a), SCR_H - 5, (u8)(239 - a * 15 / total));
        put_pixel((s16)(view_ox + 3 + a), SCR_H - 4, (u8)(239 - a * 15 / total));
    }
}

static void render_view(u8 i)
{
    set_view(i);
    draw_tiles(0);
    draw_players(0);        /* dead players */
    draw_effects(0);        /* splats, footprints, smoke */
    draw_body_parts();
    draw_crates();
    draw_bullets();
    draw_live_enemies();
    draw_players(1);
    draw_tiles(1);          /* walls */
    draw_effects(1);
    draw_target(view[i].owner);
    draw_hud(view[i].owner);
}

static void draw_map(void)
{
    s16 a, b, cx, cy;
    u8 *d;
    if (GAME_MODE != SPLIT_SCREEN) {
        cx = (s16)(SCR_W - level_x_size);
        cy = 0;
    } else {
        cx = (s16)(160 - level_x_size / 2);
        cy = (s16)(SCR_H / 2 - level_y_size / 2);
    }
    for (a = 0; a < level_y_size; a++)
        for (b = 0; b < level_x_size; b++) {
            d = fb + (s32)(cy + a) * SCR_W + cx + b;
            if (level_tile[a * level_x_size + b] & 0x80) *d = 16;
            else *d = map_dark[*d];
        }
    for (a = 0; a < ENEMIES; a++)
        if (enemy[a].DEAD || SHOW_ENEMIES)
            put_pixel((s16)((PX(enemy[a].x) + 15) / 20 + cx), (s16)((PX(enemy[a].y) + 15) / 20 + cy),
                      (u8)(enemy[a].DEAD ? 9 : 3));
    if (GAME_MODE != SPLIT_SCREEN)
        for (a = 0; a < MAX_PLAYERS; a++)
            if (player[a].enabled)
                put_pixel((s16)((PX(player[a].x) + 15) / 20 + cx), (s16)((PX(player[a].y) + 15) / 20 + cy),
                          player[a].color);
}

void render_game(void)
{
    u8 a, ang;
    s16 r;
    for (a = 0; a < num_views; a++) render_view(a);
    set_full_clip();
    if (num_views == 2)
        for (r = 0; r < SCR_H; r++) fb[(s32)r * SCR_W + 160] = 0;
    if (MAP_ON) draw_map();
    for (a = 0; a < messages; a++) writefonts(5, (s16)(5 + a * 9), message[a], C_GREY);
    if (next_level) {
        /* "MISSION COMPLETED" spirals into the centre */
        ang = (u8)(next_level * 4);
        writefonts((s16)(160 - 45 + fx_sin((s16)(160 - next_level * 160L / C_DELAY), ang)),
                   (s16)(SCR_H / 2 + fx_cos((s16)(100 - next_level * 100L / C_DELAY), ang)),
                   "MISSION COMPLETED", C_RED);
    }
    writefonts(151, 10, c_2_c(complete_time), complete_time > TIME_LIMIT(level_time_limit) ? C_RED : C_WHITE);
}
