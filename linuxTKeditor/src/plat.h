/* plat.h - the only interface between the editor and the machine. */
#ifndef ED_PLAT_H
#define ED_PLAT_H

#include "types.h"

/* keys returned by plat_key(): printable characters are their ASCII code,
 * the rest are these codes */
#define EK_NONE  0
#define EK_UP    256
#define EK_DOWN  257
#define EK_LEFT  258
#define EK_RIGHT 259
#define EK_ENTER 260
#define EK_ESC   261
#define EK_BKSP  262
#define EK_DEL   263
#define EK_PGUP  264
#define EK_PGDN  265
#define EK_HOME  266
#define EK_END   267
#define EK_TAB   268
#define EK_F1    270            /* EK_F1 + n - 1 = Fn, up to F12 */
#define EK_SHIFT 290            /* only for plat_key_down() */
#define EK_CTRL  291

/* mouse buttons */
#define MB_LEFT   1
#define MB_RIGHT  2
#define MB_MIDDLE 4

int  plat_init(void);
void plat_shutdown(void);
void plat_set_palette(const u8 *pal768);   /* 6-bit VGA values 0..63 */
void plat_present(const u8 *fb);           /* show the 320x200 frame, pump events */
int  plat_key(void);                       /* next key from the queue or EK_NONE */
int  plat_key_down(int key);               /* arrows, PgUp/PgDn, EK_SHIFT, EK_CTRL */
void plat_mouse(s16 *x, s16 *y, u8 *buttons); /* in screen pixels, may be outside */
int  plat_quit_requested(void);

/* Calls fn(rel) for each *.LEV file under dir (case insensitive, recursive).
 * rel is the path relative to dir with '/' separators. */
void plat_list_levels(const char *dir, void (*fn)(const char *rel));
int  plat_make_dirs(const char *path);    /* create the directories of a file path */

#endif
