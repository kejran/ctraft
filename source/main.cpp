#include <3ds.h>
#include <citro3d.h>
#include <citro2d.h>
#include <tex3ds.h>
#include <string.h>
#include <stdio.h>
#include <cmath>
#include <cassert>

#include "common.hpp"
#include "mesher.hpp"
//#include "simplex.hpp"
#include "noise.hpp"

#include "cursor_t3x.h"
#include "dirt_t3x.h"
#include "grass_side_t3x.h"
#include "grass_top_t3x.h"
#include "stone_t3x.h"

#include "terrain_shbin.h"
#include "focus_shbin.h"

#define CLEAR_COLOR 0x68B0D8FF

#define DISPLAY_TRANSFER_FLAGS \
	(GX_TRANSFER_FLIP_VERT(0) | GX_TRANSFER_OUT_TILED(0) | GX_TRANSFER_RAW_COPY(0) | \
	GX_TRANSFER_IN_FORMAT(GX_TRANSFER_FMT_RGBA8) | GX_TRANSFER_OUT_FORMAT(GX_TRANSFER_FMT_RGB8) | \
	GX_TRANSFER_SCALING(GX_TRANSFER_SCALE_NO))

static std::array<C3D_Tex, textureCount> textures;
// cursor, dirt, grass s, grass t, stone

struct {
	struct {
		DVLB_s* dvlb;
		shaderProgram_s program;
		struct {
			int projection, modelView;
			int lightVec, lightClr, material, chunkPos;
		} locs;
	} block;
	struct {
		DVLB_s* dvlb;
		shaderProgram_s program;
		struct {
			int projection, modelView, blockPos;
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

static C3D_Mtx projection;
static C3D_Mtx material =
{
	{ // ABGR
	//{ { 0.0f, 0.4f, 0.3f, 0.2f } }, // Ambient
	//{ { 0.0f, 0.5f, 0.7f, 0.8f } }, // Diffuse
	{ { 0.0f, 1.0f, 1.0f, 1.0f } }, // Ambient
	{ { 0.0f, 0.0f, 0.0f, 0.0f } }, // Diffuse
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

u16 blockAt(int x, int y, int z) {
	const float sc = 0.02f;
	float noise = noise3d(0, x*sc, y*sc, z*sc)*4-z+chunkSize/2;
	return noise > 0 ? 1 : noise > 0.5f ? 3 : 0;
}

ChunkMetadata &fillChunk(s16 cx, s16 cy, s16 cz) {
	auto &meta = world[{cx, cy, cz}];
	meta.data = new chunk();
	int x = cx << chunkBits; int y = cy << chunkBits; int z = cz << chunkBits;

	for (int lx = 0; lx < chunkSize; ++lx)
		for (int ly = 0; ly < chunkSize; ++ly)
			for (int lz = 0; lz < chunkSize; ++lz) {
				int _x = x + lx; int _y = y + ly; int _z = z + lz;
				u16 block = blockAt(_x, _y, _z); 
				if (block == 1) { //  we are dirt, check one block up
					u16 above = blockAt(_x, _y, _z + 1); 
					if (above == 0)
						block = 2;
				}			
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

ChunkMetadata *tryGetChunk(s16 x, s16 y, s16 z) {
	auto it = world.find({x, y, z}); 
	if (it != world.end())
		return &it->second;
	else 
		return nullptr;
}

u16 tryGetBlock(int x, int y, int z) {
	
	auto *ch = tryGetChunk(x >> chunkBits, y >> chunkBits, z >> chunkBits);
	if (ch != nullptr)
		return (*ch->data)[z & chunkMask][y & chunkMask][x & chunkMask];
	else
		return 0xffff;
}

void initMeshVisuals(ChunkMetadata &meta, std::array<chunk *, 6> const &sides) {
	if (meta.allocation.vertexCount)
		freeMesh(meta.allocation);
	meta.allocation = meshChunk(*meta.data, sides);
	if (meta.allocation.vertexCount) {
		BufInfo_Init(&meta.vertexBuffer);
		BufInfo_Add(&meta.vertexBuffer, meta.allocation.vertices, sizeof(vertex), 4, 0x3210);
	}
	meta.meshed = true;
}

void initMeshVisuals(int x, int y, int z, bool force = false) {

	auto &ch = getOrInitChunk(x, y, z);
	if (ch.meshed && !force)
		return;
	initMeshVisuals(ch, std::array<chunk *, 6> {
		getOrInitChunk(x-1, y, z).data,
		getOrInitChunk(x+1, y, z).data,
		getOrInitChunk(x, y-1, z).data,
		getOrInitChunk(x, y+1, z).data,
		getOrInitChunk(x, y, z-1).data,
		getOrInitChunk(x, y, z+1).data
	});
}

WorldMap::iterator destroyChunk(WorldMap::iterator it) {
	delete it->second.data;
	freeMesh(it->second.allocation);	
	return world.erase(it);
}

void destroyChunk(s16vec3 v) {
	auto it = world.find(v); 
	if (it != world.end()) 
		destroyChunk(it);
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

struct {
	fvec3 pos;
	fvec3 velocity;
} player;

int lfloor(float v) {
	return static_cast<int>(floorf(v));// gimme std floor to int damn it
}

/*
	3
      --- // 2.4 -> 2 -> 0.1
	2
	
	1
      --- // 0.5 -> 0 -> 1
	0
*/

float collideFloor(fvec3 pos, float height, float radius, bool &cz) {

	float tx = pos.x;
	float ty = pos.y;
	
	int l = lfloor(tx - radius);
	int r = lfloor(tx + radius);
	int b = lfloor(ty - radius);
	int f = lfloor(ty + radius);

	height -= 0.1f;

	float zz = pos.z;

	for (int _y = b; _y <= f; ++_y)
	 	for (int _x = l; _x <= r; ++_x) {
			int t = lfloor(pos.z + height);
			if (tryGetBlock(_x, _y, t)) {
				zz = t - height;
				cz = true;
			}
			int b = lfloor(pos.z);
			if (tryGetBlock(_x, _y, b)) {
				zz = b + 1;
				cz = true;
			}
		} 
	return zz;
}

//	0	1	2	3
//	|	|	|	|
//  |   (*)	|	| // 1.2 -> 0.7 -> 0 
// 	|	| (*)	| // 1.8 -> 2.3 -> 2

fvec2 collide2d(fvec2 position, int z, float radius, bool &cx, bool &cy) {

	cx = false; cy = false;
	float dx = 0, dy = 0;
	float absdx = 0, absdy = 0; 
	// any distance closer than radius + half-block are a collision
	float colDist = radius + 0.5f;

	float tx = position.x;
	float ty = position.y;
	
	int l = lfloor(tx - radius);
	int r = lfloor(tx + radius);
	int b = lfloor(ty - radius);
	int f = lfloor(ty + radius);

	for (int _y = b; _y <= f; ++_y)
	 	for (int _x = l; _x <= r; ++_x) 
			if (tryGetBlock(_x, _y, z)) {
				// distances to centers of blocks
				float distX = _x - tx + 0.5f; // vector from player to centre of block 
				float distY = _y - ty + 0.5f;
				float aDistX = fabsf(distX);
				float aDistY = fabsf(distY);
				if (aDistX < colDist && aDistX > absdx) {
					dx = distX;
					absdx = aDistX;
					cx = true;
				}	 
				if (aDistY < colDist && aDistY > absdy) {
					dy = distY;
					absdy = aDistY;
					cy = true;
				}
			}

	if (dx > 0) dx -= 0.01f; else dx += 0.01f;
	if (dy > 0) dy -= 0.01f; else dy += 0.01f;
	
	return {
		position.x + ((cx && (!cy || (absdx > absdy))) ? ((dx > 0) ? (dx - radius - 0.5f) : (dx + radius + 0.5f)) : 0),
		position.y + ((cy && (!cx || (absdy > absdx))) ? ((dy > 0) ? (dy - radius - 0.5f) : (dy + radius + 0.5f)) : 0)
	};
}

// todo dynamic height?
void moveAndCollide(fvec3 &position, fvec3 &velocity, float delta, float radius, int height) {
	fvec3 displacement {
		velocity.x * delta,
		velocity.y * delta, 
		velocity.z * delta
	};

	int baseZ = static_cast<int>(floorf(position.z + 0.01f));// gimme floor to int damn it
	float above = position.z - baseZ;
	if (above > 0.1f)
		++height;
	vec2 target { position.x + displacement.x, position.y + displacement.y };
	bool cx = false, cy = false, cz = false;
	for (int z = 0; z < height; ++z) {
		bool lcx, lcy;
		target = collide2d(target, z + baseZ, radius, lcx, lcy);
		if (lcx || lcy)
			target = collide2d(target, z + baseZ, radius, lcx, lcy);
		cx |= lcx;
		cy |= lcy;

	}
	position.x = target.x; 
	position.y = target.y;
	position.z += displacement.z;
	position.z = collideFloor(position, height, radius, cz);

	if (cx)
		velocity.x = 0;
	if (cy)
		velocity.y = 0;
	if (cz)
		velocity.z = 0;

}

// https://github.com/fenomas/fast-voxel-raycast/blob/master/index.js
bool raycast(fvec3 eye, fvec3 dir, float maxLength, vec3<s32> &out, vec3<s32> &normal) {
	float norm = 1 / sqrtf(dir.x*dir.x*+dir.y*dir.y+dir.z*dir.z);
	dir.x *= norm; dir.y *= norm; dir.z *= norm;

	int ix = lfloor(eye.x); int iy = lfloor(eye.y); int iz = lfloor(eye.z);
	int stepx = (dir.x > 0) ? 1 : -1, stepy = (dir.y > 0) ? 1 : -1, stepz = (dir.z > 0) ? 1 : -1;

	float txDelta = fabsf(1 / dir.x), tyDelta = fabsf(1 / dir.y), tzDelta = fabsf(1 / dir.z);
	
	float xdist = (stepx > 0) ? (ix + 1 - eye.x) : (eye.x - ix); 
	float ydist = (stepy > 0) ? (iy + 1 - eye.y) : (eye.y - iy);
	float zdist = (stepz > 0) ? (iz + 1 - eye.z) : (eye.z - iz);

	float txMax = (txDelta < 9999) ? txDelta * xdist : 9999;
	float tyMax = (tyDelta < 9999) ? tyDelta * ydist : 9999;
	float tzMax = (tzDelta < 9999) ? tzDelta * zdist : 9999;

	int steppedIndex = -1; 
	float t_ = 0;

	while (t_ <= maxLength) {

		// exit check
		auto b = tryGetBlock(ix, iy, iz);
		if (b == 0xffff) 
			return false;
		if (b) {
			out.x = ix; out.y = iy; out.z = iz;
			normal = {0, 0, 0};
			if (steppedIndex == 0) normal.x = (stepx < 0) ? 1 : -1;
			if (steppedIndex == 1) normal.y = (stepy < 0) ? 1 : -1;
			if (steppedIndex == 2) normal.z = (stepz < 0) ? 1 : -1;

			return true;
			// if (hit_norm) {
			// 	hit_norm[0] = hit_norm[1] = hit_norm[2] = 0
			// 	if (steppedIndex === 0) hit_norm[0] = -stepx
			// 	if (steppedIndex === 1) hit_norm[1] = -stepy
			// 	if (steppedIndex === 2) hit_norm[2] = -stepz
			// }
			//return b
		}
		
		// advance t to next nearest voxel boundary
		if (txMax < tyMax) {
			if (txMax < tzMax) {
				ix += stepx;
				t_ = txMax;
				txMax += txDelta;
				steppedIndex = 0;
			} else {
				iz += stepz;
				t_ = tzMax;
				tzMax += tzDelta;
				steppedIndex = 2;
			}
		} else {
			if (tyMax < tzMax) {
				iy += stepy;
				t_ = tyMax;
				tyMax += tyDelta;
				steppedIndex = 1;
			} else {
				iz += stepz;
				t_ = tzMax;
				tzMax += tzDelta;
				steppedIndex = 2;
			}
		}

	}
	
	// no voxel hit found
	
	return false;
}

static const fvec3 focusVtx[] = {
	{-0.01f, -0.01f, -0.01f},
	{+1.01f, -0.01f, -0.01f},
	{+1.01f, +1.01f, -0.01f},
	{-0.01f, +1.01f, -0.01f},
	{-0.01f, -0.01f, +1.01f},
	{+1.01f, -0.01f, +1.01f},
	{+1.01f, +1.01f, +1.01f},
	{-0.01f, +1.01f, +1.01f}
};

static const uint8_t basicCubeIs[] = {
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

static void sceneInit(void)
{
	shaders.block.dvlb = DVLB_ParseFile((u32*)terrain_shbin, terrain_shbin_size);
	shaderProgramInit(&shaders.block.program);
	shaderProgramSetVsh(&shaders.block.program, &shaders.block.dvlb->DVLE[0]);
	C3D_BindProgram(&shaders.block.program);

	// Get the location of the uniforms
	shaders.block.locs.projection = 
		shaderInstanceGetUniformLocation(shaders.block.program.vertexShader, "projection");
	shaders.block.locs.modelView = 
		shaderInstanceGetUniformLocation(shaders.block.program.vertexShader, "modelView");
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
	shaders.focus.locs.modelView = 
		shaderInstanceGetUniformLocation(shaders.focus.program.vertexShader, "modelView");
	shaders.focus.locs.blockPos = 
		shaderInstanceGetUniformLocation(shaders.focus.program.vertexShader, "blockPos");
		

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

vec3<s32> playerFocus;
bool drawFocus = false;

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

	float walkSpeed = (hidKeysDown() & KEY_B) ? 20 : 10;
	float walkAcc = 50;

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
		float targetMag = walkSpeed * mag / 100;
		
		targetX = sinf(angleM) * targetMag;
		targetY = cosf(angleM) * targetMag;
	} 

	float vdx = targetX - player.velocity.x;
	float vdy = targetY - player.velocity.y;
	float maxDelta = walkAcc * delta;
	float vmag2 = vdx*vdx+vdy*vdy;
	if (maxDelta*maxDelta < vmag2) {
		float imag = maxDelta / sqrtf(vmag2);
		vdx *= imag;
		vdy *= imag;
	}
	player.velocity.x += vdx;
	player.velocity.y += vdy;

	u32 kDown = hidKeysDown();

	if ((kDown & KEY_A) && player.velocity.z < 0.1f)
		// todo raycast down
		player.velocity.z = 4.8f; // adjust until it feels ok
	else
		player.velocity.z -= 9.81f * delta;
	
	// velocity > 20 will break 1-iteration collision
	if (player.velocity.z < -15) 
		player.velocity.z = -15;
	
	moveAndCollide(player.pos, player.velocity, delta, 0.4f, 2);	

	fvec3 dir {sinf(angleX) * cosf(angleY), cosf(angleX) * cosf(angleY), -sinf(angleY)};
	drawFocus = false;
	vec3<s32> normal;
	if (raycast(
		{player.pos.x, player.pos.y, player.pos.z + 1.5f}, 
		dir, 3, playerFocus, normal)
	) {
		if (kDown & KEY_Y) {
			int nx = playerFocus.x + normal.x;
			int ny = playerFocus.y + normal.y;
			int nz = playerFocus.z + normal.z;
			
			auto *ch = tryGetChunk(
				nx >> chunkBits, 
				ny >> chunkBits, 
				nz >> chunkBits
			);
			if (ch) {
				(*ch->data)[nz & chunkMask][ny & chunkMask][nx & chunkMask] = 3; 
				initMeshVisuals(
					nx >> chunkBits, 
					ny >> chunkBits, 
					nz >> chunkBits, 
					true
				);
				// todo use selected block
			}
		}
		if (kDown & KEY_X) {
			int nx = playerFocus.x;
			int ny = playerFocus.y;
			int nz = playerFocus.z;
			
			auto *ch = tryGetChunk(
				nx >> chunkBits, 
				ny >> chunkBits, 
				nz >> chunkBits
			);
			if (ch) {
				(*ch->data)[nz & chunkMask][ny & chunkMask][nx & chunkMask] = 0; 
				initMeshVisuals(
					nx >> chunkBits, 
					ny >> chunkBits, 
					nz >> chunkBits, 
					true
				);
				// todo use selected block
			}
		}
		drawFocus = true;
	}
}

static constexpr int distanceLoad = 3; // blocks to load, cage size 2n+1
static constexpr int distanceUnload = 5; // blocks to unload, keep blocks in cage of 2n+1 

// note that a 1-chunk thick shell will generate outside the render cage since it is needed for meshing

void updateWorld(fvec3 focus) {

	int chX = static_cast<int>(focus.x) >> chunkBits;
	int chY = static_cast<int>(focus.y) >> chunkBits;
	int chZ = static_cast<int>(focus.z) >> chunkBits;

	for (auto it = world.begin(); it != world.end();) {
		auto &idx = it->first;
		if (
			idx.x < chX - distanceUnload ||
			idx.x > chX + distanceUnload ||
			idx.y < chY - distanceUnload ||
			idx.y > chY + distanceUnload ||
			idx.y < chY - distanceUnload ||
			idx.z > chZ + distanceUnload
		)
			it = destroyChunk(it);
		else
			++it;
	}

	for (int z = chZ - distanceLoad; z <= chZ + distanceLoad; ++z)
		for (int y = chY - distanceLoad; y <= chY + distanceLoad; ++y)
			for (int x = chX - distanceLoad; x <= chX + distanceLoad; ++x)
				initMeshVisuals(x, y, z);
}

void sceneRender(float iod) {

	/// --- CALCULATE VIEW --- ///

	// todo: we can probably merge view and projection into one

	C3D_Mtx modelView;
	Mtx_Identity(&modelView);

	Mtx_RotateX(&modelView, angleY, true);
	Mtx_RotateY(&modelView, angleX, true);

	Mtx_RotateX(&modelView, -M_PI/2, true);

	float eyeHeight = 1.5f;
	Mtx_Translate(&modelView, -player.pos.x, -player.pos.y, -player.pos.z - eyeHeight, true);

	Mtx_PerspStereoTilt(&projection, C3D_AngleFromDegrees(40.0f), C3D_AspectRatioTop, 0.1f, 100, iod, 2.0f, false);

	/// --- DRAW BLOCKS --- ///

	C3D_SetTexEnv(0, &texEnvs.block);
	C3D_AlphaBlend(
		GPU_BLEND_ADD, GPU_BLEND_ADD, 
		GPU_SRC_ALPHA, GPU_ONE_MINUS_SRC_ALPHA, 
		GPU_SRC_ALPHA, GPU_ONE_MINUS_SRC_ALPHA
	);
	C3D_BindProgram(&shaders.block.program);

	// Update the uniforms
	C3D_FVUnifMtx4x4(GPU_VERTEX_SHADER, shaders.block.locs.projection, &projection);
	C3D_FVUnifMtx4x4(GPU_VERTEX_SHADER, shaders.block.locs.modelView,  &modelView);
	C3D_FVUnifMtx4x4(GPU_VERTEX_SHADER, shaders.block.locs.material,   &material);
	C3D_FVUnifSet(GPU_VERTEX_SHADER, shaders.block.locs.lightVec,     0.0f, 0.6f, -0.8f, 0.0f);
	C3D_FVUnifSet(GPU_VERTEX_SHADER, shaders.block.locs.lightClr,     1.0f, 1.0f,  1.0f, 1.0f);

	C3D_SetAttrInfo(&vertexLayouts.block);

	for (auto [idx, meta]: world) 
		if (meta.allocation.vertexCount) {
			C3D_SetBufInfo(&meta.vertexBuffer);
			C3D_FVUnifSet(
				GPU_VERTEX_SHADER, 
				shaders.block.locs.chunkPos,     
				idx.x * chunkSize, idx.y * chunkSize, idx.z * chunkSize, 0.0f
			);
			for (auto &m: meta.allocation.meshes) {
				C3D_TexBind(0, &textures[m.texture + 1]);
				C3D_DrawElements(
					GPU_TRIANGLES, 
					m.count, 
					C3D_UNSIGNED_SHORT, 
					(u16*)meta.allocation.indices + m.start
				);
			}
		}

	if (drawFocus) {
		C3D_SetTexEnv(0, &texEnvs.focus);
		C3D_AlphaBlend(
			GPU_BLEND_ADD, GPU_BLEND_ADD,
			GPU_SRC_ALPHA, GPU_ONE, 
			GPU_SRC_ALPHA, GPU_ONE
		);
		C3D_BindProgram(&shaders.focus.program);

		// Update the uniforms
		C3D_FVUnifMtx4x4(GPU_VERTEX_SHADER, shaders.focus.locs.projection, &projection);
		C3D_FVUnifMtx4x4(GPU_VERTEX_SHADER, shaders.focus.locs.modelView,  &modelView);
		C3D_FVUnifSet(GPU_VERTEX_SHADER, shaders.focus.locs.blockPos, 
			playerFocus.x, playerFocus.y, playerFocus.z, 0.0f
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
	shaderProgramFree(&shaders.block.program);
	DVLB_Free(shaders.block.dvlb);
}

void uiRender() {
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

int main()
{
	// Initialize graphics
	gfxInitDefault();
	C3D_Init(C3D_DEFAULT_CMDBUF_SIZE);
	C2D_Init(C2D_DEFAULT_MAX_OBJECTS);
	consoleInit(GFX_BOTTOM, NULL);

	C3D_RenderTarget* targetLeft  = C3D_RenderTargetCreate(240, 400, GPU_RB_RGBA8, GPU_RB_DEPTH24_STENCIL8);
	C3D_RenderTarget* targetRight = C3D_RenderTargetCreate(240, 400, GPU_RB_RGBA8, GPU_RB_DEPTH24_STENCIL8);
	C3D_RenderTargetSetOutput(targetLeft,  GFX_TOP, GFX_LEFT,  DISPLAY_TRANSFER_FLAGS);
	C3D_RenderTargetSetOutput(targetRight, GFX_TOP, GFX_RIGHT, DISPLAY_TRANSFER_FLAGS);

	// Initialize the scene
	sceneInit();

	u64 tick = svcGetSystemTick(); 
	
	player.pos.z = 12;
	player.pos.x = -5;
	player.pos.y = 5;	

	// Main loop
	while (aptMainLoop())
	{

		// do it before input and vsync wait
		updateWorld(player.pos);

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
		if (delta > 0.05f) // aim for minimum 20 fps, anything less is a spike
			delta = 0.05f; 

		handlePlayer(delta);

		bool should3d = iod > 0.01f;
		if (should3d != gfxIs3D()) 
			gfxSet3D(should3d);

		// Render the scene
		C3D_FrameBegin(C3D_FRAME_SYNCDRAW);
		{
			C3D_RenderTargetClear(targetLeft, C3D_CLEAR_ALL, CLEAR_COLOR, 0);
			C3D_FrameDrawOn(targetLeft);
			C2D_SceneTarget(targetLeft);
			sceneRender(-iod);
			C2D_Prepare();
			uiRender();
			C2D_Flush();
			if (should3d)
			{
				C3D_RenderTargetClear(targetRight, C3D_CLEAR_ALL, CLEAR_COLOR, 0);
				C3D_FrameDrawOn(targetRight);
				C2D_SceneTarget(targetRight);
				sceneRender(iod);
				C2D_Prepare();
				uiRender();
				C2D_Flush();
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
