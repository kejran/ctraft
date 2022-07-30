#include "worldgen.hpp"

#include "noise.hpp"

float noiseAt(int x, int y, int z) {
	const float sc = 0.02f;
	return noise3d(0, x*sc, y*sc, z*sc)*4-z+chunkSize/2;
}

Block blockFor(float noise) {
	Block block = { (noise > 0) ? (noise > 3 ? Block::solid(2) : Block::solid(0)) : Block { 0 } };
	return block;
}

Block blockAt(int x, int y, int z) {
	float noise = noiseAt(x, y, z);
	auto block = blockFor(noise);
	if (block.isAir() && noise > -1) { // we are air, check for grass; we might want to build grass later instead
		float noise2 = noiseAt(x, y, z - 1);
		auto block2 = blockFor(noise2);
		if (block2.isSolid() && block2.solidId() == 0) { // grass is below us
			float noise3 = noise3d(0, x*0.5f, y*0.5f, z*0.5f);
			if (noise3 > 0.49f)
				block = Block::foliage(1);
		}
	}

	if (block.isSolid()) {
		if (block.solidId() == 0) { //  we are dirt, check one block up
			auto noise2 = noiseAt(x, y, z + 1);
			auto above = blockFor(noise2);
			if (above.isAir())
				block = Block::solid(1);
		}
		if (block.solidId() == 2) { //  we are stone, check for coal
			const float sc = 0.1f;
			float noise = noise3d(0, x*sc, y*sc, z*sc);
			if (noise > 0.5f)
				block = Block::solid(4);
		}
	}
	return block;
}

chunk *generateChunk(s16 cx, s16 cy, s16 cz) {

    auto data = new chunk();
	int x = cx << chunkBits; int y = cy << chunkBits; int z = cz << chunkBits;

	for (int lx = 0; lx < chunkSize; ++lx)
		for (int ly = 0; ly < chunkSize; ++ly)
			for (int lz = 0; lz < chunkSize; ++lz) {
				int _x = x + lx; int _y = y + ly; int _z = z + lz;
				auto block = blockAt(_x, _y, _z);
				(*data)[lz][ly][lx] = block;
		}
	return data;
}
