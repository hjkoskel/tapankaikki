/* plat_test.c - headless backend with scripted input, for tests.
 *
 * ED_SCRIPT is a comma separated list of "frame:action":
 *   k<key>       key press: a character, or F1..F12 up down left right enter
 *                esc bksp del pgup pgdn home end tab space
 *   t<text>      type text (no commas)
 *   m<x>/<y>/<b> mouse at x,y with buttons b (1 left, 2 right, 4 middle)
 *   s            screenshot to shot_<frame>.ppm (directory ED_SHOTDIR, default .)
 *   q            close the window
 * The editor also stops after ED_END frames (default 10000). */
#define _DEFAULT_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <dirent.h>
#include <sys/stat.h>
#include "plat.h"

static u8 pal_rgb[768];
static long frame_no;
static int queue[256], q_head, q_tail;
static s16 m_x = 160, m_y = 100;
static u8 m_b;
static int quit;
static long end_frame = 10000;

int plat_init(void)
{
    const char *e = getenv("ED_END");
    if (e) end_frame = atol(e);
    return 1;
}

void plat_shutdown(void) {}

void plat_set_palette(const u8 *pal)
{
    int i;
    for (i = 0; i < 768; i++) pal_rgb[i] = (u8)(((pal[i] & 63) << 2) | ((pal[i] & 63) >> 4));
}

static void push(int k)
{
    queue[q_tail] = k;
    q_tail = (q_tail + 1) & 255;
}

static const struct { const char *n; int k; } names[] = {
    { "up", EK_UP }, { "down", EK_DOWN }, { "left", EK_LEFT }, { "right", EK_RIGHT },
    { "enter", EK_ENTER }, { "esc", EK_ESC }, { "bksp", EK_BKSP }, { "del", EK_DEL },
    { "pgup", EK_PGUP }, { "pgdn", EK_PGDN }, { "home", EK_HOME }, { "end", EK_END },
    { "tab", EK_TAB }, { "space", ' ' }
};

static void shot(const u8 *fb)
{
    char name[300];
    const char *dir = getenv("ED_SHOTDIR");
    FILE *f;
    int i;
    sprintf(name, "%.250s/shot_%04ld.ppm", dir ? dir : ".", frame_no);
    f = fopen(name, "wb");
    if (!f) return;
    fprintf(f, "P6\n%d %d\n255\n", SCR_W, SCR_H);
    for (i = 0; i < SCR_W * SCR_H; i++) fwrite(pal_rgb + fb[i] * 3, 1, 3, f);
    fclose(f);
}

static void run_script(const u8 *fb)
{
    const char *s = getenv("ED_SCRIPT");
    char item[200];
    while (s && *s) {
        size_t n = strcspn(s, ",");
        long fr;
        char *a;
        if (n >= sizeof(item)) n = sizeof(item) - 1;
        memcpy(item, s, n);
        item[n] = 0;
        s += n;
        if (*s == ',') s++;
        fr = strtol(item, &a, 10);
        if (fr != frame_no || *a != ':') continue;
        a++;
        if (*a == 'k') {
            unsigned i;
            a++;
            if (strlen(a) == 1) push((unsigned char)*a);
            else if ((a[0] == 'F' || a[0] == 'f') && atoi(a + 1) > 0) push(EK_F1 + atoi(a + 1) - 1);
            else
                for (i = 0; i < sizeof(names) / sizeof(names[0]); i++)
                    if (strcmp(a, names[i].n) == 0) push(names[i].k);
        } else if (*a == 't') {
            for (a++; *a; a++) push((unsigned char)*a);
        } else if (*a == 'm') {
            int x = 0, y = 0, b = 0;
            sscanf(a + 1, "%d/%d/%d", &x, &y, &b);
            m_x = (s16)x; m_y = (s16)y; m_b = (u8)b;
        } else if (*a == 's') {
            shot(fb);
        } else if (*a == 'q') {
            quit = 1;
        }
    }
}

void plat_present(const u8 *fb)
{
    run_script(fb);
    frame_no++;
    if (frame_no >= end_frame) quit = 1;
}

int plat_key(void)
{
    int k;
    if (q_head == q_tail) return EK_NONE;
    k = queue[q_head];
    q_head = (q_head + 1) & 255;
    return k;
}

int plat_key_down(int key)
{
    (void)key;
    return 0;
}

void plat_mouse(s16 *x, s16 *y, u8 *b)
{
    *x = m_x;
    *y = m_y;
    *b = m_b;
}

int plat_quit_requested(void)
{
    return quit;
}

static void walk(const char *dir, const char *rel, void (*fn)(const char *))
{
    char path[1024], sub[1024];
    DIR *d;
    struct dirent *e;
    struct stat st;
    sprintf(path, "%.500s%s%.500s", dir, rel[0] ? "/" : "", rel);
    d = opendir(path);
    if (!d) return;
    while ((e = readdir(d)) != NULL) {
        size_t n = strlen(e->d_name);
        if (e->d_name[0] == '.') continue;
        sprintf(sub, "%.500s%s%.300s", rel, rel[0] ? "/" : "", e->d_name);
        sprintf(path, "%.500s/%.500s", dir, sub);
        if (stat(path, &st) != 0) continue;
        if (S_ISDIR(st.st_mode)) walk(dir, sub, fn);
        else if (n > 4 && strcasecmp(e->d_name + n - 4, ".lev") == 0) fn(sub);
    }
    closedir(d);
}

void plat_list_levels(const char *dir, void (*fn)(const char *rel))
{
    walk(dir, "", fn);
}

int plat_make_dirs(const char *path)
{
    char p[1024], *s;
    strncpy(p, path, sizeof(p) - 1);
    p[sizeof(p) - 1] = 0;
    for (s = p + 1; *s; s++)
        if (*s == '/') {
            *s = 0;
            mkdir(p, 0755);
            *s = '/';
        }
    return 1;
}
