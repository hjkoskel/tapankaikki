/* bullet.c - bullets, hit tests and explosions. Ported from CLASSES.CPP,
 * NEWFUNCS.CPP and MISCFUNC.CPP. Positions 12.4 fixed point. */
#include "globals.h"

void new_bullet(u8 type, s16 x, s16 y, u8 angle, s16 time_add, s8 owner)
{
    s16 a = 0;
    while (a < MAX_BULLETS && bullet[a].in_use) a++;
    if (a < MAX_BULLETS) {
        bullet[a].type = type;
        bullet[a].x = FX(x);
        bullet[a].y = FX(y);
        bullet[a].owner = owner;
        bullet[a].angle = angle;
        bullet[a].time = (s16)(bullet_type[type].time + time_add);
        bullet[a].count = 0;
        bullet[a].in_use = 1;
    }
}

void null_bullets(void)
{
    s16 a;
    for (a = 0; a < MAX_BULLETS; a++) bullet[a].in_use = 0;
}

void explo(s16 x, s16 y, u8 size, s8 owner)
{
    u8 a, n, rays = (u8)(size * 5);
    for (a = 0; a < rays; a++)
        new_bullet(EXPLO_RAY, x, y, (u8)(a * (256 / rays)), size, owner);
    n = (u8)((rnd() % 5) + 1);
    for (a = 0; a < n; a++)
        new_effect(METAL, x, y, (u8)rnd(), (u8)((rnd() % 3) + 4));
    play_sound(EXPLOWAV, (s16)(14000 - size * 1000));
}

static void bullet_move(Bullet *bu)
{
    const Bullet_type *t = &bullet_type[bu->type];
    s16 vx = 0, vy = 0, rx, ry, b, dmg = t->power;
    s16 step = 0, sx = 0, sy = 0;
    u8 a, hit = 0, kosh = 0, looptimes, speed = t->speed;

    /* the original moved speed/(speed/4) px per sub step, speed/4 sub steps */
    looptimes = (u8)(speed >> 2);
    if (speed == 0) looptimes = 1;
    if (speed > 0) {
        step = (s16)((speed * FX_ONE) / (speed >> 2));
        sx = fx_sin(step, bu->angle);
        sy = fx_cos(step, bu->angle);
    }
    rx = PX(bu->x);
    ry = PX(bu->y);
    bu->time--;
    bu->count++;
    for (a = 0; a < looptimes && !hit && !kosh; a++) {
        bu->x += sx;
        bu->y += sy;
        rx = PX(bu->x);
        ry = PX(bu->y);
        if (speed > 0 && wall_px(rx, ry)) kosh = 1;
        if (bu->count > t->sleep_time && t->stop) {
            for (b = 0; b < ENEMIES; b++) {
                Enemy *e = &enemy[b];
                s16 ex = PX(e->x), ey = PX(e->y);
                if (!e->DEAD && rx > ex + 5 && rx < ex + 22 && ry > ey + 5 && ry < ey + 22) {
                    hit = 1;
                    if (bu->owner >= 0) e->tangle = e->angle_pl[bu->owner]; /* turn towards shooter */
                    vx = (s16)(ex + 14 + fx_sin(4, bu->angle));
                    vy = (s16)(ey + 14 + fx_cos(4, bu->angle));
                    e->push_angle = bu->angle;
                    if (t->push_power * TENTHS > e->push) e->push = (s16)(t->push_power * TENTHS);
                    enemy_get_damage(b, dmg, bu->owner);
                    if (t->type == INCENDIARY) {
                        e->burning += 10;
                        e->frying_player = bu->owner;
                    }
                }
            }
            for (b = 0; b < MAX_PLAYERS; b++) {
                Player *p = &player[b];
                s16 px = PX(p->x), py = PX(p->y);
                if (p->enabled && !p->DEAD && rx > px + 5 && rx < px + 22 && ry > py + 5 && ry < py + 22) {
                    hit = 1;
                    vx = (s16)(px + 14 + fx_sin(4, bu->angle));
                    vy = (s16)(py + 14 + fx_cos(4, bu->angle));
                    p->push_angle = bu->angle;
                    if (t->push_power * TENTHS > p->push) p->push = (s16)(t->push_power * TENTHS);
                    player_get_damage((u8)b, dmg, bu->owner);
                    if (t->type == INCENDIARY) {
                        p->burning += 10;
                        p->frying_player = bu->owner;
                    }
                }
            }
        }
    }
    if (t->type == NORMAL && kosh && rnd() % 8 == 1)
        new_effect(METAL, rx, ry, (u8)rnd(), (u8)((rnd() % 3) + 5));

    if (hit) {
        if (t->type == NORMAL) {
            u8 bp = (u8)(6 + dmg / 30 + rnd() % 3);
            if (bp > 8) bp = 8;
            for (a = 0; a < dmg / 50 + 1; a++)
                new_effect(BLOOD, (s16)(vx + (rnd() % 6) - 3), (s16)(vy + (rnd() % 6) - 3),
                           (u8)(bu->angle + (rnd() % 22) - 11), bp);
        }
        if (bu->owner >= 0) player[bu->owner].hitten++;
        if (speed < 10 && t->time == 1) play_sound(weapon[0].sound, sample_rates[0]); /* fist */
    }

    if (t->type == NORMAL && !hit && (bu->time == 0 || kosh) && !(speed < 10 && t->time == 1))
        new_effect(SMOKE, rx, ry, 0, 0);

    if (hit || kosh || bu->time == 0) {
        if (t->type == EXPLOSIVE)
            explo((s16)(rx - fx_sin(5, bu->angle)), (s16)(ry - fx_cos(5, bu->angle)), t->explos, bu->owner);
        bu->in_use = 0;
    }
    if (t->effect != 0) {
        if (t->effect == EXPLO) new_effect(EXPLO, rx, ry, (u8)(bu->angle + 128), 1);
        else new_effect(t->effect, rx, ry, 0, 0);
    }
}

void move_bullets(void)
{
    s16 a;
    for (a = 0; a < MAX_BULLETS; a++)
        if (bullet[a].in_use) bullet_move(&bullet[a]);
}
