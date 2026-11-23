/* effect.c - effects (blood, splats, footprints, smoke, sparks, explosions)
 * and body parts. Ported from NEWFUNCS.CPP and CLASSES.CPP.
 * Positions 12.4 fixed point, x/y arguments in pixels. */
#include "globals.h"

void new_effect(u8 type, s16 x, s16 y, u8 angle, u8 speed)
{
    s16 a = 0, num, oldest;

    effect_age_count++;
    while (a < MAX_EFFECTS && effect[a].in_use) a++;
    if (a < MAX_EFFECTS) {
        num = a;
    } else {
        /* pool full: reuse the oldest effect (wrap-safe age compare) */
        oldest = 0;
        for (a = 1; a < MAX_EFFECTS; a++)
            if ((u16)(effect_age_count - effect[a].age) > (u16)(effect_age_count - effect[oldest].age))
                oldest = a;
        num = oldest;
    }
    effect[num].age = effect_age_count;
    effect[num].type = type;
    effect[num].x = FX(x);
    effect[num].y = FX(y);
    effect[num].count = 0;
    effect[num].angle = angle;
    effect[num].speed = speed;
    effect[num].in_use = 1;
}

static void effect_roll(Effect *e)
{
    const Effect_type *t = &effect_type[e->type];
    u8 end = (u8)(t->tcount * t->delay);
    if (!e->in_use) return;
    if (e->count < end) {
        if (t->random == 0) e->count++;
        else e->count = (u8)(e->count + rnd() % 3);
    }
    if (e->count >= end) e->in_use = 0;
    if (e->speed > 0) {
        e->x += fx_sin((s16)(e->speed * FX_ONE), e->angle);
        e->y += fx_cos((s16)(e->speed * FX_ONE), e->angle);
        if (wall_px(PX(e->x), PX(e->y))) e->in_use = 0;
    }
    /* a flying blood drop leaves a splat where it lands */
    if (!e->in_use && t->index == BLOOD)
        new_effect(BLOOD2, PX(e->x), PX(e->y), 0, 0);
}

void roll_effects(void)
{
    s16 a;
    for (a = 0; a < MAX_EFFECTS; a++) effect_roll(&effect[a]);
}

void null_effects(void)
{
    s16 a;
    for (a = 0; a < MAX_EFFECTS; a++) effect[a].in_use = 0;
}

/* ------------------------------------------------------------------ */

/* speed in tenths of a pixel per tick */
void new_body_part(s16 x, s16 y, u8 angle, s16 speed, u8 num)
{
    s16 a = 0;
    while (a < MAX_BODY_PARTS && body_part[a].in_use) a++;
    if (a < MAX_BODY_PARTS) {
        body_part[a].num = num;
        body_part[a].x = FX(x);
        body_part[a].y = FX(y);
        body_part[a].angle = angle;
        body_part[a].speed = speed;
        body_part[a].in_use = 1;
    }
}

static void body_part_move(Body_part *b)
{
    const s16 reuna = 4, reuna2 = 4;
    s16 v = (s16)((b->speed / TENTHS) * FX_ONE);     /* original moved by (int)speed */
    s16 newx = (s16)(b->x + fx_sin(v, b->angle));
    s16 newy = (s16)(b->y + fx_cos(v, b->angle));
    s16 rnx = PX(newx), rny = PX(newy);

    /* flesh parts (< 12) leave a blood trail */
    if (b->num < 12 && rnd() % 3 != 0)
        new_effect(BLOOD2, (s16)(PX(b->x) + 10), (s16)(PX(b->y) + 10), 0, 0);

    if (newy < b->y && !wall_px(rnx + 10 - reuna2, rny + reuna) && !wall_px(rnx + 10 + reuna2, rny + reuna))
        b->y = newy;
    if (newy > b->y && !wall_px(rnx + 10 - reuna2, rny + 20 - reuna) && !wall_px(rnx + 10 + reuna2, rny + 20 - reuna))
        b->y = newy;
    if (newx < b->x && !wall_px(rnx + reuna, rny + 10 - reuna2) && !wall_px(rnx + reuna, rny + 10 + reuna2))
        b->x = newx;
    if (newx > b->x && !wall_px(rnx + 20 - reuna, rny + 10 - reuna2) && !wall_px(rnx + 20 - reuna, rny + 10 + reuna2))
        b->x = newx;
}

void move_body_parts(void)
{
    s16 a;
    for (a = 0; a < MAX_BODY_PARTS; a++)
        if (body_part[a].in_use && body_part[a].speed > 0) {
            body_part_move(&body_part[a]);
            body_part[a].speed -= 2;    /* 0.2 px per tick */
        }
}

void null_body_parts(void)
{
    s16 a;
    for (a = 0; a < MAX_BODY_PARTS; a++) body_part[a].in_use = 0;
}
