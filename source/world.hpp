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
