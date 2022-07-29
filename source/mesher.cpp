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

INLINE s8vec3 _V8(int x, int y, int z) {
	return {
		static_cast<s8>(x),
		static_cast<s8>(y),
		static_cast<s8>(z)
	};
}

struct ForwardAccessor {
	INLINE static int at(int value) { return value; }
	INLINE static int atV(int value) { return value; }
	INLINE static int atN(int value) { return value + 1; }
	INLINE static int dir() { return 1; }
};

struct ReverseAccessor {
	INLINE static int at(int value) { return chunkSize - 1 - value; }
	INLINE static int atV(int value) { return chunkSize - value; }
	INLINE static int atN(int value) { return chunkSize - 1 - value; }
	INLINE static int dir() { return -1; }
};

template <typename DirX, typename DirY, typename DirZ>
struct FaceXAccessor {
	INLINE static s8vec3 at(int u, int v, int n) {
		return _V8(DirX::at(n), DirY::at(u), DirZ::at(v));
	}
	INLINE static s8vec3 atV(int u, int v, int n) {
		return _V8(DirX::atN(n), DirY::atV(u), DirZ::atV(v));
	}
};

template  <typename DirX, typename DirY, typename DirZ>
struct FaceYAccessor {
	INLINE static s8vec3 at(int u, int v, int n) {
		return _V8(DirX::at(u), DirY::at(n), DirZ::at(v));
	}
	INLINE static s8vec3 atV(int u, int v, int n) {
		return _V8(DirX::atV(u), DirY::atN(n), DirZ::atV(v));
	}
};

template  <typename DirX, typename DirY, typename DirZ>
struct FaceZAccessor {
	INLINE static s8vec3 at(int u, int v, int n) {
		return _V8(DirX::at(u), DirY::at(v), DirZ::at(n));
	}
	INLINE static s8vec3 atV(int u, int v, int n) {
		return _V8(DirX::atV(u), DirY::atV(v), DirZ::atN(n));
	}
};

template <int t>
struct AxisAccessor {};
template <> struct AxisAccessor<0> : FaceXAccessor<ReverseAccessor, ReverseAccessor, ForwardAccessor> {};
template <> struct AxisAccessor<1> : FaceXAccessor<ForwardAccessor, ForwardAccessor, ForwardAccessor> {};
template <> struct AxisAccessor<2> : FaceYAccessor<ForwardAccessor, ReverseAccessor, ForwardAccessor> {};
template <> struct AxisAccessor<3> : FaceYAccessor<ReverseAccessor, ForwardAccessor, ForwardAccessor> {};
template <> struct AxisAccessor<4> : FaceZAccessor<ReverseAccessor, ForwardAccessor, ReverseAccessor> {};
template <> struct AxisAccessor<5> : FaceZAccessor<ForwardAccessor, ForwardAccessor, ForwardAccessor> {};

template <int s>
INLINE void meshFace(
	expandedChunk const &cch,
	std::vector<vertex> &vertices,
	std::array<std::vector<u16>, textureCount> &isPerTexture
) {
	using a = AxisAccessor<s>;
	std::array<u16, (chunkSize+1) * (chunkSize+1)> vertexCache;

	for (int l = 0; l < chunkSize; ++l) { // layer

		for (int i = 0; i < (chunkSize+1) * (chunkSize+1); ++i)
			vertexCache[i] = 0xffff;

		for (int v = 0; v < chunkSize; ++v) {
			for (int u = 0; u < chunkSize; ++u) {
				uint16_t vidxs[4];

				// this is block position in the grid
				auto idx = a::at(u, v, l);
				u16 self = cch[idx.z+1][idx.y+1][idx.x+1];

				bool draw = self > 0;
				if (draw) {
					// neighbouring block of this face
					auto normIdx = a::at(u, v, l + 1);
					u16 other = cch[normIdx.z+1][normIdx.y+1][normIdx.x+1];
					draw = other == 0;
				}
				if (draw) {
					for (int i = 0; i < 4; ++i) {
						// vertex positon in its face coordinates
						auto uv = basicCubeUVs[i];
						// vertex position in chunk plane
						auto guv = uv;
						guv.x += u; guv.y += v;

						auto cacheIdx = guv.x + guv.y * (chunkSize+1);
						auto vertexIdx = vertexCache[cacheIdx];

						if (vertexIdx == 0xffff) {
							vertexIdx = static_cast<u16>(vertices.size());
							vertexCache[cacheIdx] = vertexIdx;
							auto &n = vertices.emplace_back();

							// position of the vertex
							auto coords = a::atV(u + uv.x, v + uv.y, l);

							n.position = {
								static_cast<u8>(coords.x),
								static_cast<u8>(coords.y),
								static_cast<u8>(coords.z)
							};
							n.texcoord = guv;
							// note: we do not really need normals anymore
							n.normal = { 0, 0, 0 };

							auto u1 = uv.x ? 1 : -1;
							auto v1 = uv.y ? 1 : -1;

							auto dir1 = a::at(u + u1, v, l + 1);
							auto dir2 = a::at(u, v + v1, l + 1);

							// https://0fps.net/2013/07/03/ambient-occlusion-for-minecraft-like-worlds/
							bool side1 = cch[dir1.z+1][dir1.y+1][dir1.x+1] > 0;
							bool side2 = cch[dir2.z+1][dir2.y+1][dir2.x+1] > 0;
							int ambient = 1; // corners should not be pure black
							if (side1 && side2)
								n.ao = ambient;
							else {
								auto dirs = a::at(u + u1, v + v1, l + 1);
								bool corner = cch[dirs.z+1][dirs.y+1][dirs.x+1];
								n.ao = 3 + ambient - (side1 + side2 + corner);
							}
						}

						vidxs[i] = vertexIdx;
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
			}
		}
	}
}

MesherAllocation meshChunk(expandedChunk const &cch) {
	std::vector<vertex> vertices;
	std::array<std::vector<u16>, textureCount> isPerTexture;

	meshFace<0>(cch, vertices, isPerTexture);
	meshFace<1>(cch, vertices, isPerTexture);
	meshFace<2>(cch, vertices, isPerTexture);
	meshFace<3>(cch, vertices, isPerTexture);
	meshFace<4>(cch, vertices, isPerTexture);
	meshFace<5>(cch, vertices, isPerTexture);

	MesherAllocation result;
	std::vector<u16> indicesFlat;

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
