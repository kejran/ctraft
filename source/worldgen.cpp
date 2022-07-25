#include "worldgen.hpp"

#include "noise.hpp"

u16 blockAt(int x, int y, int z) {
	const float sc = 0.02f;
	float noise = noise3d(0, x*sc, y*sc, z*sc)*4-z+chunkSize/2;
	return noise > 0 ? 1 : noise > 0.5f ? 3 : 0;
}

chunk *generateChunk(s16 cx, s16 cy, s16 cz) {
	
    auto data = new chunk();
	int x = cx << chunkBits; int y = cy << chunkBits; int z = cz << chunkBits;

	for (int lx = 0; lx < chunkSize; ++lx)
		for (int ly = 0; ly < chunkSize; ++ly)
			for (int lz = 0; lz < chunkSize; ++lz) {
				int _x = x + lx; int _y = y + ly; int _z = z + lz;
				u16 block = blockAt(_x, _y, _z); 
				if (block == 1) { //  we are dirt, check one block up
					u16 above = blockAt(_x, _y, _z + 1); 
					if (above == 0)
						block = 2;
				}			
				(*data)[lz][ly][lx] = block;
		}
	return data;
}