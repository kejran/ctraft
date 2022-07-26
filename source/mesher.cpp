#include <stdio.h>

#include "mesher.hpp"
#include <cassert>

// right handed?
/*
    3---2
	| / |
	0---1 

	0-1-2 0-2-3

(top view)
	
    7----6
   /|    |
  3 |  2 |
  | 4----5	 Y Z    // z is up
  |/    /    |/
  0----1     *---X

*/

static const u8vec2 basicCubeUVs[4] = {
	{0, 0},
	{1, 0},
	{1, 1},
	{0, 1},
};

static const u8vec3 basicCubeVs[8] = {
	{0, 0, 0},
	{1, 0, 0},
	{1, 1, 0},
	{0, 1, 0},
	
	{0, 0, 1},
	{1, 0, 1},
	{1, 1, 1},
	{0, 1, 1},
};

// (0), dirt, grass, stone, cobble, coal, sand, planks
static constexpr BlockVisual blockVisuals[] = {
	{ 0, 0, 0, 0, 0, 0 }, // dirt
	{ 1, 1, 1, 1, 0, 2 }, // grass
	{ 3, 3, 3, 3, 3, 3 }, // stone
	{ 5, 5, 5, 5, 5, 5 }, // cobble
	{ 4, 4, 4, 4, 4, 4 }, // coal
	{ 6, 6, 6, 6, 6, 6 }, // sand
	{ 7, 7, 7, 7, 7, 7 }, // planks
};

// for first block in interation; v is -1/0/1
inline int _ins(int value) {
	if (value < 0)
		return chunkSize + value;
	else if (value > 0)
		return value - 1;
	else return 0;
}

inline int _inm(int dir, int position) {
	if (dir < 0)
		return chunkSize - position - 1;
	else if (dir > 0)
		return position;
	else return 0;
}

inline int _rev(int uv, int dir) {
	return uv ? dir : -dir;
}

// todo handle allocation errors

MesherAllocation meshChunk(expandedChunk const &cch) {
	std::vector<vertex> vertices;
	std::array<u16, (chunkSize+1) * (chunkSize+1)> vertexCache;
	std::vector<u16> indicesFlat;
	std::array<std::vector<u16>, textureCount> isPerTexture;
	
	// todo refactor this using templates + lambda accessor for each side  

// side 2 & 4 works
	for (int s = 0; s < 6; ++s) { // side
// int s = 0;{		
		// ghetto matrix math
		int8_t normX = 0, normY = 0, normZ = 0;
		int8_t texUX = 0, texUY = 0, texUZ = 0;
		int8_t texVX = 0, texVY = 0, texVZ = 0;
		switch (s) {
			case 0: normX = -1; texUY = -1; texVZ = +1; break;
			case 1: normX = +1; texUY = +1; texVZ = +1; break;
			case 2: normY = -1; texUX = +1; texVZ = +1; break;
			case 3: normY = +1; texUX = -1; texVZ = +1; break;
			case 4: normZ = -1; texUX = -1; texVY = +1; break;
			case 5: normZ = +1; texUX = +1; texVY = +1; break;
			default: break;
		}

		u8 nx = (texUX < 0 || texVX < 0) + (normX > 0);
		u8 ny = (texUY < 0 || texVY < 0) + (normY > 0);
		u8 nz = (texUZ < 0 || texVZ < 0) + (normZ > 0); 

		for (int l = 0; l < chunkSize; ++l) { // layer

			for (int i = 0; i < (chunkSize+1) * (chunkSize+1); ++i)
				vertexCache[i] = 0xffff;

			u8 lx = _inm(normX, l); u8 ly = _inm(normY, l); u8 lz = _inm(normZ, l);			
			u8 vx = _ins(texVX); u8 vy = _ins(texVY); u8 vz = _ins(texVZ);
			for (int v = 0; v < chunkSize; ++v) {
				u8 ux = _ins(texUX); u8 uy = _ins(texUY); u8 uz = _ins(texUZ);
				for (int u = 0; u < chunkSize; ++u) {
					uint16_t vidxs[4];

					// this is block position in the grid
					u8 x = lx + ux + vx;
					u8 y = ly + uy + vy;
					u8 z = lz + uz + vz;
					
					u16 self = cch[z+1][y+1][x+1];
					bool draw = self > 0;
					if (draw) {
						// neighbouring block of this face
						s8 sx = x + normX + 1;
						s8 sy = y + normY + 1;
						s8 sz = z + normZ + 1;
						u16 other = cch[sz][sy][sx];
						draw = other == 0;
					}
					if (draw) {
						for (int i = 0; i < 4; ++i) {
							// vertex positon in its face coordinates
							auto uv = basicCubeUVs[i]; 
							// vertex position in chunk plane
							auto guv = uv;
							guv.x += u; guv.y += v;
							auto cidx = guv.x + guv.y * (chunkSize+1);
							auto vidx = vertexCache[cidx];
							if (vidx == 0xffff) {
								vidx = static_cast<u16>(vertices.size()); 
								vertexCache[cidx] = vidx;
								auto &n = vertices.emplace_back();
								// position of the vertex
								u8 _x = x + nx;
								u8 _y = y + ny;
								u8 _z = z + nz;
								if (uv.x) { _x += texUX; _y += texUY; _z += texUZ; }
								if (uv.y) { _x += texVX; _y += texVY; _z += texVZ; }

								n.position = { _x, _y, _z };
								n.texcoord = guv;
								n.normal = { normX, normY, normZ };

								int aoX = x + 1 + normX;
								int aoY = y + 1 + normY;
								int aoZ = z + 1 + normZ;
								int aoUX = _rev(uv.x, texUX); 
								int aoUY = _rev(uv.x, texUY); 
								int aoUZ = _rev(uv.x, texUZ); 
								int aoVX = _rev(uv.y, texVX); 
								int aoVY = _rev(uv.y, texVY); 
								int aoVZ = _rev(uv.y, texVZ);

								// https://0fps.net/2013/07/03/ambient-occlusion-for-minecraft-like-worlds/
								bool side1 = cch[aoZ + aoUZ][aoY + aoUY][aoX + aoUX] > 0; 
								bool side2 = cch[aoZ + aoVZ][aoY + aoVY][aoX + aoVX] > 0; 
								int ambient = 1; // corners should not be pure black
								if (side1 && side2)
									n.ao = ambient;
								else {
									bool corner = cch[aoZ + aoUZ + aoVZ][aoY + aoUY + aoVY][aoX + aoUX + aoVX]; 
									n.ao = 3 + ambient - (side1 + side2 + corner);
								}
							}

							vidxs[i] = vidx;
						}
						auto vis = blockVisuals[self - 1];
						auto &vec = isPerTexture[vis[s]];

						if (
							vertices[vidxs[0]].ao + vertices[vidxs[2]].ao > 
							vertices[vidxs[1]].ao + vertices[vidxs[3]].ao
						){	
							vec.push_back(vidxs[0]);
							vec.push_back(vidxs[1]);
							vec.push_back(vidxs[2]);
							vec.push_back(vidxs[0]);
							vec.push_back(vidxs[2]);
							vec.push_back(vidxs[3]);
						} else {
							vec.push_back(vidxs[1]);
							vec.push_back(vidxs[2]);
							vec.push_back(vidxs[3]);
							vec.push_back(vidxs[1]);
							vec.push_back(vidxs[3]);
							vec.push_back(vidxs[0]);
						}
					}
					ux += texUX; uy += texUY; uz += texUZ;	
				}
				vx += texVX; vy += texVY; vz += texVZ;
			}
		}
	}

	MesherAllocation result;

	for (int i = 0; i < textureCount; ++i) {
		auto &m = isPerTexture[i];
		if (m.size()) {
			MesherAllocation::Mesh mm;
			mm.texture = i;
			mm.count = m.size();
			for (auto i: m)
				indicesFlat.push_back(i);
			result.meshes.push_back(mm);
		}
	}

	result.vertexCount = 0; 
	result.indices = nullptr;
	result.vertices = nullptr;	

	if (vertices.size() || indicesFlat.size()) {

		result.vertexCount = vertices.size();
		result.vertices = linearAlloc(result.vertexCount * sizeof(vertex));
		if (result.vertices == nullptr) 
		{
			printf("failed linear allocation\n");
			return result;
		}
		memcpy(result.vertices, vertices.data(), result.vertexCount * sizeof(vertex));
		result.indices = linearAlloc(indicesFlat.size() * sizeof(u16));
		if (result.indices == nullptr) {
			printf("failed linear allocation\n");
			linearFree(result.vertices);
			result.vertices = nullptr;
			return result;
		}
		memcpy(result.indices, indicesFlat.data(), indicesFlat.size() * sizeof(u16));
	}

	return result;
}

// todo: we probably do not need this one with the task system
MesherAllocation meshChunk(chunk const &ch, std::array<chunk *, 6> const &sides) {
	
	// -x, +x, -y, +y, -z, +z
	expandedChunk cch = {0};
	expandChunk(ch, sides, cch);
	return meshChunk(cch);
}

void expandChunk(chunk const &ch, std::array<chunk *, 6> const &sides, expandedChunk &ex) {

	for (int z = 0; z < chunkSize; ++z)
		for (int y = 0; y < chunkSize; ++y)
			for (int x = 0; x < chunkSize; ++x)
				ex[z + 1][y + 1][x + 1] = ch[z][y][x]; 

	// organise this somehow...
	if (sides[0]) // -x
		for (int z = 0; z < chunkSize; ++z)
			for (int y = 0; y < chunkSize; ++y)
				ex[z + 1][y + 1][0] = (*sides[0])[z][y][chunkSize-1];
	if (sides[1]) // +x
		for (int z = 0; z < chunkSize; ++z)
			for (int y = 0; y < chunkSize; ++y)
				ex[z + 1][y + 1][chunkSize+1] = (*sides[1])[z][y][0];
	if (sides[2]) // -y
		for (int z = 0; z < chunkSize; ++z)
			for (int x = 0; x < chunkSize; ++x)
				ex[z + 1][0][x + 1] = (*sides[2])[z][chunkSize-1][x];
	if (sides[3]) // +y
		for (int z = 0; z < chunkSize; ++z)
			for (int x = 0; x < chunkSize; ++x)
				ex[z + 1][chunkSize+1][x + 1] = (*sides[3])[z][0][x];
	if (sides[4]) // -z
		for (int y = 0; y < chunkSize; ++y)
			for (int x = 0; x < chunkSize; ++x)
				ex[0][y + 1][x + 1] = (*sides[4])[chunkSize-1][y][x];
	if (sides[5]) // -z
		for (int y = 0; y < chunkSize; ++y)
			for (int x = 0; x < chunkSize; ++x)
				ex[chunkSize+1][y + 1][x + 1] = (*sides[5])[0][y][x];
}


void freeMesh(MesherAllocation &alloc) {
	if (alloc.vertices)
		linearFree(alloc.vertices);
	if (alloc.indices)
		linearFree(alloc.indices);
}

BlockVisual const &getBlockVisual(u16 block) {
	return blockVisuals[block - 1];
}
