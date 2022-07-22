#pragma once

#include <3ds.h>
#include <citro3d.h>
#include <stdio.h>

#include <array>
#include <unordered_map>
#include <vector>

struct u8vec3 {
	u8 x, y, z;
};

struct s8vec3 {
	s8 x, y, z;
};

struct u8vec2 {
	u8 x, y;
};

struct s16vec3 {
	s16 x, y, z;
	bool operator==(s16vec3 const &o) const = default;//{ return x == o.x}
	struct hash {
		size_t operator()(const s16vec3& pos) const {
			return pos.x ^ pos.y ^ pos.z;
		}
	};
};

static constexpr int chunkSize = 16;
using chunk = std::array<std::array<std::array<uint16_t, chunkSize>, chunkSize>, chunkSize>;

struct vertex { 
    u8vec3 position; 
    u8vec2 texcoord; 
    s8vec3 normal; 
};

using BlockVisual = std::array<u8, 6>;

static constexpr int textureCount = 4;
