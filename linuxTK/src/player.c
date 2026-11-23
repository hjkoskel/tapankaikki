/* player.c - player control, movement, shooting and damage (CLASSES.CPP, MISCFUNC.CPP).
 * The player faces one of 8 directions; positions are 12.4 fixed point. */
#include <stdio.h>
#include <string.h>
#include "globals.h"
#include "plat.h"

#define LOW_ENERGY (20 * TENTHS)

static void footprint(Player *p, u8 side)
{
    u8 a, ang = (u8)(p->angle + 64 + side * 128), t;
    s16 fx = (s16)(PX(p->x) + 15 + fx_sin(4, ang));
    s16 fy = (s16)(PX(p->y) + 15 + fx_cos(4, ang));
    t = tile_px(fx, fy);
    if (t & 0x80) return;
    for (a = 0; a < DUST_BLOCKS; a++)
        if (tile_num[t] == dust_blocks[a]) {
            new_effect(FOOTPRINT, fx, fy, 0, 0);
            return;
        }
}

static void kick_body_parts(Player *p)
{
    s16 a, cx = (s16)(PX(p->x) + 14), cy = (s16)(PX(p->y) + 14), bx, by;
    for (a = 0; a < MAX_BODY_PARTS; a++) {
        Body_part *b = &body_part[a];
        if (!b->in_use) continue;
        bx = PX(b->x);
        by = PX(b->y);
        if (cx > bx && cx < bx + 20 && cy > by && cy < by + 20) {
            p->KICK = 1;
            b->angle = (u8)(p->angle - 4 + rnd() % 8);
            b->speed = 6 * TENTHS;
            play_sound(HITWAV, 10000);
        }
    }
}

/* collision with other live actors (rules from Player::move), pixel coords */
static u8 blocked_y(u8 pi, s16 rnx, s16 edge)
{
    s16 a, x, y;
    for (a = 0; a < ENEMIES; a++) {
        if (enemy[a].DEAD) continue;
        x = PX(enemy[a].x);
        y = PX(enemy[a].y);
        if (rnx + 15 > x && rnx + 15 < x + 30 && edge > y + 6 && edge < y + 22) return 1;
    }
    for (a = 0; a < MAX_PLAYERS; a++) {
        if (!player[a].enabled || a == pi || player[a].DEAD) continue;
        x = PX(player[a].x);
        y = PX(player[a].y);
        if (rnx + 15 > x && rnx + 15 < x + 30 && edge > y + 6 && edge < y + 22) return 1;
    }
    return 0;
}

static u8 blocked_x(u8 pi, s16 rny, s16 edge)
{
    s16 a, x, y;
    for (a = 0; a < ENEMIES; a++) {
        if (enemy[a].DEAD) continue;
        x = PX(enemy[a].x);
        y = PX(enemy[a].y);
        if (edge > x + 6 && edge < x + 22 && rny + 15 > y && rny + 15 < y + 30) return 1;
    }
    for (a = 0; a < MAX_PLAYERS; a++) {
        if (!player[a].enabled || a == pi || player[a].DEAD) continue;
        x = PX(player[a].x);
        y = PX(player[a].y);
        if (edge > x + 6 && edge < x + 22 && rny + 15 > y && rny + 15 < y + 30) return 1;
    }
    return 0;
}

static void take_crates(Player *p)
{
    s16 a, mx, px = PX(p->x), py = PX(p->y), full = (s16)(p->max_energy + p->shield * 10 * TENTHS);
    u8 taken;
    char text[MAX_MESSAGE_LENGTH];
    for (a = 0; a < MAX_CRATES; a++) {
        Crate *c = &crate[a];
        if (!c->in_use || !(c->x + 7 > px && c->x + 7 < px + 30 && c->y + 7 > py && c->y + 7 < py + 30))
            continue;
        taken = 0;
        if (c->type == 0 && p->weapons[c->weapon + 1] == 0) {
            p->weapons[c->weapon + 1] = 1;
            taken = 1;
        }
        if (c->type == 1) {
            mx = (s16)(bullet_type[c->bullet + 1].max * bullet_type[c->bullet + 1].mul);
            if (p->bullets[c->bullet] < mx) {
                p->bullets[c->bullet] += bullet_type[c->bullet + 1].amount_in_crate;
                if (p->bullets[c->bullet] > mx) p->bullets[c->bullet] = mx;
                taken = 1;
            }
        }
        if (c->type == 2 && p->energy < full) {
            p->energy += 40 * TENTHS;
            if (p->energy > full) p->energy = full;
            taken = 1;
        }
        if (taken) {
            play_sound(KLIKWAV, 11250);
            /* the original flew the item name to the HUD; port uses the message board */
            if (c->type == 0) sprintf(text, "%.9s: %.25s", p->name, weapon[c->weapon + 1].name);
            else if (c->type == 1) sprintf(text, "%.9s: %.25s", p->name, bullet_type[c->bullet + 1].name);
            else sprintf(text, "%.9s: Energy", p->name);
            add_message(text);
            c->in_use = 0;
        }
    }
}

/* spd_fx = speed in 12.4 pixels per tick */
void player_move(u8 pi, u8 angle, s16 spd_fx)
{
    Player *p = &player[pi];
    const s16 reuna = 6, reuna2 = 4;
    s16 newx, newy, rnx, rny;

    if (p->WALK == WALK_DELAY / 4 || p->WALK == WALK_DELAY * 3 / 4) footprint(p, 0);
    if (p->WALK == WALK_DELAY / 2 || p->WALK == WALK_DELAY - 1) footprint(p, 1);
    if (p->energy < LOW_ENERGY && p->WALK % 4 == 0 && rnd() % 3 == 0)   /* wounded: drip blood */
        new_effect(BLOOD2, (s16)(PX(p->x) + 15), (s16)(PX(p->y) + 15), 0, 0);

    newx = (s16)(p->x + fx_sin(spd_fx, angle));
    newy = (s16)(p->y + fx_cos(spd_fx, angle));
    rnx = PX(newx);
    rny = PX(newy);
    if (newy < p->y && !wall_px(rnx + 14 - reuna2, rny + reuna) && !wall_px(rnx + 14 + reuna2, rny + reuna))
        if (!blocked_y(pi, rnx, rny + reuna)) p->y = newy;
    if (newy > p->y && !wall_px(rnx + 14 - reuna2, rny + 28 - reuna) && !wall_px(rnx + 14 + reuna2, rny + 28 - reuna))
        if (!blocked_y(pi, rnx, rny + 28 - reuna)) p->y = newy;
    if (newx < p->x && !wall_px(rnx + reuna, rny + 14 - reuna2) && !wall_px(rnx + reuna, rny + 14 + reuna2))
        if (!blocked_x(pi, rny, rnx + reuna)) p->x = newx;
    if (newx > p->x && !wall_px(rnx + 28 - reuna, rny + 14 - reuna2) && !wall_px(rnx + 28 - reuna, rny + 14 + reuna2))
        if (!blocked_x(pi, rny, rnx + 28 - reuna)) p->x = newx;
    take_crates(p);
}

static void change_weapon(Player *p)
{
    do {
        p->curr_weapon = (u8)((p->curr_weapon + 1) % (DIFF_WEAPONS + 1));
    } while (p->weapons[p->curr_weapon] == 0);
    p->load_cnt = 0;
}

static void shoot(u8 pi)
{
    Player *p = &player[pi];
    const Weapon *w = &weapon[p->curr_weapon];
    s16 px = PX(p->x), py = PX(p->y);
    u8 a, r;
    p->FIRE = 1;
    p->load_cnt = 0;
    for (a = 0; a < w->bullet_amount; a++) {
        p->shooted++;
        r = (u8)((w->angle_add >> 1) - rnd() % w->angle_add);
        new_bullet(w->btype, (s16)(px + 14 + fx_sin(10, p->angle)), (s16)(py + 14 + fx_cos(10, p->angle)),
                   (u8)(p->angle + r), 0, (s8)pi);
    }
    if (p->curr_weapon > 0) {
        p->bullets[w->btype - 1]--;
        if (p->shoot_cnt % bullet_type[w->btype].mul == 0) {
            if (w->sound) play_sound(w->sound, sample_rates[p->curr_weapon]);
            if (w->smoke)
                new_effect(SMOKE3,
                           (s16)(px + 14 + fx_sin(8, p->angle) + fx_sin(8, (u8)(p->angle + 192))),
                           (s16)(py + 14 + fx_cos(8, p->angle) + fx_cos(8, (u8)(p->angle + 192))),
                           p->angle, 1);
        }
    }
}

void player_chk_keys(u8 pi)
{
    Player *p = &player[pi];
    u32 k = p->buttons, pressed = p->buttons & ~p->prev_buttons;
    u8 walk_chk = 0, a;
    s16 speedi = p->speed;                         /* whole pixels, as the original int */
    s16 strafe = (s16)(p->speed * FX_ONE * 9 / 10); /* 0.9 * speed */
    s8 turn = 0;

    /* turning: one direction (22.5 deg) at once, repeating every 2-3 ticks while held */
    if ((k & BTN_LEFT) && !(k & BTN_STRAFE)) turn = 1;
    if ((k & BTN_RIGHT) && !(k & BTN_STRAFE)) turn = (s8)(turn - 1);
    if (turn) {
        if (p->turn_cnt == 0) {
            p->angle = (u8)(p->angle + turn * DIR_STEP);
            p->turn_cnt = (u8)TURN_DELAY(p->angle);
        }
        p->turn_cnt--;
    } else p->turn_cnt = 0;

    if (((k & BTN_STRAFE) && (k & BTN_LEFT)) || (k & BTN_SLEFT)) {
        player_move(pi, (u8)(p->angle + 64), strafe);
        walk_chk = 1;
        speedi = (s16)(p->speed * 8 / 10);
    }
    if (((k & BTN_STRAFE) && (k & BTN_RIGHT)) || (k & BTN_SRIGHT)) {
        player_move(pi, (u8)(p->angle + 192), strafe);
        walk_chk = 1;
        speedi = (s16)(p->speed * 8 / 10);
    }
    if (k & BTN_UP) {
        player_move(pi, p->angle, (s16)(speedi * FX_ONE));
        walk_chk = 1;
        if (rnd() % 20 == 0) kick_body_parts(p);
    }
    if (k & BTN_DOWN) {
        player_move(pi, (u8)(p->angle + 128), (s16)(speedi * FX_ONE * 3 / 4));
        walk_chk = 1;
    }
    if (walk_chk && p->WALK == 0) p->WALK = 1;
    if (!walk_chk) p->WALK = 0;

    if ((k & BTN_FIRE) && p->load_cnt == weapon[p->curr_weapon].loading_time) {
        if (p->curr_weapon != 0) {
            if (p->bullets[weapon[p->curr_weapon].btype - 1] > 0) shoot(pi);
            else {
                p->curr_weapon = 0;
                p->load_cnt = 0;
            }
        }
        if (p->curr_weapon == 0) {
            shoot(pi);
            kick_body_parts(p);
        }
    }
    if (!(k & BTN_FIRE)) p->shoot_cnt = 0;
    else p->shoot_cnt++;

    if (pressed & BTN_CHANGE) change_weapon(p);

    if (GAME_MODE != SPLIT_SCREEN)
        for (a = 0; a <= DIFF_WEAPONS; a++)
            if ((k & (BTN_W0 << a)) && p->weapons[a]) {
                p->curr_weapon = a;
                p->load_cnt = 0;
            }
}

static s16 iabs(s16 v)
{
    return v < 0 ? (s16)-v : v;
}

void player_move_scr(u8 pi)
{
    Player *p = &player[pi];
    s16 spd, px = (s16)(PX(p->x) + 14), py = (s16)(PX(p->y) + 14);
    s16 vw = GAME_MODE == SPLIT_SCREEN ? 160 : 320;
    s16 half = (s16)(vw / 2), look = GAME_MODE == SPLIT_SCREEN ? 35 : 25, lim = GAME_MODE == SPLIT_SCREEN ? 100 : 160;
    s16 tx = (s16)(px + fx_sin(look, p->angle));
    s16 ty = (s16)(py + fx_cos(25, p->angle));

    if (iabs((s16)(p->scr_x + half - px)) > lim) p->scr_x = (s16)(px - 14 - half);
    spd = (s16)(iabs((s16)(p->scr_x + half - tx)) / 4);
    if (p->scr_x + half < tx) p->scr_x += spd;
    if (p->scr_x + half > tx) p->scr_x -= spd;
    if (p->scr_x > level_x_size * 20 - vw) p->scr_x = (s16)(level_x_size * 20 - vw);
    if (p->scr_x < 0) p->scr_x = 0;

    if (iabs((s16)(p->scr_y + SCR_H / 2 - py)) > 120) p->scr_y = (s16)(py - 14 - SCR_H / 2);
    spd = (s16)(iabs((s16)(p->scr_y + SCR_H / 2 - ty)) / 4);
    if (p->scr_y + SCR_H / 2 < ty) p->scr_y += spd;
    if (p->scr_y + SCR_H / 2 > ty) p->scr_y -= spd;
    if (p->scr_y > level_y_size * 20 - SCR_H) p->scr_y = (s16)(level_y_size * 20 - SCR_H);
    if (p->scr_y < 0) p->scr_y = 0;
}

void player_load(u8 pi)
{
    Player *p = &player[pi];
    if (p->load_cnt < weapon[p->curr_weapon].loading_time) p->load_cnt++;
}

void player_animate(u8 pi)
{
    Player *p = &player[pi];
    u8 gun = weapon[p->curr_weapon].gun;
    p->ANIM = gun == 0 ? 1 : 4;
    if (p->WALK) {
        if (p->WALK - 1 < WALK_DELAY) p->WALK++;
        else p->WALK = 1;
        p->ANIM = (u8)((p->WALK - 1) / (WALK_DELAY / 4));
        if (p->ANIM >= 3) p->ANIM = 1;
        if (gun == 1) p->ANIM += 3;
    }
    if (p->FIRE) {
        p->FIRE++;
        if (p->FIRE > 3) p->FIRE = 0;
        p->ANIM = gun == 0 ? 7 : 6;
    }
    if (p->DEAD) {
        if (p->DEAD - 1 < DEAD_DELAY) p->DEAD++;
        p->ANIM = (u8)(8 + ((p->DEAD - 1) / (DEAD_DELAY / 2)));
    }
    if (!p->DEAD && p->energy < LOW_ENERGY) p->ANIM += 11;
    if (p->KICK) {
        p->KICK++;
        if (p->KICK > 5) p->KICK = 0;
        p->ANIM = p->curr_weapon == 0 ? 19 : 20;
        if (p->energy < LOW_ENERGY) p->ANIM += 2;
    }
    if (p->ANIM >= PLAYER_ANIMS) p->ANIM = PLAYER_ANIMS - 1;
}

void player_get_damage(u8 pi, s16 damage, s8 from)
{
    Player *p = &player[pi];
    s16 px = PX(p->x), py = PX(p->y);
    u8 a;
    p->HIT = 15;
    p->energy -= damage;
    if (p->energy > 0) return;
    if (p->burning) {
        play_sound(WOOSHWAV, 20000);
        new_effect(EXPLO, (s16)(px + 12), (s16)(py + 12), 0, 0);
        new_effect(EXPLO, (s16)(px + 18), (s16)(py + 12), 0, 0);
        new_effect(EXPLO, (s16)(px + 15), (s16)(py + 18), 0, 0);
    } else {
        for (a = 0; a < 16; a++)
            if (rnd() % 2) new_effect(BLOOD, (s16)(px + 15), (s16)(py + 15), (u8)(a * 16), 4);
        play_sound(DEATHWAV, (s16)(20000 - (rnd() % 3000)));
    }
    p->DEAD = 1;
    if (from >= 0) {
        if (from != (s8)pi) player[from].player_kills++;
        else p->player_kills--;
    }
}

/* ------------------------------------------------------------------ */

void alusta_players1(void)
{
    u8 a, b;
    strcpy(player[active[0]].name, name1);
    if (ACTIVE_PLAYERS == 2) strcpy(player[active[1]].name, name2);
    player[active[0]].color = 16 * 3;
    if (ACTIVE_PLAYERS == 2) player[active[1]].color = 16 * 9;
    for (b = 0; b < ACTIVE_PLAYERS; b++) {
        Player *p = &player[active[b]];
        p->enabled = 1;
        p->cash = 900;
        p->total_kills = 0;
        p->TARGET = 0;
        p->shield = 0;
        p->DEAD = 0;
        p->speed = 2;
        p->max_energy = 100 * TENTHS;
        for (a = 0; a < DIFF_WEAPONS + 1; a++) p->weapons[a] = 0;
        for (a = 0; a < DIFF_BULLETS; a++) p->bullets[a] = 0;
        p->weapons[0] = 1;
        p->bullets[0] = 10;
    }
    if (!strcmp(player[active[0]].name, "iron")) player[active[0]].cash = 50000L;
    if (ACTIVE_PLAYERS == 2 && !strcmp(player[active[1]].name, "maiden")) player[active[1]].cash = 50000L;
}

void alusta_players2(void)
{
    u8 a, b, pi;
    for (b = 0; b < ACTIVE_PLAYERS; b++) {
        Player *p = &player[pi = active[b]];
        p->push = 0;
        p->x = FX(pl_start_x[b] * 20);
        p->y = FX(pl_start_y[b] * 20);
        p->angle = 0;
        p->turn_cnt = 0;
        p->burning = 0;
        p->frying_player = -1;
        p->shooted = 0;
        p->hitten = 0;
        p->player_kills = 0;
        p->curr_weapon = 0;
        p->KICK = 0;
        p->HIT = 0;
        p->WALK = 0;
        p->FIRE = 0;
        p->DEAD = 0;
        p->buttons = p->prev_buttons = 0;
        player_move(pi, 0, 0);
        p->scr_x = (s16)(PX(p->x) - 160);
        p->scr_y = (s16)(PX(p->y) - 100);
        player_move_scr(pi);
        p->load_cnt = 0;
        p->energy = (s16)(p->max_energy + p->shield * 10 * TENTHS);
        for (a = 0; a < DIFF_ENEMIES; a++) p->kills[a] = 0;
    }
}

/* after a split screen level: continue alone if the other player died */
void two_to_one(void)
{
    if (player[active[0]].DEAD || player[active[1]].DEAD) {
        ACTIVE_PLAYERS = 1;
        GAME_MODE = ONE_PLAYER;
        if (player[active[0]].DEAD) {
            player[active[0]].enabled = 0;
            active[0] = active[1];
        } else {
            player[active[1]].enabled = 0;
        }
    }
}
