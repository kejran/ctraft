#include "world.hpp"

// https://github.com/fenomas/fast-voxel-raycast/blob/master/index.js
bool raycast(fvec3 eye, fvec3 dir, float maxLength, vec3<s32> &out, vec3<s32> &normal) {

	float norm = 1 / sqrtf(dir.x*dir.x + dir.y*dir.y + dir.z*dir.z);
	dir.x *= norm;
	dir.y *= norm;
	dir.z *= norm;

	int ix = fastFloor(eye.x);
	int iy = fastFloor(eye.y);
	int iz = fastFloor(eye.z);

	int stepx = (dir.x > 0.0f) ? 1 : -1;
	int stepy = (dir.y > 0.0f) ? 1 : -1;
	int stepz = (dir.z > 0.0f) ? 1 : -1;

	// divide by abs so it goes to +inf instead of ±inf
	float txDelta = 1.0f / fabsf(dir.x);
	float tyDelta = 1.0f / fabsf(dir.y);
	float tzDelta = 1.0f / fabsf(dir.z);

	float xdist = (stepx > 0) ? (ix + 1 - eye.x) : (eye.x - ix);
	float ydist = (stepy > 0) ? (iy + 1 - eye.y) : (eye.y - iy);
	float zdist = (stepz > 0) ? (iz + 1 - eye.z) : (eye.z - iz);

	float txMax = (txDelta < 9999) ? txDelta * xdist : 99999;
	float tyMax = (tyDelta < 9999) ? tyDelta * ydist : 99999;
	float tzMax = (tzDelta < 9999) ? tzDelta * zdist : 99999;

	int steppedIndex = -1;
	float t_ = 0;

	while (t_ <= maxLength) {

		// exit check
		auto b = tryGetBlock(ix, iy, iz);
		if (b.value == 0xffff) // todo extract it out somewhere
			return false;
		if (b.isSolid() || b.isFoliage()) {
			out.x = ix; out.y = iy; out.z = iz;
			normal = {0, 0, 0};
			if (steppedIndex == 0) normal.x = (stepx < 0) ? 1 : -1;
			if (steppedIndex == 1) normal.y = (stepy < 0) ? 1 : -1;
			if (steppedIndex == 2) normal.z = (stepz < 0) ? 1 : -1;

			return true;
		}

		// advance t to next nearest voxel boundary
		if (txMax < tyMax) {
			if (txMax < tzMax) {
				ix += stepx;
				t_ = txMax;
				txMax += txDelta;
				steppedIndex = 0;
			} else {
				iz += stepz;
				t_ = tzMax;
				tzMax += tzDelta;
				steppedIndex = 2;
			}
		} else {
			if (tyMax < tzMax) {
				iy += stepy;
				t_ = tyMax;
				tyMax += tyDelta;
				steppedIndex = 1;
			} else {
				iz += stepz;
				t_ = tzMax;
				tzMax += tzDelta;
				steppedIndex = 2;
			}
		}

	}

	// no voxel hit found

	return false;
}

WorldMap::iterator destroyChunk(WorldMap::iterator it) {
	delete it->second.data;
	freeMesh(it->second.allocation);
	return world.erase(it);
}

void destroyChunk(s16vec3 v) {
	auto it = world.find(v);
	if (it != world.end())
		destroyChunk(it);
}

ChunkMetadata *tryGetChunk(s16 x, s16 y, s16 z) {

	auto it = world.find({x, y, z});

	if (it != world.end())
		return &it->second;
	else 
		return nullptr;
}

Block tryGetBlock(int x, int y, int z) {

	auto *ch = tryGetChunk(x >> chunkBits, y >> chunkBits, z >> chunkBits);

	if (ch != nullptr)
		return (*ch->data)[z & chunkMask][y & chunkMask][x & chunkMask];
	else
		return { 0xffff };
}