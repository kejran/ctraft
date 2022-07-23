#pragma once

#include <3ds.h>
#include <citro3d.h>
#include <stdio.h>

#include <array>
#include <unordered_map>
#include <vector>

template <typename T> struct vec2 { T x, y; };
template <typename T> struct vec3 { 
	T x, y, z; 
	bool operator==(vec3<T> const &o) const = default;
	struct hash {
		size_t operator()(const vec3<T>& pos) const {
			return pos.x ^ pos.y ^ pos.z;
		}
	};
};


using s16vec3 = vec3<s16>;
using u8vec2 = vec2<u8>;
using u8vec3 = vec3<u8>;
using s8vec3 = vec3<s8>;

using fvec2 = vec2<float>;
using fvec3 = vec3<float>;

static constexpr int chunkSize = 16;
using chunk = std::array<std::array<std::array<u16, chunkSize>, chunkSize>, chunkSize>;

struct vertex { 
    u8vec3 position; 
    u8vec2 texcoord; 
    s8vec3 normal; 
};

using BlockVisual = std::array<u8, 6>;

static constexpr int textureCount = 4;
