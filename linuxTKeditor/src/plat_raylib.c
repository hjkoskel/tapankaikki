/* plat_raylib.c - Linux backend using raylib. The only file that knows about raylib. */
#include <raylib.h>
#include <string.h>
#include <strings.h>
#include "plat.h"

static Texture2D screen_tex;
static unsigned char rgba_pal[256][4];
static unsigned char rgba_buf[SCR_W * SCR_H * 4];
static Rectangle dst;                 /* where the picture is in the window */

#define QUEUE 64
static int queue[QUEUE];
static int q_head, q_tail;

static void push(int k)
{
    int n = (q_tail + 1) % QUEUE;
    if (n != q_head) {
        queue[q_tail] = k;
        q_tail = n;
    }
}

static const struct { int ray, ek; } keymap[] = {
    { KEY_UP, EK_UP }, { KEY_DOWN, EK_DOWN }, { KEY_LEFT, EK_LEFT }, { KEY_RIGHT, EK_RIGHT },
    { KEY_ENTER, EK_ENTER }, { KEY_KP_ENTER, EK_ENTER }, { KEY_ESCAPE, EK_ESC },
    { KEY_BACKSPACE, EK_BKSP }, { KEY_DELETE, EK_DEL }, { KEY_PAGE_UP, EK_PGUP },
    { KEY_PAGE_DOWN, EK_PGDN }, { KEY_HOME, EK_HOME }, { KEY_END, EK_END }, { KEY_TAB, EK_TAB },
    { KEY_F1, EK_F1 }, { KEY_F2, EK_F1 + 1 }, { KEY_F3, EK_F1 + 2 }, { KEY_F4, EK_F1 + 3 },
    { KEY_F5, EK_F1 + 4 }, { KEY_F6, EK_F1 + 5 }, { KEY_F7, EK_F1 + 6 }, { KEY_F8, EK_F1 + 7 },
    { KEY_F9, EK_F1 + 8 }, { KEY_F10, EK_F1 + 9 }, { KEY_F11, EK_F1 + 10 }, { KEY_F12, EK_F1 + 11 }
};
#define NKEYMAP ((int)(sizeof(keymap) / sizeof(keymap[0])))

int plat_init(void)
{
    Image img;
    SetConfigFlags(FLAG_WINDOW_RESIZABLE | FLAG_VSYNC_HINT);
    SetTraceLogLevel(LOG_WARNING);
    InitWindow(960, 720, "TK level editor");
    if (!IsWindowReady()) return 0;
    SetExitKey(KEY_NULL);             /* ESC is an editor key */
    SetTargetFPS(60);
    HideCursor();                     /* the editor draws the game's pointer */
    img = GenImageColor(SCR_W, SCR_H, BLACK);
    ImageFormat(&img, PIXELFORMAT_UNCOMPRESSED_R8G8B8A8);
    screen_tex = LoadTextureFromImage(img);
    UnloadImage(img);
    SetTextureFilter(screen_tex, TEXTURE_FILTER_POINT);
    return 1;
}

void plat_shutdown(void)
{
    UnloadTexture(screen_tex);
    CloseWindow();
}

void plat_set_palette(const u8 *pal)
{
    int i, c;
    for (i = 0; i < 256; i++) {
        for (c = 0; c < 3; c++) {
            int v = pal[i * 3 + c] & 63;
            rgba_pal[i][c] = (unsigned char)((v << 2) | (v >> 4));
        }
        rgba_pal[i][3] = 255;
    }
}

void plat_present(const u8 *fb)
{
    int i, sw, sh, ch;
    float w, h;
    Rectangle src;
    unsigned char *d = rgba_buf;

    for (i = 0; i < SCR_W * SCR_H; i++, d += 4) {
        const unsigned char *c = rgba_pal[fb[i]];
        d[0] = c[0];
        d[1] = c[1];
        d[2] = c[2];
        d[3] = 255;
    }
    UpdateTexture(screen_tex, rgba_buf);

    /* fit a 4:3 picture (as on a CRT) into the window */
    sw = GetScreenWidth();
    sh = GetScreenHeight();
    w = (float)sw;
    h = w * 3.0f / 4.0f;
    if (h > sh) {
        h = (float)sh;
        w = h * 4.0f / 3.0f;
    }
    src.x = 0; src.y = 0; src.width = SCR_W; src.height = SCR_H;
    dst.x = (sw - w) / 2; dst.y = (sh - h) / 2; dst.width = w; dst.height = h;

    BeginDrawing();
    ClearBackground(BLACK);
    DrawTexturePro(screen_tex, src, dst, (Vector2){ 0, 0 }, 0.0f, WHITE);
    EndDrawing();

    /* events of this frame into the key queue */
    for (i = 0; i < NKEYMAP; i++)
        if (IsKeyPressed(keymap[i].ray) || IsKeyPressedRepeat(keymap[i].ray)) push(keymap[i].ek);
    while ((ch = GetCharPressed()) != 0)
        if (ch >= 32 && ch < 127) push(ch);
    while (GetKeyPressed() != 0) ;
}

int plat_key(void)
{
    int k;
    if (q_head == q_tail) return EK_NONE;
    k = queue[q_head];
    q_head = (q_head + 1) % QUEUE;
    return k;
}

int plat_key_down(int key)
{
    int i;
    if (key == EK_SHIFT) return IsKeyDown(KEY_LEFT_SHIFT) || IsKeyDown(KEY_RIGHT_SHIFT);
    if (key == EK_CTRL) return IsKeyDown(KEY_LEFT_CONTROL) || IsKeyDown(KEY_RIGHT_CONTROL);
    for (i = 0; i < NKEYMAP; i++)
        if (keymap[i].ek == key && IsKeyDown(keymap[i].ray)) return 1;
    return 0;
}

void plat_mouse(s16 *x, s16 *y, u8 *buttons)
{
    Vector2 m = GetMousePosition();
    float fx = dst.width > 0 ? (m.x - dst.x) * SCR_W / dst.width : -1;
    float fy = dst.height > 0 ? (m.y - dst.y) * SCR_H / dst.height : -1;
    *x = (s16)(fx < 0 ? fx - 1 : fx);
    *y = (s16)(fy < 0 ? fy - 1 : fy);
    *buttons = 0;
    if (IsMouseButtonDown(MOUSE_BUTTON_LEFT)) *buttons |= MB_LEFT;
    if (IsMouseButtonDown(MOUSE_BUTTON_RIGHT)) *buttons |= MB_RIGHT;
    if (IsMouseButtonDown(MOUSE_BUTTON_MIDDLE)) *buttons |= MB_MIDDLE;
}

int plat_quit_requested(void)
{
    return WindowShouldClose();
}

void plat_list_levels(const char *dir, void (*fn)(const char *rel))
{
    FilePathList list = LoadDirectoryFilesEx(dir, NULL, true);
    size_t skip = strlen(dir);
    unsigned int i;
    for (i = 0; i < list.count; i++) {
        const char *p = list.paths[i];
        size_t n = strlen(p);
        if (n > 4 && strcasecmp(p + n - 4, ".lev") == 0 && strncmp(p, dir, skip) == 0) {
            p += skip;
            while (*p == '/') p++;
            fn(p);
        }
    }
    UnloadDirectoryFiles(list);
}

int plat_make_dirs(const char *path)
{
    const char *dir = GetDirectoryPath(path);
    if (DirectoryExists(dir)) return 1;
    return MakeDirectory(dir) == 0;
}
