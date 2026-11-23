/* roundtrip.c - loads every level under <data>/LEVS with the editor's
 * level code, saves it to a temporary file and compares the bytes.
 * Version 5 files must come back identical; older versions are converted
 * to version 5, so for them the blocks, objects and settings are compared
 * after loading the saved file again. */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "ed.h"
#include "plat.h"

static int bad, total, same;
static char tmp[] = "/tmp/tked_roundtrip.lev";

static long file_bytes(const char *p, unsigned char **buf)
{
    FILE *f = fopen(p, "rb");
    long n;
    if (!f) return -1;
    fseek(f, 0, SEEK_END);
    n = ftell(f);
    fseek(f, 0, SEEK_SET);
    *buf = malloc((size_t)n + 1);
    if (fread(*buf, 1, (size_t)n, f) != (size_t)n) n = -1;
    fclose(f);
    return n;
}

static Level first;

static void check(const char *rel)
{
    char path[600];
    unsigned char *a, *b;
    long na, nb;
    sprintf(path, "LEVS/%.500s", rel);
    strcpy(path, data_path(path));
    total++;
    if (!level_load(path)) {
        printf("LOAD FAILED %s: %s\n", rel, level_error());
        bad++;
        return;
    }
    memcpy(&first, &lev, sizeof(Level));
    if (!level_save(tmp)) {
        printf("SAVE FAILED %s\n", rel);
        bad++;
        return;
    }
    if (first.version == LEVEL_VERSION) {
        na = file_bytes(path, &a);
        nb = file_bytes(tmp, &b);
        if (na != nb || memcmp(a, b, (size_t)na) != 0) {
            printf("DIFFERENT %s (%ld vs %ld bytes)\n", rel, na, nb);
            bad++;
        } else same++;
        free(a);
        free(b);
    } else {
        level_load(tmp);
        first.version = LEVEL_VERSION;
        if (memcmp(&first, &lev, sizeof(Level)) != 0) {
            printf("CONVERSION CHANGED %s (version %ld)\n", rel, (long)first.version);
            bad++;
        }
    }
}

int main(int argc, char *argv[])
{
    char levs[600];
    if (argc > 1) strcpy(data_dir, argv[1]);
    strcpy(levs, data_path("LEVS"));     /* data_path() reuses its buffer */
    plat_list_levels(levs, check);
    remove(tmp);
    printf("%d levels, %d byte identical (version 5), %d failed\n", total, same, bad);
    return bad != 0;
}
