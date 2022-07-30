#pragma once
#include "common.hpp"

// NOTE: worldgen is exclusively for the worker thread

namespace worldgen {

struct BlockColumn {
    int height;
    bool grass;
    u8 tree;
};

struct Column {

    std::array<std::array<BlockColumn, chunkSize>, chunkSize> blocks;
    u32 cacheIndex;
};

}

// worldgen::Column &getColumn(s16 x, s16 y);

Block blockAt(int x, int y, int z);

chunk *generateChunk(s16 cx, s16 cy, s16 cz);