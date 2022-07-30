#pragma once
#include "common.hpp"

// NOTE: worldgen is exclusively for the worker thread

namespace worldgen {

struct BlockColumn {
    int height;
    bool grass;
    u8 tree;
};

using stamp = std::tuple<s16, s16, int, Block>;

struct stampList {
	std::vector<stamp> stamps;
	void add(int x, int y, int z, Block block) {
		if (x >= 0 && y >= 0 && x < chunkSize && y < chunkSize)
			stamps.push_back({ (s16)x, (s16)y, z, block });
	}
};

struct Column {

    std::array<std::array<BlockColumn, chunkSize>, chunkSize> blocks;
    // u32 cacheIndex;
    stampList stamps;
    bool stampsGenerated;
};

}

// worldgen::Column &getColumn(s16 x, s16 y);

Block blockAt(int x, int y, int z);

chunk *generateChunk(s16 cx, s16 cy, s16 cz);