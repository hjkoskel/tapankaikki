/* plat.h - the only interface between the game core and the machine.
 * A new platform (DOS, Amiga, SDL, ...) only needs to implement this file. */
#ifndef TK_PLAT_H
#define TK_PLAT_H

#include "types.h"

/* action buttons, one mask per key set */
#define BTN_LEFT    0x00000001u  /* rotate counter clockwise */
#define BTN_RIGHT   0x00000002u  /* rotate clockwise */
#define BTN_UP      0x00000004u  /* walk forward */
#define BTN_DOWN    0x00000008u  /* walk backward */
#define BTN_FIRE    0x00000010u
#define BTN_CHANGE  0x00000020u  /* next weapon */
#define BTN_STRAFE  0x00000040u  /* hold: left/right strafe */
#define BTN_SLEFT   0x00000080u  /* side step left */
#define BTN_SRIGHT  0x00000100u  /* side step right */
#define BTN_W0      0x00001000u  /* direct weapon select 0..11 = (BTN_W0 << n) */

/* menu / system keys returned by plat_key_pressed() */
#define PK_NONE   0
#define PK_UP     1
#define PK_DOWN   2
#define PK_LEFT   3
#define PK_RIGHT  4
#define PK_ENTER  5
#define PK_ESC    6
#define PK_SPACE  7
#define PK_OTHER  8
/* printable keys are returned as their lowercase ASCII code (>= 32) */

int  plat_init(void);
void plat_shutdown(void);
void plat_set_palette(const u8 *pal768);   /* 6-bit VGA values 0..63 */
void plat_present(const u8 *fb);           /* show 320x200 indexed frame, pump events */
u32  plat_buttons(int keyset);             /* keyset 0 = arrows, 1 = WASD */
int  plat_key_pressed(void);               /* next key from queue or PK_NONE */
u32  plat_ticks_ms(void);
int  plat_quit_requested(void);
void plat_load_sound(int id, const char *path); /* may be a no-op */
void plat_sound(int id, s32 rate);         /* rate = playback rate in Hz, may be no-op */

#endif
