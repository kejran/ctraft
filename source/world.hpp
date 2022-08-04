#pragma once

#include "common.hpp"
#include "mesher.hpp"

struct ChunkMetadata {
	MesherAllocation allocation;
	C3D_BufInfo vertexBuffer;
	chunk *data = nullptr;
	u8 visibility = 0;
	bool meshed = false;
};

using WorldMap = std::unordered_map<s16vec3, ChunkMetadata, s16vec3::hash>;
inline WorldMap world;

bool raycast(fvec3 eye, fvec3 dir, float maxLength, vec3<s32> &out, vec3<s32> &normal);

WorldMap::iterator destroyChunk(WorldMap::iterator it);

void destroyChunk(s16vec3 v);

ChunkMetadata *tryGetChunk(s16 x, s16 y, s16 z);

Block tryGetBlock(int x, int y, int z);
