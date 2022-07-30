#pragma once

#include <3ds.h>
#include <citro3d.h>
#include <stdio.h>

#include <array>
#include <unordered_map>
#include <vector>

template <typename T> struct vec2 {
	T x = 0, y = 0;
	bool operator==(vec2<T> const &o) const = default;
	struct hash {
		size_t operator()(const vec2<T>& pos) const {
			return pos.x ^ pos.y;
		}
	};
};
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

#define INLINE __attribute__((always_inline)) inline

/* BLOCK STRUCTURE

	non-opaque blocks
	(XX - sun + artificial light)
	0x00XX - air
	0x01XX..0FXX - foliage, 15 types of plants?
	air can be considered *plant zero*

	0b0001'XX... 0b0111'XX... - entities?

	0b1TTT'TTTT'XX... - solid, 7-bit Type + 8bit payload?
*/
struct Block {
	u16 value;

	INLINE bool isNonSolid() const { return (value & 0x8000) == 0; }
	INLINE bool isSolid() const { return value & 0x8000; }
	INLINE bool isFoliage() const { return (value & 0xf000) == 0 && (value & 0x0f00) != 0; }
	INLINE bool isAir() const { return (value & 0xff00) == 0; }
	int solidId() const { return (value >> 8) & 0x7f; }

	bool operator==(Block const &) const = delete; // do not compare blocks, it does not make sense
	static Block solid(u16 value) { return { static_cast<u16>(0x8000 | (value << 8)) }; }
	static Block foliage(u16 value) { return { static_cast<u16>(value << 8) }; }
};

using chunk = std::array<std::array<std::array<Block, chunkSize>, chunkSize>, chunkSize>;

struct vertex {
    u8vec3 position;
    u8vec2 texcoord;
    s8vec3 normal;
	u8 ao;
};

struct BlockVisual {

	enum class Type : u8 {
		Solid,
		Foliage
	} type;

	std::array<u8, 6> faces;
};

static constexpr int textureCount = 10;

static constexpr float invTickRate = 1.0f / SYSCLOCK_ARM11;

constexpr int renderDistance = 5;
constexpr int zChunks = 2; // 5 chunks = 80 blocks of height total

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
