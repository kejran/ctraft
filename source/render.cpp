#include "render.hpp"

#include <citro2d.h>

#include "mesher.hpp"
#include "world.hpp"

#include "coal_ore_pt3x.h"
#include "cobblestone_pt3x.h"
#include "cursor_pt3x.h"
#include "dirt_pt3x.h"
#include "grass_side_pt3x.h"
#include "grass_top_pt3x.h"
#include "planks_oak_pt3x.h"
#include "sand_pt3x.h"
#include "stone_pt3x.h"
#include "tallgrass_pt3x.h"
#include "log_oak_pt3x.h"
#include "log_oak_top_pt3x.h"

#include "terrain_shbin.h"
#include "focus_shbin.h"
#include "sky_shbin.h"

namespace {

#define CLEAR_COLOR 0x9999B2FF

#define DISPLAY_TRANSFER_FLAGS \
	(GX_TRANSFER_FLIP_VERT(0) | GX_TRANSFER_OUT_TILED(0) | GX_TRANSFER_RAW_COPY(0) | \
	GX_TRANSFER_IN_FORMAT(GX_TRANSFER_FMT_RGBA8) | GX_TRANSFER_OUT_FORMAT(GX_TRANSFER_FMT_RGB8) | \
	GX_TRANSFER_SCALING(GX_TRANSFER_SCALE_NO))

struct {
	struct {
		DVLB_s* dvlb;
		shaderProgram_s program;
		struct {
			int projection;
			int lightVec, lightClr, material, chunkPos;
		} locs;
	} block;
	struct {
		DVLB_s* dvlb;
		shaderProgram_s program;
		struct {
			int projection, blockPos;
		} locs;
	} focus;
	struct {
		DVLB_s* dvlb;
		shaderProgram_s program;
		struct {
			int projection;
		} locs;
	} sky;
} shaders;


struct {
	C3D_AttrInfo block;
	C3D_AttrInfo focus;
	C3D_AttrInfo sky;
} vertexLayouts;

struct
{
	C3D_TexEnv textured;
	C3D_TexEnv solid;
} texEnvs;

void *focusvbo;
C3D_BufInfo focusBuffer;

void *skyvbo;
C3D_BufInfo skyBuffer;

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

C3D_RenderTarget *targetBottom = nullptr, *targetLeft, *targetRight;

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
		img.tex = &textures[getBlockVisual(Block::solid(i-1)).faces[0] + 1]; // -Y
		C2D_DrawImageAt(img,
			160 - (4 * spacing / 2) + (spacing - size) / 2 + x * spacing,
			120 - (2 * spacing / 2) + (spacing - size) / 2 + y * spacing,
			1);
	}
}

C2D_Text loadText;
C2D_TextBuf loadTextBuf;

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

	shaders.focus.locs.projection =
		shaderInstanceGetUniformLocation(shaders.focus.program.vertexShader, "projection");
	shaders.focus.locs.blockPos =
		shaderInstanceGetUniformLocation(shaders.focus.program.vertexShader, "blockPos");

	shaders.sky.dvlb = DVLB_ParseFile((u32*)sky_shbin, sky_shbin_size);
	shaderProgramInit(&shaders.sky.program);
	shaderProgramSetVsh(&shaders.sky.program, &shaders.sky.dvlb->DVLE[0]);
	C3D_BindProgram(&shaders.sky.program);

	shaders.sky.locs.projection =
		shaderInstanceGetUniformLocation(shaders.sky.program.vertexShader, "projection");
}

const int skyW = 16;
const int skyH = 10;
static_assert(skyH * skyW + 1 < 256);

struct skyVertex {
	float x;
	float y;
	float z;
	u32 color;
};
int skyIndexOffset;
int skyIndexCount;

// 0..1 angle from horizon
u32 skyColorAt(float angle) {

	float a = 1 - angle;

	float r = pow(a, 10) * 0.7f + 0.1f;
    float g = pow(a, 5) * 0.7f + 0.1f;
    float b = pow(a, 2) * 0.6f + 0.3f;

	int rr = r * 255;
	int gg = g * 255;
	int bb = b * 255;

	return (0xff << 24 | bb << 16 | gg << 8 | rr);
}

void generateSky() {

	std::array<skyVertex, skyH * skyW + 1> vertices;
	std::vector<u8> indices;

	for (int y = 0; y < skyH; ++y) {

		float angle = std::pow(((float)y) / skyH, 3);
		float ay = angle * M_PI_2;
		float fy = std::sin(ay);
		float fx = std::cos(ay);
		u32 color = skyColorAt(angle);
		for (int x = 0; x < skyW; ++x) {
			float at = (x/* - y * 0.5f*/) * M_TAU / skyW; // unskew it a bit, doubt it will do much
			vertices[x + y * skyW] = {
				std::sin(at) * fx,
				std::cos(at) * fx,
				fy,
				color
			};
		}
	}

	vertices[skyH * skyW] = { 0, 0, 1, skyColorAt(1) };

	for (int y = 0; y < (skyH-1); ++y) {
		for (int x = 0; x < skyW; ++x) {
			int x2 = (x + 1) % skyW;
			int y2 = y + 1;

			indices.push_back(x + y * skyW);
			indices.push_back(x2 + y * skyW);
			indices.push_back(x2 + y2 * skyW);

			indices.push_back(x + y * skyW);
			indices.push_back(x2 + y2 * skyW);
			indices.push_back(x + y2 * skyW);
		}
	}

	int dy = (skyH - 1) * skyW;
	int dy2 = skyH * skyW;

	for (int x = 0; x < skyW; ++x) {
		int x2 = (x + 1) % skyW;
		indices.push_back(x + dy);
		indices.push_back(x2 + dy);
		indices.push_back(dy2);
	}

	skyIndexOffset = vertices.size() * sizeof(vertices);
	skyIndexCount = indices.size();
	skyvbo = linearAlloc(skyIndexOffset + skyIndexCount);
	memcpy(skyvbo, vertices.data(), skyIndexOffset);
	memcpy((u8*)skyvbo + skyIndexOffset, indices.data(), skyIndexCount);
	BufInfo_Init(&skyBuffer);
	BufInfo_Add(&skyBuffer, skyvbo, sizeof(skyVertex), 2, 0x10);
}

struct textureData_t {
	u8 const *data;
	size_t size;
};

#define _t(x) { x ## _pt3x, x ## _pt3x_size } 
textureData_t textureData[] = {
	_t(cursor),
	_t(dirt),
	_t(grass_side),
	_t(grass_top),
	_t(stone),
	_t(coal_ore),
	_t(cobblestone),
	_t(sand),
	_t(planks_oak),
	_t(tallgrass),
	_t(log_oak),
	_t(log_oak_top),
};
#undef t

void resourceInit() {

	// Configure attributes for use with the vertex shader
	AttrInfo_Init(&vertexLayouts.block);
	AttrInfo_AddLoader(&vertexLayouts.block, 0, GPU_UNSIGNED_BYTE, 3); // v0=position
	AttrInfo_AddLoader(&vertexLayouts.block, 1, GPU_UNSIGNED_BYTE, 2); // v1=texcoord
	AttrInfo_AddLoader(&vertexLayouts.block, 2, GPU_BYTE, 3); // v2=normal
	AttrInfo_AddLoader(&vertexLayouts.block, 3, GPU_UNSIGNED_BYTE, 1); // v3=ao

	AttrInfo_Init(&vertexLayouts.focus);
	AttrInfo_AddLoader(&vertexLayouts.focus, 0, GPU_FLOAT, 3); // v0=position

	AttrInfo_Init(&vertexLayouts.sky);
	AttrInfo_AddLoader(&vertexLayouts.sky, 0, GPU_FLOAT, 3); // v0=position
	AttrInfo_AddLoader(&vertexLayouts.sky, 1, GPU_UNSIGNED_BYTE, 4); // v1=color

	for (int i = 0; i < textureCount; ++i)
		if (!loadTextureFromMem(&textures[i], nullptr, 
			textureData[i].data, textureData[i].size))
			exit(0);
	
	for (auto &t: textures) {
		C3D_TexSetFilter(&t, GPU_NEAREST, GPU_LINEAR);
		C3D_TexSetWrap(&t, GPU_REPEAT, GPU_REPEAT);
	}

	// Configure the first fragment shading substage to blend the texture color with
	// the vertex color (calculated by the vertex shader using a lighting algorithm)
	// See https://www.opengl.org/sdk/docs/man2/xhtml/glTexEnv.xml for more insight
	C3D_TexEnvInit(&texEnvs.textured);
	C3D_TexEnvSrc(&texEnvs.textured, C3D_Both, GPU_TEXTURE0, GPU_PRIMARY_COLOR, GPU_PRIMARY_COLOR);
	C3D_TexEnvFunc(&texEnvs.textured, C3D_Both, GPU_MODULATE);

	C3D_TexEnvInit(&texEnvs.solid);
	C3D_TexEnvSrc(&texEnvs.solid, C3D_Both, GPU_PRIMARY_COLOR, GPU_PRIMARY_COLOR, GPU_PRIMARY_COLOR);
	C3D_TexEnvFunc(&texEnvs.solid, C3D_Both, GPU_REPLACE);

	focusvbo = linearAlloc(3*4*8 + 2*3*6);
	memcpy(focusvbo, focusVtx, 4*3*8);
	memcpy((u8*)focusvbo + 4*3*8, basicCubeIs, 2*3*6);
	BufInfo_Init(&focusBuffer);
	BufInfo_Add(&focusBuffer, focusvbo, 4*3, 1, 0x0);
}

}

void renderInit(bool bottomScreen) {

	gfxInitDefault();
	C3D_Init(C3D_DEFAULT_CMDBUF_SIZE);
	C2D_Init(C2D_DEFAULT_MAX_OBJECTS);
	if (!bottomScreen)
		consoleInit(GFX_BOTTOM, NULL);
	else {
		targetBottom = C3D_RenderTargetCreate(240, 320, GPU_RB_RGBA8, GPU_RB_DEPTH16);
		C3D_RenderTargetSetOutput(targetBottom, GFX_BOTTOM, GFX_LEFT, DISPLAY_TRANSFER_FLAGS);
	}
	targetLeft  = C3D_RenderTargetCreate(240, 400, GPU_RB_RGBA8, GPU_RB_DEPTH24_STENCIL8);
	targetRight = C3D_RenderTargetCreate(240, 400, GPU_RB_RGBA8, GPU_RB_DEPTH24_STENCIL8);
	C3D_RenderTargetSetOutput(targetLeft,  GFX_TOP, GFX_LEFT,  DISPLAY_TRANSFER_FLAGS);
	C3D_RenderTargetSetOutput(targetRight, GFX_TOP, GFX_RIGHT, DISPLAY_TRANSFER_FLAGS);

	FogLut_Exp(&fog, 0.01f, 1.5f, 0.2f, farPlane);

	shaderInit();
	generateSky();
	resourceInit();

	loadTextBuf = C2D_TextBufNew(32);
	C2D_TextParse(&loadText, loadTextBuf, "loading...");
	C2D_TextOptimize(&loadText);
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

int chunksDrawn;

void sceneRender(fvec3 &camera, float rx, float ry, vec3<s32> *focus, float iod) {

	/// --- CALCULATE VIEW --- ///

	C3D_Mtx modelView;
	Mtx_Identity(&modelView);

	Mtx_RotateX(&modelView, ry, true);
	Mtx_RotateY(&modelView, rx, true);

	Mtx_RotateX(&modelView, -M_PI/2, true);

	Mtx_Translate(&modelView, -camera.x, -camera.y, -camera.z, true);

	C3D_Mtx perspective;
	Mtx_PerspStereoTilt(
		&perspective,
		C3D_AngleFromDegrees(70.0f),
		C3D_AspectRatioTop,
		0.2f, farPlane,
		iod, 1.5f, false
		);

	C3D_Mtx localView;
	Mtx_Identity(&localView);
	Mtx_RotateX(&localView, ry, true);
	Mtx_RotateY(&localView, rx, true);
	Mtx_RotateX(&localView, -M_PI/2, true);

	C3D_Mtx projection;
	Mtx_Multiply(&projection, &perspective, &modelView);

	C3D_Mtx projectionSky;
	Mtx_Multiply(&projectionSky, &perspective, &localView);

	/// --- DRAW SKY --- ///

	C3D_TexBind(0, nullptr);
	C3D_FogLutBind(nullptr);
	C3D_SetTexEnv(0, &texEnvs.solid);
	C3D_AlphaBlend(
		GPU_BLEND_ADD, GPU_BLEND_ADD,
		GPU_SRC_ALPHA, GPU_ONE_MINUS_SRC_ALPHA,
		GPU_SRC_ALPHA, GPU_ONE_MINUS_SRC_ALPHA
	);
	C3D_CullFace(GPU_CULL_NONE);
	C3D_DepthTest(false, GPU_GREATER, GPU_WRITE_COLOR);

	C3D_BindProgram(&shaders.sky.program);
	C3D_FVUnifMtx4x4(GPU_VERTEX_SHADER, shaders.sky.locs.projection, &projectionSky);

	C3D_SetAttrInfo(&vertexLayouts.sky);
	C3D_SetBufInfo(&skyBuffer);

	C3D_DrawElements(
		GPU_TRIANGLES, skyIndexCount, C3D_UNSIGNED_BYTE,
		(u8*)skyvbo + skyIndexOffset
	);

	/// --- SETUP BLOCK RENDERING --- ///

	C3D_DepthTest(true, GPU_GREATER, GPU_WRITE_ALL);

	C3D_FogGasMode(GPU_FOG, GPU_PLAIN_DENSITY, false);
	C3D_FogColor(0xB29999);
	C3D_FogLutBind(&fog);

	C3D_SetTexEnv(0, &texEnvs.textured);
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

	chunksDrawn = 0;
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
			++chunksDrawn;
			C3D_SetBufInfo(&meta.vertexBuffer);
			C3D_FVUnifSet(
				GPU_VERTEX_SHADER,
				shaders.block.locs.chunkPos,
				idx.x * chunkSize, idx.y * chunkSize, idx.z * chunkSize, 0.0f
			);
			u16 offset = 0;
			u8 oldFlags = 0xff;
			for (auto &m: meta.allocation.meshes) {
				if (m.flags != oldFlags) {
					C3D_CullFace((m.flags & MesherAllocation::MESH_NOCULL) ?
						GPU_CULL_NONE : GPU_CULL_BACK_CCW);
					C3D_AlphaTest((m.flags & MesherAllocation::MESH_ALPHATEST), 
						GPU_GREATER, 0x7f);
				}
				oldFlags = m.flags;
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

	/// --- DRAW FOCUS HIGHLIGHT --- ///

	C3D_FogLutBind(nullptr);

	if (focus) {
		C3D_SetTexEnv(0, &texEnvs.solid);
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

u32 frameStartTimestamp;
#ifdef PROFILER
u32 lastPrintTimestamp;
u32 debugDelay = SYSCLOCK_ARM11 / 3; // 3hz
#endif

void render(fvec3 &playerPos, float rx, float ry, vec3<s32> *focus, float depthSlider) {
	float iod = depthSlider / 5;
	bool should3d = iod > 0.01f;
	if (should3d != gfxIs3D())
		gfxSet3D(should3d);

	float eyeHeight = 1.5f;

	fvec3 camera = playerPos;
	camera.z += eyeHeight;

	u32 timeBeforeSync = svcGetSystemTick();
	u32 nextTimeAfterSync;

	// Render the scene
	C3D_FrameBegin(C3D_FRAME_SYNCDRAW);
	{
		nextTimeAfterSync = svcGetSystemTick();
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
		if (targetBottom) { // skip drawing if using console
			C3D_RenderTargetClear(targetBottom, C3D_CLEAR_ALL, 0x222222ff, 0);
			C3D_FrameDrawOn(targetBottom);
			C2D_SceneTarget(targetBottom);
			C2D_Prepare();
			bottomUI();
			C2D_Flush();
		}
	}
	C3D_FrameEnd(0);

	#ifdef PROFILER
	if (targetBottom == nullptr) {
		u32 now = svcGetSystemTick();
		if ((now - lastPrintTimestamp) > debugDelay) {
			lastPrintTimestamp += debugDelay; // do so it does not drift, not like it matters
			if ((now - lastPrintTimestamp) > debugDelay) // if the diff is too large, reset
				lastPrintTimestamp = now;

			float fdiff = timeBeforeSync - frameStartTimestamp;
			fdiff *= invTickRate;
			float ftime = fdiff * 1000;
			float fbudget = fdiff * 60 * 100;
			float gpu = C3D_GetDrawingTime()*6;
			float fcustom = _customProfileTime;
			float custom = fcustom * invTickRate * 60 * 100;
			printf("\x1b[1;1f");
			printf("Frame time   : %5.2fms    \n", ftime);
			printf("Frame budget : %4.1f%%    \n", fbudget + 0.1f);
			printf("Draw time    : %4.1f%%    \n", gpu + 0.1f);
			printf("Profile time : %4.1f%%    \n", custom + 0.1f);
			printf("Profile calls: %3i    \n", (int)_customProfileCalls);
			printf("Chunks drawn : %3i    \n", chunksDrawn);
		}
	}
	_customProfileCalls = 0;
	_customProfileTime = 0;
	#endif
	frameStartTimestamp = nextTimeAfterSync;
}

void renderExit() {

	for (auto &t: textures)
		C3D_TexDelete(&t);

	shaderProgramFree(&shaders.block.program);
	DVLB_Free(shaders.block.dvlb);
	shaderProgramFree(&shaders.focus.program);
	DVLB_Free(shaders.focus.dvlb);
	shaderProgramFree(&shaders.sky.program);
	DVLB_Free(shaders.sky.dvlb);

	linearFree(skyvbo);
	linearFree(focusvbo);

	// todo: verify if there are other resources to free

	C3D_Fini();
	C2D_Fini();
	gfxExit();

}


void renderLoading() {

	C3D_FrameBegin(C3D_FRAME_SYNCDRAW);
	{
		// left or main buffer
		C3D_RenderTargetClear(targetLeft, C3D_CLEAR_ALL, 0x222222ff, 0);
		C3D_FrameDrawOn(targetLeft);
		C2D_SceneTarget(targetLeft);
		C2D_Prepare();

		float w, h;
		C2D_TextGetDimensions(&loadText, 1, 1, &w, &h);
		C2D_DrawText(&loadText, C2D_WithColor, 200-w/2, 120-h/2, 1, 1, 1, 0xeeeeeeff);

		C2D_Flush();

		// bottom buffer
		if (targetBottom) // skip drawing if using console
			C3D_RenderTargetClear(targetBottom, C3D_CLEAR_ALL, 0x222222ff, 0);
	}
	C3D_FrameEnd(0);
}