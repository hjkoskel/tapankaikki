/* screens.c - minimal text screens on a black background: menu, level info,
 * statistics, shop, quit dialog, victory. Only the 8x8 font is used.
 * Every wait loop keeps presenting frames so the window stays responsive. */
#include <stdio.h>
#include <string.h>
#include "globals.h"
#include "plat.h"

static void flush_keys(void)
{
    while (plat_key_pressed() != PK_NONE) ;
}

/* present fb until a key is pressed */
int wait_key(void)
{
    int k;
    flush_keys();
    for (;;) {
        plat_present(fb);
        if (plat_quit_requested()) {
            app_quit = 1;
            return PK_ESC;
        }
        k = plat_key_pressed();
        if (k != PK_NONE) return k;
    }
}

static void center(s16 y, const char *s, u8 col)
{
    writefonts((s16)(160 - str_length(s) / 2), y, s, col);
}

static void title(const char *s)
{
    clear_screen();
    plat_set_palette(pal_game);
    center(12, s, C_RED);
}

int ingame_quit(void)
{
    const char *s = "REALLY WANNA QUIT ? (y)";
    s16 a, x;
    set_full_clip();
    x = (s16)(160 - str_length(s) / 2);
    for (a = 0; a < 12; a++) memset(fb + (s32)(a + SCR_H / 2 - 6) * SCR_W + x - 2, 0, (size_t)(SCR_W - x * 2 + 4));
    writefonts(x, SCR_H / 2 - 4, s, C_WHITE);
    return wait_key() == 'y' || app_quit;
}

/* yes/no question on its own screen */
static int ask_quit(void)
{
    title("QUIT TO MENU ?");
    center(90, "y = yes, any other key = no", C_GREY);
    return wait_key() == 'y' || app_quit;
}

/* ------------------------------------------------------------------ */

#define MENU_ITEMS 4

void main_menu(void)
{
    s16 a, selected = 0;
    char items[MENU_ITEMS][40];
    int k;

    for (;;) {
        sprintf(items[0], "single player");
        sprintf(items[1], "split screen");
        sprintf(items[2], "episode: %s", episode_names[episode]);
        sprintf(items[3], "exit");
        title("ULTIMATE TAPAN KAIKKI");
        for (a = 0; a < MENU_ITEMS; a++)
            center((s16)(70 + a * 14), items[a], a == selected ? C_YELLOW : C_GREY);
        center(140, "up/down select, left/right episode, enter start", C_GREY);
        writefonts((s16)(SCR_W - 2 - str_length(TK_VERSION)), 188, TK_VERSION, C_GREY);
        k = wait_key();
        if (app_quit) return;
        if (k == PK_UP) selected = (s16)((selected + MENU_ITEMS - 1) % MENU_ITEMS);
        if (k == PK_DOWN) selected = (s16)((selected + 1) % MENU_ITEMS);
        if (selected == 2 && (k == PK_LEFT || k == PK_RIGHT || k == PK_ENTER)) {
            do {
                episode = (s16)((episode + (k == PK_LEFT ? num_episodes - 1 : 1)) % num_episodes);
            } while (episode_levels[episode] == 0);
        }
        if (k != PK_NONE) play_sound(KLIKWAV, 22500);
        if (k == PK_ESC || (k == PK_ENTER && selected == 3)) return;
        if (k == PK_ENTER && selected < 2) {
            GAME_MODE = selected == 0 ? ONE_PLAYER : SPLIT_SCREEN;
            game();
            if (app_quit) return;
        }
    }
}

/* ------------------------------------------------------------------ */

void level_info(void)
{
    char text[64];
    s16 a, y = 70;
    sprintf(text, "LEVEL %d", LEVEL + 1);
    title(text);
    center(30, level_comment, C_YELLOW);
    center(55, "the killing list:", C_GREY);
    for (a = 0; a < DIFF_ENEMIES; a++)
        if (level_enemies[a] > 0) {
            sprintf(text, "%d %s%s", level_enemies[a], enemy_info[a].name, level_enemies[a] > 1 ? "s" : "");
            center(y, text, C_YELLOW);
            y += 10;
        }
    sprintf(text, "action time %s", c_2_c(TIME_LIMIT(level_time_limit)));
    center(170, text, C_GREY);
    if (wait_key() == PK_ESC && ask_quit()) back_to_menu = 1;
}

static void player_stats(u8 pi, s16 xo, s16 timebonus)
{
    Player *p = &player[pi];
    char text[64];
    s16 a, y = 50;
    writefonts((s16)(16 + xo), 38, p->name, C_YELLOW);
    for (a = 0; a < DIFF_ENEMIES; a++)
        if (p->kills[a] > 0) {
            p->total_kills += p->kills[a];
            p->cash += (s32)enemy_info[a].reward * p->kills[a];
            sprintf(text, "%d %s +%ld", p->kills[a], enemy_info[a].name, (long)enemy_info[a].reward * p->kills[a]);
            writefonts((s16)(16 + xo), y, text, C_GREY);
            y += 9;
        }
    p->cash += timebonus;
    if (p->cash < 0) p->cash = 0;
    sprintf(text, "kills %d", p->total_kills);
    writefonts((s16)(16 + xo), 130, text, C_GREY);
    sprintf(text, "accuracy %ld%%", p->shooted ? (long)p->hitten * 100L / p->shooted : 0L);
    writefonts((s16)(16 + xo), 140, text, C_GREY);
    sprintf(text, "cash %ld", (long)p->cash);
    writefonts((s16)(16 + xo), 150, text, C_YELLOW);
}

void statics(void)
{
    char text[64];
    s16 timebonus = (s16)((TIME_LIMIT(level_time_limit) - complete_time) * 5);
    if (timebonus < -500) timebonus = -500;
    title(next_level && !quit ? "COMPLETED !" : "GAME OVER");
    player_stats(active[0], 0, timebonus);
    if (GAME_MODE == SPLIT_SCREEN) player_stats(active[1], 160, timebonus);
    sprintf(text, "limit %s", c_2_c(TIME_LIMIT(level_time_limit)));
    sprintf(text + strlen(text), "  time %s  bonus %d", c_2_c(complete_time), timebonus);
    center(172, text, C_GREY);
    if (wait_key() == PK_ESC && !quit && ask_quit()) back_to_menu = 1;
}

/* ------------------------------------------------------------------ */
/* shop: one text list of 22 items                                     */

#define SHOP_ITEMS (DIFF_WEAPONS + DIFF_BULLETS + 2)
#define SHOP_LINES 14

static void shop_line(Player *p, s16 i, const char **name, s16 *cost, s16 *have)
{
    if (i < DIFF_WEAPONS) {
        *name = weapon[i + 1].name;
        *cost = weapon[i + 1].cost;
        *have = p->weapons[i + 1];
    } else if (i < DIFF_WEAPONS + DIFF_BULLETS) {
        i = (s16)(i - DIFF_WEAPONS);
        *name = bullet_type[i + 1].name;
        *cost = bullet_type[i + 1].cost;
        *have = (s16)(p->bullets[i] / bullet_type[i + 1].mul);
    } else if (i == DIFF_WEAPONS + DIFF_BULLETS) {
        *name = "Shield";
        *cost = (s16)(shield_cost + p->shield * 15);
        *have = p->shield;
    } else {
        *name = "Target system";
        *cost = target_cost;
        *have = p->TARGET;
    }
}

static void shop_buy(Player *p, s16 i)
{
    s16 b;
    if (i < DIFF_WEAPONS) {
        if (!p->weapons[i + 1] && p->cash >= weapon[i + 1].cost) {
            p->weapons[i + 1] = 1;
            p->cash -= weapon[i + 1].cost;
        }
    } else if (i < DIFF_WEAPONS + DIFF_BULLETS) {
        b = (s16)(i - DIFF_WEAPONS);
        if (p->bullets[b] < bullet_type[b + 1].max * bullet_type[b + 1].mul && p->cash >= bullet_type[b + 1].cost) {
            p->bullets[b] += bullet_type[b + 1].mul;
            p->cash -= bullet_type[b + 1].cost;
        }
    } else if (i == DIFF_WEAPONS + DIFF_BULLETS) {
        if (p->cash >= shield_cost + p->shield * 15 && p->shield < shield_max) {
            p->cash -= shield_cost + p->shield * 15;
            p->shield++;
        }
    } else if (!p->TARGET && p->cash >= target_cost) {
        p->TARGET = 1;
        p->cash -= target_cost;
    }
}

static void shop_sell(Player *p, s16 i)
{
    s16 b;
    if (i < DIFF_WEAPONS) {
        if (p->weapons[i + 1]) {
            p->weapons[i + 1] = 0;
            p->cash += sell_price[i];
        }
    } else if (i < DIFF_WEAPONS + DIFF_BULLETS) {
        b = (s16)(i - DIFF_WEAPONS);
        if (p->bullets[b] > 0) {
            p->bullets[b] -= bullet_type[b + 1].mul;
            if (p->bullets[b] < 0) p->bullets[b] = 0;
            p->cash += bullet_type[b + 1].cost;
        }
    } else if (i == DIFF_WEAPONS + DIFF_BULLETS) {
        if (p->shield > 0) {
            b = (s16)(p->shield - 1);
            p->cash += sell_price[DIFF_WEAPONS] + (15 * b) / 2;
            p->shield--;
        }
    } else if (p->TARGET) {
        p->TARGET = 0;
        p->cash += sell_price[DIFF_WEAPONS + 1];
    }
}

void shop(u8 pi)
{
    Player *p = &player[pi];
    s16 i, sel = 0, top = 0, cost, have, y;
    const char *name;
    char text[64];
    u32 now, prev, pr;

    flush_keys();
    prev = plat_buttons(p->keyset);
    for (;;) {
        sprintf(text, "SHOP - %s", p->name);
        title(text);
        sprintf(text, "cash %ld", (long)p->cash);
        center(24, text, C_YELLOW);
        if (sel < top) top = sel;
        if (sel >= top + SHOP_LINES) top = (s16)(sel - SHOP_LINES + 1);
        for (i = top, y = 40; i < top + SHOP_LINES && i < SHOP_ITEMS; i++, y += 10) {
            shop_line(p, i, &name, &cost, &have);
            writefonts(30, y, name, i == sel ? C_YELLOW : C_GREY);
            sprintf(text, "%d", cost);
            writefonts(190, y, text, i == sel ? C_YELLOW : C_GREY);
            if (have) {
                sprintf(text, "[%d]", have);
                writefonts(250, y, text, i == sel ? C_YELLOW : C_GREY);
            }
            if (i == sel) writefonts(18, y, ">", C_YELLOW);
        }
        center(186, "fire buy, change sell, esc done", C_GREY);
        plat_present(fb);
        if (plat_quit_requested()) {
            app_quit = 1;
            return;
        }
        if (plat_key_pressed() == PK_ESC) return;
        now = plat_buttons(p->keyset);
        pr = now & ~prev;
        prev = now;
        if ((pr & BTN_UP) && sel > 0) sel--;
        if ((pr & BTN_DOWN) && sel < SHOP_ITEMS - 1) sel++;
        if (pr & BTN_FIRE) shop_buy(p, sel);
        if (pr & BTN_CHANGE) shop_sell(p, sel);
        if (pr) play_sound(KLIKWAV, 22000);
    }
}

void victory(void)
{
    title("VICTORY !");
    center(90, "the episode is cleared", C_YELLOW);
    center(110, "press esc", C_GREY);
    while (wait_key() != PK_ESC && !app_quit) ;
}
