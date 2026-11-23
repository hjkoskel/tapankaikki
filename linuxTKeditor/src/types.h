/* types.h - basic types and limits of the level editor (C89). */
#ifndef ED_TYPES_H
#define ED_TYPES_H

typedef unsigned char  u8;
typedef signed char    s8;
typedef unsigned short u16;
typedef short          s16;
typedef unsigned long  u32;
typedef long           s32;

#define ED_VERSION "linuxTKeditor 1.0"

#define SCR_W 320
#define SCR_H 200
#define TILE 20                 /* block size in pixels */
#define NUM_TILES 160           /* 16 x 10 tiles per sheet */
#define SHADOW_TYPES 16         /* SHADOWS.EFP: 16 shadows of 20 x 20 */
#define CRATE_TYPES 3
#define CRATE_SIZE 14

/* level limits (the game port uses the same maximum size) */
#define MAX_LEVEL_W 100
#define MAX_LEVEL_H 100
#define MIN_LEVEL_W 2
#define MIN_LEVEL_H 2
#define MAX_SPOTS 100
#define MAX_STEAM 100
#define MAX_CRATE_INFO 100
#define LEVEL_VERSION 5         /* the version written by the editor */

#define DIFF_WEAPONS 11
#define DIFF_BULLETS 9
#define DIFF_ENEMIES 8
#define LIGHTS 10               /* spot light sizes 0..9 */
#define STEAM_SPEEDS 8          /* steam speeds 0..7 */

/* font colour ramps of the game palette (text colour = ramp + row) */
#define C_WHITE  1
#define C_GREY   32
#define C_RED    48
#define C_YELLOW 112
#define C_BLUE   144

#endif
