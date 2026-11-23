/* gore_test.c - visual test: loads a level, kills enemies next to the player
 * with different damage types and renders the result (blood, splats, body
 * parts, explosions, footprints) through the headless backend.
 * Build: make gore_test ; run: TK_OUT=dir TK_SHOTS=1,2,3 ./gore_test */
#include <stdio.h>
#include <string.h>
#include "../src/globals.h"
#include "../src/plat.h"

static void tick(void)
{
    move_bullets();
    move_body_parts();
    update_enemies();
    player_animate(0);
    roll_effects();
    tick_count++;
}

int main(void)
{
    s32 a, i;
    strcpy(data_dir, "..");

    rnd_seed(1234);
    if (!assets_load_all() || !plat_init()) return 1;
    if (!level_load("LEVS/DEFAULT/LEVEL1.LEV")) return 1;

    GAME_MODE = ONE_PLAYER;
    ACTIVE_PLAYERS = 1;
    active[0] = 0;
    num_views = 1;
    view[0].origin_x = 0; view[0].clip_x0 = 0; view[0].clip_x1 = SCR_W; view[0].width = SCR_W; view[0].owner = 0;
    alusta_players1();
    alusta_players2();
    alusta_enemies();
    place_enemies();
    null_effects(); null_body_parts(); null_bullets(); null_crates();

    /* line the enemies up right of the player and kill them in different ways */
    for (a = 0; a < ENEMIES; a++) {
        enemy[a].x = (s16)(player[0].x + FX(50 + a * 40));
        enemy[a].y = (s16)(player[0].y + FX(40));
    }
    plat_set_palette(pal_game);
    for (i = 0; i < 3; i++) tick();
    enemy_get_damage(0, 9999, 0);                 /* normal death: blood + body parts */
    if (ENEMIES > 1) { enemy[1].burning = 5; enemy_get_damage(1, 9999, 0); }  /* burned */
    if (ENEMIES > 2) explo((s16)(PX(enemy[2].x) + 14), (s16)(PX(enemy[2].y) + 14), 2, 0); /* grenade */
    for (i = 0; i < 2; i++) { tick(); }
    player_move_scr(0);
    render_game(); plat_present(fb);            /* frame 1: right after */
    for (i = 0; i < 12; i++) tick();
    render_game(); plat_present(fb);            /* frame 2: parts flying, splats */
    /* walk the player over the gore to leave footprints and kick parts */
    for (i = 0; i < 60; i++) {
        player[0].buttons = BTN_UP; player[0].angle = 32;
        player_chk_keys(0);
        tick();
    }
    player_move_scr(0);
    render_game(); plat_present(fb);            /* frame 3 */
    for (i = 0; i < 200; i++) tick();
    render_game(); plat_present(fb);            /* frame 4: long-lived decals */
    printf("enemies %ld dead %ld\n", (long)ENEMIES, (long)DEAD_ENEMIES);
    plat_shutdown();
    return 0;
}
