#include <3ds.h>
#include <citro3d.h>
#include <tex3ds.h>
#include <string.h>
#include "vshader_shbin.h"
#include <stdio.h>

#include "common.hpp"
#include "mesher.hpp"
#include "simplex.hpp"

#include "dirt_t3x.h"
#include "grass_side_t3x.h"
#include "grass_top_t3x.h"
#include "stone_t3x.h"

#define CLEAR_COLOR 0x68B0D8FF

#define DISPLAY_TRANSFER_FLAGS \
	(GX_TRANSFER_FLIP_VERT(0) | GX_TRANSFER_OUT_TILED(0) | GX_TRANSFER_RAW_COPY(0) | \
	GX_TRANSFER_IN_FORMAT(GX_TRANSFER_FMT_RGBA8) | GX_TRANSFER_OUT_FORMAT(GX_TRANSFER_FMT_RGB8) | \
	GX_TRANSFER_SCALING(GX_TRANSFER_SCALE_NO))

static std::array<C3D_Tex, textureCount> textures;
// dirt, grass s, grass t, stone 

static DVLB_s* vshader_dvlb;
static shaderProgram_s program;
static int uLoc_projection, uLoc_modelView;
static int uLoc_lightVec, uLoc_lightClr, uLoc_material, uLoc_chunkPos;
static C3D_Mtx projection;
static C3D_Mtx material =
{
	{ // ABGR
	{ { 0.0f, 0.4f, 0.3f, 0.2f } }, // Ambient
	{ { 0.0f, 0.5f, 0.7f, 0.8f } }, // Diffuse
	}
};

struct ChunkMetadata {
	MesherAllocation allocation;
	C3D_BufInfo vertexBuffer;
	chunk *data = nullptr;
	u8 sideSolid = 0;
	bool meshed = false;
};

using WorldMap = std::unordered_map<s16vec3, ChunkMetadata, s16vec3::hash>;

WorldMap world;

ChunkMetadata &fillChunk(s16 cx, s16 cy, s16 cz) {
	auto &meta = world[{cx, cy, cz}];
	meta.data = new chunk();
	int x = cx * chunkSize; int y = cy * chunkSize; int z = cz * chunkSize;

	for (int lx = 0; lx < chunkSize; ++lx)
		for (int ly = 0; ly < chunkSize; ++ly)
			for (int lz = 0; lz < chunkSize; ++lz) {
				int _x = x + lx; int _y = y + ly; int _z = z + lz;
				u16 block = (Noise3_ImproveXY(0, _x*0.1, _y*0.1, _z*0.1)*4-_z+chunkSize/2) > 0 ? 2 : 0;// z > 5 ? 0 : z == 5 ? 2 : z > 2 ? 1 : 3; 
				(*meta.data)[lz][ly][lx] = block;
		}

	return meta;
}

// todo: this will be async/queued on a separate thread later
ChunkMetadata &getOrInitChunk(s16 x, s16 y, s16 z) {
	auto it = world.find({x, y, z}); // todo could optimise it
	if (it != world.end())
		return it->second;
	else 
		return fillChunk(x, y, z);
}

void initMeshVisuals(ChunkMetadata &meta, std::array<chunk *, 6> const &sides) {
	meta.allocation = meshChunk(*meta.data, sides);
	if (meta.allocation.vertexCount) {
		BufInfo_Init(&meta.vertexBuffer);
		BufInfo_Add(&meta.vertexBuffer, meta.allocation.vertices, sizeof(vertex), 3, 0x210);
	}
	meta.meshed = true;
}

void initMeshVisuals(int x, int y, int z) {
	initMeshVisuals(getOrInitChunk(x, y, z), std::array<chunk *, 6> {
		getOrInitChunk(x-1, y, z).data,
		getOrInitChunk(x+1, y, z).data,
		getOrInitChunk(x, y-1, z).data,
		getOrInitChunk(x, y+1, z).data,
		getOrInitChunk(x, y, z-1).data,
		getOrInitChunk(x, y, z+1).data
	});
}

static float angleX = 0.0, angleY = 0.0;

// Helper function for loading a texture from memory
static bool loadTextureFromMem(C3D_Tex* tex, C3D_TexCube* cube, const void* data, size_t size)
{
	Tex3DS_Texture t3x = Tex3DS_TextureImport(data, size, tex, cube, false);
	if (!t3x)
		return false;

	// Delete the t3x object since we don't need it
	Tex3DS_TextureFree(t3x);
	return true;
}	

float playerX = 0, playerY = 0, playerVelX = 0, playerVelY = 0;

fvec2 collide(fvec2 position, float radius) {
	
}

fvec3 collide(fvec3 position, float radius, int height) {

}

static void sceneInit(void)
{
	// Load the vertex shader, create a shader program and bind it
	vshader_dvlb = DVLB_ParseFile((u32*)vshader_shbin, vshader_shbin_size);
	shaderProgramInit(&program);
	shaderProgramSetVsh(&program, &vshader_dvlb->DVLE[0]);
	C3D_BindProgram(&program);

	// Get the location of the uniforms
	uLoc_projection   = shaderInstanceGetUniformLocation(program.vertexShader, "projection");
	uLoc_modelView    = shaderInstanceGetUniformLocation(program.vertexShader, "modelView");
	uLoc_lightVec     = shaderInstanceGetUniformLocation(program.vertexShader, "lightVec");
	uLoc_lightClr     = shaderInstanceGetUniformLocation(program.vertexShader, "lightClr");
	uLoc_material     = shaderInstanceGetUniformLocation(program.vertexShader, "material");
	uLoc_chunkPos     = shaderInstanceGetUniformLocation(program.vertexShader, "chunkPos");

	// Configure attributes for use with the vertex shader
	C3D_AttrInfo* attrInfo = C3D_GetAttrInfo();
	AttrInfo_Init(attrInfo);
	AttrInfo_AddLoader(attrInfo, 0, GPU_UNSIGNED_BYTE, 3); // v0=position
	AttrInfo_AddLoader(attrInfo, 1, GPU_UNSIGNED_BYTE, 2); // v1=texcoord
	AttrInfo_AddLoader(attrInfo, 2, GPU_BYTE, 3); // v2=normal
			
	for (int x = -2; x <= 2; ++x)
		for (int y = -2; y <= 2; ++y)
			initMeshVisuals(x, y, 0);

	// Load the texture and bind it to the first texture unit

	// todo optimize it with arrays or something
	if (!loadTextureFromMem(&textures[0], nullptr, dirt_t3x, dirt_t3x_size))
		svcBreak(USERBREAK_PANIC);
	if (!loadTextureFromMem(&textures[1], nullptr, grass_side_t3x, grass_side_t3x_size))
		svcBreak(USERBREAK_PANIC);
	if (!loadTextureFromMem(&textures[2], nullptr, grass_top_t3x, grass_top_t3x_size))
		svcBreak(USERBREAK_PANIC);
	if (!loadTextureFromMem(&textures[3], nullptr, stone_t3x, stone_t3x_size))
		svcBreak(USERBREAK_PANIC);
		
	for (auto &t: textures) {
		C3D_TexSetFilter(&t, GPU_NEAREST, GPU_LINEAR);
		C3D_TexSetWrap(&t, GPU_REPEAT, GPU_REPEAT);
	}

	// Configure the first fragment shading substage to blend the texture color with
	// the vertex color (calculated by the vertex shader using a lighting algorithm)
	// See https://www.opengl.org/sdk/docs/man2/xhtml/glTexEnv.xml for more insight
	C3D_TexEnv* env = C3D_GetTexEnv(0);
	C3D_TexEnvInit(env);
	C3D_TexEnvSrc(env, C3D_Both, GPU_TEXTURE0, GPU_PRIMARY_COLOR, GPU_PRIMARY_COLOR);
	C3D_TexEnvFunc(env, C3D_Both, GPU_MODULATE);
}

void handlePlayer(float delta) {

	// rotation

	circlePosition cp;
	hidCstickRead(&cp);

	float dx = cp.dx * 0.01f;
	float dy = cp.dy * 0.01f;
	float t = 0.1f;
	if (dx > t || dx < -t) dx = dx > 0 ? dx - t : dx + t; else dx = 0;
	if (dy > t || dy < -t) dy = dy > 0 ? dy - t : dy + t; else dy = 0;

	angleX += dx * 0.05f;
	angleY -= dy * 0.05f;
	if (angleY > M_PI_2) angleY = M_PI_2;
	if (angleY < -M_PI_2) angleY = -M_PI_2;

	// movement

	float walkSpeed = 5;
	float walkAcc = 5;

	hidCircleRead(&cp);
	float angleM = atan2f(cp.dx, cp.dy) + angleX;
	float mag2 = cp.dx*cp.dx + cp.dy*cp.dy;

	float targetX = 0;
	float targetY = 0;
	if (mag2 > 40*40) {
		if (mag2 > 140*140) 
			mag2 = 140*140;
		float mag = sqrtf(mag2);
		mag -= 40;
		float targetMag = delta * walkSpeed * mag / 100;
		
		targetX = sinf(angleM) * targetMag;
		targetY = cosf(angleM) * targetMag;
	} 

	float vdx = targetX - playerVelX;
	float vdy = targetY - playerVelY;
	float maxDelta = walkAcc * delta;
	float vmag2 = vdx*vdx+vdy*vdy;
	if (maxDelta*maxDelta < vmag2) {
		float imag = maxDelta / sqrtf(vmag2);
		vdx *= imag;
		vdy *= imag;
	}
	playerVelX += vdx;
	playerVelY += vdy;
	playerX += playerVelX;
	playerY += playerVelY;
}

void sceneRender(float iod) {

	// Calculate the modelView matrix
	C3D_Mtx modelView;
	Mtx_Identity(&modelView);
	// Mtx_Translate(&modelView, 0.0f, 0.0f, -chunkSize*2, true);

	Mtx_RotateX(&modelView, angleY, true);
	Mtx_RotateY(&modelView, angleX, true);

	Mtx_RotateX(&modelView, -M_PI/2, true);
	Mtx_Translate(&modelView, -playerX, -playerY, -10, true);

	Mtx_PerspStereoTilt(&projection, C3D_AngleFromDegrees(40.0f), C3D_AspectRatioTop, 0.1f, 100, iod, 2.0f, false);

	// Update the uniforms
	C3D_FVUnifMtx4x4(GPU_VERTEX_SHADER, uLoc_projection, &projection);
	C3D_FVUnifMtx4x4(GPU_VERTEX_SHADER, uLoc_modelView,  &modelView);
	C3D_FVUnifMtx4x4(GPU_VERTEX_SHADER, uLoc_material,   &material);
	C3D_FVUnifSet(GPU_VERTEX_SHADER, uLoc_lightVec,     0.0f, 0.6f, -0.8f, 0.0f);
	// C3D_FVUnifSet(GPU_VERTEX_SHADER, uLoc_lightHalfVec, 0.0f, 0.0f, -1.0f, 0.0f);
	C3D_FVUnifSet(GPU_VERTEX_SHADER, uLoc_lightClr,     1.0f, 1.0f,  1.0f, 1.0f);

	// Draw the VBO
	for (auto [idx, meta]: world) 
		if (meta.allocation.vertexCount) {
			C3D_SetBufInfo(&meta.vertexBuffer);
			C3D_FVUnifSet(
				GPU_VERTEX_SHADER, 
				uLoc_chunkPos,     
				idx.x * chunkSize, idx.y * chunkSize, idx.z * chunkSize, 0.0f
			);
			for (auto &m: meta.allocation.meshes) {
				C3D_TexBind(0, &textures[m.texture]);
				C3D_DrawElements(
					GPU_TRIANGLES, 
					m.count, 
					C3D_UNSIGNED_SHORT, 
					(u16*)meta.allocation.indices + m.start
				);
			}
		}
}

void sceneExit(void)
{
	// Free the texture
	for (auto &t: textures)
		C3D_TexDelete(&t);

	// Free the VBO
	// linearFree(vbo_data);
	// linearFree(ibo_data);
	for (auto &[idx, meta]: world)
		if (meta.meshed)
			freeMesh(meta.allocation);

	// Free the shader program
	shaderProgramFree(&program);
	DVLB_Free(vshader_dvlb);
}

static constexpr float invTickRate = 1.0f / SYSCLOCK_ARM11;



int main()
{
	initSimplex();
	// Initialize graphics
	gfxInitDefault();
	C3D_Init(C3D_DEFAULT_CMDBUF_SIZE);
	consoleInit(GFX_BOTTOM, NULL);

	// Initialize the render target
	// C3D_RenderTarget* target = C3D_RenderTargetCreate(240, 400, GPU_RB_RGBA8, GPU_RB_DEPTH24_STENCIL8);
	// C3D_RenderTargetSetOutput(target, GFX_TOP, GFX_LEFT, DISPLAY_TRANSFER_FLAGS);

	C3D_RenderTarget* targetLeft  = C3D_RenderTargetCreate(240, 400, GPU_RB_RGBA8, GPU_RB_DEPTH24_STENCIL8);
	C3D_RenderTarget* targetRight = C3D_RenderTargetCreate(240, 400, GPU_RB_RGBA8, GPU_RB_DEPTH24_STENCIL8);
	C3D_RenderTargetSetOutput(targetLeft,  GFX_TOP, GFX_LEFT,  DISPLAY_TRANSFER_FLAGS);
	C3D_RenderTargetSetOutput(targetRight, GFX_TOP, GFX_RIGHT, DISPLAY_TRANSFER_FLAGS);

	// Initialize the scene
	sceneInit();

	u64 tick = svcGetSystemTick(); 

	// Main loop
	while (aptMainLoop())
	{
		hidScanInput();

		float slider = osGet3DSliderState();
		float iod = slider/3;

		// Respond to user input
		u32 kDown = hidKeysDown();
		if (kDown & KEY_START)
			break; // break in order to return to hbmenu

		u64 newTick = svcGetSystemTick();
		u32 tickDelta = newTick - tick;
		tick = newTick;
		float delta = tickDelta * invTickRate;
		if (delta > 0.5f) 
			delta = 0.5f; 

		handlePlayer(delta);

		bool should3d = iod > 0.01f;
		if (should3d != gfxIs3D()) 
			gfxSet3D(should3d);

		// Render the scene
		C3D_FrameBegin(C3D_FRAME_SYNCDRAW);
		{
			C3D_RenderTargetClear(targetLeft, C3D_CLEAR_ALL, CLEAR_COLOR, 0);
			C3D_FrameDrawOn(targetLeft);
			// C2D_SceneTarget(targetLeft);
			sceneRender(-iod);

			if (should3d)
			{
				C3D_RenderTargetClear(targetRight, C3D_CLEAR_ALL, CLEAR_COLOR, 0);
				C3D_FrameDrawOn(targetRight);
				// C2D_SceneTarget(targetRight);
				sceneRender(iod);
			}
		}
		C3D_FrameEnd(0);
	}

	// Deinitialize the scene
	sceneExit();

	// Deinitialize graphics
	C3D_Fini();
	gfxExit();
	return 0;
}