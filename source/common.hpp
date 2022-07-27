#pragma once

#include <3ds.h>
#include <citro3d.h>
#include <stdio.h>

#include <array>
#include <unordered_map>
#include <vector>

template <typename T> struct vec2 { T x, y; };
template <typename T> struct vec3 { 
	T x = 0, y = 0, z = 0; 
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

static constexpr int chunkBits = 4;
static constexpr int chunkSize = (1 << chunkBits);
using chunk = std::array<std::array<std::array<u16, chunkSize>, chunkSize>, chunkSize>;

static constexpr int chunkMask = chunkSize - 1;

struct vertex { 
    u8vec3 position; 
    u8vec2 texcoord; 
    s8vec3 normal; 
	u8 ao;
};

using BlockVisual = std::array<u8, 6>;

static constexpr int textureCount = 9;

static constexpr float invTickRate = 1.0f / SYSCLOCK_ARM11;

constexpr int renderDistance = 3;

struct timeit {
	char const *name_;
	u32 start_;
	
	timeit(char const *name) { 
		name_ = name; 
		start_ = svcGetSystemTick();
	}

	~timeit() {
		u32 end = svcGetSystemTick();
		printf("%s: %fms\n", name_, (end - start_) * invTickRate * 1000);
	}
};
