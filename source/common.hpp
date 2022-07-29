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
static constexpr int chunkMask = chunkSize - 1;
using chunk = std::array<std::array<std::array<u16, chunkSize>, chunkSize>, chunkSize>;

struct vertex {
    u8vec3 position;
    u8vec2 texcoord;
    s8vec3 normal;
	u8 ao;
};

using BlockVisual = std::array<u8, 6>;
static constexpr int textureCount = 9;

static constexpr float invTickRate = 1.0f / SYSCLOCK_ARM11;

constexpr int renderDistance = 5;
constexpr int zChunks = 2; // 5 chunks = 80 blocks of height total

#define INLINE __attribute__((always_inline)) inline

INLINE int fastFloor(float f) { return (f >= 0 ? (int)f : (int)f - 1); }

#define PROFILER

#ifdef PROFILER
inline u32 _customProfileStart = 0;
inline u32 _customProfileCalls = 0;
inline u32 _customProfileTime = 0;

#define PROFILE_START _customProfileStart = svcGetSystemTick();
#define PROFILE_END do { \
_customProfileTime += svcGetSystemTick() - _customProfileStart; \
++_customProfileCalls; } while (0)
#else
#define PROFILE_START
#define PROFILE_END
#endif

#ifdef PROFILER
struct ProfilerWrapper {
	ProfilerWrapper() { PROFILE_START; }
	~ProfilerWrapper() { PROFILE_END; }
};
#define PROFILE_SCOPE ProfilerWrapper ___wrapper
#else
#define PROFILE_SCOPE
#endif
