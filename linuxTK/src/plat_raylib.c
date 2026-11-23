/* plat_raylib.c - Linux backend using raylib. The only file that knows about raylib. */
#include <raylib.h>
#include "plat.h"

static Texture2D screen_tex;
static unsigned char rgba_pal[256][4];
static unsigned char rgba_buf[SCR_W * SCR_H * 4];

#define SND_ALIASES 4
static Sound sounds[DIFF_WAVS + 1][SND_ALIASES];
static int sound_ok[DIFF_WAVS + 1];
static int sound_next[DIFF_WAVS + 1];
static int audio_ok;

int plat_init(void)
{
    Image img;
    SetConfigFlags(FLAG_WINDOW_RESIZABLE | FLAG_VSYNC_HINT);
    SetTraceLogLevel(LOG_WARNING);
    InitWindow(960, 720, "Ultimate Tapan Kaikki");
    if (!IsWindowReady()) return 0;
    SetExitKey(KEY_NULL);           /* ESC is a game key */
    SetTargetFPS(60);
    img = GenImageColor(SCR_W, SCR_H, BLACK);
    ImageFormat(&img, PIXELFORMAT_UNCOMPRESSED_R8G8B8A8);
    screen_tex = LoadTextureFromImage(img);
    UnloadImage(img);
    SetTextureFilter(screen_tex, TEXTURE_FILTER_POINT);
    InitAudioDevice();
    audio_ok = IsAudioDeviceReady();
    return 1;
}

void plat_shutdown(void)
{
    int i, j;
    for (i = 0; i <= DIFF_WAVS; i++)
        if (sound_ok[i]) {
            for (j = 1; j < SND_ALIASES; j++) UnloadSoundAlias(sounds[i][j]);
            UnloadSound(sounds[i][0]);
        }
    if (audio_ok) CloseAudioDevice();
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
    int i, sw, sh;
    float w, h;
    Rectangle src, dst;
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
}

/* key sets: 0 = arrows (player 1 / single player), 1 = WASD (left view in split screen).
 * Defaults follow the original OPTIONS.CFG, with a few extra aliases. */
u32 plat_buttons(int keyset)
{
    u32 b = 0;
    if (keyset == 0) {
        if (IsKeyDown(KEY_LEFT)) b |= BTN_LEFT;
        if (IsKeyDown(KEY_RIGHT)) b |= BTN_RIGHT;
        if (IsKeyDown(KEY_UP)) b |= BTN_UP;
        if (IsKeyDown(KEY_DOWN)) b |= BTN_DOWN;
        if (IsKeyDown(KEY_RIGHT_CONTROL)) b |= BTN_FIRE;
        if (IsKeyDown(KEY_RIGHT_SHIFT) || IsKeyDown(KEY_ENTER)) b |= BTN_CHANGE;
        if (IsKeyDown(KEY_RIGHT_ALT)) b |= BTN_STRAFE;
        if (IsKeyDown(KEY_COMMA)) b |= BTN_SLEFT;
        if (IsKeyDown(KEY_PERIOD)) b |= BTN_SRIGHT;
        /* direct weapon keys: ` 1 2 ... 0 - = */
        if (IsKeyDown(KEY_GRAVE)) b |= BTN_W0;
        if (IsKeyDown(KEY_ONE)) b |= BTN_W0 << 1;
        if (IsKeyDown(KEY_TWO)) b |= BTN_W0 << 2;
        if (IsKeyDown(KEY_THREE)) b |= BTN_W0 << 3;
        if (IsKeyDown(KEY_FOUR)) b |= BTN_W0 << 4;
        if (IsKeyDown(KEY_FIVE)) b |= BTN_W0 << 5;
        if (IsKeyDown(KEY_SIX)) b |= BTN_W0 << 6;
        if (IsKeyDown(KEY_SEVEN)) b |= BTN_W0 << 7;
        if (IsKeyDown(KEY_EIGHT)) b |= BTN_W0 << 8;
        if (IsKeyDown(KEY_NINE)) b |= BTN_W0 << 9;
        if (IsKeyDown(KEY_ZERO)) b |= BTN_W0 << 10;
        if (IsKeyDown(KEY_MINUS)) b |= BTN_W0 << 11;
    } else {
        if (IsKeyDown(KEY_A)) b |= BTN_LEFT;
        if (IsKeyDown(KEY_D)) b |= BTN_RIGHT;
        if (IsKeyDown(KEY_W)) b |= BTN_UP;
        if (IsKeyDown(KEY_S)) b |= BTN_DOWN;
        if (IsKeyDown(KEY_TAB) || IsKeyDown(KEY_LEFT_CONTROL)) b |= BTN_FIRE;
        if (IsKeyDown(KEY_CAPS_LOCK) || IsKeyDown(KEY_LEFT_SHIFT)) b |= BTN_CHANGE;
        if (IsKeyDown(KEY_GRAVE) || IsKeyDown(KEY_LEFT_ALT)) b |= BTN_STRAFE;
        if (IsKeyDown(KEY_ONE) || IsKeyDown(KEY_Q)) b |= BTN_SLEFT;
        if (IsKeyDown(KEY_TWO) || IsKeyDown(KEY_E)) b |= BTN_SRIGHT;
    }
    return b;
}

int plat_key_pressed(void)
{
    int k = GetKeyPressed();
    if (k == 0) return PK_NONE;
    switch (k) {
    case KEY_UP: return PK_UP;
    case KEY_DOWN: return PK_DOWN;
    case KEY_LEFT: return PK_LEFT;
    case KEY_RIGHT: return PK_RIGHT;
    case KEY_ENTER: case KEY_KP_ENTER: return PK_ENTER;
    case KEY_ESCAPE: return PK_ESC;
    case KEY_SPACE: return PK_SPACE;
    default: break;
    }
    if (k >= KEY_A && k <= KEY_Z) return 'a' + (k - KEY_A);
    if (k >= KEY_ZERO && k <= KEY_NINE) return '0' + (k - KEY_ZERO);
    return PK_OTHER;
}

u32 plat_ticks_ms(void)
{
    return (u32)(GetTime() * 1000.0);
}

int plat_quit_requested(void)
{
    return WindowShouldClose();
}

void plat_load_sound(int id, const char *path)
{
    int j;
    if (!audio_ok || id < 0 || id > DIFF_WAVS || !FileExists(path)) return;
    sounds[id][0] = LoadSound(path);
    if (!IsSoundValid(sounds[id][0])) return;
    for (j = 1; j < SND_ALIASES; j++) sounds[id][j] = LoadSoundAlias(sounds[id][0]);
    sound_ok[id] = 1;
}

void plat_sound(int id, s32 rate)
{
    Sound *s;
    if (!audio_ok || id < 0 || id > DIFF_WAVS || !sound_ok[id]) return;
    s = &sounds[id][sound_next[id]];
    sound_next[id] = (sound_next[id] + 1) % SND_ALIASES;
    if (rate > 0 && s->stream.sampleRate > 0) SetSoundPitch(*s, (float)rate / (float)s->stream.sampleRate);
    else SetSoundPitch(*s, 1.0f);
    PlaySound(*s);
}
