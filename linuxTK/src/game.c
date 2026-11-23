/* game.c - episode/level flow and the fixed timestep game loop (GAME.CPP game()) */
#include <stdio.h>
#include <string.h>
#include "globals.h"
#include "plat.h"

static void do_prices(void)
{
    s16 a;
    for (a = 0; a < DIFF_WEAPONS; a++)
        sell_price[a] = (s16)(weapon[a + 1].cost * 4 / 5 - rnd() % (weapon[a + 1].cost / 2));
    sell_price[DIFF_WEAPONS] = (s16)(shield_cost * 4 / 5 - rnd() % (shield_cost / 2));
    sell_price[DIFF_WEAPONS + 1] = (s16)(target_cost * 4 / 5 - rnd() % (target_cost / 2));
}

static void are_all_dead(void)
{
    u8 a;
    all_dead = 1;
    for (a = 0; a < ACTIVE_PLAYERS; a++)
        if (player[active[a]].enabled && player[active[a]].DEAD <= 14) all_dead = 0;
    if (all_dead) complete_time = (s16)(TIME_LIMIT(level_time_limit) + 100);
}

static void setup_views(void)
{
    u8 a;
    num_views = ACTIVE_PLAYERS;
    for (a = 0; a < num_views; a++) view[a].owner = active[a];
    if (num_views == 1) {
        view[0].origin_x = 0;
        view[0].clip_x0 = 0;
        view[0].clip_x1 = SCR_W;
        view[0].width = SCR_W;
    } else {
        view[0].origin_x = 0;
        view[0].clip_x0 = 0;
        view[0].clip_x1 = 160;
        view[0].width = 160;
        view[1].origin_x = 160;
        view[1].clip_x0 = 161;
        view[1].clip_x1 = SCR_W;
        view[1].width = 160;
    }
}

/* one 40 Hz tick; order follows the original game_shit() */
static void game_update(void)
{
    u8 a;
    int k;

    /* system keys */
    while ((k = plat_key_pressed()) != PK_NONE) {
        if (k == PK_SPACE) MAP_ON = (u8)!MAP_ON;
        if (k == PK_ESC) {
            render_game();
            if (ingame_quit()) quit = 1;
            return;
        }
    }
    for (a = 0; a < ACTIVE_PLAYERS; a++) {
        Player *p = &player[active[a]];
        p->prev_buttons = p->buttons;
        p->buttons = plat_buttons(p->keyset);
        if (!p->DEAD) player_chk_keys(active[a]);
    }
    for (a = 0; a < ACTIVE_PLAYERS; a++) {      /* push_players */
        Player *p = &player[active[a]];
        if (p->push > 0) {
            if (p->push >= TENTHS) player_move(active[a], p->push_angle, (s16)(p->push / TENTHS * FX_ONE));
            p->push -= 2;   /* 0.2 px per tick */
        }
    }
    chk_burn();
    move_bullets();
    move_body_parts();
    for (a = 0; a < ACTIVE_PLAYERS; a++) player_move_scr(active[a]);
    update_enemies();
    for (a = 0; a < ACTIVE_PLAYERS; a++) player_load(active[a]);
    for (a = 0; a < ACTIVE_PLAYERS; a++) player_animate(active[a]);
    for (a = 0; a < ACTIVE_PLAYERS; a++)
        if (player[active[a]].HIT > 0) player[active[a]].HIT--;
    roll_effects();

    if (message_time > 0) message_time--;
    else messages = 0;
    if (next_level) next_level++;
    else complete_time = (s16)(level_ticks / TARGET_FRAMES);
    if (level_ticks < 65535u) level_ticks++;
    tick_count++;
    plot_count++;
    are_all_dead();
}

static void play_level(void)
{
    u32 last, now, acc = 0;

    plat_set_palette(pal_game);
    last = plat_ticks_ms();
    while (!all_dead && !quit && next_level < C_DELAY) {
        now = plat_ticks_ms();
        acc += now - last;
        last = now;
        if (acc > 250) acc = 250;       /* don't try to catch up after a stall */
        while (acc >= TICK_MS && !quit) {
            game_update();
            acc -= TICK_MS;
        }
        render_game();
        plat_present(fb);
        if (plat_quit_requested()) {
            app_quit = 1;
            quit = 1;
        }
    }
}

void game(void)
{
    char filename[64];
    u8 a;

    for (a = 0; a < MAX_PLAYERS; a++) {
        memset(&player[a], 0, sizeof(Player));
        player[a].frying_player = -1;
    }
    if (GAME_MODE == SPLIT_SCREEN) {
        ACTIVE_PLAYERS = 2;
        active[0] = 0;
        active[1] = 1;
        player[0].keyset = 1;   /* left view: WASD keys */
        player[1].keyset = 0;   /* right view: arrow keys */
    } else {
        ACTIVE_PLAYERS = 1;
        active[0] = 0;
        player[0].keyset = 0;
    }
    LEVELS = episode_levels[episode];
    LEVEL = START_LEVEL;
    if (LEVEL > LEVELS - 1) LEVEL = (s16)(LEVELS - 1);
    alusta_players1();
    quit = 0;
    all_dead = 0;
    back_to_menu = 0;
    next_level = 0;

    while (!quit && !back_to_menu && !app_quit) {
        sprintf(filename, "LEVS/%s/LEVEL%d.LEV", episode_names[episode], LEVEL + 1);
        if (!level_load(filename)) break;
        level_info();
        if (!back_to_menu && !app_quit) {
            do_prices();
            if (LEVEL > 0)
                for (a = 0; a < ACTIVE_PLAYERS && !app_quit; a++) shop(active[a]);
            setup_views();
            alusta_players2();
            DEAD_ENEMIES = 0;
            alusta_enemies();
            place_enemies();
            null_body_parts();
            null_effects();
            null_bullets();
            null_crates();
            place_normal_game_crates();
            next_level = 0;
            level_ticks = 0;
            complete_time = 0;
            effect_age_count = 0;
            messages = 0;
            message_time = 0;
            MAP_ON = 0;
            play_level();
        }
        if (app_quit) return;

        all_dead = 0;
        are_all_dead();
        if (all_dead) quit = 1;
        else next_level = 1;
        if (!(LEVEL == 0 && back_to_menu)) statics();
        if (!all_dead && GAME_MODE == SPLIT_SCREEN) two_to_one();
        if (LEVEL == LEVELS - 1 && !back_to_menu && next_level == 1 && quit == 0) {
            quit = 1;
            next_level = 0;
            victory();
        }
        if (next_level) LEVEL++;
        next_level = 0;
    }
}
