#include <stdio.h>

#include "mesher.hpp"

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

static const uint8_t basicCubeIs[6*2*3] = {
	// -X
	3, 0, 4, 3, 4, 7,
	// +X
	1, 2, 6, 1, 6, 5,
	// -Y
	0, 1, 5, 0, 5, 4,
	// +Y
	2, 3, 7, 2, 7, 6,
	// -Z
	1, 0, 3, 1, 3, 2,
	// +Z
	4, 5, 6, 4, 6, 7,
};

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

// (0), dirt, grass, stone
static constexpr BlockVisual blockVisuals[] = {
	{ 0, 0, 0, 0, 0, 0 },
	{ 1, 1, 1, 1, 0, 2 },
	{ 3, 3, 3, 3, 3, 3 }
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

// todo handle allocation errors

MesherAllocation meshChunk(chunk const &ch, std::array<chunk *, 6> const &sides) {
	std::vector<vertex> vertices;
	std::array<u16, (chunkSize+1) * (chunkSize+1)> vertexCache;
	std::vector<u16> indicesFlat;
	std::array<std::vector<u16>, textureCount> isPerTexture;
	
	// -x, +x, -y, +y, -z, +z
	std::array<std::array<std::array<u16, chunkSize+2>, chunkSize+2>, chunkSize+2> cch;// = {0};
	for (int z = 0; z < chunkSize; ++z)
		for (int y = 0; y < chunkSize; ++y)
			for (int x = 0; x < chunkSize; ++x)
				cch[z + 1][y + 1][x + 1] = ch[z][y][x];  

	// organise this somehow...
	if (sides[0]) // -x
		for (int z = 0; z < chunkSize; ++z)
			for (int y = 0; y < chunkSize; ++y)
				cch[z + 1][y + 1][0] = (*sides[0])[z][y][chunkSize-1];
	if (sides[1]) // +x
		for (int z = 0; z < chunkSize; ++z)
			for (int y = 0; y < chunkSize; ++y)
				cch[z + 1][y + 1][chunkSize+1] = (*sides[1])[z][y][0];
	if (sides[2]) // -y
		for (int z = 0; z < chunkSize; ++z)
			for (int x = 0; x < chunkSize; ++x)
				cch[z + 1][0][x + 1] = (*sides[2])[z][chunkSize-1][x];
	if (sides[3]) // +y
		for (int z = 0; z < chunkSize; ++z)
			for (int x = 0; x < chunkSize; ++x)
				cch[z + 1][chunkSize+1][x + 1] = (*sides[3])[z][0][x];
	if (sides[4]) // -z
		for (int y = 0; y < chunkSize; ++y)
			for (int x = 0; x < chunkSize; ++x)
				cch[0][y + 1][x + 1] = (*sides[4])[chunkSize-1][y][x];
	if (sides[5]) // -z
		for (int y = 0; y < chunkSize; ++y)
			for (int x = 0; x < chunkSize; ++x)
				cch[chunkSize+1][y + 1][x + 1] = (*sides[5])[0][y][x];

	for (int s = 0; s < 6; ++s) { // side
		
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

					u8 x = lx + ux + vx;
					u8 y = ly + uy + vy;
					u8 z = lz + uz + vz;
					
					u16 self = ch[z][y][x] & 0x3; // mod for now
					bool draw = self > 0;
					if (draw) {
						s8 sx = x + normX;
						s8 sy = y + normY;
						s8 sz = z + normZ;
						u16 other = cch[sz+1][sy+1][sx+1] & 0x3;
						draw = other == 0;
					}
					if (draw) {
						for (int i = 0; i < 4; ++i) {
							auto uv = basicCubeUVs[i]; 
							auto guv = uv;
							guv.x += u; guv.y += v;
							auto cidx = guv.x + guv.y * (chunkSize+1);
							auto vidx = vertexCache[cidx];
							if (vidx == 0xffff) {
								vidx = static_cast<u16>(vertices.size()); 
								vertexCache[cidx] = vidx;
								auto &n = vertices.emplace_back();
								u8 _x = x + nx;
								u8 _y = y + ny;
								u8 _z = z + nz;
								if (uv.x) { _x += texUX; _y += texUY; _z += texUZ; }
								if (uv.y) { _x += texVX; _y += texVY; _z += texVZ; }
								// printf("%i,%i,%i: %i, %i, %i / %i\n", u, v, l, x, y, z, s);

								n.position = { _x, _y, _z };
								n.texcoord = guv;
								n.normal = { normX, normY, normZ };
							}

							vidxs[i] = vidx;
						}
						auto vis = blockVisuals[self - 1];
						auto &vec = isPerTexture[vis[s]];
						vec.push_back(vidxs[0]);
						vec.push_back(vidxs[1]);
						vec.push_back(vidxs[2]);
						vec.push_back(vidxs[0]);
						vec.push_back(vidxs[2]);
						vec.push_back(vidxs[3]);
					}
					ux += texUX; uy += texUY; uz += texUZ;	
				}
				vx += texVX; vy += texVY; vz += texVZ;
			}
		}
	}

	//printf("v: %i, i: %i\n", (int)vertices.size(), (int)indices.size());

	MesherAllocation result;

	u16 totalIdxCount = 0;
	
	for (int i = 0; i < textureCount; ++i) {
		auto &m = isPerTexture[i];
		if (m.size()) {
			MesherAllocation::Mesh mm;
			mm.texture = i;
			mm.count = m.size();
			mm.start = totalIdxCount;
			totalIdxCount += m.size();
			for (auto i: m)
				indicesFlat.push_back(i);
			result.meshes.push_back(mm);
		}
	}

	if (vertices.size() || indicesFlat.size()) {

		result.vertexCount = vertices.size();
		result.vertices = linearAlloc(result.vertexCount * sizeof(vertex));
		if (result.vertices == nullptr) // todo safe allocation	
			svcBreak(USERBREAK_PANIC);
		memcpy(result.vertices, vertices.data(), result.vertexCount * sizeof(vertex));
		result.indices = linearAlloc(indicesFlat.size() * sizeof(uint16_t));
		if (result.indices == nullptr)
			svcBreak(USERBREAK_PANIC);
		memcpy(result.indices, indicesFlat.data(), indicesFlat.size() * sizeof(uint16_t));
	}
	else {
		result.vertexCount = 0; 
		result.indices = nullptr;
		result.vertices = nullptr;	
	}
	return result;
}

void freeMesh(MesherAllocation &alloc) {
	if (alloc.vertices)
		linearFree(alloc.vertices);
	if (alloc.indices)
		linearFree(alloc.indices);
}
