#include "render.hpp"

#include <citro2d.h>

#include "mesher.hpp"
#include "world.hpp"

#include "coal_ore_t3x.h"
#include "cobblestone_t3x.h"
#include "cursor_t3x.h"
#include "dirt_t3x.h"
#include "grass_side_t3x.h"
#include "grass_top_t3x.h"
#include "planks_oak_t3x.h"
#include "sand_t3x.h"
#include "stone_t3x.h"

#include "terrain_shbin.h"
#include "focus_shbin.h"

namespace {

#define CLEAR_COLOR 0x68B0D8FF

#define DISPLAY_TRANSFER_FLAGS \
	(GX_TRANSFER_FLIP_VERT(0) | GX_TRANSFER_OUT_TILED(0) | GX_TRANSFER_RAW_COPY(0) | \
	GX_TRANSFER_IN_FORMAT(GX_TRANSFER_FMT_RGBA8) | GX_TRANSFER_OUT_FORMAT(GX_TRANSFER_FMT_RGB8) | \
	GX_TRANSFER_SCALING(GX_TRANSFER_SCALE_NO))


struct {
	struct {
		DVLB_s* dvlb;
		shaderProgram_s program;
		struct {
			int projection;//, modelView;
			int lightVec, lightClr, material, chunkPos;
		} locs;
	} block;
	struct {
		DVLB_s* dvlb;
		shaderProgram_s program;
		struct {
			int projection, /*modelView,*/ blockPos;
		} locs;
	} focus;
} shaders;


struct {
	C3D_AttrInfo block;
	C3D_AttrInfo focus;
} vertexLayouts;

struct 
{
	C3D_TexEnv block;
	C3D_TexEnv focus;
} texEnvs;

void *focusvbo;
C3D_BufInfo focusBuffer;

static C3D_Mtx projection; // todo unnecessary static
static C3D_Mtx material = // todo use two vectors instead
{
	{ // ABGR
	//{ { 0.0f, 0.4f, 0.3f, 0.2f } }, // Ambient
	//{ { 0.0f, 0.5f, 0.7f, 0.8f } }, // Diffuse
	{ { 0.0f, 0.9f, 0.9f, 0.9f } }, // Ambient
	{ { 0.0f, 0.0f, 0.0f, 0.0f } }, // Diffuse
	}
};

const fvec3 focusVtx[] = {
	{-0.01f, -0.01f, -0.01f},
	{+1.01f, -0.01f, -0.01f},
	{+1.01f, +1.01f, -0.01f},
	{-0.01f, +1.01f, -0.01f},
	{-0.01f, -0.01f, +1.01f},
	{+1.01f, -0.01f, +1.01f},
	{+1.01f, +1.01f, +1.01f},
	{-0.01f, +1.01f, +1.01f}
};

const uint8_t basicCubeIs[] = {
	// -X
	3, 0, 4, 3, 4, 7,
	// +X
	1, 2, 6, 1, 6, 5,
	// -Y
	0, 1, 5, 0, 5, 4,
	// +Y
	2, 3, 7, 2, 7, 6,
	// -Z
	1, 0, 3, 1, 3, 2,
	// +Z
	4, 5, 6, 4, 6, 7,
};

C3D_RenderTarget *targetBottom, *targetLeft, *targetRight;

C3D_FogLut fog;

constexpr float farPlane = (renderDistance + 1) * chunkSize;

std::array<C3D_Tex, textureCount> textures;
// cursor, dirt, grass s, grass t, stone

void topUI() {
	C2D_Image image;
	image.tex = &textures[0];
	Tex3DS_SubTexture sub { 16, 16, 0, 0, 1, 1 };
	image.subtex = &sub;
	
	C3D_AlphaBlend(
		GPU_BLEND_ADD, GPU_BLEND_ADD, 
		GPU_SRC_ALPHA, GPU_ONE_MINUS_SRC_ALPHA, 
		GPU_SRC_ALPHA, GPU_ONE_MINUS_SRC_ALPHA
	);
	int scale = 2;
	C2D_Tint tint {0xaa'ff'ff'ff, 1.0f };
	C2D_ImageTint it { { tint, tint, tint, tint } };
	C2D_DrawImageAt(image, 200-scale*8, 120-scale*8, 1, &it, scale, scale);
}

void bottomUI() {
	const int blockCount = 8;
	const int size = 32;
	Tex3DS_SubTexture sub { size, size, 0, 1, 1, 0 };
	C2D_Image img;
	img.subtex = &sub;
	const int spacing = 40;
	for (int i = 1; i < blockCount; ++i) {
		int x = i % 4;
		int y = i / 4;
		img.tex = &textures[getBlockVisual(i)[0] + 1]; // -Y
		C2D_DrawImageAt(img, 
			160 - (4 * spacing / 2) + (spacing - size) / 2 + x * spacing, 
			120 - (2 * spacing / 2) + (spacing - size) / 2 + y * spacing, 
			1);
	}
}

// Helper function for loading a texture from memory
bool loadTextureFromMem(C3D_Tex* tex, C3D_TexCube* cube, const void* data, size_t size) {
	Tex3DS_Texture t3x = Tex3DS_TextureImport(data, size, tex, cube, false);
	if (!t3x)
		return false;

	// Delete the t3x object since we don't need it
	Tex3DS_TextureFree(t3x);
	return true;
}	

void shaderInit() {
	
	shaders.block.dvlb = DVLB_ParseFile((u32*)terrain_shbin, terrain_shbin_size);
	shaderProgramInit(&shaders.block.program);
	shaderProgramSetVsh(&shaders.block.program, &shaders.block.dvlb->DVLE[0]);
	C3D_BindProgram(&shaders.block.program);

	// Get the location of the uniforms
	shaders.block.locs.projection = 
		shaderInstanceGetUniformLocation(shaders.block.program.vertexShader, "projection");
	shaders.block.locs.lightVec = 
		shaderInstanceGetUniformLocation(shaders.block.program.vertexShader, "lightVec");
	shaders.block.locs.lightClr = 
		shaderInstanceGetUniformLocation(shaders.block.program.vertexShader, "lightClr");
	shaders.block.locs.material = 
		shaderInstanceGetUniformLocation(shaders.block.program.vertexShader, "material");
	shaders.block.locs.chunkPos = 
		shaderInstanceGetUniformLocation(shaders.block.program.vertexShader, "chunkPos");

	shaders.focus.dvlb = DVLB_ParseFile((u32*)focus_shbin, focus_shbin_size);
	shaderProgramInit(&shaders.focus.program);
	shaderProgramSetVsh(&shaders.focus.program, &shaders.focus.dvlb->DVLE[0]);
	C3D_BindProgram(&shaders.focus.program);

	// Get the location of the uniforms
	shaders.focus.locs.projection = 
		shaderInstanceGetUniformLocation(shaders.focus.program.vertexShader, "projection");
	shaders.focus.locs.blockPos = 
		shaderInstanceGetUniformLocation(shaders.focus.program.vertexShader, "blockPos");
}

void resourceInit() {
		
	// Configure attributes for use with the vertex shader
	AttrInfo_Init(&vertexLayouts.block);
	AttrInfo_AddLoader(&vertexLayouts.block, 0, GPU_UNSIGNED_BYTE, 3); // v0=position
	AttrInfo_AddLoader(&vertexLayouts.block, 1, GPU_UNSIGNED_BYTE, 2); // v1=texcoord
	AttrInfo_AddLoader(&vertexLayouts.block, 2, GPU_BYTE, 3); // v2=normal
	AttrInfo_AddLoader(&vertexLayouts.block, 3, GPU_UNSIGNED_BYTE, 1); // v3=ao
			
	AttrInfo_Init(&vertexLayouts.focus);
	AttrInfo_AddLoader(&vertexLayouts.focus, 0, GPU_FLOAT, 3); // v0=position

	// todo optimize it with arrays or something
	if (!loadTextureFromMem(&textures[0], nullptr, cursor_t3x, cursor_t3x_size))
		svcBreak(USERBREAK_PANIC);
	if (!loadTextureFromMem(&textures[1], nullptr, dirt_t3x, dirt_t3x_size))
		svcBreak(USERBREAK_PANIC);
	if (!loadTextureFromMem(&textures[2], nullptr, grass_side_t3x, grass_side_t3x_size))
		svcBreak(USERBREAK_PANIC);
	if (!loadTextureFromMem(&textures[3], nullptr, grass_top_t3x, grass_top_t3x_size))
		svcBreak(USERBREAK_PANIC);
	if (!loadTextureFromMem(&textures[4], nullptr, stone_t3x, stone_t3x_size))
		svcBreak(USERBREAK_PANIC);
	if (!loadTextureFromMem(&textures[5], nullptr, coal_ore_t3x, coal_ore_t3x_size))
		svcBreak(USERBREAK_PANIC);
	if (!loadTextureFromMem(&textures[6], nullptr, cobblestone_t3x, cobblestone_t3x_size))
		svcBreak(USERBREAK_PANIC);
	if (!loadTextureFromMem(&textures[7], nullptr, sand_t3x, sand_t3x_size))
		svcBreak(USERBREAK_PANIC);
	if (!loadTextureFromMem(&textures[8], nullptr, planks_oak_t3x, planks_oak_t3x_size))
		svcBreak(USERBREAK_PANIC);
		
	for (auto &t: textures) {
		C3D_TexSetFilter(&t, GPU_NEAREST, GPU_LINEAR);
		C3D_TexSetWrap(&t, GPU_REPEAT, GPU_REPEAT);
	}

	// Configure the first fragment shading substage to blend the texture color with
	// the vertex color (calculated by the vertex shader using a lighting algorithm)
	// See https://www.opengl.org/sdk/docs/man2/xhtml/glTexEnv.xml for more insight
	C3D_TexEnvInit(&texEnvs.block);
	C3D_TexEnvSrc(&texEnvs.block, C3D_Both, GPU_TEXTURE0, GPU_PRIMARY_COLOR, GPU_PRIMARY_COLOR);
	C3D_TexEnvFunc(&texEnvs.block, C3D_Both, GPU_MODULATE);

	C3D_TexEnvInit(&texEnvs.focus);
	C3D_TexEnvSrc(&texEnvs.focus, C3D_Both, GPU_PRIMARY_COLOR, GPU_PRIMARY_COLOR, GPU_PRIMARY_COLOR);
	C3D_TexEnvFunc(&texEnvs.focus, C3D_Both, GPU_REPLACE);

	focusvbo = linearAlloc(3*4*8 + 2*3*6);
	memcpy(focusvbo, focusVtx, 4*3*8);
	memcpy((u8*)focusvbo + 4*3*8, basicCubeIs, 2*3*6);
	BufInfo_Init(&focusBuffer);
	BufInfo_Add(&focusBuffer, focusvbo, 4*3, 1, 0x0);
}

}

void renderInit() {

	gfxInitDefault();
	C3D_Init(C3D_DEFAULT_CMDBUF_SIZE);
	C2D_Init(C2D_DEFAULT_MAX_OBJECTS);
	#ifdef CONSOLE
		consoleInit(GFX_BOTTOM, NULL);
	#else
		targetBottom = C3D_RenderTargetCreate(240, 320, GPU_RB_RGBA8, GPU_RB_DEPTH16);
		C3D_RenderTargetSetOutput(targetBottom, GFX_BOTTOM, GFX_LEFT, DISPLAY_TRANSFER_FLAGS);
	#endif
	targetLeft  = C3D_RenderTargetCreate(240, 400, GPU_RB_RGBA8, GPU_RB_DEPTH24_STENCIL8);
	targetRight = C3D_RenderTargetCreate(240, 400, GPU_RB_RGBA8, GPU_RB_DEPTH24_STENCIL8);
	C3D_RenderTargetSetOutput(targetLeft,  GFX_TOP, GFX_LEFT,  DISPLAY_TRANSFER_FLAGS);
	C3D_RenderTargetSetOutput(targetRight, GFX_TOP, GFX_RIGHT, DISPLAY_TRANSFER_FLAGS);

	FogLut_Exp(&fog, 0.02f, 1.5f, 0.2f, farPlane);

	shaderInit();
	resourceInit();
}

// this is not working well... debug it later, too tired for this now
bool inFrustum(C3D_Mtx const &mtx, s16vec3 v) {
	fvec3 vmin { 2, 2, 2 }; fvec3 vmax { -2, -2, -2 };
	for (int z = 0; z < 2; ++z)
		for (int y = 0; y < 2; ++y)
			for (int x = 0; x < 2; ++x) {
				auto vf = FVec4_New((v.x+x)*16, (v.y+y)*16, (v.z+z)*16, 1);
				vf = Mtx_MultiplyFVec4(&mtx, vf);
				vf = FVec4_PerspDivide(vf);
				if (vf.z > 0) continue;
				
				vmin.x = std::min(vf.x, vmin.x);
				vmin.y = std::min(vf.y, vmin.y);

				vmax.x = std::max(vf.x, vmax.x);
				vmax.y = std::max(vf.y, vmax.y);
			}

	bool out = 
		vmin.x > 1 || vmax.x < -1 ||   
		vmin.y > 1 || vmax.y < -1;
	return !out;
}

void sceneRender(fvec3 &camera, float rx, float ry, vec3<s32> *focus, float iod) {

	/// --- CALCULATE VIEW --- ///

	C3D_FogGasMode(GPU_FOG, GPU_PLAIN_DENSITY, false);
	C3D_FogColor(0xD8B068);
	C3D_FogLutBind(&fog);

	C3D_CullFace(GPU_CULL_BACK_CCW);

	C3D_Mtx modelView;
	Mtx_Identity(&modelView);

	Mtx_RotateX(&modelView, ry, true);
	Mtx_RotateY(&modelView, rx, true);

	Mtx_RotateX(&modelView, -M_PI/2, true);

	Mtx_Translate(&modelView, -camera.x, -camera.y, -camera.z, true);

	Mtx_PerspStereoTilt(
		&projection, 
		C3D_AngleFromDegrees(70.0f), 
		C3D_AspectRatioTop, 
		0.2f, farPlane, 
		iod, 1.5f, false
		);

	C3D_Mtx combined;
	Mtx_Multiply(&combined, &projection, &modelView);
	projection = combined;

	/// --- SETUP BLOCK RENDERING --- ///

	C3D_SetTexEnv(0, &texEnvs.block);
	C3D_AlphaBlend(
		GPU_BLEND_ADD, GPU_BLEND_ADD, 
		GPU_SRC_ALPHA, GPU_ONE_MINUS_SRC_ALPHA, 
		GPU_SRC_ALPHA, GPU_ONE_MINUS_SRC_ALPHA
	);
	C3D_BindProgram(&shaders.block.program);

	// Update the uniforms
	C3D_FVUnifMtx4x4(GPU_VERTEX_SHADER, shaders.block.locs.projection, &projection);
	C3D_FVUnifMtx4x4(GPU_VERTEX_SHADER, shaders.block.locs.material,   &material);
	C3D_FVUnifSet(GPU_VERTEX_SHADER, shaders.block.locs.lightVec,     0.0f, 0.6f, -0.8f, 0.0f);
	C3D_FVUnifSet(GPU_VERTEX_SHADER, shaders.block.locs.lightClr,     1.0f, 1.0f,  1.0f, 1.0f);

	C3D_SetAttrInfo(&vertexLayouts.block);

	//todo organize
	int chX = static_cast<int>(camera.x) >> chunkBits;
	int chY = static_cast<int>(camera.y) >> chunkBits;
	int chZ = static_cast<int>(camera.z) >> chunkBits;
	int maxDist2 = renderDistance*renderDistance;

	/// --- DRAW BLOCKS --- ///

	int count = 0;
	for (auto [idx, meta]: world) {
		int dx = idx.x-chX;
		int dy = idx.y-chY;
		int dz = idx.z-chZ;
		// if (idx.x != 0 || idx.y != 0 || idx.z != 0) continue; // to the solitary mode
		int distance2 = dx*dx+dy*dy+dz*dz;
		 // todo do this check once for stereo rendering
		if (
			(distance2 <= maxDist2) && 
			meta.meshed && 
			meta.allocation.vertexCount && 
			(distance2 < 4 || inFrustum(projection, idx)) // frustum is buggy, always immediate neighbourhood 
		) {
			++count;
			C3D_SetBufInfo(&meta.vertexBuffer);
			C3D_FVUnifSet(
				GPU_VERTEX_SHADER, 
				shaders.block.locs.chunkPos,     
				idx.x * chunkSize, idx.y * chunkSize, idx.z * chunkSize, 0.0f
			);
			u16 offset = 0;
			for (auto &m: meta.allocation.meshes) {
				// if ((idx.x^idx.y^idx.z) & 1) continue; // funk mode
				// if (idx.x&1 || idx.y & 1 || idx.z & 1) continue; // DISCO FEVER MODE
				C3D_TexBind(0, &textures[m.texture + 1]);
				C3D_DrawElements(
					GPU_TRIANGLES, 
					m.count, 
					C3D_UNSIGNED_SHORT, 
					(u16*)meta.allocation.indices + offset
				);
				offset += m.count;
			}
		}
	}
	#ifdef CONSOLE
		static int i = 0;
		if (i++ == 30) i = 0;
		if (i == 0) printf("Chunks drawn: %i\n", count);
	#endif

	/// --- DRAW FOCUS HIGHLIGHT --- ///

	C3D_FogLutBind(nullptr);

	if (focus) {
		C3D_SetTexEnv(0, &texEnvs.focus);
		C3D_AlphaBlend(
			GPU_BLEND_ADD, GPU_BLEND_ADD,
			GPU_SRC_ALPHA, GPU_ONE, 
			GPU_SRC_ALPHA, GPU_ONE
		);
		C3D_BindProgram(&shaders.focus.program);

		// Update the uniforms
		C3D_FVUnifMtx4x4(GPU_VERTEX_SHADER, shaders.focus.locs.projection, &projection);
		// C3D_FVUnifMtx4x4(GPU_VERTEX_SHADER, shaders.focus.locs.modelView,  &modelView);
		C3D_FVUnifSet(GPU_VERTEX_SHADER, shaders.focus.locs.blockPos, 
			focus->x, focus->y, focus->z, 0.0f
		);

		C3D_SetAttrInfo(&vertexLayouts.focus);
		C3D_SetBufInfo(&focusBuffer);
		C3D_DrawElements(
			GPU_TRIANGLES, 
			2*3*6, 
			C3D_UNSIGNED_BYTE, 
			(u8*)focusvbo + 3*4*8
		);
	}
}

void render(fvec3 &playerPos, float rx, float ry, vec3<s32> *focus, float depthSlider) {
	float iod = depthSlider / 5; 
	bool should3d = iod > 0.01f;
	if (should3d != gfxIs3D()) 
		gfxSet3D(should3d);

	float eyeHeight = 1.5f;

	fvec3 camera = playerPos;
	camera.z += eyeHeight;

	// Render the scene
	C3D_FrameBegin(C3D_FRAME_SYNCDRAW);
	{
		// left or main buffer
		C3D_RenderTargetClear(targetLeft, C3D_CLEAR_ALL, CLEAR_COLOR, 0);
		C3D_FrameDrawOn(targetLeft);
		C2D_SceneTarget(targetLeft);
		sceneRender(camera, rx, ry, focus, -iod);
		C2D_Prepare();
		topUI();
		C2D_Flush();
		if (should3d)
		{
			// right buffer
			C3D_RenderTargetClear(targetRight, C3D_CLEAR_ALL, CLEAR_COLOR, 0);
			C3D_FrameDrawOn(targetRight);
			C2D_SceneTarget(targetRight);
			sceneRender(camera, rx, ry, focus, iod);
			C2D_Prepare();
			topUI();
			C2D_Flush();
		}

		// bottom buffer
		#ifndef CONSOLE
			C3D_RenderTargetClear(targetBottom, C3D_CLEAR_ALL, 0x222222ff, 0);
			C3D_FrameDrawOn(targetBottom);
			C2D_SceneTarget(targetBottom);
			C2D_Prepare();
			bottomUI();
			C2D_Flush();
		#endif
	}
	C3D_FrameEnd(0);
}

void renderExit() {

	for (auto &t: textures)
		C3D_TexDelete(&t);

	shaderProgramFree(&shaders.block.program);
	DVLB_Free(shaders.block.dvlb);
	shaderProgramFree(&shaders.focus.program);
	DVLB_Free(shaders.focus.dvlb);  

	// todo: verify if there are other resources to free
	
	C3D_Fini();
	C2D_Fini();	
	gfxExit();
 
}
