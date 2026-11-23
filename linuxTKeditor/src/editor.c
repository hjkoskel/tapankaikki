/* editor.c - the level editor: map view, tools, dialogs and screens.
 *
 * Screen: the map view is 320x180 at the top, the status bar (20 lines)
 * below it shows the blocks of the left and right mouse buttons, the
 * current tool and the position. Every dialog runs its own loop and draws
 * the editor under itself, so the level stays visible. */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include "ed.h"
#include "plat.h"

#define VIEW_H 180
#define BAR_Y VIEW_H
#define UNDO_STEPS 16
#define MSG_FRAMES 150
#define PICK_DIST 12

enum { MODE_BLOCKS, MODE_SHADOWS };
enum {
    T_PAINT, T_START1, T_START2, T_FILL1, T_FILL2,
    T_SPOT, T_SPOT_DEL, T_STEAM, T_STEAM_DEL, T_CRATE_N, T_CRATE_D, T_CRATE_DEL
};

static const char *const tool_text[] = {
    "", "place player 1 start point", "place player 2 start point",
    "place fill point 1", "place fill point 2",
    "place spot light", "delete spot light", "place steam", "delete steam",
    "place normal game crate", "place deathmatch crate", "delete crate"
};

typedef struct { s32 type, num; } Pick;

static int quit_now;                   /* window closed */
static s16 mouse_x, mouse_y;
static u8 mouse_b, mouse_prev;
static s16 view_x, view_y;
static int mode = MODE_BLOCKS, tool = T_PAINT, grid;
static Pick pick[2] = { { 1, 0 }, { 0, 0 } };  /* left = wall 0, right = floor 0 */
static int sheet_walls = 1;            /* sheet shown in the selection screen */
static s32 pick_shadow = 1;            /* shadow of the left button */
static s32 fill_x, fill_y;
static char file_name[256] = "";       /* relative to LEVS/ */
static int modified;
static char msg[80];
static int msg_time;
static int stroke;                     /* painting with the button held */

static Level undo_buf[UNDO_STEPS];
static int undo_top, undo_count;

/* ------------------------------------------------------------------ */
/* helpers                                                             */

static void message(const char *s)
{
    strncpy(msg, s, sizeof(msg) - 1);
    msg[sizeof(msg) - 1] = 0;
    msg_time = MSG_FRAMES;
}

static void push_undo(void)
{
    memcpy(&undo_buf[undo_top], &lev, sizeof(Level));
    undo_top = (undo_top + 1) % UNDO_STEPS;
    if (undo_count < UNDO_STEPS) undo_count++;
    modified = 1;
}

static void undo(void)
{
    if (!undo_count) {
        message("nothing to undo");
        return;
    }
    undo_top = (undo_top + UNDO_STEPS - 1) % UNDO_STEPS;
    undo_count--;
    memcpy(&lev, &undo_buf[undo_top], sizeof(Level));
    modified = 1;
    message("undone");
}

static void clamp_view(void)
{
    s16 max_x = (s16)(lev.w * TILE - SCR_W + TILE), max_y = (s16)(lev.h * TILE - VIEW_H + TILE);
    if (max_x < -TILE) max_x = -TILE;
    if (max_y < -TILE) max_y = -TILE;
    if (view_x > max_x) view_x = max_x;
    if (view_y > max_y) view_y = max_y;
    if (view_x < -TILE) view_x = -TILE;
    if (view_y < -TILE) view_y = -TILE;
}

static void centre_view(s32 px, s32 py)
{
    view_x = (s16)(px - SCR_W / 2);
    view_y = (s16)(py - VIEW_H / 2);
    clamp_view();
}

static int in_view(void)
{
    return mouse_x >= 0 && mouse_x < SCR_W && mouse_y >= 0 && mouse_y < VIEW_H;
}

/* mouse position on the map in pixels and blocks */
static s32 map_px(void) { return (s32)view_x + mouse_x; }
static s32 map_py(void) { return (s32)view_y + mouse_y; }
static s32 floor_div(s32 a, s32 b) { return a >= 0 ? a / b : -((-a + b - 1) / b); }
static s32 map_bx(void) { return floor_div(map_px(), TILE); }
static s32 map_by(void) { return floor_div(map_py(), TILE); }
static int on_level(s32 bx, s32 by) { return bx >= 0 && by >= 0 && bx < lev.w && by < lev.h; }

static const u8 *tile_of(s32 type, s32 num)
{
    if (num < 0 || num >= NUM_TILES) num = 0;
    return type ? wall_pix[num] : floor_pix[num];
}

static const char *crate_text(const Crate_info *c)
{
    static char buf[48];
    if (c->type1 == 0) sprintf(buf, "weapon: %s", c->type2 >= 0 && c->type2 < DIFF_WEAPONS ? weapon_name[c->type2] : "?");
    else if (c->type1 == 1) sprintf(buf, "bullets: %s", c->type2 >= 0 && c->type2 < DIFF_BULLETS ? bullet_name[c->type2] : "?");
    else sprintf(buf, "energy");
    return buf;
}

/* ------------------------------------------------------------------ */
/* drawing                                                             */

static void draw_pointer(void)
{
    clip_full();
    blit(mouse_x, mouse_y, 9, 13, pointer_pix, 1);
}

/* present the frame; returns 0 when the window was closed */
static int frame(void)
{
    draw_pointer();
    plat_present(fb);
    mouse_prev = mouse_b;
    plat_mouse(&mouse_x, &mouse_y, &mouse_b);
    if (plat_quit_requested()) quit_now = 1;
    return !quit_now;
}

#define CLICKED(b) ((mouse_b & (b)) && !(mouse_prev & (b)))

static void draw_crate(const Crate_info *c, int deathmatch)
{
    s16 x = (s16)(c->x - view_x), y = (s16)(c->y - view_y);
    s32 t = c->type1 < 0 || c->type1 > 2 ? 2 : c->type1;
    blit(x, y, CRATE_SIZE, CRATE_SIZE, crate_pix[t], 1);
    rect((s16)(x - 1), (s16)(y - 1), CRATE_SIZE + 2, CRATE_SIZE + 2, deathmatch ? col_green : col_blue);
}

static void draw_spot(const Spot *s, u8 c)
{
    s16 x = (s16)(s->x - view_x), y = (s16)(s->y - view_y);
    circle(x, y, (s16)(light_size[s->size] / 2), c);
    line((s16)(x - 2), y, (s16)(x + 2), y, c);
    line(x, (s16)(y - 2), x, (s16)(y + 2), c);
}

/* steam: angle in degrees, 0 = down, 90 = right; the line shows the speed */
static void draw_steam(const Steam *s, u8 c)
{
    static const s16 sin16[17] = { 0, 98, 181, 237, 256, 237, 181, 98, 0, -98, -181, -237, -256, -237, -181, -98, 0 };
    s16 x = (s16)(s->x - view_x), y = (s16)(s->y - view_y);
    s32 a = ((s->angle % 360) + 360) % 360;
    s16 len = (s16)(6 + s->speed * 3);
    /* sine by linear interpolation of 22.5 degree steps (enough for a marker) */
    s32 i = a * 16 / 360, f = a * 16 % 360;
    s32 sn = sin16[i] + (sin16[i + 1] - sin16[i]) * f / 360;
    s32 j = (i + 4) % 16;
    s32 cs = sin16[j] + (sin16[j + 1] - sin16[j]) * f / 360;
    s16 ex = (s16)(x + sn * len / 256), ey = (s16)(y + cs * len / 256);
    circle(x, y, 3, c);
    line(x, y, ex, ey, c);
    put_pixel(ex, ey, col_white);
}

static void draw_start(int p)
{
    char t[2];
    s16 x = (s16)(lev.start_x[p] * TILE - view_x), y = (s16)(lev.start_y[p] * TILE - view_y);
    u8 c = p ? col_yellow : col_red;
    rect(x, y, TILE, TILE, c);
    rect((s16)(x + 1), (s16)(y + 1), TILE - 2, TILE - 2, c);
    t[0] = (char)('1' + p);
    t[1] = 0;
    text((s16)(x + 7), (s16)(y + 6), t, p ? C_YELLOW : C_RED);
}

static void draw_map(void)
{
    s32 bx0 = floor_div(view_x, TILE), by0 = floor_div(view_y, TILE), bx, by;
    s16 x, y;
    s32 i, k;

    clip_set(0, 0, SCR_W, VIEW_H);
    fill_rect(0, 0, SCR_W, VIEW_H, col_dgrey);
    for (by = by0; by <= by0 + VIEW_H / TILE + 1; by++)
        for (bx = bx0; bx <= bx0 + SCR_W / TILE + 1; bx++) {
            const Block *b;
            if (!on_level(bx, by)) continue;
            b = &BLK(bx, by);
            x = (s16)(bx * TILE - view_x);
            y = (s16)(by * TILE - view_y);
            blit(x, y, TILE, TILE, tile_of(b->type, b->num), 0);
            if (b->shadow > 0 && b->shadow <= SHADOW_TYPES) blit_shadow(x, y, shadow_pix[b->shadow - 1]);
            if (grid) {
                fill_rect(x, y, TILE, 1, col_black);
                fill_rect(x, y, 1, TILE, col_black);
            }
        }
    rect((s16)(-view_x - 1), (s16)(-view_y - 1), (s16)(lev.w * TILE + 2), (s16)(lev.h * TILE + 2), col_grey);
    for (k = 0; k < 2; k++)
        for (i = 0; i < lev.crates[k]; i++) draw_crate(&lev.crate[k][i], (int)k);
    for (i = 0; i < lev.spots; i++) draw_spot(&lev.spot[i], col_yellow);
    for (i = 0; i < lev.steams; i++) draw_steam(&lev.steam[i], col_cyan);
    draw_start(0);
    draw_start(1);

    /* tool feedback at the mouse */
    if (in_view()) {
        s16 mx = (s16)(map_bx() * TILE - view_x), my = (s16)(map_by() * TILE - view_y);
        if (tool == T_FILL2) {
            s32 x0 = fill_x < map_bx() ? fill_x : map_bx(), x1 = fill_x < map_bx() ? map_bx() : fill_x;
            s32 y0 = fill_y < map_by() ? fill_y : map_by(), y1 = fill_y < map_by() ? map_by() : fill_y;
            rect((s16)(x0 * TILE - view_x), (s16)(y0 * TILE - view_y), (s16)((x1 - x0 + 1) * TILE), (s16)((y1 - y0 + 1) * TILE), col_white);
        } else if (tool == T_PAINT || tool == T_FILL1 || tool == T_START1 || tool == T_START2) {
            rect(mx, my, TILE, TILE, col_white);
        }
    }
    clip_full();
}

static void draw_bar(void)
{
    char line1[96], line2[96];
    const char *name = file_name[0] ? file_name : "(new level)";

    clip_full();
    fill_rect(0, BAR_Y, SCR_W, SCR_H - BAR_Y, col_black);
    if (mode == MODE_BLOCKS) {
        blit(0, BAR_Y, TILE, TILE, tile_of(pick[0].type, pick[0].num), 0);
        blit(22, BAR_Y, TILE, TILE, tile_of(pick[1].type, pick[1].num), 0);
    } else {
        blit(0, BAR_Y, TILE, TILE, floor_pix[0], 0);
        blit_shadow(0, BAR_Y, shadow_pix[pick_shadow - 1]);
        blit(22, BAR_Y, TILE, TILE, floor_pix[0], 0);
    }
    text(1, BAR_Y + 1, "L", C_WHITE);
    text(23, BAR_Y + 1, "R", C_WHITE);

    if (msg_time > 0) {
        sprintf(line1, "%s", msg);
    } else if (tool != T_PAINT) {
        sprintf(line1, "%s  (right button: done)", tool_text[tool]);
    } else if (mode == MODE_BLOCKS) {
        sprintf(line1, "edit blocks  L: %s %ld  R: %s %ld", pick[0].type ? "wall" : "floor", (long)pick[0].num,
                pick[1].type ? "wall" : "floor", (long)pick[1].num);
    } else {
        sprintf(line1, "edit shadows  L: shadow %ld  R: no shadow", (long)pick_shadow);
    }
    text(46, BAR_Y + 1, line1, msg_time > 0 ? C_YELLOW : C_WHITE);

    if (in_view() && on_level(map_bx(), map_by()))
        sprintf(line2, "%ld,%ld  %ldx%ld  %.40s%s", (long)map_bx(), (long)map_by(), (long)lev.w, (long)lev.h, name, modified ? "*" : "");
    else
        sprintf(line2, "%ldx%ld  %.40s%s", (long)lev.w, (long)lev.h, name, modified ? "*" : "");
    text(46, BAR_Y + 11, line2, C_GREY);
    text((s16)(SCR_W - text_width("F1 help") - 2), BAR_Y + 11, "F1 help", C_GREY);
}

static void draw_editor(void)
{
    draw_map();
    draw_bar();
}

/* background of the full screen dialogs */
static void screen_bg(void)
{
    clip_full();
    fill_rect(0, 0, SCR_W, SCR_H, col_black);
}

/* a panel with lines of text at the top of the map view */
static void panel(const char *l1, const char *l2, const char *l3)
{
    s16 n = (s16)((l1 ? 1 : 0) + (l2 ? 1 : 0) + (l3 ? 1 : 0)), y = 6;
    box(4, 2, SCR_W - 8, (s16)(n * 10 + 6));
    if (l1) { text_centre(y, l1, C_YELLOW); y += 10; }
    if (l2) { text_centre(y, l2, C_WHITE); y += 10; }
    if (l3) text_centre(y, l3, C_WHITE);
}

/* ------------------------------------------------------------------ */
/* dialogs                                                             */

static int confirm(const char *q, const char *note)
{
    int k;
    while (frame()) {
        draw_editor();
        panel(q, note, "press Y to confirm");
        while ((k = plat_key()) != EK_NONE) return k == 'y' || k == 'Y';
        if (CLICKED(MB_LEFT) || CLICKED(MB_RIGHT)) return 0;
    }
    return 0;
}

/* line input, returns 0 on ESC; allowed = NULL for any printable char */
static int input_text(const char *prompt, char *buf, int maxlen, const char *allowed)
{
    char shown[300];
    int k, len, blink = 0;
    while (frame()) {
        len = (int)strlen(buf);
        while ((k = plat_key()) != EK_NONE) {
            if (k == EK_ENTER) return 1;
            if (k == EK_ESC) return 0;
            if (k == EK_BKSP && len > 0) buf[--len] = 0;
            if (k >= 32 && k < 127 && len < maxlen && (!allowed || strchr(allowed, k))) {
                buf[len++] = (char)k;
                buf[len] = 0;
            }
        }
        draw_editor();
        sprintf(shown, "%.250s%s", buf, (blink++ / 20) & 1 ? " " : "_");
        panel(prompt, shown, "ENTER accept, ESC cancel");
    }
    return 0;
}

/* number with UP/DOWN (1) and LEFT/RIGHT (10) */
static int input_number(const char *prompt, s32 *v, s32 lo, s32 hi)
{
    char shown[40];
    int k;
    while (frame()) {
        while ((k = plat_key()) != EK_NONE) {
            if (k == EK_ENTER) return 1;
            if (k == EK_ESC) return 0;
            if (k == EK_UP) (*v)++;
            if (k == EK_DOWN) (*v)--;
            if (k == EK_RIGHT) *v += 10;
            if (k == EK_LEFT) *v -= 10;
            if (*v < lo) *v = lo;
            if (*v > hi) *v = hi;
        }
        draw_editor();
        sprintf(shown, "%s %ld", prompt, (long)*v);
        panel(shown, "UP/DOWN +-1, LEFT/RIGHT +-10", "ENTER accept, ESC cancel");
    }
    return 0;
}

/* ------------------------------------------------------------------ */
/* F1 help                                                             */

static const char *const help_lines[] = {
    "ESC        quit (or cancel a tool)",
    "F1         this help",
    "F2 / F3    save level / load level",
    "F4         make a new level",
    "F5         level map",
    "F6         make shadows automatically",
    "F7         general level variables",
    "F8 / F9    random crates: normal game / deathmatch",
    "Q / W      place / delete spot light",
    "A / S      place / delete steam",
    "Z / X      place normal game (blue) / deathmatch (green) crate",
    "C          delete crate",
    "1 / 2      place player 1 / player 2 start",
    "F          fill a rectangle",
    "ENTER      edit shadows / edit blocks",
    "SPACE      choose blocks (or shadow) for the buttons",
    "PGUP/PGDN  walls / floor in the block selection",
    "U          undo        G   grid on/off",
    "arrows, middle button drag: scroll",
};

static void help_screen(void)
{
    s16 i, n = (s16)(sizeof(help_lines) / sizeof(help_lines[0]));
    while (frame()) {
        if (plat_key() != EK_NONE || CLICKED(MB_LEFT) || CLICKED(MB_RIGHT)) return;
        screen_bg();
        box(2, 2, SCR_W - 4, SCR_H - 4);
        text_centre(6, ED_VERSION " - level editor for Ultimate Tapan Kaikki", C_YELLOW);
        for (i = 0; i < n; i++) text(10, (s16)(18 + i * 9), help_lines[i], C_WHITE);
    }
}

/* ------------------------------------------------------------------ */
/* F3 load: a list of all .LEV files under LEVS                        */

#define MAX_FILES 1000
static char files[MAX_FILES][64];
static int nfiles;

static void add_file(const char *rel)
{
    if (nfiles < MAX_FILES && strlen(rel) < 64) strcpy(files[nfiles++], rel);
}

static int cmp_file(const void *a, const void *b)
{
    return strcmp((const char *)a, (const char *)b);
}

static int pick_file(char *out)
{
    static int sel;
    int k, i, top, rows = 17;
    nfiles = 0;
    plat_list_levels(data_path("LEVS"), add_file);
    if (!nfiles) {
        message("no levels found in LEVS");
        return 0;
    }
    qsort(files, (size_t)nfiles, sizeof(files[0]), cmp_file);
    for (i = 0; i < nfiles; i++)
        if (strcmp(files[i], file_name) == 0) sel = i;
    if (sel >= nfiles) sel = 0;
    while (frame()) {
        while ((k = plat_key()) != EK_NONE) {
            if (k == EK_ESC) return 0;
            if (k == EK_ENTER) {
                strcpy(out, files[sel]);
                return 1;
            }
            if (k == EK_UP) sel--;
            if (k == EK_DOWN) sel++;
            if (k == EK_PGUP) sel -= rows;
            if (k == EK_PGDN) sel += rows;
            if (k == EK_HOME) sel = 0;
            if (k == EK_END) sel = nfiles - 1;
        }
        if (sel < 0) sel = 0;
        if (sel >= nfiles) sel = nfiles - 1;
        top = sel - rows / 2;
        if (top > nfiles - rows) top = nfiles - rows;
        if (top < 0) top = 0;
        screen_bg();
        box(30, 2, SCR_W - 60, SCR_H - 4);
        text_centre(6, "LOAD LEVEL", C_YELLOW);
        for (i = 0; i < rows && top + i < nfiles; i++) {
            s16 y = (s16)(18 + i * 9);
            if (mouse_x >= 30 && mouse_x < SCR_W - 30 && mouse_y >= y && mouse_y < y + 9) {
                if (CLICKED(MB_LEFT)) {
                    if (sel == top + i) {
                        strcpy(out, files[sel]);
                        return 1;
                    }
                    sel = top + i;
                }
            }
            if (top + i == sel) fill_rect(32, (s16)(y - 1), SCR_W - 64, 9, col_blue);
            text(36, y, files[top + i], top + i == sel ? C_YELLOW : C_WHITE);
        }
        text_centre(SCR_H - 13, "ENTER or double click load, ESC cancel", C_GREY);
    }
    return 0;
}

static void load_level(void)
{
    char name[64], path[300];
    if (modified && !confirm("load level ?", "the changes are not saved")) return;
    if (!pick_file(name)) return;
    sprintf(path, "LEVS/%s", name);
    if (level_load(data_path(path))) {
        strcpy(file_name, name);
        modified = 0;
        undo_count = 0;
        view_x = view_y = -TILE;
        centre_view(lev.start_x[0] * TILE, lev.start_y[0] * TILE);
        sprintf(path, "loaded %.60s (version %ld)", name, (long)lev.version);
        message(path);
    } else {
        sprintf(path, "load failed: %.60s", level_error());
        message(path);
    }
}

/* ------------------------------------------------------------------ */
/* F2 save                                                             */

static void save_level(void)
{
    char name[64], path[300], *dot;
    FILE *f;
    strcpy(name, file_name[0] ? file_name : "NEW.LEV");
    if (!input_text("save level, file name (under LEVS/):", name, 60,
                    "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789_-./"))
        return;
    if (!name[0] || strstr(name, "..") || name[0] == '/') {
        message("bad file name");
        return;
    }
    dot = strrchr(name, '.');
    if (!dot || strchr(dot, '/')) strcat(name, ".LEV");
    sprintf(path, "LEVS/%s", name);
    if (strcmp(name, file_name) != 0 && (f = fopen(data_path(path), "rb")) != NULL) {
        fclose(f);
        if (!confirm("the file exists, overwrite ?", name)) return;
    }
    if (level_save(data_path(path))) {
        strcpy(file_name, name);
        modified = 0;
        sprintf(path, "saved LEVS/%.60s", name);
    } else {
        sprintf(path, "saving failed: %.60s", level_error());
    }
    message(path);
}

/* ------------------------------------------------------------------ */
/* F4 new level                                                        */

static void new_level(void)
{
    s32 w = lev.w, h = lev.h;
    if (!confirm("create new level ?", modified ? "the changes are not saved" : NULL)) return;
    if (!input_number("x-size:", &w, MIN_LEVEL_W, MAX_LEVEL_W)) return;
    if (!input_number("y-size:", &h, MIN_LEVEL_H, MAX_LEVEL_H)) return;
    level_new((s16)w, (s16)h, pick[1].type, pick[1].num);
    if (lev.w < 3) lev.start_x[1] = 1;
    file_name[0] = 0;
    modified = 0;
    undo_count = 0;
    view_x = view_y = -TILE;
    clamp_view();
    message("new level, filled with the right button block");
}

/* ------------------------------------------------------------------ */
/* F5 level map                                                        */

static void level_map(void)
{
    s16 s = 1, ox, oy, x, y;
    s32 bx, by;
    while (s < 4 && (s + 1) * lev.w <= SCR_W && (s + 1) * lev.h <= SCR_H) s++;
    ox = (s16)((SCR_W - lev.w * s) / 2);
    oy = (s16)((SCR_H - lev.h * s) / 2);
    while (frame()) {
        if (plat_key() != EK_NONE || CLICKED(MB_RIGHT)) return;
        if (CLICKED(MB_LEFT)) {
            centre_view((s32)(mouse_x - ox) * TILE / s, (s32)(mouse_y - oy) * TILE / s);
            return;
        }
        clip_full();
        fill_rect(0, 0, SCR_W, SCR_H, col_black);
        for (by = 0; by < lev.h; by++)
            for (bx = 0; bx < lev.w; bx++) {
                const Block *b = &BLK(bx, by);
                u8 c = b->type ? wall_avg[b->num] : floor_avg[b->num];
                if (b->shadow) c = shadow_table[c][6];
                fill_rect((s16)(ox + bx * s), (s16)(oy + by * s), s, s, c);
            }
        for (bx = 0; bx < 2; bx++) {
            x = (s16)(ox + lev.start_x[bx] * s);
            y = (s16)(oy + lev.start_y[bx] * s);
            fill_rect(x, y, s, s, bx ? col_yellow : col_red);
        }
        rect((s16)(ox + view_x * s / TILE), (s16)(oy + view_y * s / TILE), (s16)(SCR_W * s / TILE), (s16)(VIEW_H * s / TILE), col_white);
        text(2, 2, "click: go there", C_GREY);
    }
}

/* ------------------------------------------------------------------ */
/* F7 general level variables, F8/F9 random crates                     */

static s32 total_enemies(void)
{
    s32 t = 0;
    int i;
    for (i = 0; i < DIFF_ENEMIES; i++) t += lev.enemies[i];
    return t;
}

static void general_vars(void)
{
    int sel = 0, k, i, rows = 2 + DIFF_ENEMIES, changed = 0;
    char buf[80];
    Level before;
    memcpy(&before, &lev, sizeof(Level));
    while (frame()) {
        while ((k = plat_key()) != EK_NONE) {
            s32 *v = NULL, step = 1, max = 100;
            if (k == EK_ESC || k == EK_F1 + 6) {
                if (changed) {
                    Level after;
                    memcpy(&after, &lev, sizeof(Level));
                    memcpy(&lev, &before, sizeof(Level));
                    push_undo();
                    memcpy(&lev, &after, sizeof(Level));
                }
                return;
            }
            if (k == EK_UP) sel = (sel + rows - 1) % rows;
            if (k == EK_DOWN) sel = (sel + 1) % rows;
            if (sel == 0 && (k == EK_ENTER || (k >= 32 && k < 127))) {
                char c[21];
                strcpy(c, lev.comment);
                if (k != EK_ENTER) {        /* typing starts a new comment */
                    c[0] = (char)k;
                    c[1] = 0;
                }
                if (input_text("level comment:", c, 19, NULL)) {
                    memset(lev.comment, 0, sizeof(lev.comment));
                    strcpy(lev.comment, c);
                    changed = 1;
                }
                continue;
            }
            if (sel == 1) { v = &lev.time_limit; step = 10; max = 9990; }
            if (sel >= 2) v = &lev.enemies[sel - 2];
            if (!v) continue;
            if (plat_key_down(EK_SHIFT)) step *= 10;
            if (k == EK_RIGHT) *v += step;
            if (k == EK_LEFT) *v -= step;
            if (k == EK_PGUP) *v += step * 10;
            if (k == EK_PGDN) *v -= step * 10;
            if (*v < 0) *v = 0;
            if (*v > max) *v = max;
            changed = 1;
        }
        screen_bg();
        box(20, 6, SCR_W - 40, 160);
        text_centre(10, "GENERAL LEVEL VARIABLES", C_YELLOW);
        for (i = 0; i < rows; i++) {
            s16 y = (s16)(26 + i * 11 + (i >= 2 ? 6 : 0));
            if (i == 0) sprintf(buf, "Level comment:  %s", lev.comment);
            else if (i == 1) {
                if (lev.time_limit) sprintf(buf, "Time limit:  %ld seconds", (long)lev.time_limit);
                else sprintf(buf, "Time limit:  none");
            } else strcpy(buf, enemy_name[i - 2]);
            if (i == sel) fill_rect(24, (s16)(y - 2), SCR_W - 48, 11, col_blue);
            text(i >= 2 ? 60 : 30, y, buf, i == sel ? C_YELLOW : C_WHITE);
            if (i >= 2) {
                sprintf(buf, "%ld", (long)lev.enemies[i - 2]);
                text((s16)(250 - text_width(buf)), y, buf, lev.enemies[i - 2] ? C_YELLOW : C_GREY);
            }
        }
        sprintf(buf, "enemies: %ld", (long)total_enemies());
        text(30, 150, buf, total_enemies() > 100 ? C_RED : C_GREY);
        text_centre(170, "UP/DOWN select  LEFT/RIGHT change (SHIFT x10)", C_WHITE);
        text_centre(180, "ENTER edit comment  ESC done", C_WHITE);
    }
}

static void crate_counts(int dm)
{
    int sel = 0, k, i, rows = DIFF_WEAPONS + DIFF_BULLETS + 1, changed = 0;
    char buf[80];
    Level before;
    Crate_counts *c = &lev.rnd[dm];
    memcpy(&before, &lev, sizeof(Level));
    while (frame()) {
        while ((k = plat_key()) != EK_NONE) {
            s32 *v, step = plat_key_down(EK_SHIFT) ? 10 : 1;
            if (k == EK_ESC || k == EK_F1 + 7 || k == EK_F1 + 8) {
                if (changed) {
                    Level after;
                    memcpy(&after, &lev, sizeof(Level));
                    memcpy(&lev, &before, sizeof(Level));
                    push_undo();
                    memcpy(&lev, &after, sizeof(Level));
                }
                return;
            }
            if (k == EK_UP) sel = (sel + rows - 1) % rows;
            if (k == EK_DOWN) sel = (sel + 1) % rows;
            if (k == EK_TAB) sel = sel < DIFF_WEAPONS ? DIFF_WEAPONS : 0;
            v = sel < DIFF_WEAPONS ? &c->weapon[sel] :
                sel < DIFF_WEAPONS + DIFF_BULLETS ? &c->bullet[sel - DIFF_WEAPONS] : &c->energy;
            if (k == EK_RIGHT) { *v += step; changed = 1; }
            if (k == EK_LEFT) { *v -= step; changed = 1; }
            if (*v < 0) *v = 0;
            if (*v > 99) *v = 99;
        }
        screen_bg();
        box(4, 2, SCR_W - 8, SCR_H - 4);
        text_centre(5, dm ? "RANDOM CRATES FOR DEATHMATCH" : "RANDOM CRATES FOR NORMAL GAME", C_YELLOW);
        text(12, 16, "weapon crates", C_GREY);
        text(166, 16, "bullet crates", C_GREY);
        for (i = 0; i < rows; i++) {
            s16 x, y;
            const char *n;
            s32 v;
            if (i < DIFF_WEAPONS) {
                x = 10; y = (s16)(27 + i * 12); n = weapon_name[i]; v = c->weapon[i];
            } else if (i < DIFF_WEAPONS + DIFF_BULLETS) {
                x = 164; y = (s16)(27 + (i - DIFF_WEAPONS) * 12); n = bullet_name[i - DIFF_WEAPONS]; v = c->bullet[i - DIFF_WEAPONS];
            } else {
                x = 164; y = (s16)(27 + DIFF_BULLETS * 12 + 6); n = "Energy crates"; v = c->energy;
            }
            if (i == sel) fill_rect((s16)(x - 2), (s16)(y - 2), 148, 11, col_blue);
            text(x, y, n, i == sel ? C_YELLOW : C_WHITE);
            sprintf(buf, "%ld", (long)v);
            text((s16)(x + 140 - text_width(buf)), y, buf, v ? C_YELLOW : C_GREY);
        }
        sprintf(buf, "placed %s crates on the map: %ld", dm ? "deathmatch" : "normal game", (long)lev.crates[dm]);
        text_centre(165, buf, C_GREY);
        text_centre(176, "UP/DOWN TAB select  LEFT/RIGHT change (SHIFT x10)", C_WHITE);
        text_centre(186, "ESC done", C_WHITE);
    }
}

/* ------------------------------------------------------------------ */
/* SPACE: block or shadow selection                                    */

static void select_blocks(void)
{
    int k, i;
    while (frame()) {
        s16 hx = (s16)(mouse_x / TILE), hy = (s16)(mouse_y / TILE);
        int hover = mouse_x >= 0 && mouse_x < SCR_W && mouse_y >= 0 && mouse_y < SCR_H ? hy * 16 + hx : -1;
        while ((k = plat_key()) != EK_NONE) {
            if (k == EK_ESC || k == ' ' || k == EK_ENTER) return;
            if (k == EK_PGUP) sheet_walls = 1;
            if (k == EK_PGDN) sheet_walls = 0;
            if (k == EK_TAB) sheet_walls = !sheet_walls;
        }
        for (i = 0; i < 2; i++)
            if (hover >= 0 && CLICKED(i ? MB_RIGHT : MB_LEFT)) {
                pick[i].type = sheet_walls;
                pick[i].num = hover;
            }
        clip_full();
        for (i = 0; i < NUM_TILES; i++)
            blit((s16)((i % 16) * TILE), (s16)((i / 16) * TILE), TILE, TILE, sheet_walls ? wall_pix[i] : floor_pix[i], 0);
        for (i = 1; i >= 0; i--)
            if (pick[i].type == sheet_walls) {
                rect((s16)((pick[i].num % 16) * TILE), (s16)((pick[i].num / 16) * TILE), TILE, TILE, i ? col_cyan : col_yellow);
                rect((s16)((pick[i].num % 16) * TILE + 1), (s16)((pick[i].num / 16) * TILE + 1), TILE - 2, TILE - 2, i ? col_cyan : col_yellow);
            }
        if (hover >= 0) rect((s16)(hx * TILE), (s16)(hy * TILE), TILE, TILE, col_white);
        {
            char b[80];
            s16 y = (s16)(mouse_y > SCR_H / 2 ? 2 : SCR_H - 22);
            sprintf(b, "%s %d   left/right button: choose", sheet_walls ? "WALLS" : "FLOOR", hover);
            box(2, y, SCR_W - 4, 20);
            text(6, (s16)(y + 2), b, C_YELLOW);
            text(6, (s16)(y + 11), "PGUP walls, PGDN floor, SPACE back", C_WHITE);
        }
    }
}

static void select_shadow(void)
{
    int k, i;
    while (frame()) {
        int hover = -1;
        while ((k = plat_key()) != EK_NONE)
            if (k == EK_ESC || k == ' ' || k == EK_ENTER) return;
        draw_editor();
        box(20, 30, SCR_W - 40, 110);
        text_centre(36, "CHOOSE SHADOW TYPE", C_YELLOW);
        for (i = 0; i < SHADOW_TYPES; i++) {
            s16 x = (s16)(36 + (i % 8) * 32), y = (s16)(56 + (i / 8) * 34);
            const u8 *t = pick[1].type ? floor_pix[0] : floor_pix[pick[1].num];
            blit(x, y, TILE, TILE, t, 0);
            blit_shadow(x, y, shadow_pix[i]);
            if (mouse_x >= x && mouse_x < x + TILE && mouse_y >= y && mouse_y < y + TILE) hover = i;
            if (pick_shadow == i + 1) rect((s16)(x - 2), (s16)(y - 2), TILE + 4, TILE + 4, col_yellow);
            if (hover == i) rect((s16)(x - 1), (s16)(y - 1), TILE + 2, TILE + 2, col_white);
            {
                char n[4];
                sprintf(n, "%d", i + 1);
                text((s16)(x + 6), (s16)(y + 22), n, C_GREY);
            }
        }
        text_centre(126, "left button: choose, SPACE back", C_WHITE);
        if (hover >= 0 && CLICKED(MB_LEFT)) {
            pick_shadow = hover + 1;
            return;
        }
    }
}

/* ------------------------------------------------------------------ */
/* object tools                                                        */

static s32 dist2(s32 x0, s32 y0, s32 x1, s32 y1)
{
    return (x1 - x0) * (x1 - x0) + (y1 - y0) * (y1 - y0);
}

static void place_spot(void)
{
    Spot s;
    int k;
    char t[40];
    if (lev.spots >= MAX_SPOTS) {
        message("too many spot lights");
        return;
    }
    s.x = map_px();
    s.y = map_py();
    s.size = 3;
    while (frame()) {
        while ((k = plat_key()) != EK_NONE) {
            if (k == EK_ESC) return;
            if (k == EK_ENTER) {
                push_undo();
                lev.spot[lev.spots++] = s;
                return;
            }
            if (k == EK_UP && s.size < LIGHTS - 1) s.size++;
            if (k == EK_DOWN && s.size > 0) s.size--;
        }
        draw_editor();
        clip_set(0, 0, SCR_W, VIEW_H);
        draw_spot(&s, col_white);
        clip_full();
        sprintf(t, "spot light size %ld", (long)s.size);
        panel(t, "UP and DOWN adjust size, ENTER to accept", NULL);
    }
}

static void place_steam(void)
{
    Steam s;
    int k;
    char t[60];
    if (lev.steams >= MAX_STEAM) {
        message("too many steams");
        return;
    }
    s.x = map_px();
    s.y = map_py();
    s.angle = 180;
    s.speed = 2;
    while (frame()) {
        while ((k = plat_key()) != EK_NONE) {
            if (k == EK_ESC) return;
            if (k == EK_ENTER) {
                push_undo();
                lev.steam[lev.steams++] = s;
                return;
            }
            if (k == EK_UP && s.speed < STEAM_SPEEDS - 1) s.speed++;
            if (k == EK_DOWN && s.speed > 0) s.speed--;
            if (k == EK_LEFT) s.angle = (s.angle + 5) % 360;
            if (k == EK_RIGHT) s.angle = (s.angle + 355) % 360;
        }
        draw_editor();
        clip_set(0, 0, SCR_W, VIEW_H);
        draw_steam(&s, col_white);
        clip_full();
        sprintf(t, "steam: range %ld, angle %ld", (long)s.speed, (long)s.angle);
        panel(t, "UP,DOWN adjust RANGE, LEFT,RIGHT adjust ANGLE", "ENTER to accept");
    }
}

static void place_crate(int dm)
{
    Crate_info c;
    int k;
    if (lev.crates[dm] >= MAX_CRATE_INFO) {
        message("too many crates");
        return;
    }
    c.x = map_px() - CRATE_SIZE / 2;
    c.y = map_py() - CRATE_SIZE / 2;
    c.type1 = 0;
    c.type2 = 0;
    while (frame()) {
        s32 n;
        while ((k = plat_key()) != EK_NONE) {
            if (k == EK_ESC) return;
            if (k == EK_ENTER) {
                push_undo();
                lev.crate[dm][lev.crates[dm]++] = c;
                return;
            }
            if (k == EK_UP) { c.type1 = (c.type1 + 1) % 3; c.type2 = 0; }
            if (k == EK_DOWN) { c.type1 = (c.type1 + 2) % 3; c.type2 = 0; }
            n = c.type1 == 0 ? DIFF_WEAPONS : c.type1 == 1 ? DIFF_BULLETS : 1;
            if (k == EK_RIGHT) c.type2 = (c.type2 + 1) % n;
            if (k == EK_LEFT) c.type2 = (c.type2 + n - 1) % n;
        }
        draw_editor();
        clip_set(0, 0, SCR_W, VIEW_H);
        draw_crate(&c, dm);
        clip_full();
        panel(crate_text(&c), "UP,DOWN adjust TYPE1, LEFT,RIGHT adjust TYPE2", "ENTER to accept");
    }
}

static void delete_nearest(int what)
{
    s32 best = PICK_DIST * PICK_DIST + 1, d, x = map_px(), y = map_py();
    s32 i, k, bi = -1, bk = 0;
    if (what == T_SPOT_DEL)
        for (i = 0; i < lev.spots; i++)
            if ((d = dist2(x, y, lev.spot[i].x, lev.spot[i].y)) < best) { best = d; bi = i; }
    if (what == T_STEAM_DEL)
        for (i = 0; i < lev.steams; i++)
            if ((d = dist2(x, y, lev.steam[i].x, lev.steam[i].y)) < best) { best = d; bi = i; }
    if (what == T_CRATE_DEL)
        for (k = 0; k < 2; k++)
            for (i = 0; i < lev.crates[k]; i++)
                if ((d = dist2(x, y, lev.crate[k][i].x + CRATE_SIZE / 2, lev.crate[k][i].y + CRATE_SIZE / 2)) < best) {
                    best = d; bi = i; bk = k;
                }
    if (bi < 0) {
        message("nothing here to delete");
        return;
    }
    push_undo();
    if (what == T_SPOT_DEL) {
        lev.spot[bi] = lev.spot[--lev.spots];
    } else if (what == T_STEAM_DEL) {
        lev.steam[bi] = lev.steam[--lev.steams];
    } else {
        for (i = bi; i < lev.crates[bk] - 1; i++) lev.crate[bk][i] = lev.crate[bk][i + 1];
        lev.crates[bk]--;
    }
}

static void fill_area(s32 x0, s32 y0, s32 x1, s32 y1)
{
    s32 x, y, t;
    if (x0 > x1) { t = x0; x0 = x1; x1 = t; }
    if (y0 > y1) { t = y0; y0 = y1; y1 = t; }
    if (x0 < 0) x0 = 0;
    if (y0 < 0) y0 = 0;
    if (x1 >= lev.w) x1 = lev.w - 1;
    if (y1 >= lev.h) y1 = lev.h - 1;
    push_undo();
    for (y = y0; y <= y1; y++)
        for (x = x0; x <= x1; x++) {
            Block *b = &BLK(x, y);
            if (mode == MODE_SHADOWS) b->shadow = pick_shadow;
            else {
                b->type = pick[0].type;
                b->num = pick[0].num;
            }
        }
}

/* ------------------------------------------------------------------ */
/* mouse in the map view                                               */

static void paint(int button)
{
    s32 bx = map_bx(), by = map_by();
    Block *b;
    if (!on_level(bx, by)) return;
    b = &BLK(bx, by);
    if (mode == MODE_SHADOWS) {
        s32 s = button ? 0 : pick_shadow;
        if (b->shadow == s) return;
        if (!stroke) { push_undo(); stroke = 1; }
        b->shadow = s;
    } else {
        if (b->type == pick[button].type && b->num == pick[button].num) return;
        if (!stroke) { push_undo(); stroke = 1; }
        b->type = pick[button].type;
        b->num = pick[button].num;
    }
}

static void mouse_in_view(void)
{
    static s16 drag_x, drag_y;
    if (mouse_b & MB_MIDDLE) {
        if (mouse_prev & MB_MIDDLE) {
            view_x = (s16)(view_x - (mouse_x - drag_x));
            view_y = (s16)(view_y - (mouse_y - drag_y));
            clamp_view();
        }
        drag_x = mouse_x;
        drag_y = mouse_y;
        return;
    }
    if (!(mouse_b & (MB_LEFT | MB_RIGHT))) stroke = 0;
    if (!in_view()) return;

    if (tool == T_PAINT) {
        if (mouse_b & MB_LEFT) paint(0);
        else if (mouse_b & MB_RIGHT) paint(1);
        return;
    }
    if (CLICKED(MB_RIGHT)) {
        tool = T_PAINT;
        stroke = 1;          /* the click must not paint */
        return;
    }
    if (!CLICKED(MB_LEFT)) return;
    switch (tool) {
    case T_START1:
    case T_START2:
        if (on_level(map_bx(), map_by())) {
            int p = tool == T_START2;
            push_undo();
            lev.start_x[p] = map_bx();
            lev.start_y[p] = map_by();
        }
        tool = T_PAINT;
        stroke = 1;
        break;
    case T_FILL1:
        fill_x = map_bx();
        fill_y = map_by();
        tool = T_FILL2;
        break;
    case T_FILL2:
        fill_area(fill_x, fill_y, map_bx(), map_by());
        tool = T_PAINT;
        stroke = 1;
        break;
    case T_SPOT: place_spot(); break;
    case T_STEAM: place_steam(); break;
    case T_CRATE_N: place_crate(0); break;
    case T_CRATE_D: place_crate(1); break;
    default: delete_nearest(tool); break;
    }
}

/* ------------------------------------------------------------------ */

static void autosave(void)
{
    if (!modified) return;
    if (level_save(data_path("LEVS/AUTOSAVE.LEV")))
        printf("Unsaved changes written to %s\n", data_path("LEVS/AUTOSAVE.LEV"));
}

static int handle_key(int k)
{
    if (k >= 'A' && k <= 'Z') k = tolower(k);
    switch (k) {
    case EK_ESC:
        if (tool != T_PAINT) tool = T_PAINT;
        else if (confirm("really quit ?", modified ? "the changes are not saved" : NULL)) return 0;
        break;
    case EK_F1: help_screen(); break;
    case EK_F1 + 1: save_level(); break;
    case EK_F1 + 2: load_level(); break;
    case EK_F1 + 3: new_level(); break;
    case EK_F1 + 4: level_map(); break;
    case EK_F1 + 5:
        if (confirm("make shadows automatically ?", "all shadows are replaced")) {
            push_undo();
            level_auto_shadows();
            message("shadows done");
        }
        break;
    case EK_F1 + 6: general_vars(); break;
    case EK_F1 + 7: crate_counts(0); break;
    case EK_F1 + 8: crate_counts(1); break;
    case 'q': tool = T_SPOT; break;
    case 'w': tool = T_SPOT_DEL; break;
    case 'a': tool = T_STEAM; break;
    case 's': tool = T_STEAM_DEL; break;
    case 'z': tool = T_CRATE_N; break;
    case 'x': tool = T_CRATE_D; break;
    case 'c': tool = T_CRATE_DEL; break;
    case '1': tool = T_START1; break;
    case '2': tool = T_START2; break;
    case 'f': tool = T_FILL1; break;
    case 'u': undo(); break;
    case 'g': grid = !grid; break;
    case EK_ENTER:
        mode = mode == MODE_BLOCKS ? MODE_SHADOWS : MODE_BLOCKS;
        tool = T_PAINT;
        break;
    case ' ':
        if (mode == MODE_BLOCKS) select_blocks();
        else select_shadow();
        stroke = 1;          /* the selecting click must not paint */
        break;
    case EK_PGUP: sheet_walls = 1; message("selection: walls"); break;
    case EK_PGDN: sheet_walls = 0; message("selection: floor"); break;
    default: break;
    }
    return 1;
}

void editor_run(const char *start_file)
{
    int k, run = 1;
    level_new(30, 20, 0, 0);
    view_x = view_y = -TILE;
    ui_colours();
    plat_set_palette(pal);
    if (start_file) {
        char path[300];
        const char *rel = start_file;
        if (strncmp(rel, "LEVS/", 5) == 0) rel += 5;
        sprintf(path, "LEVS/%.250s", rel);
        if (level_load(data_path(path)) || level_load(start_file)) {
            strncpy(file_name, rel, sizeof(file_name) - 1);
            centre_view(lev.start_x[0] * TILE, lev.start_y[0] * TILE);
            message("level loaded");
        } else {
            message("level not found, new level");
        }
    }
    plat_mouse(&mouse_x, &mouse_y, &mouse_b);
    while (run && frame()) {
        while (run && (k = plat_key()) != EK_NONE) run = handle_key(k);
        if (quit_now) break;
        {
            s16 sp = (s16)(plat_key_down(EK_SHIFT) ? 12 : 4);
            if (plat_key_down(EK_LEFT)) view_x = (s16)(view_x - sp);
            if (plat_key_down(EK_RIGHT)) view_x = (s16)(view_x + sp);
            if (plat_key_down(EK_UP)) view_y = (s16)(view_y - sp);
            if (plat_key_down(EK_DOWN)) view_y = (s16)(view_y + sp);
            clamp_view();
        }
        mouse_in_view();
        if (msg_time > 0) msg_time--;
        draw_editor();
    }
    if (quit_now) autosave();
}
