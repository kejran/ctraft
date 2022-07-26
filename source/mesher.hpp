#pragma once

#include "common.hpp"
#include <span>

struct MesherAllocation {

    struct Mesh {
        u16 count;
        u8 texture;
    };

	void *vertices = nullptr; // todo merge into one allocation?
	void *indices = nullptr;
	u16 vertexCount = 0;
    std::vector<Mesh> meshes;
};

using expandedChunk = std::array<std::array<std::array<u16, chunkSize+2>, chunkSize+2>, chunkSize+2>;

MesherAllocation meshChunk(expandedChunk const &ch);
MesherAllocation meshChunk(chunk const &ch, std::array<chunk *, 6> const &sides);
void expandChunk(chunk const &ch, std::array<chunk *, 6> const &sides, expandedChunk &ex);

void freeMesh(MesherAllocation &);

BlockVisual const &getBlockVisual(u16 block);