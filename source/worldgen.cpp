#include "worldgen.hpp"

#include "noise.hpp"
#include <unordered_map>

using namespace worldgen;

namespace {

// constexpr int columnCache = 8*8;

// todo: we need to evict from cache ocassionally; maybe a separate job?
std::unordered_map<vec2<s16>, Column, vec2<s16>::hash> cache;
//u32 cacheIndex = 0;

INLINE u32 xorshift(u32 v, int s) {
	return v ^ (v >> s);
}

// todo come up with something better
INLINE u32 simpleHash(u32 a, u32 b, u32 c) {
	u32 i = xorshift(a * (u32)0x1234567, 16);//0x55555555
	u32 j = xorshift((b + i) * (u32)1073726623, 16);
    u32 k = xorshift((c + j) * (u32)2654435761, 16);
    return k;
}

constexpr int maxTreeRadius = 1;

Column &generateColumn(s16 cx, s16 cy) {

	auto &c = cache[{ cx, cy }];

	for (int ly = 0; ly < chunkSize; ++ly)
		for (int lx = 0; lx < chunkSize; ++lx) {

			int x = lx + cx * chunkSize;
			int y = ly + cy * chunkSize;

			auto &b = c.blocks[ly][lx];

			b.height = noise2d(0, x*0.02f, y*0.02f) * 4 + chunkSize/2;

			b.grass = (simpleHash(x, y, 0) & 0xf) == 0; // 1 in 16

			b.tree = (simpleHash(x, y, 0) & 0xff) == 5; // 1 in 256
		}

	return c;
}

Column &getColumn(s16 x, s16 y) {
	auto it = cache.find({ x, y });
	if (it == cache.end())
		return generateColumn(x, y);
	else
		return it->second;
}

}

INLINE Block blockAt(Column &c, int locX, int locY, int x, int y, int z) {

	auto &cc = c.blocks[locY][locX];

	if (z >= cc.height) { // above ground
		if (cc.grass && (z == cc.height)) // generate tall grass
			return Block::foliage(1);
		else // just air
			return { 0 };
	} else {
		if (z < cc.height - 3) { // 3 blocks below
			const float sc = 0.1f;
			float noise = noise3d(0, x*sc, y*sc, z*sc);
			if (noise > 0.5f) // rather rare 3d noise
				return Block::solid(4); // generate coal
			else
				return Block::solid(2); // generate stone
		} else {
			if (z == (cc.height - 1)) // just below air
				return Block::solid(1); // generate grass
			else
				return Block::solid(0); // generate dirt
		}
	}
}

using stamp = std::tuple<s16, s16, int, Block>;

struct stampList {
	std::vector<stamp> stamps;
	void add(int x, int y, int z, Block block) {
		if (x >= 0 && y >= 0 && x < chunkSize && y < chunkSize)
			stamps.push_back({ (s16)x, (s16)y, z, block });
	}
};

void treeStamp(int x, int y, int z, stampList &stamps) {

	for (int lz = 0; lz < 3; ++lz)
		for (int ly = -1; ly < 2; ++ly)
			for (int lx = -1; lx < 2; ++lx)
				if (lz != 2 || ly == 0 || lx == 0)
					stamps.add(x + lx, y + ly, 2 + z + lz, Block::solid(8));

	for (int iz = 0; iz < 3; ++iz)
		stamps.add(x, y, z + iz, Block::solid(7));
}

void generateTreeStamps(Column const &c, s16 offX, s16 offY, stampList &stamps) {
	for (int ly = 0; ly < chunkSize; ++ly)
		for (int lx = 0; lx < chunkSize; ++lx) {
			auto &b = c.blocks[ly][lx];
			if (b.tree)
				treeStamp(lx + offX, ly + offY, b.height, stamps);
		}
}

// todo: stamps should be generated once per block
void generateStamps(s16 cx, s16 cy, stampList &stamps) {
	for (int y = -1; y < 2; ++y)
		for (int x = -1; x < 2; ++x) {
			auto &column = getColumn(cx + x, cy + y);
			generateTreeStamps(column, x*chunkSize, y*chunkSize, stamps);
		}
}

chunk *generateChunk(s16 cx, s16 cy, s16 cz) {

    auto data = new chunk();
	int x = cx << chunkBits; int y = cy << chunkBits; int z = cz << chunkBits;

	auto &column = getColumn(cx, cy);

	for (int lx = 0; lx < chunkSize; ++lx)
		for (int ly = 0; ly < chunkSize; ++ly)
			for (int lz = 0; lz < chunkSize; ++lz) {
				int _x = x + lx; int _y = y + ly; int _z = z + lz;
				auto block = blockAt(column, lx, ly, _x, _y, _z);
				(*data)[lz][ly][lx] = block;
		}

	stampList stamps;
	generateStamps(cx, cy, stamps);
	for (auto [x, y, z, block]: stamps.stamps) {
		z -= cz * chunkSize;
		if (z >= 0 && z < chunkSize)
			(*data)[z][y][x] = block;
	}

	return data;
}
