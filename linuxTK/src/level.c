/* level.c - crates and other per level world logic (GAME.CPP) */
#include "globals.h"

#define BURN_DAMAGE 4   /* 0.4 per tick, in tenths */

void null_crates(void)
{
    s16 a;
    for (a = 0; a < MAX_CRATES; a++) {
        crate[a].weapon = 0;
        crate[a].bullet = 0;
        crate[a].in_use = 0;
    }
}

static u8 check_place(s16 x, s16 y, s16 size)
{
    u8 a;
    if (wall_px(x, y) || wall_px(x, y + size) || wall_px(x + size, y) || wall_px(x + size, y + size)) return 1;
    for (a = 0; a < ACTIVE_PLAYERS; a++)
        if (get_dist(x, y, PX(player[active[a]].x), PX(player[active[a]].y)) < 50) return 1;
    return 0;
}

static void random_crate(s16 crat, u8 type)
{
    s16 x = 0, y = 0, tries = 0;
    while (check_place(x, y, 14) && tries++ < 30000) {
        x = (s16)(rnd() % (level_x_size * 20));
        y = (s16)(rnd() % (level_y_size * 20));
    }
    crate[crat].type = type;
    crate[crat].x = x;
    crate[crat].y = y;
    crate[crat].in_use = 1;
}

void place_normal_game_crates(void)
{
    s16 a, b, crat = 0;
    for (a = 0; a < DIFF_WEAPONS; a++)
        for (b = 0; b < lev_weapon_crates[a] && crat < MAX_CRATES; b++, crat++) {
            crate[crat].weapon = (u8)a;
            random_crate(crat, 0);
        }
    for (a = 0; a < DIFF_BULLETS; a++)
        for (b = 0; b < lev_bullet_crates[a] && crat < MAX_CRATES; b++, crat++) {
            crate[crat].bullet = (u8)a;
            random_crate(crat, 1);
        }
    for (b = 0; b < lev_energy_crates && crat < MAX_CRATES; b++, crat++)
        random_crate(crat, 2);
    /* version 5+: crates placed in the editor */
    for (a = 0; a < crate_info_amount && crat < MAX_CRATES; a++, crat++) {
        Crate *c = &crate[crat];
        s16 t1 = crate_info[a].type1, t2 = crate_info[a].type2;
        if (t1 < 0 || t1 > 2) t1 = 2;
        c->type = (u8)t1;
        c->x = crate_info[a].x;
        c->y = crate_info[a].y;
        c->weapon = (u8)(t1 == 0 && t2 >= 0 && t2 < DIFF_WEAPONS ? t2 : 0);
        c->bullet = (u8)(t1 == 1 && t2 >= 0 && t2 < DIFF_BULLETS ? t2 : 0);
        c->in_use = 1;
    }
}

void chk_burn(void)
{
    s16 a;
    for (a = 0; a < MAX_PLAYERS; a++) {
        Player *p = &player[a];
        if (p->enabled && !p->DEAD && p->burning) {
            p->burning--;
            player_get_damage((u8)a, BURN_DAMAGE, p->frying_player);
            if (tick_count % 6 == 0) new_effect(STEAM, (s16)(PX(p->x) + 14), (s16)(PX(p->y) + 14), 96, 1);
        }
    }
    for (a = 0; a < ENEMIES; a++) {
        Enemy *e = &enemy[a];
        if (!e->DEAD && e->burning) {
            e->burning--;
            enemy_get_damage(a, BURN_DAMAGE, e->frying_player);
            if (tick_count % 6 == 0) new_effect(STEAM, (s16)(PX(e->x) + 14), (s16)(PX(e->y) + 14), 96, 1);
        }
    }
}
