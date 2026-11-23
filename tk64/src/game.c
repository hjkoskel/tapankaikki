/* game.c - gameplay for the C64 port: player, weapons, enemies, projectiles,
 * explosions, crates and gore decals. Logic follows linuxTK (player.c,
 * enemy.c, bullet.c) with the simplifications of c64portplan.md section 6:
 * hitscan guns, area explosions, sleeping far-away enemies, round-robin
 * line of sight, gore as map tiles. */
#pragma static-locals(on)

#include <c64.h>
#include <string.h>
#include "defs.h"

/* ------------------------------------------------------------------ */
/* state                                                               */

/* players: arrays indexed by player number. The macros below select the
 * current player cp, so the player code is the same as for one player. */
u8 nplayers = 1, cp, joy[MAXPL];
#ifndef SKILL_START
#define SKILL_START SKILL_NORMAL      /* test builds: -DSKILL_START=3 */
#endif
u8 skill = SKILL_START;
s16 pl_x[MAXPL], pl_y[MAXPL], pl_energy[MAXPL], pl_max_energy[MAXPL];
s16 pl_bullets[MAXPL][NBULLETS];
u8 pl_angle[MAXPL], pl_dead[MAXPL];
u8 pl_weapons[MAXPL][NWEAPONS], pl_curr[MAXPL], pl_shield[MAXPL];
u16 pl_cash[MAXPL], pl_kills[MAXPL];
char pl_msg[MAXPL][21];
u8 pl_msg_time[MAXPL], pl_msg_seq[MAXPL] = { 1, 1 };   /* odd: 0 means "no message" in the HUD */
static u8 pl_moving[MAXPL], pl_walk[MAXPL], pl_fire[MAXPL], pl_hit[MAXPL];
static u8 pl_turn[MAXPL], pl_load[MAXPL], pl_shoot_cnt[MAXPL], pl_push[MAXPL], pl_pushang[MAXPL];
static u8 pl_burn[MAXPL], pl_prev_joy[MAXPL], pl_tx[MAXPL], pl_ty[MAXPL];
static s16 pl_dirx[MAXPL], pl_diry[MAXPL], pl_pxl[MAXPL], pl_pyl[MAXPL];

#define px          pl_x[cp]
#define py          pl_y[cp]
#define pangle      pl_angle[cp]
#define pmoving     pl_moving[cp]
#define pwalk       pl_walk[cp]
#define pfire       pl_fire[cp]
#define pdead       pl_dead[cp]
#define phit        pl_hit[cp]
#define penergy     pl_energy[cp]
#define pmax_energy pl_max_energy[cp]
#define pweapons    pl_weapons[cp]
#define pbullets    pl_bullets[cp]
#define pcurr       pl_curr[cp]
#define pshield     pl_shield[cp]
#define pturn       pl_turn[cp]
#define pload       pl_load[cp]
#define pshoot_cnt  pl_shoot_cnt[cp]
#define ppush       pl_push[cp]
#define ppushang    pl_pushang[cp]
#define pburn       pl_burn[cp]
#define prev_joy    pl_prev_joy[cp]
#define pdirx       pl_dirx[cp]
#define pdiry       pl_diry[cp]
#define ppx         pl_pxl[cp]       /* pixel position cache of px/py */
#define ppy         pl_pyl[cp]
#define ptx         pl_tx[cp]        /* tile of the actor centre */
#define pty         pl_ty[cp]
#define msg         pl_msg[cp]
#define msg_time    pl_msg_time[cp]

u8 ne, edead_count;
s16 ex[MAXE], ey[MAXE], eenergy[MAXE];
u8 eang[MAXE], etang[MAXE], etype[MAXE], ewalk[MAXE], eload[MAXE];
u8 efire[MAXE], eflags[MAXE], eburn[MAXE], epush[MAXE], epushang[MAXE];
static u8 eanim[MAXE];
static s16 epx[MAXE], epy[MAXE];      /* pixel position caches of ex/ey */
static u8 etx[MAXE], ety[MAXE];       /* tile of the actor centre */
static u8 etgt[MAXE];                 /* player the enemy is after */
static u8 elast[MAXE];                /* player who hurt it last (kill credit) */
static u8 shooter;                    /* who is doing damage now: player, or 255 */
static s16 walkv[3][16][2];           /* walking vectors per speed class and direction */

static void e_sync(u8 i)
{
    epx[i] = PX(ex[i]);
    epy[i] = PX(ey[i]);
    etx[i] = (u8)((epx[i] + 11) >> 4);
    ety[i] = (u8)((epy[i] + 11) >> 4);
}

static void p_sync(void)
{
    ppx = PX(px);
    ppy = PX(py);
    ptx = (u8)((ppx + 11) >> 4);
    pty = (u8)((ppy + 11) >> 4);
}

u8 prj_on[MAXP], prj_type[MAXP], prj_owner[MAXP];
s16 prj_x[MAXP], prj_y[MAXP], prj_dx[MAXP], prj_dy[MAXP];
u16 prj_time[MAXP];
u8 exp_on[MAXX], exp_frame[MAXX];
s16 exp_x[MAXX], exp_y[MAXX];
static u8 exp_kind[MAXX];

static u8 crate_n, crate_tx[MAXC], crate_ty[MAXC], crate_kind[MAXC], crate_item[MAXC], crate_under[MAXC];


static u8 los_next;            /* round robin line of sight */
static u8 tick4;
static u8 cand[MAXE], ncand;   /* hitscan candidates */

static const u8 player_col[MAXPL] = { 10, 14 };   /* light red, light blue */

/* enemy is far from (every) player: asleep. Tile distance in byte math. */
#define FAR1(i, p) ((u8)(etx[i] - pl_tx[p] + 15) > 30 || (u8)(ety[i] - pl_ty[p] + 11) > 22)
#define FAR(i) (FAR1(i, 0) && (nplayers == 1 || FAR1(i, 1)))

/* trace() results */
#define HIT_NONE 255
#define HIT_PL0  254            /* player p = HIT_PL0 - p */
#define HIT_PL1  253
#define ROT 10                 /* enemy turn speed: 9 deg/tick @ 40 Hz = 14.4 deg @ 25 Hz */
#define SPEED_FX 41

/* ------------------------------------------------------------------ */
/* small helpers                                                       */

static u16 rs = 0xACE1;
u8 rnd(void)
{
    rs ^= rs << 7;
    rs ^= rs >> 9;
    rs ^= rs << 8;
    return (u8)rs;
}

static s16 iabs(s16 v) { return v < 0 ? -v : v; }

/* v * sin(a), v small (fits 16 bit after * 64) */
static s16 msin(s16 v, u8 a) { return (v * sintab[a]) >> 6; }
static s16 mcos(s16 v, u8 a) { return (v * sintab[(u8)(a + 64)]) >> 6; }

static u16 dist(s16 dx, s16 dy)
{
    dx = iabs(dx);
    dy = iabs(dy);
    return dx > dy ? dx + ((dy * 3) >> 3) : dy + ((dx * 3) >> 3);
}

/* direction of (dx, dy): dx = sin, dy = cos (same as linuxTK get_angle) */
static u8 angle_to(s16 dx, s16 dy)
{
    u16 ax = iabs(dx), ay = iabs(dy);
    u8 a;
    if (!ax && !ay) return 0;
    if (ax <= ay) a = atan_tab[(ax << 5) / ay];
    else a = (u8)(64 - atan_tab[(ay << 5) / ax]);
    if (dy < 0) a = (u8)(128 - a);
    if (dx < 0) a = (u8)(0 - a);
    return a;
}

static void message(const char *s)
{
    strncpy(msg, s, 20);
    msg[20] = 0;
    msg_time = 50;
    pl_msg_seq[cp] += 2;   /* tells the HUD there is a new message */
}

static u8 tile_at(s16 x, s16 y)
{
    u8 tx, ty;
    if (x < 0 || y < 0) return 0x80;
    tx = (u8)(x >> 4);
    ty = (u8)(y >> 4);
    if (tx >= map_w || ty >= map_h) return 0x80;
    return map[rowoff[ty] + tx];
}

/* gore: replace a floor tile (not crates or decals of higher rank) */
static void decal(s16 x, s16 y, u8 id)
{
    u8 t = tile_at(x, y);
    if (t & 0x80) return;
    if (t >= CRATE_TILE) return;
    if (t >= DECAL_BLOOD && t >= id) return;
    stamp_tile((u8)(x >> 4), (u8)(y >> 4), id);
}

/* ------------------------------------------------------------------ */
/* movement with wall collision (box 22x22, test points as linuxTK x0.8) */

static s16 mx, my;             /* actor being moved, 12.4 */

static void move_box(s16 dx, s16 dy)
{
    s16 nx = mx + dx, ny = my + dy, rx = PX(nx), ry = PX(ny);
    if (ny < my && !wall(rx + 8, ry + 5) && !wall(rx + 14, ry + 5)) my = ny;
    if (ny > my && !wall(rx + 8, ry + 17) && !wall(rx + 14, ry + 17)) my = ny;
    if (nx < mx && !wall(rx + 5, ry + 8) && !wall(rx + 5, ry + 14)) mx = nx;
    if (nx > mx && !wall(rx + 17, ry + 8) && !wall(rx + 17, ry + 14)) mx = nx;
}

/* is (x,y) (pixels) inside the hit box of an actor at (ax,ay) (12.4)? */
static u8 in_box(s16 x, s16 y, s16 ax, s16 ay)
{
    ax = PX(ax);
    ay = PX(ay);
    return x > ax + 4 && x < ax + 18 && y > ay + 4 && y < ay + 18;
}

/* ------------------------------------------------------------------ */
/* damage                                                              */

static void explode(s16 x, s16 y, u8 size);

static void player_damage(s16 dmg, u8 push, u8 ang)
{
    if (pdead || skill == SKILL_GOD) return;
    /* difficulty: damage from enemies (and burning) x0.5 easy, x1.5 hard */
    if (shooter >= MAXPL) {
        if (skill == SKILL_EASY) dmg = (dmg + 1) >> 1;
        else if (skill == SKILL_HARD) dmg += dmg >> 1;
    }
    penergy -= dmg;
    phit = 8;
    if (push * 10 > ppush) {
        ppush = (u8)(push * 10);
        ppushang = ang;
    }
    if (rnd() < 64) decal(ppx + 11, ppy + 11, DECAL_BLOOD);
    if (penergy <= 0) {
        penergy = 0;
        pdead = 1;
        sfx(SFX_DEATH);
        decal(ppx + 11, ppy + 11, DECAL_BLOOD2);
    }
}

/* damage to player p (not necessarily the current one) */
static void hurt_player(u8 p, s16 dmg, u8 push, u8 ang, u8 burn)
{
    u8 keep = cp;
    cp = p;
    player_damage(dmg, push, ang);
    pburn += burn;
    cp = keep;
}

static void enemy_die(u8 i)
{
    s16 x = epx[i] + 11, y = epy[i] + 11;
    u8 t = etype[i], k = elast[i];
    eflags[i] |= EF_DEAD;
    edead_count++;
    pl_kills[k]++;
    pl_cash[k] += enemy64[t].reward;
    if (enemy64[t].explo_death) {
        explode(x, y, 2);
        decal(x, y, DECAL_GIBS);
    } else if (eburn[i]) {
        decal(x, y, DECAL_SCORCH);
        sfx(SFX_EXPLO);
    } else {
        decal(x, y, DECAL_GIBS);
        decal(x + 12, y + 4, DECAL_BLOOD2);
        decal(x - 10, y - 6, DECAL_BLOOD);
        sfx(SFX_DEATH);
    }
}

static void enemy_damage(u8 i, s16 dmg, u8 push, u8 ang)
{
    if (eflags[i] & EF_DEAD) return;
    if (shooter < MAXPL) elast[i] = shooter;
    eenergy[i] -= dmg;
    if (push * 10 > epush[i]) {
        epush[i] = (u8)(push * 10);
        epushang[i] = ang;
    }
    etang[i] = (u8)(ang + 128);                 /* turn towards the shooter */
    if (rnd() < 80) decal(epx[i] + 11, epy[i] + 11, DECAL_BLOOD);
    if (eenergy[i] <= 0) enemy_die(i);
}

/* ------------------------------------------------------------------ */
/* hitscan from the shooter's centre (x0, y0) along angle a; returns HIT_NONE,
 * HIT_PL0 - player or an enemy. The start point is tested first, so a target
 * standing on top of the shooter is hit too. Reach = range + 12 px from the
 * centre (= range from the muzzle). Players can hit each other, like the original. */

static s16 hit_x, hit_y;

static u8 trace(s16 x0, s16 y0, u8 a, u8 range, u8 from_enemy)
{
    s16 x = FX(x0), y = FX(y0), sx = sintab[a], sy = sintab[(u8)(a + 64)], rx, ry;
    u8 d, i, k, p, self = from_enemy ? 255 : cp, reach = (u8)(range + 12);
    if (!from_enemy) {
        ncand = 0;
        for (i = 0; i < ne; i++)
            if (!(eflags[i] & EF_DEAD) && iabs(epx[i] + 11 - x0) < reach + 8 && iabs(epy[i] + 11 - y0) < reach + 8)
                cand[ncand++] = i;
    }
    for (d = 0; d < reach; d += 4, x += sx, y += sy) {
        rx = PX(x);
        ry = PX(y);
        hit_x = rx;
        hit_y = ry;
        if (wall(rx, ry)) return HIT_NONE;
        for (p = 0; p < nplayers; p++)
            if (p != self && !pl_dead[p] && in_box(rx, ry, pl_x[p], pl_y[p])) return (u8)(HIT_PL0 - p);
        if (!from_enemy)
            for (k = 0; k < ncand; k++) {
                i = cand[k];
                if (!(eflags[i] & EF_DEAD) && in_box(rx, ry, ex[i], ey[i])) return i;
            }
    }
    return HIT_NONE;
}

/* ------------------------------------------------------------------ */
/* projectiles and explosions                                          */

static void effect(s16 x, s16 y, u8 kind)
{
    u8 i;
    for (i = 0; i < MAXX; i++)
        if (!exp_on[i]) {
            exp_on[i] = 1;
            exp_frame[i] = 0;
            exp_kind[i] = kind;
            exp_x[i] = x;
            exp_y[i] = y;
            return;
        }
}

static void explode(s16 x, s16 y, u8 size)
{
    u8 i, p, r = (u8)(size * 16 + 11);
    u16 d;
    effect(x, y, 0);
    decal(x, y, DECAL_SCORCH);
    sfx(SFX_EXPLO);
    for (i = 0; i < ne; i++) {
        if (eflags[i] & EF_DEAD) continue;
        d = dist(epx[i] + 11 - x, epy[i] + 11 - y);
        if (d < r) enemy_damage(i, (s16)(200 + (r - d) * 10), 8, angle_to(epx[i] + 11 - x, epy[i] + 11 - y));
    }
    for (p = 0; p < nplayers; p++) {
        d = dist(pl_pxl[p] + 11 - x, pl_pyl[p] + 11 - y);
        if (d < r) hurt_player(p, (s16)(200 + (r - d) * 10), 8, angle_to(pl_pxl[p] + 11 - x, pl_pyl[p] + 11 - y), 0);
    }
}

static void spawn_projectile(u8 bt, s16 x, s16 y, u8 a, u8 owner)
{
    u8 i;
    for (i = 0; i < MAXP; i++)
        if (!prj_on[i]) {
            prj_on[i] = 1;
            prj_type[i] = bt;
            prj_owner[i] = owner;
            prj_x[i] = FX(x) - FX(3);
            prj_y[i] = FX(y) - FX(3);
            prj_dx[i] = bullet64[bt].kind == K_PROJ ? msin(164, a) : 0;   /* 10.2 px/tick */
            prj_dy[i] = bullet64[bt].kind == K_PROJ ? mcos(164, a) : 0;
            prj_time[i] = bullet64[bt].time;
            return;
        }
}

static void update_projectiles(void)
{
    u8 i, j, kind, boom;
    s16 x, y;
    /* prj_owner: player number, or MAXPL for enemies */
    for (i = 0; i < MAXP; i++) {
        if (!prj_on[i]) continue;
        kind = bullet64[prj_type[i]].kind;
        boom = 0;
        prj_x[i] += prj_dx[i];
        prj_y[i] += prj_dy[i];
        x = PX(prj_x[i]) + 3;
        y = PX(prj_y[i]) + 3;
        if (kind == K_PROJ && wall(x, y)) {
            prj_x[i] -= prj_dx[i];
            prj_y[i] -= prj_dy[i];
            boom = 1;
        }
        if (kind == K_PROJ || (kind == K_MINE && bullet64[prj_type[i]].time - prj_time[i] > 16)) {
            for (j = 0; j < ne && !boom; j++)
                if (!(eflags[j] & EF_DEAD) && in_box(x, y, ex[j], ey[j])) boom = 1;
            for (j = 0; j < nplayers && !boom; j++)
                if (!pl_dead[j] && in_box(x, y, pl_x[j], pl_y[j])) boom = 1;
        }
        if (--prj_time[i] == 0) {
            if (kind != K_MINE) boom = 1;
            else prj_on[i] = 0;
        }
        if (boom) {
            prj_on[i] = 0;
            shooter = prj_owner[i] < MAXPL ? prj_owner[i] : 255;
            explode(PX(prj_x[i]) + 3, PX(prj_y[i]) + 3, bullet64[prj_type[i]].explos);
        }
    }
    for (i = 0; i < MAXX; i++)
        if (exp_on[i] && ++exp_frame[i] >= (exp_kind[i] ? 3 : 8)) exp_on[i] = 0;
}

/* ------------------------------------------------------------------ */
/* firing (player and enemies)                                         */

/* x, y = shooter's centre (pixels) */
static void fire_weapon(u8 w, s16 x, s16 y, u8 a, u8 from_enemy)
{
    const W64 *wp = &weapon64[w];
    const B64 *bt = &bullet64[wp->btype];
    u8 n, r, hit = HIT_NONE;
    shooter = from_enemy ? 255 : cp;
    if (bt->kind == K_HIT || bt->kind == K_FLAME) {
        for (n = 0; n < wp->amount; n++) {
            r = (u8)(a + (wp->spread >> 1) - rnd() % wp->spread);
            hit = trace(x, y, r, bt->range, from_enemy);
            if (hit == HIT_NONE) continue;
            if (hit >= HIT_PL1) {
                hurt_player((u8)(HIT_PL0 - hit), bt->power, bt->push, r, bt->kind == K_FLAME ? 10 : 0);
            } else {
                enemy_damage(hit, bt->power, bt->push, r);
                if (bt->kind == K_FLAME) eburn[hit] += 10;
            }
        }
        if (bt->kind == K_FLAME) effect(hit_x, hit_y, 1);
    } else {
        spawn_projectile(wp->btype, x + msin(20, a), y + mcos(20, a), a, from_enemy ? MAXPL : cp);
    }
    sfx(wp->sfx);
}

/* ------------------------------------------------------------------ */
/* player                                                              */

static void set_dir(void)
{
    pdirx = msin(SPEED_FX, pangle);
    pdiry = mcos(SPEED_FX, pangle);
}

static void next_weapon(void)
{
    do pcurr = (u8)((pcurr + 1) % NWEAPONS); while (!pweapons[pcurr]);
    pload = 0;
    message(weapon64[pcurr].name);
}

static void take_crate(void)
{
    s16 cx = ppx + 11, cy = ppy + 11;
    u8 t = tile_at(cx, cy), i, tx = (u8)(cx >> 4), ty = (u8)(cy >> 4), b, taken = 0;
    if (t < CRATE_TILE || (t & 0x80)) return;
    for (i = 0; i < crate_n; i++)
        if (crate_tx[i] == tx && crate_ty[i] == ty) break;
    if (i == crate_n) return;
    b = crate_item[i];
    if (crate_kind[i] == 0 && !pweapons[b + 1]) {
        pweapons[b + 1] = 1;
        message(weapon64[b + 1].name);
        taken = 1;
    } else if (crate_kind[i] == 1 && pbullets[b] < (s16)(bullet64[b + 1].max * bullet64[b + 1].mul)) {
        pbullets[b] += bullet64[b + 1].crate;
        if (pbullets[b] > (s16)(bullet64[b + 1].max * bullet64[b + 1].mul)) pbullets[b] = bullet64[b + 1].max * bullet64[b + 1].mul;
        message(bullet64[b + 1].name);
        taken = 1;
    } else if (crate_kind[i] == 2 && penergy < pmax_energy) {
        penergy += 400;
        if (penergy > pmax_energy) penergy = pmax_energy;
        message("ENERGY");
        taken = 1;
    }
    if (taken) {
        stamp_tile(tx, ty, crate_under[i]);
        crate_tx[i] = 255;
        sfx(SFX_PICKUP);
    }
}

static void player_tick(u8 joy)
{
    s8 turn = 0;
    const W64 *wp;
    if (phit) phit--;
    if (pfire) pfire--;
    if (pburn) {
        pburn--;
        shooter = 255;
        player_damage(6, 0, 0);
    }
    if (ppush >= 10) {
        mx = px; my = py;
        move_box(msin(ppush * 2, ppushang), mcos(ppush * 2, ppushang));   /* push/10 px -> fx */
        px = mx; py = my;
        p_sync();
    }
    if (ppush) ppush = ppush > 3 ? ppush - 3 : 0;
    if (pdead) {
        if (pdead < 255) pdead++;
        return;
    }
    if (joy & J_LEFT) turn = 1;
    if (joy & J_RIGHT) turn = -1;
    if (turn) {
        if (pturn == 0) {
            pangle = (u8)(pangle + turn * 16);
            pturn = (pangle & 16) ? 2 : 1;
            set_dir();
        }
        pturn--;
    } else pturn = 0;
    pmoving = 0;
    mx = px; my = py;
    if (joy & J_UP) { move_box(pdirx, pdiry); pmoving = 1; }
    if (joy & J_DOWN) { move_box(-(pdirx * 3 >> 2), -(pdiry * 3 >> 2)); pmoving = 1; }
    px = mx; py = my;
    p_sync();
    if (pmoving) {
        pwalk++;
        take_crate();
    }
    /* weapon change key (C= / right SHIFT, see main.c) */
    if ((joy & J_CHANGE) && !(prev_joy & J_CHANGE)) next_weapon();
    prev_joy = joy;

    wp = &weapon64[pcurr];
    if (pload < wp->load) pload++;
    if ((joy & J_FIRE) && pload >= wp->load) {
        if (pcurr && pbullets[wp->btype - 1] <= 0) {
            pcurr = 0;
            message("OUT OF AMMO");
        } else {
            pload = 0;
            pfire = 3;
            if (pcurr) pbullets[wp->btype - 1] -= bullet64[wp->btype].mul > 1 ? (pshoot_cnt % bullet64[wp->btype].mul == 0) : 1;
            fire_weapon(pcurr, ppx + 11, ppy + 11, pangle, 0);
        }
    }
    pshoot_cnt = (joy & J_FIRE) ? pshoot_cnt + 1 : 0;
}

/* ------------------------------------------------------------------ */
/* enemies                                                             */

/* line of sight in 1/16 px units (u16): the tile index is simply the high
 * byte, so no shifts are needed; 16 steps of 8 px = 128 px range */
static u8 sees_player(u8 i)
{
    u16 x = (u16)(epx[i] + 11) << 4, y = (u16)(epy[i] + 11) << 4;
    u16 bx0 = (u16)(ppx + 4) << 4, bx1 = (u16)(ppx + 18) << 4, by0 = (u16)(ppy + 4) << 4, by1 = (u16)(ppy + 18) << 4;
    u8 a = angle_to(ppx - epx[i], ppy - epy[i]), k, tx, ty;
    s8 diff = (s8)(a - eang[i]);
    s16 sx = sintab[a] << 1, sy = sintab[(u8)(a + 64)] << 1;
    if (pdead || diff <= -64 || diff >= 64) return 0;
    for (k = 0; k < 16; k++) {
        x += sx;
        y += sy;
        tx = (u8)(x >> 8);
        ty = (u8)(y >> 8);
        if (tx >= map_w || ty >= map_h || (map[rowoff[ty] + tx] & 0x80)) return 0;
        if (x > bx0 && x < bx1 && y > by0 && y < by1) {
            etang[i] = a;
            return 1;
        }
    }
    return 0;
}

static void enemy_tick(u8 i)
{
    u8 t = etype[i], w = enemy64[t].weapon, dir, cls;
    s8 diff;
    s16 dx, dy;

    if (efire[i]) efire[i]--;
    if (eburn[i]) {
        eburn[i]--;
        shooter = 255;                         /* keeps the kill credit */
        enemy_damage(i, 6, 0, eang[i] + 128);
        if (eflags[i] & EF_DEAD) return;
    }
    if (epush[i] >= 10) {
        mx = ex[i]; my = ey[i];
        move_box(msin(epush[i] * 2, epushang[i]), mcos(epush[i] * 2, epushang[i]));
        ex[i] = mx; ey[i] = my;
        e_sync(i);
    }
    if (epush[i]) epush[i] = epush[i] > 3 ? epush[i] - 3 : 0;

    /* sleep when far from the players */
    if (FAR(i)) return;
    dx = ppx - epx[i];
    dy = ppy - epy[i];

    if (eflags[i] & EF_SEES) {
        etang[i] = angle_to(dx, dy);
        eflags[i] |= EF_SAW;
    } else if (eflags[i] & EF_SAW) {
        eflags[i] &= ~EF_SAW;                  /* lost sight: go where the player was */
        ewalk[i] = (u8)(dist(dx, dy) / 3);
    }
    /* turn towards target angle */
    diff = (s8)(etang[i] - eang[i]);
    if (diff > -ROT && diff < ROT) eang[i] = etang[i];
    else eang[i] = (u8)(eang[i] + (diff > 0 ? ROT : -ROT));

    if (!(eflags[i] & EF_SEES)) {
        if (etang[i] == eang[i] && !ewalk[i] && rnd() < 8) etang[i] = rnd();
        if (!ewalk[i] && rnd() < 4) ewalk[i] = (u8)(12 + (rnd() & 31));
    }
    if (ewalk[i]) {
        ewalk[i]--;
        dir = (u8)(((u8)(eang[i] + 8)) >> 4);         /* walk in the sprite direction */
        cls = enemy64[t].speed_fx == 41 ? 0 : enemy64[t].speed_fx == 61 ? 1 : 2;
        mx = ex[i]; my = ey[i];
        move_box(walkv[cls][dir][0], walkv[cls][dir][1]);
        if (!pdead && iabs(PX(mx) - ppx) < 18 && iabs(PX(my) - ppy) < 18) ewalk[i] = 0;   /* don't walk into the player */
        else { ex[i] = mx; ey[i] = my; e_sync(i); }
        eanim[i]++;
    }
    /* shoot */
    if (eload[i] < weapon64[w].load) eload[i]++;
    if (w && (eflags[i] & EF_SEES) && eang[i] == etang[i] && eload[i] >= weapon64[w].load) {
        eload[i] = 0;
        efire[i] = 2;
        fire_weapon(w, epx[i] + 11, epy[i] + 11, eang[i], 1);
        if (bullet64[weapon64[w].btype].kind == K_HIT && dist(dx, dy) > bullet64[weapon64[w].btype].range)
            ewalk[i] = 8;                        /* too far: walk closer */
    }
}

/* ------------------------------------------------------------------ */
/* level set-up                                                        */

static u8 free_floor(u8 tx, u8 ty, u8 keep)
{
    u8 t, p;
    if (tx == 0 || ty == 0 || tx >= map_w - 1 || ty >= map_h - 1) return 0;
    t = map[rowoff[ty] + tx];
    if ((t & 0x80) || t >= DECAL_BLOOD) return 0;
    for (p = 0; p < nplayers; p++)
        if (iabs((s16)tx - (s16)(pl_pxl[p] >> 4)) < keep && iabs((s16)ty - (s16)(pl_pyl[p] >> 4)) < keep) return 0;
    return 1;
}

static void place_crate(u8 kind, u8 item, u8 tx, u8 ty)
{
    if (crate_n >= MAXC) return;
    crate_tx[crate_n] = tx;
    crate_ty[crate_n] = ty;
    crate_kind[crate_n] = kind;
    crate_item[crate_n] = item;
    crate_under[crate_n] = map[rowoff[ty] + tx];
    map[rowoff[ty] + tx] = (u8)(CRATE_TILE + kind);
    crate_n++;
}

static void random_crate(u8 kind, u8 item)
{
    u8 tx, ty, tries = 0;
    do {
        tx = rnd() % map_w;
        ty = rnd() % map_h;
    } while (!free_floor(tx, ty, 3) && ++tries < 200);
    if (tries < 200) place_crate(kind, item, tx, ty);
}

void game_level_init(void)
{
    u8 i, t, n, tx, ty;
    u16 tries;
    const u8 *ci;
    for (cp = 0; cp < MAXPL; cp++) {
        px = FX(levelbuf[cp ? LH_P2X : LH_P1X] * TILE_PX);
        py = FX(levelbuf[cp ? LH_P2Y : LH_P1Y] * TILE_PX);
        pangle = 0;
        set_dir();
        p_sync();
        pdead = phit = pfire = ppush = pburn = pturn = prev_joy = msg_time = 0;
        penergy = pmax_energy = 1000 + pshield * 100;
        pcurr = 0;
        pload = 0;
    }
    cp = 0;
    for (t = 0; t < 16; t++) {
        walkv[0][t][0] = msin(41, (u8)(t << 4)); walkv[0][t][1] = mcos(41, (u8)(t << 4));
        walkv[1][t][0] = msin(61, (u8)(t << 4)); walkv[1][t][1] = mcos(61, (u8)(t << 4));
        walkv[2][t][0] = msin(20, (u8)(t << 4)); walkv[2][t][1] = mcos(20, (u8)(t << 4));
    }
    /* enemies */
    ne = 0;
    edead_count = 0;
    for (t = 0; t < NENEMYTYPES; t++)
        for (n = 0; n < levelbuf[LH_ENEMIES + t] && ne < MAXE; n++) {
            tries = 0;
            do {
                tx = rnd() % map_w;
                ty = rnd() % map_h;
            } while (!free_floor(tx, ty, 8) && ++tries < 1000);
            ex[ne] = FX(tx * TILE_PX - 3);
            ey[ne] = FX(ty * TILE_PX - 3);
            e_sync(ne);
            etype[ne] = t;
            eenergy[ne] = enemy64[t].energy;
            if (skill == SKILL_EASY) eenergy[ne] >>= 1;    /* easy: enemies half energy */
            eang[ne] = etang[ne] = rnd();
            ewalk[ne] = eload[ne] = efire[ne] = eflags[ne] = eburn[ne] = epush[ne] = eanim[ne] = 0;
            etgt[ne] = elast[ne] = 0;
            ne++;
        }
    /* crates */
    crate_n = 0;
    for (i = 0; i < 11; i++)
        for (n = 0; n < levelbuf[LH_WCRATES + i]; n++) random_crate(0, i);
    for (i = 0; i < 9; i++)
        for (n = 0; n < levelbuf[LH_BCRATES + i]; n++) random_crate(1, i);
    for (n = 0; n < levelbuf[LH_ECRATES]; n++) random_crate(2, 0);
    ci = levelbuf + LH_SIZE;
    for (i = 0; i < levelbuf[LH_NCINFO]; i++, ci += 4)
        if (ci[0] < map_w && ci[1] < map_h && ci[2] < 3 && !(map[rowoff[ci[1]] + ci[0]] & 0x80))
            place_crate(ci[2], ci[2] == 2 ? 0 : ci[3], ci[0], ci[1]);
#ifdef AUTOPLAY_CLOSE
    /* test: one enemy overlapping the player (4 px below), killed with the fist */
    if (ne) {
        ex[0] = px;
        ey[0] = py + FX(4);
        e_sync(0);
    }
#endif
#ifdef AUTOPLAY_KILL
    /* test: two enemies right in front of the player (facing down) */
    for (i = 0; i < 2 && i < ne; i++) {
        ex[i] = px + FX(i * 30);
        ey[i] = py + FX(40);
        eang[i] = etang[i] = 128;
        e_sync(i);
    }
#endif
    for (i = 0; i < MAXP; i++) prj_on[i] = 0;
    for (i = 0; i < MAXX; i++) exp_on[i] = 0;
    los_next = 0;
}

/* ------------------------------------------------------------------ */

u16 prof[4];
#ifdef AUTOPLAY
#define PROF_T() (((u16)CIA2.ta_hi << 8) | CIA2.ta_lo)
#define PROF(n, code) { u16 t_ = PROF_T(); code; prof[n] = t_ - PROF_T(); }
#else
#define PROF(n, code) code
#endif

/* nearer living player (tile distance) */
static u8 udiff(u8 a, u8 b) { return a > b ? (u8)(a - b) : (u8)(b - a); }
static u8 pick_target(u8 i)
{
    if (pl_dead[0]) return 1;
    if (pl_dead[1]) return 0;
    return udiff(etx[i], pl_tx[1]) + udiff(ety[i], pl_ty[1]) < udiff(etx[i], pl_tx[0]) + udiff(ety[i], pl_ty[0]);
}

/* line of sight: one awake enemy per tick (round robin) */
static void los_step(void)
{
    u8 i, k;
    for (k = 0; k < ne; k++) {
        i = los_next;
        if (++los_next >= ne) los_next = 0;
        if ((eflags[i] & EF_DEAD) || FAR(i)) continue;
        if (nplayers > 1) etgt[i] = pick_target(i);
        cp = etgt[i];
        if (sees_player(i)) eflags[i] |= EF_SEES;
        else eflags[i] &= ~EF_SEES;
        break;
    }
}

static void enemies_step(void)
{
    u8 i;
    for (i = 0; i < ne; i++) {
        if (eflags[i] & EF_DEAD) continue;
        /* far away enemies (asleep) are only looked at every 4th tick */
        if (((i ^ tick4) & 3) && FAR(i)) continue;
        cp = etgt[i];
        enemy_tick(i);
    }
}

/* one 25 Hz game tick; joy[] = input of each player */
void game_tick(void)
{
    for (cp = 0; cp < nplayers; cp++) {
        PROF(0, player_tick(joy[cp]));
        if (msg_time) msg_time--;
    }
    PROF(1, los_step());
    PROF(2, enemies_step());
    tick4++;
    PROF(3, update_projectiles());
}

/* ------------------------------------------------------------------ */
/* sprites of one view: own player, other player, up to 5 enemies,
 * 1 projectile, 1 explosion. Written to shadow set `view` (irq.s). */

static u8 sv, so;              /* view and its sprite set offset */
static s16 sbx, sby;           /* sprite position = world pixel + sbx/sby */

static u8 sprite_pos(u8 n, s16 wx, s16 wy)     /* wx, wy = pixels */
{
    s16 sx = wx + sbx, sy = wy + sby;
    if (sx <= 0 || sx >= 344 || sy < view_ymin[sv] || sy > view_ymax[sv]) return 0;
    spr_x[so + n] = (u8)sx;
    spr_y[so + n] = (u8)sy;
    if (sx > 255) spr_msb[sv] |= (u8)(1 << n);
    spr_ena[sv] |= (u8)(1 << n);
    return 1;
}

static u8 player_sprite(u8 n, u8 p)
{
    u8 dir;
    if (!sprite_pos(n, pl_pxl[p], pl_pyl[p])) return 0;
    dir = (u8)(((u8)(pl_angle[p] + 8)) >> 4);
    if (pl_dead[p]) spr_ptr[so + n] = SPR_PDEAD + (pl_dead[p] > 8);
    else spr_ptr[so + n] = (u8)(SPR_PLAYER + dir * 3 + (pl_fire[p] ? 2 : pl_moving[p] ? ((pl_walk[p] >> 1) & 1) : 0));
    spr_col[so + n] = pl_hit[p] & 2 ? 1 : player_col[p];
    return 1;
}

void game_sprites(u8 view)
{
    u8 i, n = 1, look, dir, vcx = view_cx[view], vcy = view_cy[view], h = (u8)(view_rows[view] + 6);
    sv = view;
    so = (u8)(view << 3);
    sbx = 23 - ((s16)vcx << 3);
    sby = view_top[view] - ((s16)vcy << 3);
    spr_ena[sv] = spr_msb[sv] = 0;
    spr_mc[sv] = 0x3F;
    player_sprite(0, view);
    if (nplayers > 1 && player_sprite(1, (u8)(view ^ 1))) n = 2;
    /* enemies on screen */
    for (i = 0; i < ne && n < 6; i++) {
        if (eflags[i] & EF_DEAD) continue;
        /* quick reject in characters before the exact test */
        if ((u8)((etx[i] << 1) - vcx + 3) >= 46 || (u8)((ety[i] << 1) - vcy + 3) >= h) continue;
        if (!sprite_pos(n, epx[i], epy[i])) continue;
        look = enemy_look[etype[i]];
        dir = (u8)(((u8)(eang[i] + 8)) >> 4);
        spr_ptr[so + n] = look == 3 ? (u8)(SPR_LOOK_D + dir)
                                    : (u8)(SPR_LOOK_A + look * 32 + dir * 2 + (ewalk[i] ? ((eanim[i] >> 1) & 1) : 0));
        spr_col[so + n] = efire[i] ? 1 : enemy_col[etype[i]];
        n++;
    }
    /* one projectile */
    for (i = 0; i < MAXP; i++)
        if (prj_on[i] && sprite_pos(6, PX(prj_x[i]) - 8, PX(prj_y[i]) - 6)) {
            spr_ptr[so + 6] = (u8)(prj_type[i] == 7 ? SPR_C4 : prj_type[i] == 9 ? SPR_MINE : SPR_GRENADE);
            spr_col[so + 6] = 1;
            break;
        }
    /* one explosion or flame puff */
    for (i = 0; i < MAXX; i++)
        if (exp_on[i] && sprite_pos(7, exp_x[i] - 12, exp_y[i] - 10)) {
            spr_ptr[so + 7] = exp_kind[i] ? SPR_FLAME : (u8)(SPR_EXPLO + (exp_frame[i] >> 1));
            spr_col[so + 7] = exp_kind[i] ? 8 : 7;
            break;
        }
}
