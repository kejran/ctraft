#include "worldgen.hpp"

#include "noise.hpp"
#include <unordered_map>

using namespace worldgen;

namespace {

// constexpr int columnCache = 8*8;

// todo: we need to evict from cache ocassionally; maybe a separate job?
std::unordered_map<vec2<s16>, Column, vec2<s16>::hash> cache;
//u32 cacheIndex = 0;

Column &generateColumn(s16 cx, s16 cy) {

	auto &c = cache[{ cx, cy }];

	for (int ly = 0; ly < chunkSize; ++ly)
		for (int lx = 0; lx < chunkSize; ++lx) {

			int x = lx + cx * chunkSize;
			int y = ly + cy * chunkSize;

			auto &b = c.blocks[ly][lx];

			b.height = noise2d(0, x*0.02f, y*0.02f) * 4 + chunkSize/2;

			b.grass = noise2d(0, x*0.5f, y*0.5f) > 0.49f;
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
	return data;
}
