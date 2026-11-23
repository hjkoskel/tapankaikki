/* main.c - command line, startup and shutdown */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include "globals.h"
#include "plat.h"

static const char *const wav_files[DIFF_WAVS + 1] = {
    "", "WAVS/MAGNUM.WAV", "WAVS/RIFLE.WAV", "WAVS/PISTOL.WAV", "WAVS/SINKO.WAV", "WAVS/HIT.WAV",
    "WAVS/EXPLO.WAV", "WAVS/KLIK.WAV", "WAVS/DEATH.WAV", "WAVS/WOOSH.WAV", "WAVS/FLAME.WAV"
};

static int data_ok(void)
{
    FILE *f = fopen(data_path("EFPS/TK.EFP"), "rb");
    if (f) fclose(f);
    return f != NULL;
}

static void usage(void)
{
    printf("Ultimate Tapan Kaikki - %s\n", TK_VERSION);
    printf("parameters:\n");
    printf("  -h      this help\n");
    printf("  -d dir  game data directory (contains EFPS, LEVS, ...)\n");
    printf("  -l ##   start at level ##\n");
    printf("  -e ##   start in episode ## (1 = DEFAULT)\n");
    printf("  -m      show enemies on map\n");
}

int main(int argc, char *argv[])
{
    int a;
    s32 e;

    for (a = 1; a < argc; a++) {
        if (argv[a][0] != '-' || !argv[a][1]) continue;
        switch (argv[a][1]) {
        case 'd': case 'D':
            if (a + 1 < argc) {
                strncpy(data_dir, argv[++a], sizeof(data_dir) - 1);
                data_dir[sizeof(data_dir) - 1] = 0;
            }
            break;
        case 'l': case 'L':
            if (a + 1 < argc && atoi(argv[a + 1]) > 0) START_LEVEL = atoi(argv[++a]) - 1;
            break;
        case 'e': case 'E':
            if (a + 1 < argc && atoi(argv[a + 1]) > 0) episode = atoi(argv[++a]) - 1;
            break;
        case 'm': case 'M':
            SHOW_ENEMIES = 1;
            break;
        default:
            usage();
            return 0;
        }
    }
    if (!data_ok()) {
        strcpy(data_dir, ".");
        if (!data_ok()) {
            printf("Game data not found. Use -d <dir> to point at the TK directory.\n");
            return 1;
        }
    }

    rnd_seed((u16)time(NULL));
    if (!assets_load_all()) {
        printf("Loading game data failed.\n");
        return 1;
    }
    count_episodes();
    if (episode < 0 || episode >= num_episodes || episode_levels[episode] == 0) episode = 0;
    for (e = 0; e < num_episodes; e++)
        if (episode_levels[e]) printf("episode %-10s %ld levels\n", episode_names[e], (long)episode_levels[e]);

    if (!plat_init()) {
        printf("Platform init failed.\n");
        return 1;
    }
    for (a = 1; a <= DIFF_WAVS; a++) plat_load_sound(a, data_path(wav_files[a]));
    main_menu();
    plat_shutdown();
    return 0;
}
