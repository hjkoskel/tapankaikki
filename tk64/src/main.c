/* main.c - Ultimate Tapan Kaikki 64: start-up, loading, video, sound,
 * text screens, level flow and the 25 Hz game loop. Gameplay is in game.c. */
#pragma static-locals(on)

#include <c64.h>
#include <cbm.h>
#include <device.h>
#include <conio.h>
#include <string.h>
#include "defs.h"

u8 levelbuf[LH_SIZE + 64 * 4 + 100 * 100];
u8 *map;
u8 map_w, map_h;
u16 rowoff[100];

/* Views. One player: one 40x22 view, HUD rows 22-24.
 * Two players: split screen top/bottom, 40x12 chars each, with the shared
 * HUD row 12 between them. Top/bottom suits the VIC-II: a raster interrupt
 * in the HUD row reuses all 8 hardware sprites for the lower view, and the
 * map routine draws whole 40 char rows. A left/right split would have to
 * share 8 sprites between both views on every raster line. */
u8 view_cx[MAXPL], view_cy[MAXPL];           /* camera, in characters */
u8 view_top[MAXPL], view_ymin[MAXPL], view_ymax[MAXPL];
u8 view_rows[MAXPL];
static u8 view_row0[MAXPL];
static s16 camx[MAXPL], camy[MAXPL];
static u8 drawn_cx[MAXPL], drawn_cy[MAXPL];
#define HUD2_ROW 12                         /* split screen HUD row */
#define SPLIT_LINE 147                      /* raster line of HUD2_ROW */

static u8 device;
static u8 *scr_show, *scr_draw;
static u8 level, episode;
static u16 ticks;

/* ------------------------------------------------------------------ */
/* helpers                                                             */

static void fail(const char *s)
{
    asm("sei");                                   /* stop the raster interrupt */
    *(u8 *)0xDD00 = (*(u8 *)0xDD00 & 0xFC) | 3;   /* back to bank 0 */
    VIC.addr = 0x15;
    VIC.ctrl1 |= 0x10;
    clrscr();                                     /* overwrites the overlay: never returns */
    cputs(s);
    for (;;) ;
}

static u8 load(const char *name, void *addr)
{
    return cbm_load(name, device, addr) != 0;
}

/* copy sprite data to $D000-$DFFF: needs the I/O area switched off */
static void copy_under_io(void)
{
    u16 i;
    u8 *d = (u8 *)0xD000;
    asm("sei");
    *(u8 *)1 = 0x34;
    for (i = 0; i < 64 * 64; i++) d[i] = levelbuf[i];
    *(u8 *)1 = 0x36;
    asm("cli");
}

/* uppercase font from the character ROM into chars 0-63 */
static void copy_rom_font(void)
{
    u16 i;
    const u8 *src = (const u8 *)0xD000;
    asm("sei");
    *(u8 *)1 = 0x32;          /* char ROM in, BASIC ROM out: BSS reaches $A000+ */
    for (i = 0; i < 64 * 8; i++) CHARSET[i] = src[i];
    *(u8 *)1 = 0x36;
    asm("cli");
}

/* PETSCII (cc65 string) -> screen code */
static u8 scode(u8 c)
{
    if (c >= 0x40 && c < 0x60) return (u8)(c - 0x40);
    if (c >= 0xC0 && c < 0xE0) return (u8)(c - 0xC0);
    if (c >= 0x20 && c < 0x40) return c;
    return 0x20;
}

/* text into both screens, at most n characters */
static void print_n(u8 x, u8 y, const char *s, u8 n)
{
    u16 o = y * 40 + x;
    while (*s && n-- && x++ < 40) {
        SCREEN_A[o] = SCREEN_B[o] = scode((u8)*s++);
        o++;
    }
}

void print(u8 x, u8 y, const char *s)
{
    print_n(x, y, s, 40);
}

void print_num(u8 x, u8 y, u16 v, u8 width)
{
    char b[6];
    u8 i = width;
    b[width] = 0;
    while (i) {
        b[--i] = (char)('0' + v % 10);
        v /= 10;
    }
    print(x, y, b);
}

static void center(u8 y, const char *s)
{
    print((u8)(20 - strlen(s) / 2), y, s);
}

static void clear_rows(u8 from, u8 to)
{
    memset(SCREEN_A + from * 40, 32, (to - from) * 40);
    memset(SCREEN_B + from * 40, 32, (to - from) * 40);
}

/* ------------------------------------------------------------------ */
/* sound: one table driven SID effect per voice                        */

typedef struct { u8 freq, ctrl, ad, sr, gate, voice; } SFX;
static const SFX sfx_tab[] = {
    { 0, 0, 0, 0, 0, 0 },
    { 0x28, 0x81, 0x08, 0x00, 1, 0 },   /* gun */
    { 0x10, 0x81, 0x09, 0x00, 2, 0 },   /* shotgun */
    { 0x30, 0x81, 0x06, 0x00, 1, 0 },   /* uzi */
    { 0x08, 0x81, 0x0A, 0x00, 2, 0 },   /* launcher */
    { 0x08, 0x11, 0x05, 0x00, 1, 1 },   /* hit */
    { 0x04, 0x81, 0x0C, 0x09, 4, 2 },   /* explosion */
    { 0x40, 0x41, 0x03, 0x00, 2, 1 },   /* pickup */
    { 0x10, 0x21, 0x0A, 0x00, 3, 1 },   /* death */
    { 0x50, 0x81, 0x04, 0x00, 1, 0 }    /* flame */
};
static u8 sfx_gate[3], sfx_ctrl[3];

void sfx(u8 n)
{
    const SFX *s = &sfx_tab[n];
    u8 *v = (u8 *)0xD400 + s->voice * 7;
    if (!n) return;
    v[4] = 0;                         /* gate off: restart envelope */
    v[0] = 0;
    v[1] = s->freq;
    v[2] = 0;
    v[3] = 8;                         /* pulse width 50% */
    v[5] = s->ad;
    v[6] = s->sr;
    v[4] = s->ctrl;
    sfx_gate[s->voice] = s->gate;
    sfx_ctrl[s->voice] = s->ctrl;
}

static void sfx_frame(void)
{
    u8 i;
    for (i = 0; i < 3; i++)
        if (sfx_gate[i] && --sfx_gate[i] == 0) *((u8 *)0xD404 + i * 7) = sfx_ctrl[i] & 0xFE;
}

/* ------------------------------------------------------------------ */
/* video                                                               */

static void sprites_off(void)
{
    irq_split = 0;
    spr_ena[0] = spr_ena[1] = 0;
}

/* set up the views and their colour RAM: map rows multicolour (gore red),
 * HUD rows white hi-res text */
static void views_init(void)
{
    u8 r, v, c;
    if (nplayers == 1) {
        view_row0[0] = 0;
        view_rows[0] = PLAY_ROWS;
        view_top[0] = 51;
        view_ymin[0] = 30;                       /* may reach into the top border */
        view_ymax[0] = 51 + PLAY_ROWS * 8 - 22;
        irq_split = 0;
    } else {
        view_row0[0] = 0;
        view_rows[0] = 12;
        view_top[0] = 51;
        view_ymin[0] = 30;
        view_ymax[0] = SPLIT_LINE - 21;          /* sprite ends above the HUD row */
        view_row0[1] = 13;
        view_rows[1] = 12;
        view_top[1] = 51 + 13 * 8;
        view_ymin[1] = 51 + 13 * 8 + 2;          /* after the interrupt has moved the sprites */
        view_ymax[1] = 240;                      /* may reach into the bottom border */
        irq_split = SPLIT_LINE;
    }
    for (r = 0; r < 25; r++) {
        c = 1;
        for (v = 0; v < nplayers; v++)
            if (r >= view_row0[v] && r < view_row0[v] + view_rows[v]) c = COL_CRAM | 8;
        memset(COLORRAM + r * 40, c, 40);
    }
}

static void text_screen(void)
{
    sprites_off();
    VIC.bgcolor0 = 0;
    clear_rows(0, 25);
    memset(COLORRAM, 1, 1000);
}

static void video_init(void)
{
    *(u8 *)0xDD00 = *(u8 *)0xDD00 & 0xFC;          /* VIC bank 3: $C000-$FFFF */
    VIC.addr = D018_A;
    VIC.ctrl2 = 0x18;                               /* multicolour chars, 40 columns */
    VIC.bordercolor = 0;
    VIC.bgcolor0 = COL_BG;
    VIC.bgcolor1 = COL_D022;
    VIC.bgcolor2 = COL_D023;
    scr_show = SCREEN_A;
    scr_draw = SCREEN_B;
    sprites_off();
    irq_init();
    VIC.spr_mcolor0 = SPR_MC0;
    VIC.spr_mcolor1 = SPR_MC1;
    VIC.spr_ena = 0;
    *(u8 *)0xD418 = 15;                             /* SID volume */
    *(u8 *)0x0291 = 0x80;                           /* lock SHIFT+C= case switching */
}

/* wait for the next frame (the raster interrupt counts them) */
static void wait_vblank(void)
{
    u8 f = irq_frame;
    while (irq_frame == f) ;
}

u8 wall(s16 x, s16 y)
{
    u8 tx, ty;
    if (x < 0 || y < 0) return 1;
    tx = (u8)(x >> 4);
    ty = (u8)(y >> 4);
    if (tx >= map_w || ty >= map_h) return 1;
    return map[rowoff[ty] + tx] & 0x80;
}

/* change a map tile and patch its 4 chars in the views on screen */
void stamp_tile(u8 tx, u8 ty, u8 id)
{
    s16 cx, cy;
    u8 v, rows;
    u8 *o;
    map[rowoff[ty] + tx] = id;
    for (v = 0; v < nplayers; v++) {
        if (drawn_cx[v] == 255) continue;
        cx = (s16)tx * 2 - drawn_cx[v];
        cy = (s16)ty * 2 - drawn_cy[v];
        rows = view_rows[v];
        if (cx < -1 || cx > 39 || cy < -1 || cy > rows - 1) continue;
        o = scr_show + (view_row0[v] + cy) * 40 + cx;
        if (cy >= 0) {
            if (cx >= 0) o[0] = mt_tl[id];
            if (cx < 39) o[1] = mt_tr[id];
        }
        if (cy < rows - 1) {
            if (cx >= 0) o[40] = mt_bl[id];
            if (cx < 39) o[41] = mt_br[id];
        }
    }
}

/* camera of view v follows player v, a bit ahead in the facing direction
 * (shifts only: signed division is slow in cc65) */
static void camera_update(u8 v)
{
    static s16 cx, cy, t, maxx, maxy;
    u8 a = pl_angle[v], rows = view_rows[v];
    cx = camx[v];
    cy = camy[v];
    t = PX(pl_x[v]) + 11 + (sintab[a] >> 2) - 160;
    cx += (t - cx) >> 2;
    t = PX(pl_y[v]) + 11 + (sintab[(u8)(a + 64)] >> 2) - (rows << 2);
    cy += (t - cy) >> 2;
    maxx = ((s16)map_w << 4) - 320;
    maxy = ((s16)map_h << 4) - (rows << 3);
    if (cx > maxx) cx = maxx;
    if (cy > maxy) cy = maxy;
    if (cx < 0) cx = 0;
    if (cy < 0) cy = 0;
    camx[v] = cx;
    camy[v] = cy;
    view_cx[v] = (u8)(cx >> 3);
    view_cy[v] = (u8)(cy >> 3);
}

/* draw the views whose camera moved into the hidden screen, copy the others
 * from the visible one, then flip (in the raster interrupt) */
static void redraw_if_needed(void)
{
    u8 v, moved = 0, *t;
    u16 off;
    for (v = 0; v < nplayers; v++)
        if (view_cx[v] != drawn_cx[v] || view_cy[v] != drawn_cy[v]) moved = 1;
    if (!moved) return;
    for (v = 0; v < nplayers; v++) {
        off = view_row0[v] * 40;
        if (view_cx[v] != drawn_cx[v] || view_cy[v] != drawn_cy[v]) {
            dm_map = map + rowoff[view_cy[v] >> 1] + (view_cx[v] >> 1);
            dm_w = map_w;
            dm_px = view_cx[v] & 1;
            dm_py = view_cy[v] & 1;
            dm_scr = scr_draw + off;
            dm_rows = view_rows[v];
            draw_map();
            drawn_cx[v] = view_cx[v];
            drawn_cy[v] = view_cy[v];
        } else
            memcpy(scr_draw + off, scr_show + off, view_rows[v] * 40);
    }
    t = scr_show; scr_show = scr_draw; scr_draw = t;
    irq_d018 = scr_show == SCREEN_A ? D018_A : D018_B;
}

/* ------------------------------------------------------------------ */
/* input                                                               */

#ifdef AUTOPLAY
/* scripted joystick for automated tests: {ticks, joy}; 32 = change weapon */
#if defined(AUTOPLAY_CLOSE)
static const u8 script[] = { 5, 0, 200, J_FIRE, 255, 0 };     /* fist only */
#elif defined(AUTOPLAY_KILL)
static const u8 script[] = { 5, 0, 1, 32, 1, 0, 60, J_FIRE, 1, 32, 1, 0, 1, 32, 80, J_FIRE, 255, 0 };
#else
static const u8 script[] = {
    10, 0, 1, 32, 1, 0, 2, J_LEFT, 30, J_UP, 2, J_RIGHT, 40, J_UP | J_FIRE, 1, 32, 1, 0, 1, 32,
    4, J_LEFT, 40, J_UP | J_FIRE, 1, 32, 1, 0, 1, 32, 1, 0, 1, 32, 1, 0, 1, 32,
    6, J_LEFT, 60, J_UP | J_FIRE, 8, J_RIGHT, 80, J_UP | J_FIRE, 255, 0
};
#endif
static u8 sc_pos, sc_left;
static u8 script_joy(void)
{
    if (sc_left == 0) {
        if (script[sc_pos] == 255) return J_FIRE;
        sc_left = script[sc_pos];
        sc_pos += 2;
    }
    sc_left--;
    return script[sc_pos - 1];
}
#endif

/* ------------------------------------------------------------------
 * Keyboard and joysticks, read directly from the CIA1 matrix.
 *
 * Key sets (see c64portplan.md section 7):
 *   player 1: W up, S down, A left, D right, left SHIFT fire, Q or C= weapon
 *   player 2: - up, @ down, P left, * right, + fire, right SHIFT weapon
 * No three of these 12 keys form a rectangle in the 8x8 matrix, so any
 * combination can be pressed without ghost keys. Player 2's keys are all on
 * lines PA5-PA7, which a joystick in port 2 (PA0-PA4) cannot disturb.
 * A joystick in port 1 shares PB0-PB4 with the keyboard: while it is
 * pressed the keyboard is ignored for that tick.
 */
typedef struct { u8 line, mask, bit; } KEYDEF;    /* $DC00 line, $DC01 bit, joystick bit */
/* next weapon: player 1 Q or C=, player 2 right SHIFT (listed twice). Q is on
 * line 7 like C=: no ghosts onto the other game keys, no joystick 2 trouble. */
#define NKEYS 7
static const KEYDEF keydef[2][NKEYS] = {
    { {1, 0x02, J_UP}, {1, 0x20, J_DOWN}, {1, 0x04, J_LEFT}, {2, 0x04, J_RIGHT}, {1, 0x80, J_FIRE}, {7, 0x40, J_CHANGE}, {7, 0x20, J_CHANGE} },
    { {5, 0x08, J_UP}, {5, 0x40, J_DOWN}, {5, 0x02, J_LEFT}, {6, 0x02, J_RIGHT}, {5, 0x01, J_FIRE}, {6, 0x10, J_CHANGE}, {6, 0x10, J_CHANGE} }
};
static u8 kmat[8];
static u8 input[2];            /* per player: joystick bits incl. J_CHANGE */
static u8 key_stop, key_return, key_space, key_1, key_2, key_f1;

static void scan_input(void)
{
    u8 a, j1, j2, p, k, n = 0;
    const KEYDEF *d;
    CIA1.pra = 0xFF;           /* no line selected: what is read now are the joysticks */
    j2 = (u8)(~CIA1.pra & 0x1F);
    j1 = (u8)(~CIA1.prb & 0x1F);
    for (a = 0; a < 8; a++) {
        CIA1.pra = (u8)~(1 << a);
        kmat[a] = (u8)~CIA1.prb;
    }
    CIA1.pra = 0x7F;
    input[0] = j2;
    input[1] = j1;
    key_stop = key_return = key_space = key_1 = key_2 = key_f1 = 0;
    if (j1) return;            /* joystick 1 garbles the keyboard rows */
    for (p = 0; p < 2; p++)
        for (k = 0, d = keydef[p]; k < NKEYS; k++, d++)
            if (kmat[d->line] & d->mask) input[p] |= d->bit;
    for (a = 0; a < 8; a++) n |= kmat[a] & (a == 7 ? 0x7F : 0xFF);
    key_stop = (kmat[7] & 0x80) && !n;   /* RUN/STOP alone (a ghost of S+SHIFT+C= otherwise) */
    key_return = (kmat[0] & 0x02) != 0;
    key_space = (kmat[7] & 0x10) != 0;
    key_1 = kmat[7] & 0x01;
    key_2 = kmat[7] & 0x08;
    key_f1 = kmat[0] & 0x10;
}

/* input of each player into joy[]. Player 1: joystick 2 or W A S D keys,
 * player 2: joystick 1 or - @ P * keys. One player: any of them. */
static void read_input(void)
{
#ifdef AUTOPLAY
    joy[0] = joy[1] = script_joy();
#else
    scan_input();
    if (nplayers == 1) joy[0] = (u8)(input[0] | input[1]);
    else {
        joy[0] = input[0];
        joy[1] = input[1];
    }
#endif
}

static u8 fire_pressed(void)
{
    return (u8)((((input[0] | input[1]) & J_FIRE) | key_return | key_space) != 0);
}

#ifndef AUTOPLAY
static void wait_release(void)
{
    u8 i;
    do { wait_vblank(); scan_input(); } while (fire_pressed() | key_1 | key_2);
    for (i = 0; i < 10; i++) wait_vblank();
    *(u8 *)0xC6 = 0;           /* empty the KERNAL key buffer */
}
#endif

static void wait_fire(void)
{
    u8 i;
#ifdef AUTOPLAY
    for (i = 0; i < 50; i++) { wait_vblank(); wait_vblank(); }
#else
    (void)i;
    do { wait_vblank(); scan_input(); } while (!fire_pressed());
    wait_release();
#endif
}

/* CIA2 timer A as a free running cycle counter */
static void timer_init(void)
{
    CIA2.ta_lo = 0xFF;
    CIA2.ta_hi = 0xFF;
    CIA2.cra = 0x11;
}

static u16 timer_read(void)
{
    u8 lo = CIA2.ta_lo, hi = CIA2.ta_hi;
    return ((u16)hi << 8) | lo;
}

/* ------------------------------------------------------------------ */
/* HUD                                                                 */

/* HUD fields are only redrawn when their value changes (number printing
 * with 16-bit division is slow on the 6502). HUD rows are hi-res text:
 * the colour RAM colour (0-7) of each character is its colour. */
#define HC_WEAPON 7      /* yellow: the selected weapon */
#define HC_AMMO   3      /* cyan */
#define HC_LOW    2      /* red: low ammo / energy */
#define HC_OK     5      /* green */
#define HC_LABEL  1      /* white */
#define HC_DIM    6      /* blue: weapons not owned */
#define LOW_AMMO  10

static u8 h_weapon[MAXPL], h_energy[MAXPL], h_msg[MAXPL], h_left;
static s16 h_ammo[MAXPL];
static u16 h_cash, h_sec, h_owned;

static const char wcode[NWEAPONS][3] = {
    "FI", "PI", "SG", "UZ", "RI", "GL", "AG", "HL", "AS", "C4", "FL", "MI"
};

static void hud_reset(void)
{
    h_weapon[0] = h_weapon[1] = h_energy[0] = h_energy[1] = h_msg[0] = h_msg[1] = h_left = 255;
    h_ammo[0] = h_ammo[1] = -1;
    h_cash = h_sec = h_owned = 65535u;
}

static void color(u8 x, u8 y, u8 len, u8 c)
{
    memset(COLORRAM + y * 40 + x, c, len);
}

/* ammo of the current weapon, -2 = fist (no ammo) */
static s16 ammo_of(u8 p)
{
    u8 w = pl_curr[p], b = weapon64[w].btype;
    return w ? pl_bullets[p][b - 1] / bullet64[b].mul : -2;
}

/* 3 digits, red when low, "---" for the fist */
static void ammo_field(u8 x, u8 y, s16 ammo)
{
    if (ammo < 0) {
        print(x, y, "---");
        color(x, y, 3, HC_LABEL);
    } else {
        print_num(x, y, (u16)(ammo > 999 ? 999 : ammo), 3);
        color(x, y, 3, ammo < LOW_AMMO ? HC_LOW : HC_AMMO);
    }
}

/* energy as '*' characters: green, yellow under half, red under a quarter */
static void energy_bar(u8 x, u8 y, u8 p, u8 len, u8 per)
{
    char bar[11];
    u8 i, e = (u8)((pl_energy[p] + per - 1) / per);
    if (e == h_energy[p]) return;
    h_energy[p] = e;
    for (i = 0; i < len; i++) bar[i] = i < e ? '*' : '.';
    bar[len] = 0;
    print(x, y, bar);
    color(x, y, len, e * 4 <= len ? HC_LOW : e * 2 <= len ? HC_WEAPON : HC_OK);
}

/* one player: row 22  WEAPON NAME      AMMO 098  ENERGY **********
 *             row 23  message          04 LEFT  $00900  0:08
 *             row 24  all weapons, owned ones white, the selected yellow */
static void hud1(void)
{
    s16 ammo;
    u16 sec, owned = 0;
    u8 m, i, x;
    if (h_weapon[0] != pl_curr[0]) {
        h_weapon[0] = pl_curr[0];
        print(0, HUD_ROW, "                ");
        print(0, HUD_ROW, weapon64[pl_curr[0]].name);
        color(0, HUD_ROW, 16, HC_WEAPON);
        print(17, HUD_ROW, "AMMO");
        color(17, HUD_ROW, 4, HC_LABEL);
        print(26, HUD_ROW, "ENERGY");
        color(26, HUD_ROW, 6, HC_LABEL);
        h_ammo[0] = -1;
        h_owned = 65535u;
    }
    ammo = ammo_of(0);
    if (ammo != h_ammo[0]) {
        h_ammo[0] = ammo;
        ammo_field(22, HUD_ROW, ammo);
    }
    energy_bar(33, HUD_ROW, 0, 7, 143);
    sec = ticks / 25;
    if (sec != h_sec) {
        h_sec = sec;
        print_num(35, HUD_ROW + 1, sec / 60, 2);
        print(37, HUD_ROW + 1, ":");
        print_num(38, HUD_ROW + 1, sec % 60, 2);
    }
    m = pl_msg_time[0] ? pl_msg_seq[0] : 0;
    if (m != h_msg[0]) {
        h_msg[0] = m;
        print(0, HUD_ROW + 1, "                    ");
        if (m) print(0, HUD_ROW + 1, pl_msg[0]);
    }
    if ((u8)(ne - edead_count) != h_left) {
        h_left = (u8)(ne - edead_count);
        print_num(20, HUD_ROW + 1, h_left, 2);
        print(23, HUD_ROW + 1, "LEFT");
    }
    if (pl_cash[0] != h_cash) {
        h_cash = pl_cash[0];
        print(28, HUD_ROW + 1, "$");
        print_num(29, HUD_ROW + 1, pl_cash[0], 5);
    }
    for (i = 0; i < NWEAPONS; i++)
        if (pl_weapons[0][i]) owned |= 1 << i;
    if (owned != h_owned) {
        h_owned = owned;
        for (i = 0, x = 2; i < NWEAPONS; i++, x += 3) {
            print(x, HUD_ROW + 2, wcode[i]);
            color(x, HUD_ROW + 2, 2, i == pl_curr[0] ? HC_WEAPON : (owned & (1 << i)) ? HC_LABEL : HC_DIM);
            print(x - 1, HUD_ROW + 2, i == pl_curr[0] ? ">" : " ");
            color(x - 1, HUD_ROW + 2, 1, HC_WEAPON);
        }
    }
}

/* split screen: row 12 = player 1 (cols 0-16), enemies left (19-20),
 * player 2 (cols 23-39). Per player: weapon 8 (yellow), ammo 3, energy 5
 * chars; a message replaces weapon and ammo for a while. */
static void hud2(void)
{
    u8 p, b, m;
    s16 ammo;
    for (p = 0; p < 2; p++) {
        b = p ? 23 : 0;
        m = pl_msg_time[p] ? pl_msg_seq[p] : 0;
        if (m != h_msg[p]) {
            h_msg[p] = m;
            h_weapon[p] = 255;
            if (m) {
                print(b, HUD2_ROW, "           ");
                print_n(b, HUD2_ROW, pl_msg[p], 11);
                color(b, HUD2_ROW, 11, HC_LABEL);
            }
        }
        if (!m) {
            if (h_weapon[p] != pl_curr[p]) {
                h_weapon[p] = pl_curr[p];
                print(b, HUD2_ROW, "           ");
                print(b, HUD2_ROW, wshort[pl_curr[p]]);
                color(b, HUD2_ROW, 8, HC_WEAPON);
                h_ammo[p] = -1;
            }
            ammo = ammo_of(p);
            if (ammo != h_ammo[p]) {
                h_ammo[p] = ammo;
                ammo_field(b + 8, HUD2_ROW, ammo);
            }
        }
        energy_bar(b + 12, HUD2_ROW, p, 5, 200);
    }
    if ((u8)(ne - edead_count) != h_left) {
        h_left = (u8)(ne - edead_count);
        print_num(19, HUD2_ROW, h_left, 2);
    }
}

/* ------------------------------------------------------------------ */
/* level flow                                                          */

static u8 level_load(void)
{
    char name[6];
    u8 i;
    name[0] = 'l';
    name[1] = (char)('a' + episode);
    name[2] = (char)('0' + (level + 1) / 10);
    name[3] = (char)('0' + (level + 1) % 10);
    name[4] = 0;
    if (!load(name, levelbuf)) return 0;
    map_w = levelbuf[LH_W];
    map_h = levelbuf[LH_H];
    map = levelbuf + LH_SIZE + levelbuf[LH_NCINFO] * 4;
    for (i = 0; i < map_h; i++) rowoff[i] = (u16)i * map_w;
    return 1;
}

/* in the overlay (src/ovl.s), so the test build runs overlay code too */
#pragma code-name (push, "OVERLAY")
#pragma rodata-name (push, "OVERLAY")
static void level_intro(void)
{
    text_screen();
    print(14, 4, "LEVEL");
    print_num(20, 4, level + 1, 2);
    center(6, (const char *)levelbuf + LH_COMMENT);
    print_num(15, 9, ne, 2);
    print(18, 9, "ENEMIES");
    center(20, "PRESS FIRE");
    wait_fire();
}
#pragma rodata-name (pop)
#pragma code-name (pop)

#ifndef AUTOPLAY
/* any key (or a joystick) held */
static u8 any_input(void)
{
    u8 a, n = (u8)(input[0] | input[1]);
    for (a = 0; a < 8; a++) n |= kmat[a];
    return n;
}

/* RUN/STOP in the game: ask, as the PC version does. The game is paused
 * meanwhile. Y = quit to the title, any other key = continue. */
static u8 ask_quit(void)
{
    u8 row = nplayers == 1 ? HUD_ROW + 1 : HUD2_ROW, yes;
    clear_rows(row, row + 1);
    color(0, row, 40, HC_WEAPON);
    print(0, row, "QUIT TO TITLE ?  Y = YES, OTHER KEY = NO");
    do { wait_vblank(); sfx_frame(); scan_input(); } while (any_input());
    do { wait_vblank(); sfx_frame(); scan_input(); } while (!any_input());
    yes = (kmat[3] & 0x02) != 0;
    do { wait_vblank(); sfx_frame(); scan_input(); } while (any_input());
    clear_rows(row, row + 1);
    color(0, row, 40, HC_LABEL);
    hud_reset();
    return yes;
}
#endif

/* returns 1 = completed, 0 = all players dead or quit */
static u8 play_level(void)
{
    u8 frame = 0, done = 0, v, alive;
    u16 t0, cycles;
#ifdef AUTOPLAY
    u8 prof_row = nplayers == 1 ? HUD_ROW + 2 : HUD2_ROW, show_prof = 0;
    u16 t_red, t_spr;
#endif

    sprites_off();
    VIC.bgcolor0 = COL_BG;
    clear_rows(0, 25);
    views_init();
    for (v = 0; v < nplayers; v++) {
        camx[v] = PX(pl_x[v]) - 160;
        camy[v] = PX(pl_y[v]) - view_rows[v] * 4;
        drawn_cx[v] = 255;
    }
    ticks = 0;
    hud_reset();
    for (;;) {
        wait_vblank();
        sfx_frame();
        if (++frame & 1) continue;      /* game tick every 2nd frame = 25 Hz */

        t0 = timer_read();
        read_input();
#ifndef AUTOPLAY
        if (key_stop && ask_quit()) return 0;   /* RUN/STOP: quit? */
#endif
        game_tick();
#ifdef AUTOPLAY
        t_red = timer_read();
#endif
        for (v = 0; v < nplayers; v++) camera_update(v);
        redraw_if_needed();
#ifdef AUTOPLAY
        t_spr = timer_read();
        t_red -= t_spr;
#endif
        for (v = 0; v < nplayers; v++) game_sprites(v);
#ifdef AUTOPLAY
        t_spr -= timer_read();
#endif
        ticks++;
        cycles = t0 - timer_read();
#ifdef AUTOPLAY
        /* cycles of the tick, then player, sight, enemies, projectiles, HUD.
         * Split screen: shown every other second in the HUD row. */
        if (ticks % 25 == 0) {
            extern u16 prof[4];
            u16 td = timer_read();
            show_prof = nplayers == 1 || ((ticks / 25) & 1);
            if (!show_prof) {
                hud_reset();
                clear_rows(HUD2_ROW, HUD2_ROW + 1);
            } else {
                print_num(0, prof_row, cycles, 5);
                print_num(6, prof_row, prof[0], 5);
                print_num(12, prof_row, prof[1], 5);
                print_num(18, prof_row, prof[2], 5);
                print_num(24, prof_row, prof[3], 5);
                print_num(30, prof_row, (u16)(td - timer_read()), 5);
                if (nplayers == 1) {
                    print_num(0, HUD_ROW + 1, t_red, 5);
                    print_num(6, HUD_ROW + 1, t_spr, 5);
                }
            }
        }
        if (!done && (nplayers == 1 || !show_prof)) {
            if (nplayers == 1) hud1(); else hud2();
        }
#else
        (void)cycles;
        if (!done) {
            if (nplayers == 1) hud1(); else hud2();
        }
#endif
        if (edead_count >= ne && !done) {
            done = 1;
            if (nplayers == 1) {
                clear_rows(HUD_ROW + 2, HUD_ROW + 3);
                color(0, HUD_ROW + 2, 40, HC_WEAPON);
                print(10, HUD_ROW + 2, "MISSION COMPLETED");
            }
            else {
                clear_rows(HUD2_ROW, HUD2_ROW + 1);
                color(0, HUD2_ROW, 40, HC_WEAPON);
                print(11, HUD2_ROW, "MISSION COMPLETED");
            }
        }
        if (done && ++done > 75) return 1;
        alive = 0;
        for (v = 0; v < nplayers; v++)
            if (pl_dead[v] <= 60) alive = 1;
        if (!alive) return 0;
    }
}

/* menus, shop and the main loop are not time critical: compile them small */
#pragma codesize (push, 100)

/* ------------------------------------------------------------------ */
/* shop between levels (linuxTK screens.c): 11 weapons, 9 ammo types and
 * the shield, then DONE. Up/down select, fire or right buys, the weapon key
 * (Q, C=, right SHIFT) or left sells, RUN/STOP or DONE leaves.             */

#ifndef AUTOPLAY
#define SHOP_W     (NWEAPONS - 1)          /* weapons 1..11 (no fist) */
#define SHOP_SH    (SHOP_W + NBULLETS - 1) /* ammo 1..9, then the shield */
#define SHOP_DONE  (SHOP_SH + 1)
#define SHOP_ROW0  2
#define SHIELD_COST 160
#define SHIELD_MAX  30

static u16 sell_price[SHOP_W + 1];         /* weapons, then the shield */

/* the item being looked at (shop_item): player sp, item i */
static u8 sp;
static const char *s_name;
static u16 s_cost, s_have;
static u8 s_full;
static s16 *s_ammo;                        /* ammo: pl_bullets entry */
static const B64 *s_bt;                    /* ammo: its type */

static void shop_item(u8 i)
{
    if (i < SHOP_W) {
        s_name = weapon64[i + 1].name;
        s_cost = weapon64[i + 1].cost;
        s_full = s_have = pl_weapons[sp][i + 1];
    } else if (i < SHOP_SH) {
        s_bt = &bullet64[i - SHOP_W + 1];
        s_ammo = &pl_bullets[sp][i - SHOP_W];
        s_name = s_bt->name;
        s_cost = s_bt->cost;
        s_have = *s_ammo / s_bt->mul;
        s_full = s_have >= s_bt->max;
    } else {
        s_name = "SHIELD";
        s_cost = SHIELD_COST + pl_shield[sp] * 15;
        s_have = pl_shield[sp];
        s_full = s_have >= SHIELD_MAX;
    }
}

static void do_prices(void)
{
    u8 i;
    u16 c;
    for (i = 0; i <= SHOP_W; i++) {
        c = i < SHOP_W ? weapon64[i + 1].cost : SHIELD_COST;
        sell_price[i] = c * 4 / 5 - rnd() % (c / 2);
    }
}

static u8 shop_buy(u8 i)
{
    shop_item(i);
    if (s_full || pl_cash[sp] < s_cost) return 0;
    pl_cash[sp] -= s_cost;
    if (i < SHOP_W) pl_weapons[sp][i + 1] = 1;
    else if (i < SHOP_SH) *s_ammo += s_bt->mul;
    else pl_shield[sp]++;
    return 1;
}

static u8 shop_sell(u8 i)
{
    u16 v;
    shop_item(i);
    if (!s_have && !(i >= SHOP_W && i < SHOP_SH && *s_ammo > 0)) return 0;   /* gas: < 1 unit */
    if (i < SHOP_W) {
        pl_weapons[sp][i + 1] = 0;
        v = sell_price[i];
    } else if (i < SHOP_SH) {
        *s_ammo -= s_bt->mul;
        if (*s_ammo < 0) *s_ammo = 0;
        v = s_cost;
    } else {
        pl_shield[sp]--;
        v = sell_price[SHOP_W] + 15 * pl_shield[sp] / 2;
    }
    pl_cash[sp] = pl_cash[sp] > 65000u - v ? 65000u : pl_cash[sp] + v;
    return 1;
}

/* colour of a line: cursor yellow, full green, affordable white, else blue */
static u8 line_col(u8 i, u8 sel)
{
    if (i == sel) return HC_WEAPON;
    if (i == SHOP_DONE) return HC_LABEL;
    shop_item(i);
    return s_full ? HC_OK : pl_cash[sp] >= s_cost ? HC_LABEL : HC_DIM;
}

/* the shop screen runs from the overlay area $0334-$07FF (src/ovl.s) */
#pragma code-name (push, "OVERLAY")
#pragma rodata-name (push, "OVERLAY")

/* number without leading zeros, right aligned in w characters */
static void num_sp(u8 x, u8 y, u16 v, u8 w)
{
    u16 o = y * 40 + x;
    print_num(x, y, v, w);
    while (--w && SCREEN_A[o] == '0') SCREEN_A[o] = SCREEN_B[o] = ' ', o++;
}

/* one line: > NAME             COST  HAVE FULL */
static void shop_line(u8 i, u8 sel)
{
    u8 y = SHOP_ROW0 + i;
    print(0, y, "                                        ");
    if (i == SHOP_DONE) print(2, y, "DONE");
    else {
        shop_item(i);
        print(2, y, s_name);
        num_sp(20, y, s_cost, 5);
        if (i < SHOP_W) {
            if (s_have) print(28, y, "OWNED");
        } else {
            if (s_have) num_sp(28, y, s_have, 3);
            if (s_full) print(32, y, "FULL");
        }
    }
    if (i == sel) print(0, y, ">");
    color(0, y, 40, line_col(i, sel));
}

static void shop_cash(void)
{
    print(28, 0, "CASH $");
    print_num(34, 0, pl_cash[sp], 5);
}

static void shop(u8 p)
{
    u8 sel = 0, old, j, prev = 0, pr, rep = 0, changed, i;
    sp = p;
    text_screen();
    print(0, 0, nplayers == 1 ? "SHOP" : p ? "SHOP - PLAYER 2" : "SHOP - PLAYER 1");
    color(0, 0, 40, HC_WEAPON);
    print(21, 1, "COST  HAVE");
    color(0, 1, 40, HC_DIM);
    center(24, "FIRE BUY  WEAPON KEY SELL  STOP DONE");
    color(0, 24, 40, HC_DIM);
    shop_cash();
    for (i = 0; i <= SHOP_DONE; i++) shop_line(i, sel);
    do { wait_vblank(); scan_input(); } while (any_input());
    for (;;) {
        wait_vblank();
        sfx_frame();
        read_input();
        if (key_stop) break;
        j = joy[nplayers == 1 ? 0 : p];
        pr = (u8)(j & ~prev);
        /* up/down repeat while held */
        if (j & (J_UP | J_DOWN)) {
            if (++rep > 15) { rep = 11; pr |= j & (J_UP | J_DOWN); }
        } else rep = 0;
        prev = j;
        if (!pr) continue;
        old = sel;
        changed = 0;
        if ((pr & J_UP) && sel > 0) sel--;
        if ((pr & J_DOWN) && sel < SHOP_DONE) sel++;
        if (pr & (J_FIRE | J_RIGHT)) {
            if (sel == SHOP_DONE) break;
            changed = shop_buy(sel);
            sfx(changed ? SFX_PICKUP : SFX_HIT);
        }
        if ((pr & (J_CHANGE | J_LEFT)) && sel < SHOP_DONE && (changed = shop_sell(sel)) != 0)
            sfx(SFX_PICKUP);
        if (changed) {
            /* only the selected line's text changes; the cash may change
             * which other items are affordable, so recolour those (no text) */
            shop_cash();
            shop_line(sel, sel);
            for (i = 0; i < SHOP_DONE; i++)
                if (i != sel) color(0, SHOP_ROW0 + i, 40, line_col(i, sel));
        } else if (sel != old) {
            shop_line(old, sel);
            shop_line(sel, sel);
        }
    }
    do { wait_vblank(); scan_input(); } while (any_input());
}

#pragma rodata-name (pop)
#pragma code-name (pop)
#endif

/* kills and cash of each player */
static void scores(u8 y)
{
    u8 p;
    for (p = 0; p < nplayers; p++, y += 2) {
        print(6, y, "PLAYER");
        print_num(13, y, p + 1, 1);
        print_num(16, y, pl_kills[p], 4);
        print(21, y, "KILLS");
        print_num(28, y, pl_cash[p], 5);
    }
}

static const char *const skill_name[NSKILLS] = { "  EASY  ", " NORMAL ", "  HARD  ", "  GOD   " };

static void show_skill(void)
{
    print(9, 13, "DIFFICULTY <");
    print(21, 13, skill_name[skill]);
    print(29, 13, ">");
}

/* title: 1 (or fire) = one player, 2 = two players in split screen.
 * Joystick/A D left-right or F1 changes the difficulty. */
static void title(void)
{
    u8 prev = 0, j;
    text_screen();
    center(3, "THE ULTIMATE");
    center(5, "TAPAN KAIKKI 64");
    center(9, "1  ONE PLAYER");
    center(11, "2  TWO PLAYERS");
    show_skill();
    center(16, "PLAYER 1  JOY 2 OR W A S D");
    center(17, "SHIFT FIRE  Q WEAPON");
    center(19, "PLAYER 2  JOY 1 OR - @ P *");
    center(20, "+ FIRE  SHIFT WEAPON");
    center(23, "BASED ON TK BY ERROR FREE");
#ifdef AUTOPLAY
    wait_fire();
#ifdef AUTOPLAY_2P
    nplayers = 2;
#else
    nplayers = 1;
#endif
#else
    nplayers = 0;
    do {
        wait_vblank();
        scan_input();
        j = (u8)((input[0] | input[1]) & (J_LEFT | J_RIGHT));
        if (key_f1) j |= J_RIGHT;
        if (j && !prev) {
            if (j & J_LEFT) skill = skill ? skill - 1 : NSKILLS - 1;
            else skill = skill < NSKILLS - 1 ? skill + 1 : 0;
            show_skill();
        }
        prev = j;
        if (key_2) nplayers = 2;
        else if (key_1 || fire_pressed()) nplayers = 1;
    } while (!nplayers);
    wait_release();
#endif
}

static void new_game(void)
{
    u8 i, p;
    for (p = 0; p < MAXPL; p++) {
        for (i = 0; i < NWEAPONS; i++) pl_weapons[p][i] = 0;
        for (i = 0; i < NBULLETS; i++) pl_bullets[p][i] = 0;
        pl_weapons[p][0] = 1;
        pl_bullets[p][0] = 10;
#ifdef AUTOPLAY
        for (i = 0; i < NWEAPONS; i++) pl_weapons[p][i] = 1;
        for (i = 0; i < NBULLETS; i++) pl_bullets[p][i] = 100;
#endif
        pl_cash[p] = 900;
        pl_shield[p] = 0;
        pl_kills[p] = 0;
    }
    level = 0;
    episode = 0;
}

int main(void)
{
#ifndef AUTOPLAY
    u8 p;
#endif
    device = getcurrentdevice();
    if (device < 8) device = 8;
    /* $0334-$07FF holds the overlay code: the default screen at $0400 must not
     * be written. Blank it, load the charset, then show the loading text from
     * the game's own screen in VIC bank 3. */
    VIC.ctrl1 &= 0xEF;
    VIC.bordercolor = 0;
    if (!load("tkchr", CHARSET)) fail("can't load graphics");
    copy_rom_font();
    *(u8 *)0xDD00 = *(u8 *)0xDD00 & 0xFC;
    VIC.addr = D018_A;
    text_screen();
    center(12, "LOADING...");
    VIC.ctrl1 |= 0x10;
    if (!load("tkspre", (void *)0xE000) || !load("tksprd", levelbuf))
        fail("can't load graphics");
    copy_under_io();
    timer_init();
    video_init();
#ifdef SHOPTEST
    /* make test-shop: the shop screen with some cash and items */
    nplayers = 1;
    new_game();
    pl_cash[0] = 5000;
    pl_weapons[0][1] = pl_weapons[0][3] = 1;
    pl_bullets[0][0] = 120;
    pl_shield[0] = 2;
    do_prices();
    shop(0);
#endif

    for (;;) {
        title();
        new_game();
        for (;;) {
            if (level >= episode_levels[episode] || !level_load()) break;
            game_level_init();
            level_intro();
#ifndef AUTOPLAY
            if (level > 0) {
                do_prices();
                for (p = 0; p < nplayers; p++) {
                    shop(p);
                    pl_energy[p] = pl_max_energy[p] = 1000 + pl_shield[p] * 100;
                    pl_curr[p] = 0;
                }
            }
#endif
            if (!play_level()) {
                text_screen();
                center(8, "GAME OVER");
                scores(12);
                wait_fire();
                break;
            }
            level++;
        }
        if (level >= episode_levels[episode]) {
            text_screen();
            center(8, "VICTORY !");
            scores(12);
            wait_fire();
        }
    }
    return 0;
}

#pragma codesize (pop)
