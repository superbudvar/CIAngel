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

#define CLEAR_COLOR 0x00000000

#define DISPLAY_TRANSFER_FLAGS \
    (GX_TRANSFER_FLIP_VERT(0) | GX_TRANSFER_OUT_TILED(0) | GX_TRANSFER_RAW_COPY(0) | \
    GX_TRANSFER_IN_FORMAT(GX_TRANSFER_FMT_RGBA8) | GX_TRANSFER_OUT_FORMAT(GX_TRANSFER_FMT_RGB8) | \
    GX_TRANSFER_SCALING(GX_TRANSFER_SCALE_NO))

#define FONT_DEFAULT_SIZE 0.5f

void sceneInit(void);
void sceneRender(float size);
void sceneExit(void);
void renderText(float x, float y, float scaleX, float scaleY, bool baseline, const char* text);
void setTextColor(u32 color);
void sceneRenderFooter(const char* text);
void sceneDraw();

#ifdef __cplusplus
}
#endif
