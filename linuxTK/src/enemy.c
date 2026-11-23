/* enemy.c - enemy setup, AI, movement, shooting and death (GAME.CPP, CLASSES.CPP).
 * Enemies aim with full binary angles; sprites show the nearest of 8 directions. */
#include "globals.h"

#define LOW_ENERGY (10 * TENTHS)

void alusta_enemies(void)
{
    s16 a, b, c = 0;
    u8 d;
    ENEMIES = 0;
    for (b = 0; b < DIFF_ENEMIES; b++) {
        ENEMIES += level_enemies[b];
        for (a = 0; a < level_enemies[b] && c < MAX_ENEMIES; a++, c++) {
            Enemy *e = &enemy[c];
            e->push = 0;
            e->burning = 0;
            e->frying_player = -1;
            e->angle = 0;
            e->tangle = 0;
            e->load_cnt = 0;
            e->shoot_cnt = 0;
            e->WALK = 0;
            e->FIRE = 0;
            e->ANIM = 1;
            for (d = 0; d < MAX_PLAYERS; d++) {
                e->SEE_PL[d] = 0;
                e->angle_pl[d] = 0;
            }
            e->DEAD = 0;
            e->walk_cnt = 0;
            e->see_somebody = 0;
            e->type = (u8)b;
            e->energy = enemy_info[b].energy;
        }
    }
    if (ENEMIES > MAX_ENEMIES) ENEMIES = MAX_ENEMIES;
}

void place_enemies(void)
{
    s16 a, b, nx = 0, ny = 0, tries;
    u8 c, done;
    static s16 tx[MAX_ENEMIES], ty[MAX_ENEMIES];
    for (a = 0; a < ENEMIES; a++) {
        done = 0;
        tries = 0;
        while (!done && tries++ < 30000) {
            nx = (s16)((rnd() % (level_x_size - 2)) + 1);
            ny = (s16)((rnd() % (level_y_size - 2)) + 1);
            if (!(level_tile[ny * level_x_size + nx] & 0x80)) {
                done = 1;
                for (b = 0; b < a; b++)
                    if (nx == tx[b] && ny == ty[b]) done = 0;
                for (c = 0; c < ACTIVE_PLAYERS && done; c++)
                    if (get_dist(nx, ny, pl_start_x[c], pl_start_y[c]) < 8) done = 0;
            }
        }
        tx[a] = nx;
        ty[a] = ny;
        enemy[a].x = FX(nx * 20 - 5);
        enemy[a].y = FX(ny * 20 - 5);
        enemy[a].angle = (u8)rnd();
        enemy[a].tangle = enemy[a].angle;
    }
}

/* ------------------------------------------------------------------ */

static void footprint(Enemy *e, u8 side)
{
    u8 a, ang = (u8)(e->angle + 64 + side * 128), t;
    s16 fx = (s16)(PX(e->x) + 15 + fx_sin(4, ang));
    s16 fy = (s16)(PX(e->y) + 15 + fx_cos(4, ang));
    t = tile_px(fx, fy);
    if (t & 0x80) return;
    for (a = 0; a < DUST_BLOCKS; a++)
        if (tile_num[t] == dust_blocks[a]) {
            new_effect(FOOTPRINT, fx, fy, 0, 0);
            return;
        }
}

static u8 blocked_y(s16 ei, s16 rnx, s16 edge)
{
    s16 a, x, y;
    for (a = 0; a < ENEMIES; a++) {
        if (a == ei || enemy[a].DEAD) continue;
        x = PX(enemy[a].x);
        y = PX(enemy[a].y);
        if (rnx + 15 > x && rnx + 15 < x + 30 && edge > y + 6 && edge < y + 22) return 1;
    }
    for (a = 0; a < MAX_PLAYERS; a++) {
        if (!player[a].enabled || player[a].DEAD) continue;
        x = PX(player[a].x);
        y = PX(player[a].y);
        if (rnx + 15 > x && rnx + 15 < x + 30 && edge > y + 6 && edge < y + 22) return 1;
    }
    return 0;
}

static u8 blocked_x(s16 ei, s16 rny, s16 edge)
{
    s16 a, x, y;
    for (a = 0; a < ENEMIES; a++) {
        if (a == ei || enemy[a].DEAD) continue;
        x = PX(enemy[a].x);
        y = PX(enemy[a].y);
        if (edge > x + 6 && edge < x + 22 && rny + 15 > y && rny + 15 < y + 30) return 1;
    }
    for (a = 0; a < MAX_PLAYERS; a++) {
        if (!player[a].enabled || player[a].DEAD) continue;
        x = PX(player[a].x);
        y = PX(player[a].y);
        if (edge > x + 6 && edge < x + 22 && rny + 15 > y && rny + 15 < y + 30) return 1;
    }
    return 0;
}

/* spd = whole pixels per tick, 0 = the enemy type's own speed */
static void enemy_move(s16 ei, u8 angle, s16 spd)
{
    Enemy *e = &enemy[ei];
    const s16 reuna = 6, reuna2 = 4;
    s16 rnx, rny, newx, newy, v;

    if (e->WALK == WALK_DELAY / 4 || e->WALK == WALK_DELAY * 3 / 4) footprint(e, 0);
    if (e->WALK == WALK_DELAY / 2 || e->WALK == WALK_DELAY - 1) footprint(e, 1);
    if (e->energy < LOW_ENERGY && e->WALK % 4 == 0 && rnd() % 3 == 0)
        new_effect(BLOOD2, (s16)(PX(e->x) + 15), (s16)(PX(e->y) + 15), 0, 0);
    v = (s16)((spd == 0 ? enemy_info[e->type].speed : spd) * FX_ONE);
    newx = (s16)(e->x + fx_sin(v, angle));
    newy = (s16)(e->y + fx_cos(v, angle));
    rnx = PX(newx);
    rny = PX(newy);
    if (newy < e->y && !wall_px(rnx + 14 - reuna2, rny + reuna) && !wall_px(rnx + 14 + reuna2, rny + reuna))
        if (!blocked_y(ei, rnx, rny + reuna)) e->y = newy;
    if (newy > e->y && !wall_px(rnx + 14 - reuna2, rny + 28 - reuna) && !wall_px(rnx + 14 + reuna2, rny + 28 - reuna))
        if (!blocked_y(ei, rnx, rny + 28 - reuna)) e->y = newy;
    if (newx < e->x && !wall_px(rnx + reuna, rny + 14 - reuna2) && !wall_px(rnx + reuna, rny + 14 + reuna2))
        if (!blocked_x(ei, rny, rnx + reuna)) e->x = newx;
    if (newx > e->x && !wall_px(rnx + 28 - reuna, rny + 14 - reuna2) && !wall_px(rnx + 28 - reuna, rny + 14 + reuna2))
        if (!blocked_x(ei, rny, rnx + 28 - reuna)) e->x = newx;
}

/* turn towards tangle along the shorter way */
static void enemy_rotate(Enemy *e)
{
    s8 diff = (s8)(e->tangle - e->angle);
    if (diff == 0) return;
    if (diff > -ROT_SPEED && diff < ROT_SPEED) e->angle = e->tangle;
    else if (diff > 0) e->angle = (u8)(e->angle + ROT_SPEED);
    else e->angle = (u8)(e->angle - ROT_SPEED);
}

static void enemy_get_angles(Enemy *e)
{
    u8 b;
    for (b = 0; b < MAX_PLAYERS; b++)
        if (player[b].enabled)
            e->angle_pl[b] = get_angle((s16)(PX(player[b].x) - PX(e->x)), (s16)(PX(player[b].y) - PX(e->y)));
}

static void try_see_players(Enemy *e)
{
    s16 rx, ry, ex = (s16)(PX(e->x) + 14), ey = (s16)(PX(e->y) + 14), px, py, dist;
    u8 a, b, hit, weakest = 0;
    s8 diff;
    u8 see[MAX_PLAYERS], saw[MAX_PLAYERS];

    e->see_somebody = 0;
    for (b = 0; b < MAX_PLAYERS; b++) {
        see[b] = 0;
        saw[b] = e->SEE_PL[b];
        e->SEE_PL[b] = 0;
    }
    for (b = 0; b < MAX_PLAYERS; b++) {
        if (!player[b].enabled || player[b].DEAD) continue;
        diff = (s8)(e->angle_pl[b] - e->angle);
        if (diff <= -64 || diff >= 64) continue;          /* 180 degree field of view */
        px = PX(player[b].x);
        py = PX(player[b].y);
        hit = 0;
        for (a = 0; a < 8 * 4 && !hit; a++) {
            rx = (s16)(ex + fx_sin((s16)(a * 5), e->angle_pl[b]));
            ry = (s16)(ey + fx_cos((s16)(a * 5), e->angle_pl[b]));
            if (wall_px(rx, ry)) hit = 1;
            if (rx > px + 5 && rx < px + 25 && ry > py + 5 && ry < py + 25) {
                e->SEE_PL[b] = 1;
                see[b] = 1;
                e->see_somebody = 1;
            }
        }
    }
    if (e->see_somebody) {
        for (b = 0; b < MAX_PLAYERS; b++)
            if (player[b].enabled && !player[b].DEAD && see[b]) weakest = b;
        for (b = 0; b < MAX_PLAYERS; b++)
            if (player[b].enabled && !player[b].DEAD && see[b] && player[b].energy < player[weakest].energy)
                weakest = b;
        e->tangle = e->angle_pl[weakest];
    } else {
        for (b = 0; b < MAX_PLAYERS; b++)
            if (player[b].enabled && saw[b]) {
                dist = get_dist(PX(e->x), PX(e->y), PX(player[b].x), PX(player[b].y));
                e->walk_cnt = (s16)(dist / enemy_info[e->type].speed);   /* go where the player was */
            }
    }
}

static void enemy_shoot(Enemy *e)
{
    u8 a, r, wi = enemy_info[e->type].curr_weapon, dir = (u8)(DIR_OF(e->angle) * DIR_STEP);
    const Weapon *w = &weapon[wi];
    s16 ex = PX(e->x), ey = PX(e->y);
    e->FIRE = 1;
    e->load_cnt = 0;
    for (a = 0; a < w->bullet_amount; a++) {
        r = (u8)((w->angle_add / 2) - rnd() % w->angle_add);
        new_bullet(w->btype, (s16)(ex + 14 + fx_sin(10, e->angle)), (s16)(ey + 14 + fx_cos(10, e->angle)),
                   (u8)(e->angle + r), 0, -1);
    }
    if (e->shoot_cnt % bullet_type[w->btype].mul == 0) {
        if (wi > 0) play_sound(w->sound, sample_rates[wi]);
        if (w->smoke)
            new_effect(SMOKE3, (s16)(ex + 14 + fx_sin(8, dir) + fx_sin(5, (u8)(dir + 192))),
                       (s16)(ey + 14 + fx_cos(8, dir) + fx_cos(5, (u8)(dir + 192))), e->angle, 1);
    }
}

static void chk_for_shoot(Enemy *e)
{
    u8 a, want = 0, wi = enemy_info[e->type].curr_weapon;
    const Bullet_type *bt = &bullet_type[weapon[wi].btype];
    s16 dist;
    if (wi > 0)
        for (a = 0; a < MAX_PLAYERS; a++)
            if (player[a].enabled && e->SEE_PL[a] && e->tangle == e->angle) {
                e->shoot_cnt++;
                want = 1;
                if (e->load_cnt == weapon[wi].loading_time) {
                    enemy_shoot(e);
                    dist = get_dist(PX(e->x), PX(e->y), PX(player[a].x), PX(player[a].y));
                    if (dist > bt->time * bt->speed)   /* too far: walk closer */
                        e->walk_cnt = (s16)(20 / enemy_info[e->type].speed);
                }
            }
    if (!want) e->shoot_cnt = 0;
}

static void enemy_animate(Enemy *e)
{
    e->ANIM = 1;
    if (e->WALK) {
        if (e->WALK - 1 < WALK_DELAY) e->WALK++;
        else e->WALK = 1;
        e->ANIM = (u8)((e->WALK - 1) / (WALK_DELAY / 4));
        if (e->ANIM >= 3) e->ANIM = 1;
    }
    if (e->FIRE) {
        e->FIRE++;
        if (e->FIRE > 3) e->FIRE = 0;
        e->ANIM = 3;
    }
    if (!e->DEAD && e->energy < LOW_ENERGY) e->ANIM += 4;
}

void enemy_get_damage(s16 ei, s16 damage, s8 from)
{
    Enemy *e = &enemy[ei];
    s16 ex = PX(e->x), ey = PX(e->y);
    u8 a, alku = (u8)(rnd() % 7);
    s16 spd = (s16)(1 + damage / (3 * TENTHS) + e->push / (3 * TENTHS));
    if (spd > 5) spd = 5;
    e->energy -= damage;
    if (e->energy > 0) return;

    if (enemy_info[e->type].explo_death) {
        explo((s16)(ex + 4), (s16)(ey + 4), 2, -1);
        for (a = 0; a < 3; a++) new_body_part((s16)(ex + 4), (s16)(ey + 4), (u8)rnd(), 5 * TENTHS, (u8)(a + 12));
    } else if (e->burning) {
        play_sound(WOOSHWAV, 20000);
        new_effect(EXPLO, (s16)(ex + 12), (s16)(ey + 12), 0, 0);
        new_effect(EXPLO, (s16)(ex + 18), (s16)(ey + 12), 0, 0);
        new_effect(EXPLO, (s16)(ex + 15), (s16)(ey + 18), 0, 0);
        for (a = 0; a < 3; a++) new_body_part((s16)(ex + 4), (s16)(ey + 4), (u8)rnd(), 5 * TENTHS, (u8)(a + 12));
    } else {
        for (a = 0; a < 16; a++)
            if (rnd() % 2) new_effect(BLOOD, (s16)(ex + 15), (s16)(ey + 15), (u8)(a * 16), 4);
        for (a = 0; a < 3; a++)
            new_body_part((s16)(ex + 4), (s16)(ey + 4), (u8)rnd(), (s16)((spd + rnd() % 2) * TENTHS),
                          (u8)((a + alku) % 7));
        play_sound(DEATHWAV, (s16)(20000 - (rnd() % 3000)));
    }
    e->DEAD = 1;
    DEAD_ENEMIES++;
    if (DEAD_ENEMIES >= ENEMIES && !next_level) next_level = 1;
    if (from >= 0) player[from].kills[e->type]++;
}

/* one tick of enemy AI, in the order of the original game_shit() */
void update_enemies(void)
{
    s16 a;
    u8 b, see;
    Enemy *e;

    for (a = 0; a < ENEMIES; a++) {           /* roll_enemies */
        e = &enemy[a];
        if (e->DEAD) continue;
        if (e->tangle == e->angle && e->walk_cnt == 0 && e->see_somebody == 0 && rnd() % 50 == 1)
            e->tangle = (u8)rnd();
        enemy_rotate(e);
    }
    for (a = 0; a < ENEMIES; a++)
        if (!enemy[a].DEAD) enemy_get_angles(&enemy[a]);
    for (a = 0; a < ENEMIES; a++)
        if (!enemy[a].DEAD) try_see_players(&enemy[a]);
    for (a = 0; a < ENEMIES; a++) {           /* move_enemies */
        e = &enemy[a];
        if (!e->DEAD) {
            see = 0;
            for (b = 0; b < MAX_PLAYERS && !see; b++)
                if (player[b].enabled && e->SEE_PL[b]) see = 1;
            if (!see && e->walk_cnt == 0 && rnd() % 100 == 1) e->walk_cnt = (s16)((rnd() % 40) + 20);
            if (e->walk_cnt > 0) {
                /* walk in the facing direction shown by the sprite */
                enemy_move(a, (u8)(DIR_OF(e->angle) * DIR_STEP), 0);
                e->walk_cnt--;
                if (e->WALK == 0) e->WALK = 1;
            } else e->WALK = 0;
        }
        if (e->push > 0) {
            if (e->push >= TENTHS) enemy_move(a, e->push_angle, (s16)(e->push / TENTHS));
            e->push -= 2;   /* 0.2 px per tick */
        }
    }
    for (a = 0; a < ENEMIES; a++)
        if (!enemy[a].DEAD) chk_for_shoot(&enemy[a]);
    for (a = 0; a < ENEMIES; a++) {           /* load */
        e = &enemy[a];
        if (!e->DEAD && e->load_cnt < weapon[enemy_info[e->type].curr_weapon].loading_time) e->load_cnt++;
    }
    for (a = 0; a < ENEMIES; a++) enemy_animate(&enemy[a]);
}
