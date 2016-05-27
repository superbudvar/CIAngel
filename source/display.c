#include "display.h"

#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <3ds.h>
#include <citro3d.h>
#include "stb/stb_image.h"
#include "vshader_shbin.h"

GX_TRANSFER_FORMAT gpuToGxFormat[13] = {
    GX_TRANSFER_FMT_RGBA8, GX_TRANSFER_FMT_RGB8, GX_TRANSFER_FMT_RGB5A1, GX_TRANSFER_FMT_RGB565, GX_TRANSFER_FMT_RGBA4,
    GX_TRANSFER_FMT_RGBA8, // Unsupported
    GX_TRANSFER_FMT_RGBA8, // Unsupported
    GX_TRANSFER_FMT_RGBA8, // Unsupported
    GX_TRANSFER_FMT_RGBA8, // Unsupported
    GX_TRANSFER_FMT_RGBA8, // Unsupported
    GX_TRANSFER_FMT_RGBA8, // Unsupported
    GX_TRANSFER_FMT_RGBA8, // Unsupported
    GX_TRANSFER_FMT_RGBA8  // Unsupported
};

typedef struct
{
    float position[3];
    float texcoord[2];
} textVertex_s;

static struct
{
    bool initialized;
    C3D_Tex tex;
    u32 width;
    u32 height;
    u32 pow2Width;
    u32 pow2Height;
} textures[MAX_TEXTURES];

static DVLB_s *vshader_dvlb;
static shaderProgram_s program;
static int uLoc_projection;
static C3D_Mtx projection;
static C3D_Tex *glyphSheets;
static C3D_RenderTarget *target_top;
static C3D_RenderTarget *target_bottom;
static textVertex_s *textVtxArray;
static int textVtxArrayPos;
static uiConsole top_console;
static float fontSize = 0.5f;

#define TEXT_VTX_ARRAY_COUNT (4 * 1024)
static bool c3dInitialized;
static bool shaderInitialized;

void setTextColor(u32 color)
{
    C3D_TexEnv *env = C3D_GetTexEnv(0);
    C3D_TexEnvSrc(env, C3D_RGB, GPU_CONSTANT, 0, 0);
    C3D_TexEnvSrc(env, C3D_Alpha, GPU_TEXTURE0, GPU_CONSTANT, 0);
    C3D_TexEnvOp(env, C3D_Both, 0, 0, 0);
    C3D_TexEnvFunc(env, C3D_RGB, GPU_REPLACE);
    C3D_TexEnvFunc(env, C3D_Alpha, GPU_MODULATE);
    C3D_TexEnvColor(env, color);
}

static void addTextVertex(float vx, float vy, float tx, float ty)
{
    textVertex_s *vtx = &textVtxArray[textVtxArrayPos++];
    vtx->position[0] = vx;
    vtx->position[1] = vy;
    vtx->position[2] = 0.5f;
    vtx->texcoord[0] = tx;
    vtx->texcoord[1] = ty;
}

void screen_begin_frame(bool with_bg)
{
    if (!C3D_FrameBegin(C3D_FRAME_SYNCDRAW)) {
        printf("Failed to begin frame.");
        return;
    }
    textVtxArrayPos = 0; // Clear the text vertex array
    C3D_FrameDrawOn(target_top);
    if(with_bg) {
       screen_draw_texture(TEXTURE_SCREEN_TOP_BG, 0, 0, TOP_SCREEN_WIDTH, TOP_SCREEN_HEIGHT);
    }  
    top_console.cursorX = 0;
    top_console.cursorY = 0;
}

void screen_end_frame() { C3D_FrameEnd(0); }

void printfText(float *pos_x, float *pos_y, float scaleX, float scaleY, bool baseline, const char *text)
{
    ssize_t units;
    uint32_t code;
    float x = *pos_x;
    float y = *pos_y;
    // Configure buffers
    C3D_BufInfo *bufInfo = C3D_GetBufInfo();
    BufInfo_Init(bufInfo);
    BufInfo_Add(bufInfo, textVtxArray, sizeof(textVertex_s), 2, 0x10);

    const uint8_t *p = (const uint8_t *)text;
    float firstX = x;
    u32 flags = GLYPH_POS_CALC_VTXCOORD | (baseline ? GLYPH_POS_AT_BASELINE : 0);
    int lastSheet = -1;
    do {
        if (!*p)
            break;
        units = decode_utf8(&code, p);
        if (units == -1)
            break;
        p += units;
        if (code == '\n') {
            x = firstX;
            y += scaleY * fontGetInfo()->lineFeed;
        } else if (code > 0) {
            int glyphIdx = fontGlyphIndexFromCodePoint(code);
            fontGlyphPos_s data;
            fontCalcGlyphPos(&data, glyphIdx, flags, scaleX, scaleY);

            // Bind the correct texture sheet
            if (data.sheetIndex != lastSheet) {
                lastSheet = data.sheetIndex;
                C3D_TexBind(0, &glyphSheets[lastSheet]);
            }

            int arrayIndex = textVtxArrayPos;
            if ((arrayIndex + 4) >= TEXT_VTX_ARRAY_COUNT)
                break; // We can't render more characters

            // Add the vertices to the array
            addTextVertex(x + data.vtxcoord.left, y + data.vtxcoord.bottom, data.texcoord.left, data.texcoord.bottom);
            addTextVertex(x + data.vtxcoord.right, y + data.vtxcoord.bottom, data.texcoord.right, data.texcoord.bottom);
            addTextVertex(x + data.vtxcoord.left, y + data.vtxcoord.top, data.texcoord.left, data.texcoord.top);
            addTextVertex(x + data.vtxcoord.right, y + data.vtxcoord.top, data.texcoord.right, data.texcoord.top);

            // Draw the glyph
            C3D_DrawArrays(GPU_TRIANGLE_STRIP, arrayIndex, 4);

            x += data.xAdvance;
        }
//        uiConsole.cursorX = x;
//        uiConsole.cursorY = y;
    } while (code > 0);
}

void renderText(float x, float y, float scaleX, float scaleY, bool baseline, const char *text)
{
    printfText(&x, &y, scaleX, scaleY, baseline, text);
}

void sceneRender(float size)
{
    // Update the uniforms
    C3D_FVUnifMtx4x4(GPU_VERTEX_SHADER, uLoc_projection, &projection);

    const char *teststring = "Hello World - working text rendering!\n"
                             "The quick brown fox jumped over the lazy dog.\n"
                             "\n"
                             "En français: Vous ne devez pas éteindre votre console.\n"
                             "日本語も見せるのが出来ますよ。\n"
                             "Un poco de texto en español nunca queda mal.\n"
                             "Some more random text in English.\n"
                             "В чащах юга жил бы цитрус? Да, но фальшивый экземпляр!\n";

    setTextColor(0xFF000000); // black
    renderText(8.0f, 8.0f, size, size, false, teststring);

    setTextColor(0xFF0000FF); // red
    renderText(16.0f, 210.0f, 0.5f, 0.75f, true, "I am red skinny text!");
    setTextColor(0xA0FF0000); // semi-transparent blue
    renderText(150.0f, 210.0f, 0.75f, 0.5f, true, "I am blue fat text!");

    char buf[160];
    sprintf(buf, "Current text size: %f (Use  to change)", size);
    setTextColor(0xFF000000); // black
    renderText(8.0f, 220.0f, 0.5f, 0.5f, false, buf);
}

void sceneDraw()
{
    if (!target_top) {
        printf("no target");
        return;
    }
    // Render the scene
    screen_end_frame();
    screen_begin_frame(false);
    //    C3D_FrameBegin(C3D_FRAME_SYNCDRAW);
    //   textVtxArrayPos = 0; // Clear the text vertex array
    C3D_FVUnifMtx4x4(GPU_VERTEX_SHADER, uLoc_projection, &projection);
    C3D_FrameDrawOn(target_top);
}

void sceneExit(void)
{
    // Free the textures
    free(glyphSheets);

    // Free the shader program
    shaderProgramFree(&program);
    DVLB_Free(vshader_dvlb);
}

static FILE *screen_open_resource(const char *path)
{
    u32 realPathSize = strlen(path) + 16;
    char realPath[realPathSize];

    snprintf(realPath, realPathSize, "sdmc:/fbitheme/%s", path);
    FILE *fd = fopen(realPath, "rb");

    if (fd != NULL) {
        return fd;
    } else {
        snprintf(realPath, realPathSize, "romfs:/%s", path);

        return fopen(realPath, "rb");
    }
}
static u32 screen_next_pow_2(u32 i)
{
    i--;
    i |= i >> 1;
    i |= i >> 2;
    i |= i >> 4;
    i |= i >> 8;
    i |= i >> 16;
    i++;

    return i;
}

void screen_load_texture(u32 id, void *data, u32 size, u32 width, u32 height, GPU_TEXCOLOR format, bool linearFilter)
{
    if (id >= MAX_TEXTURES) {
        printf("Attempted to load buffer to invalid texture ID \"%lu\".", id);
        return;
    }

    u32 pow2Width = screen_next_pow_2(width);
    if (pow2Width < 64) {
        pow2Width = 64;
    }

    u32 pow2Height = screen_next_pow_2(height);
    if (pow2Height < 64) {
        pow2Height = 64;
    }

    u32 pixelSize = size / width / height;

    u8 *pow2Tex = linearAlloc(pow2Width * pow2Height * pixelSize);
    if (pow2Tex == NULL) {
        printf("Failed to allocate temporary texture buffer.");
        return;
    }

    memset(pow2Tex, 0, pow2Width * pow2Height * pixelSize);

    for (u32 x = 0; x < width; x++) {
        for (u32 y = 0; y < height; y++) {
            u32 dataPos = (y * width + x) * pixelSize;
            u32 pow2TexPos = (y * pow2Width + x) * pixelSize;

            for (u32 i = 0; i < pixelSize; i++) {
                pow2Tex[pow2TexPos + i] = ((u8 *)data)[dataPos + i];
            }
        }
    }

    textures[id].initialized = true;
    textures[id].width = width;
    textures[id].height = height;
    textures[id].pow2Width = pow2Width;
    textures[id].pow2Height = pow2Height;

    if (!C3D_TexInit(&textures[id].tex, (int)pow2Width, (int)pow2Height, format)) {
        printf("Failed to initialize texture.");
        return;
    }

    C3D_TexSetFilter(&textures[id].tex, linearFilter ? GPU_LINEAR : GPU_NEAREST, GPU_NEAREST);

    Result flushRes = GSPGPU_FlushDataCache(pow2Tex, pow2Width * pow2Height * 4);
    if (R_FAILED(flushRes)) {
        printf("Failed to flush texture buffer: 0x%08lX", flushRes);
        return;
    }

    C3D_SafeDisplayTransfer((u32 *)pow2Tex, GX_BUFFER_DIM(pow2Width, pow2Height), (u32 *)textures[id].tex.data,
                            GX_BUFFER_DIM(pow2Width, pow2Height),
                            GX_TRANSFER_FLIP_VERT(1) | GX_TRANSFER_OUT_TILED(1) | GX_TRANSFER_RAW_COPY(0) |
                                GX_TRANSFER_IN_FORMAT((u32)gpuToGxFormat[format]) |
                                GX_TRANSFER_OUT_FORMAT((u32)gpuToGxFormat[format]) | GX_TRANSFER_SCALING(GX_TRANSFER_SCALE_NO));
    gspWaitForPPF();

    linearFree(pow2Tex);
}
void screen_draw_quad(float x1, float y1, float x2, float y2, float tx1, float ty1, float tx2, float ty2)
{
    C3D_ImmDrawBegin(GPU_TRIANGLES);

    C3D_ImmSendAttrib(x1, y1, 0.5f, 0.0f);
    C3D_ImmSendAttrib(tx1, ty1, 0.0f, 0.0f);

    C3D_ImmSendAttrib(x2, y2, 0.5f, 0.0f);
    C3D_ImmSendAttrib(tx2, ty2, 0.0f, 0.0f);

    C3D_ImmSendAttrib(x2, y1, 0.5f, 0.0f);
    C3D_ImmSendAttrib(tx2, ty1, 0.0f, 0.0f);

    C3D_ImmSendAttrib(x1, y1, 0.5f, 0.0f);
    C3D_ImmSendAttrib(tx1, ty1, 0.0f, 0.0f);

    C3D_ImmSendAttrib(x1, y2, 0.5f, 0.0f);
    C3D_ImmSendAttrib(tx1, ty2, 0.0f, 0.0f);

    C3D_ImmSendAttrib(x2, y2, 0.5f, 0.0f);
    C3D_ImmSendAttrib(tx2, ty2, 0.0f, 0.0f);

    C3D_ImmDrawEnd();
}
void screen_draw_texture(u32 id, float x, float y, float width, float height)
{
    if (id >= MAX_TEXTURES || !textures[id].initialized) {
        printf("Attempted to draw invalid texture ID \"%lu\".", id);
        return;
    }

    C3D_TexBind(0, &textures[id].tex);
    C3D_TexEnv *env = C3D_GetTexEnv(0);
    C3D_TexEnvSrc(env, C3D_Both, GPU_TEXTURE0, 0, 0);
    C3D_TexEnvOp(env, C3D_Both, 0, 0, 0);
    C3D_TexEnvFunc(env, C3D_Both, GPU_REPLACE);
    screen_draw_quad(x, y, x + width, y + height, 0, 0, (float)textures[id].width / (float)textures[id].pow2Width,
                     (float)textures[id].height / (float)textures[id].pow2Height);
}

void screen_get_texture_size(u32 *width, u32 *height, u32 id)
{
    if (id >= MAX_TEXTURES || !textures[id].initialized) {
        printf("Attempted to get size of invalid texture ID \"%lu\".", id);
        return;
    }

    if (width) {
        *width = textures[id].width;
    }

    if (height) {
        *height = textures[id].height;
    }
}
void screen_load_texture_file(u32 id, const char *path, bool linearFilter)
{
    if (id >= MAX_TEXTURES) {
        printf("Attempted to load path \"%s\" to invalid texture ID \"%lu\".", path, id);
        return;
    }

    FILE *fd = screen_open_resource(path);
    if (fd == NULL) {
        printf("Failed to load PNG file \"%s\"", path);
        return;
    }

    int width;
    int height;
    int depth;
    u8 *image = stbi_load_from_file(fd, &width, &height, &depth, STBI_rgb_alpha);
    fclose(fd);

    if (image == NULL || depth != STBI_rgb_alpha) {
        printf("Failed to load PNG file \"%s\".", path);
        return;
    }

    for (u32 x = 0; x < width; x++) {
        for (u32 y = 0; y < height; y++) {
            u32 pos = (y * width + x) * 4;

            u8 c1 = image[pos + 0];
            u8 c2 = image[pos + 1];
            u8 c3 = image[pos + 2];
            u8 c4 = image[pos + 3];

            image[pos + 0] = c4;
            image[pos + 1] = c3;
            image[pos + 2] = c2;
            image[pos + 3] = c1;
        }
    }

    screen_load_texture(id, image, (u32)(width * height * 4), (u32)width, (u32)height, GPU_RGBA8, linearFilter);

    free(image);
}
void renderBG()
{
    //    C3D_FVUnifMtx4x4(GPU_VERTEX_SHADER, uLoc_projection, &projection);
    C3D_FVUnifMtx4x4(GPU_VERTEX_SHADER, shaderInstanceGetUniformLocation(program.vertexShader, "projection"), &projection);

    u32 topScreenBgWidth = 0;
    u32 topScreenBgHeight = 0;
    u32 bannerWidth = 0;
    u32 bannerHeight = 0;
    screen_get_texture_size(&topScreenBgWidth, &topScreenBgHeight, TEXTURE_SCREEN_TOP_SPLASH_BG);
    screen_draw_texture(TEXTURE_SCREEN_TOP_SPLASH_BG, (TOP_SCREEN_WIDTH - topScreenBgWidth) / 2,
                        (TOP_SCREEN_HEIGHT - topScreenBgHeight) / 2, topScreenBgWidth, topScreenBgHeight);

    screen_get_texture_size(&bannerWidth, &bannerHeight, TEXTURE_APP_BANNER);
    screen_draw_texture(TEXTURE_APP_BANNER, (TOP_SCREEN_WIDTH - bannerWidth) / 2, (TOP_SCREEN_HEIGHT - bannerHeight) / 2,
                        bannerWidth, bannerHeight);
}

void loadTextures()
{
    screen_load_texture_file(TEXTURE_SCREEN_TOP_SPLASH_BG, "sky.png", true);
    screen_load_texture_file(TEXTURE_SCREEN_TOP_BG, "background.png", true);
    screen_load_texture_file(TEXTURE_APP_BANNER, "banner.png", true);
    screen_load_texture_file(TEXTURE_PROGRESS_BAR, "pbar.png", true);
    screen_load_texture_file(TEXTURE_FLAG_EUR, "eur16.png", false);
    screen_load_texture_file(TEXTURE_FLAG_USA, "usa16.png", true);
    screen_load_texture_file(TEXTURE_FLAG_JPN, "jpn16.png", true);
    screen_load_texture_file(TEXTURE_FLAG_ALL, "all16.png", true);
}

void sceneInit(void)
{
    if (!C3D_Init(C3D_DEFAULT_CMDBUF_SIZE * 4)) {
        printf("Failed to initialize the GPU.");
        return;
    }
    top_console.cursorX = 0;
    top_console.cursorY = 0;
    c3dInitialized = true;

    // Initialize the render target
    target_top = C3D_RenderTargetCreate(TOP_SCREEN_HEIGHT, TOP_SCREEN_WIDTH, GPU_RB_RGBA8, GPU_RB_DEPTH24_STENCIL8);
    if (target_top == NULL) {
        printf("Failed to initialize the top screen target.\n");
        return;
    }
    C3D_RenderTargetSetClear(target_top, C3D_CLEAR_ALL, CLEAR_COLOR, 0);
    C3D_RenderTargetSetOutput(target_top, GFX_TOP, GFX_LEFT, DISPLAY_TRANSFER_FLAGS);

    Result res = fontEnsureMapped();

    if (R_FAILED(res))
        printf("fontEnsureMapped: %08lX\n", res);

    // Load the vertex shader, create a shader program and bind it
    vshader_dvlb = DVLB_ParseFile((u32 *)vshader_shbin, vshader_shbin_len);
    if (vshader_dvlb == NULL) {
        printf("Failed to parse shader.\n");
        return;
    }
    Result progInitRes = shaderProgramInit(&program);
    if (R_FAILED(progInitRes)) {
        printf("Failed to initialize shader program: 0x%08lX", progInitRes);
        return;
    }
    shaderInitialized = true;

    Result progSetVshRes = shaderProgramSetVsh(&program, &vshader_dvlb->DVLE[0]);
    if (R_FAILED(progSetVshRes)) {
        printf("Failed to set up vertex shader: 0x%08lX", progSetVshRes);
        return;
    }

    C3D_BindProgram(&program);

    C3D_AttrInfo *attrInfo = C3D_GetAttrInfo();
    if (attrInfo == NULL) {
        printf("Failed to retrieve attribute info.");
        return;
    }

    // Get the location of the uniforms
    uLoc_projection = shaderInstanceGetUniformLocation(program.vertexShader, "projection");

    // Configure attributes for use with the vertex shader
    AttrInfo_Init(attrInfo);
    AttrInfo_AddLoader(attrInfo, 0, GPU_FLOAT, 3); // v0=position
    AttrInfo_AddLoader(attrInfo, 1, GPU_FLOAT, 2); // v1=texcoord

    // Compute the projection matrix
    Mtx_OrthoTilt(&projection, 0.0, 400.0, 240.0, 0.0, 0.0, 1.0);

    // Configure depth test to overwrite pixels with the same depth (needed to draw overlapping glyphs)
    C3D_DepthTest(true, GPU_GEQUAL, GPU_WRITE_ALL);

    // Load the glyph texture sheets
    int i;
    TGLP_s *glyphInfo = fontGetGlyphInfo();
    glyphSheets = malloc(sizeof(C3D_Tex) * glyphInfo->nSheets);
    for (i = 0; i < glyphInfo->nSheets; i++) {
        C3D_Tex *tex = &glyphSheets[i];
        tex->data = fontGetGlyphSheetTex(i);
        tex->fmt = glyphInfo->sheetFmt;
        tex->size = glyphInfo->sheetSize;
        tex->width = glyphInfo->sheetWidth;
        tex->height = glyphInfo->sheetHeight;
        tex->param = GPU_TEXTURE_MAG_FILTER(GPU_LINEAR) | GPU_TEXTURE_MIN_FILTER(GPU_LINEAR) |
                     GPU_TEXTURE_WRAP_S(GPU_CLAMP_TO_EDGE) | GPU_TEXTURE_WRAP_T(GPU_CLAMP_TO_EDGE);
    }

    // Create the text vertex array
    textVtxArray = (textVertex_s *)linearAlloc(sizeof(textVertex_s) * TEXT_VTX_ARRAY_COUNT);
    loadTextures();
}

void ui_printf(char *output) {
//    uiConsole.buffer.append(output);
    //renderText(uiConsole.cursorX, uiConsole.cursorY, 0.5f, 0.5f, false, uiConsole.buffer);
    printfText(&top_console.cursorX, &top_console.cursorY, 0.5f, 0.5f, false, output);
}
