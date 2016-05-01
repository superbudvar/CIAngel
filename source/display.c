#include "display.h"

#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <3ds.h>
#include <citro3d.h>
#include "vshader_shbin.h"

typedef struct { float position[3]; float texcoord[2]; } textVertex_s;

static DVLB_s* vshader_dvlb;
static shaderProgram_s program;
static int uLoc_projection;
static C3D_Mtx projection;
static C3D_Tex* glyphSheets;
static C3D_RenderTarget* target_top;
static C3D_RenderTarget* target_bottom;
static textVertex_s* textVtxArray;
static int textVtxArrayPos;
static float fontSize = 0.5f;

#define TEXT_VTX_ARRAY_COUNT (4*1024)
static bool c3dInitialized;
static bool shaderInitialized;

void sceneInit(void)
{
    if(!C3D_Init(C3D_DEFAULT_CMDBUF_SIZE * 4)) {
        printf("Failed to initialize the GPU.");
        return;
    }

    c3dInitialized = true;

    // Initialize the render target
	target_top = C3D_RenderTargetCreate(TOP_SCREEN_HEIGHT, TOP_SCREEN_WIDTH, GPU_RB_RGBA8, GPU_RB_DEPTH24_STENCIL8);
    if(target_top == NULL) {
       printf("Failed to initialize the top screen target.\n");
        return;
    }
	C3D_RenderTargetSetClear(target_top, C3D_CLEAR_ALL, CLEAR_COLOR, 0);
	C3D_RenderTargetSetOutput(target_top, GFX_TOP, GFX_LEFT, DISPLAY_TRANSFER_FLAGS);
    
	Result res = fontEnsureMapped();

	if (R_FAILED(res))
		printf("fontEnsureMapped: %08lX\n", res);
	
	// Load the vertex shader, create a shader program and bind it
	vshader_dvlb = DVLB_ParseFile((u32*)vshader_shbin, vshader_shbin_len);
    if(vshader_dvlb == NULL) {
        printf("Failed to parse shader.\n");
        return;
    }
    Result progInitRes = shaderProgramInit(&program);
    if(R_FAILED(progInitRes)) {
        printf("Failed to initialize shader program: 0x%08lX", progInitRes);
        return;
    }
    shaderInitialized = true;

    Result progSetVshRes = shaderProgramSetVsh(&program, &vshader_dvlb->DVLE[0]);
    if(R_FAILED(progSetVshRes)) {
        printf("Failed to set up vertex shader: 0x%08lX", progSetVshRes);
        return;
    }

    C3D_BindProgram(&program);

    C3D_AttrInfo* attrInfo = C3D_GetAttrInfo();
    if(attrInfo == NULL) {
        printf("Failed to retrieve attribute info.");
        return;
    }
	C3D_BindProgram(&program);

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
	TGLP_s* glyphInfo = fontGetGlyphInfo();
	glyphSheets = malloc(sizeof(C3D_Tex)*glyphInfo->nSheets);
	for (i = 0; i < glyphInfo->nSheets; i ++)
	{
		C3D_Tex* tex = &glyphSheets[i];
		tex->data = fontGetGlyphSheetTex(i);
		tex->fmt = glyphInfo->sheetFmt;
		tex->size = glyphInfo->sheetSize;
		tex->width = glyphInfo->sheetWidth;
		tex->height = glyphInfo->sheetHeight;
		tex->param = GPU_TEXTURE_MAG_FILTER(GPU_LINEAR) | GPU_TEXTURE_MIN_FILTER(GPU_LINEAR)
			| GPU_TEXTURE_WRAP_S(GPU_CLAMP_TO_EDGE) | GPU_TEXTURE_WRAP_T(GPU_CLAMP_TO_EDGE);
	}

	// Create the text vertex array
	textVtxArray = (textVertex_s*)linearAlloc(sizeof(textVertex_s)*TEXT_VTX_ARRAY_COUNT);
}

void setTextColor(u32 color)
{
	C3D_TexEnv* env = C3D_GetTexEnv(0);
	C3D_TexEnvSrc(env, C3D_RGB, GPU_CONSTANT, 0, 0);
	C3D_TexEnvSrc(env, C3D_Alpha, GPU_TEXTURE0, GPU_CONSTANT, 0);
	C3D_TexEnvOp(env, C3D_Both, 0, 0, 0);
	C3D_TexEnvFunc(env, C3D_RGB, GPU_REPLACE);
	C3D_TexEnvFunc(env, C3D_Alpha, GPU_MODULATE);
	C3D_TexEnvColor(env, color);
}

static void addTextVertex(float vx, float vy, float tx, float ty)
{
	textVertex_s* vtx = &textVtxArray[textVtxArrayPos++];
	vtx->position[0] = vx;
	vtx->position[1] = vy;
	vtx->position[2] = 0.5f;
	vtx->texcoord[0] = tx;
	vtx->texcoord[1] = ty;
}
void sceneRenderFooter(const char* text) {
	setTextColor(0xFF0000FF); // red
	renderText(16.0f, 210.0f, 0.5f, 0.75f, true, text);
//	setTextColor(0xFF00FF00); // black
//	renderText(0, TOP_SCREEN_HEIGHT-(8), FONT_DEFAULT_SIZE, FONT_DEFAULT_SIZE, false, text);
	sceneDraw();
}
void renderText(float x, float y, float scaleX, float scaleY, bool baseline, const char* text)
{
	ssize_t  units;
	uint32_t code;

	// Configure buffers
	C3D_BufInfo* bufInfo = C3D_GetBufInfo();
	BufInfo_Init(bufInfo);
	BufInfo_Add(bufInfo, textVtxArray, sizeof(textVertex_s), 2, 0x10);

	const uint8_t* p = (const uint8_t*)text;
	float firstX = x;
	u32 flags = GLYPH_POS_CALC_VTXCOORD | (baseline ? GLYPH_POS_AT_BASELINE : 0);
	int lastSheet = -1;
	do
	{
		if (!*p) break;
		units = decode_utf8(&code, p);
		if (units == -1)
			break;
		p += units;
		if (code == '\n')
		{
			x = firstX;
			y += scaleY*fontGetInfo()->lineFeed;
		}
		else if (code > 0)
		{
			int glyphIdx = fontGlyphIndexFromCodePoint(code);
			fontGlyphPos_s data;
			fontCalcGlyphPos(&data, glyphIdx, flags, scaleX, scaleY);

			// Bind the correct texture sheet
			if (data.sheetIndex != lastSheet)
			{
				lastSheet = data.sheetIndex;
				C3D_TexBind(0, &glyphSheets[lastSheet]);
			}

			int arrayIndex = textVtxArrayPos;
			if ((arrayIndex+4) >= TEXT_VTX_ARRAY_COUNT)
				break; // We can't render more characters

			// Add the vertices to the array
			addTextVertex(x+data.vtxcoord.left,  y+data.vtxcoord.bottom, data.texcoord.left,  data.texcoord.bottom);
			addTextVertex(x+data.vtxcoord.right, y+data.vtxcoord.bottom, data.texcoord.right, data.texcoord.bottom);
			addTextVertex(x+data.vtxcoord.left,  y+data.vtxcoord.top,    data.texcoord.left,  data.texcoord.top);
			addTextVertex(x+data.vtxcoord.right, y+data.vtxcoord.top,    data.texcoord.right, data.texcoord.top);

			// Draw the glyph
			C3D_DrawArrays(GPU_TRIANGLE_STRIP, arrayIndex, 4);

			x += data.xAdvance;

		}
	} while (code > 0);
}

void sceneRender(float size)
{
	// Update the uniforms
	C3D_FVUnifMtx4x4(GPU_VERTEX_SHADER, uLoc_projection, &projection);

	const char* teststring =
		"Hello World - working text rendering!\n"
		"The quick brown fox jumped over the lazy dog.\n"
		"\n"
		"En français: Vous ne devez pas éteindre votre console.\n"
		"日本語も見せるのが出来ますよ。\n"
		"Un poco de texto en español nunca queda mal.\n"
		"Some more random text in English.\n"
		"В чащах юга жил бы цитрус? Да, но фальшивый экземпляр!\n"
		;

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

void sceneDraw() {
	if(!target_top) {
		printf("no target");
		return;
	}
	// Render the scene
	C3D_FrameBegin(C3D_FRAME_SYNCDRAW);
	textVtxArrayPos = 0; // Clear the text vertex array
	C3D_FrameDrawOn(target_top);
	C3D_FrameEnd(0);
}
void sceneExit(void)
{
	// Free the textures
	free(glyphSheets);

	// Free the shader program
	shaderProgramFree(&program);
	DVLB_Free(vshader_dvlb);
}
