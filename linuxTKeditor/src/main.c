/* main.c - command line, startup and shutdown of the level editor */
#include <stdio.h>
#include <string.h>
#include "ed.h"
#include "plat.h"

static int data_ok(void)
{
    FILE *f = fopen(data_path("EFPS/WALLS1.EFP"), "rb");
    if (f) fclose(f);
    return f != NULL;
}

static void usage(void)
{
    printf("%s - level editor for Ultimate Tapan Kaikki\n", ED_VERSION);
    printf("usage: tked [-d dir] [level]\n");
    printf("  -d dir  game data directory (contains EFPS, LEVS, PALETTE.TAB)\n");
    printf("  level   level to open, relative to LEVS (e.g. DEFAULT/LEVEL1.LEV)\n");
}

int main(int argc, char *argv[])
{
    const char *level = NULL;
    int a;
    for (a = 1; a < argc; a++) {
        if (strcmp(argv[a], "-d") == 0 && a + 1 < argc) {
            strncpy(data_dir, argv[++a], sizeof(data_dir) - 1);
            data_dir[sizeof(data_dir) - 1] = 0;
        } else if (argv[a][0] == '-') {
            usage();
            return 0;
        } else {
            level = argv[a];
        }
    }
    if (!data_ok()) {
        strcpy(data_dir, ".");
        if (!data_ok()) {
            printf("Game data not found. Use -d <dir> to point at the TK directory.\n");
            return 1;
        }
    }
    if (!data_load_all()) {
        printf("Loading game data failed.\n");
        return 1;
    }
    if (!plat_init()) {
        printf("Platform init failed.\n");
        return 1;
    }
    editor_run(level);
    plat_shutdown();
    return 0;
}
