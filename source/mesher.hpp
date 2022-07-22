#pragma once

#include "common.hpp"
#include <span>

struct MesherAllocation {

    struct Mesh {
        u16 start;
        u16 count;
        u8 texture;
    };

	void *vertices; // todo merge into one allocation?
	void *indices;
	u16 vertexCount;
    std::vector<Mesh> meshes;
};

MesherAllocation meshChunk(chunk const &ch, std::array<chunk *, 6> const &sides);

void freeMesh(MesherAllocation &);
