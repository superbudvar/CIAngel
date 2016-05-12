#pragma once
#include <3ds.h>
#include <citro3d.h>
#include "vshader_shbin.h"

#ifdef __cplusplus
extern "C" {
#endif

#define TOP_SCREEN_WIDTH 400
#define TOP_SCREEN_HEIGHT 240

#define BOTTOM_SCREEN_WIDTH 320
#define BOTTOM_SCREEN_HEIGHT 240

#define MAX_TEXTURES 1024
#define TEXTURE_SCREEN_TOP_SPLASH_BG 0
#define TEXTURE_APP_BANNER 1
#define TEXTURE_FLAG_EUR 2
#define TEXTURE_FLAG_USA 3
#define TEXTURE_FLAG_JPN 4
#define TEXTURE_FLAG_ALL 5

#define COLOR_BLACK 0xFF000000
#define COLOR_BLUE 0xFFff0000
#define COLOR_RED 0xFF0000ff
#define COLOR_GREEN 0xFF00ff80
#define COLOR_WHITE 0xFFffffff
#define COLOR_PURPLE 0xFFd126b8
#define COLOR_CYAN 0xFFd19426

#define CLEAR_COLOR 0x00000000

#define DISPLAY_TRANSFER_FLAGS                                                                                                     \
    (GX_TRANSFER_FLIP_VERT(0) | GX_TRANSFER_OUT_TILED(0) | GX_TRANSFER_RAW_COPY(0) |                                               \
     GX_TRANSFER_IN_FORMAT(GX_TRANSFER_FMT_RGBA8) | GX_TRANSFER_OUT_FORMAT(GX_TRANSFER_FMT_RGB8) |                                 \
     GX_TRANSFER_SCALING(GX_TRANSFER_SCALE_NO))

#define FONT_DEFAULT_SIZE 0.5f

void sceneInit(void);
void sceneRender(float size);
void sceneExit(void);
void renderText(float x, float y, float scaleX, float scaleY, bool baseline, const char *text);
void setTextColor(u32 color);
void sceneRenderFooter(const char *text);
void screen_end_frame();
void screen_begin_frame();
void sceneDraw();
void renderBG();
void screen_draw_texture(u32 id, float x, float y, float width, float height);
void screen_get_texture_size(u32 *width, u32 *height, u32 id);
#ifdef __cplusplus
}
#endif
