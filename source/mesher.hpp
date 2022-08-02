#pragma once

#include "common.hpp"
#include <span>

struct MesherAllocation {
    /* FLAGS

    0: draw both sides (no culling)
    1: alpha cutout mode?

    */

   static constexpr int MESH_NOCULL = 1;
   static constexpr int MESH_ALPHATEST = 2;

    struct Mesh {
        u16 count;
        u8 texture;
        u8 flags; // we could have flags stored per texture instead?
    };

	void *vertices = nullptr; // todo merge into one allocation?
	void *indices = nullptr;
	u16 vertexCount = 0;
    std::vector<Mesh> meshes;
};

using expandedChunk = std::array<std::array<std::array<Block, chunkSize+2>, chunkSize+2>, chunkSize+2>;

MesherAllocation meshChunk(expandedChunk const &ch);
MesherAllocation meshChunk(chunk const &ch, std::array<chunk *, 6> const &sides);
void expandChunk(chunk const &ch, std::array<chunk *, 6> const &sides, expandedChunk &ex);

void freeMesh(MesherAllocation &);

BlockVisual getBlockVisual(Block block);

u8 getSidesOpaque(expandedChunk const &ch);
u8 getSidesOpaque(chunk const &ch);