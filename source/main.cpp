#include <3ds.h>
#include <cmath>
#include <stdio.h>

#include "common.hpp"
#include "mesher.hpp"
#include "render.hpp"
#include "worker.hpp"
#include "world.hpp"

Block tryGetBlock(int x, int y, int z);

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

static float angleX = 0.0, angleY = 0.0;

struct {
	fvec3 pos;
	fvec3 velocity;
} player;

/*
	3
      --- // 2.4 -> 2 -> 0.1
	2

	1
      --- // 0.5 -> 0 -> 1
	0
*/

float collideFloor(fvec3 pos, float height, float radius, bool &cz) {

	float tx = pos.x;
	float ty = pos.y;

	int l = fastFloor(tx - radius);
	int r = fastFloor(tx + radius);
	int b = fastFloor(ty - radius);
	int f = fastFloor(ty + radius);

	height -= 0.1f;

	float zz = pos.z;

	for (int _y = b; _y <= f; ++_y)
	 	for (int _x = l; _x <= r; ++_x) {
			int t = fastFloor(pos.z + height);
			if (tryGetBlock(_x, _y, t).isSolid()) { // todo: make isblocking or similar
				zz = t - height;
				cz = true;
			}
			int b = fastFloor(pos.z);
			if (tryGetBlock(_x, _y, b).isSolid()) {
				zz = b + 1;
				cz = true;
			}
		}
	return zz;
}

//	0	1	2	3
//	|	|	|	|
//  |   (*)	|	| // 1.2 -> 0.7 -> 0
// 	|	| (*)	| // 1.8 -> 2.3 -> 2

fvec2 collide2d(fvec2 position, int z, float radius, bool &cx, bool &cy) {

	cx = false; cy = false;
	float dx = 0, dy = 0;
	float absdx = 0, absdy = 0;

	// any distance closer than radius + half-block are a collision
	float colDist = radius + 0.5f;

	float tx = position.x;
	float ty = position.y;

	int l = fastFloor(tx - radius);
	int r = fastFloor(tx + radius);
	int b = fastFloor(ty - radius);
	int f = fastFloor(ty + radius);

	for (int _y = b; _y <= f; ++_y)
	 	for (int _x = l; _x <= r; ++_x)
			if (tryGetBlock(_x, _y, z).isSolid()) {
				// distances to centers of blocks
				float distX = _x - tx + 0.5f; // vector from player to centre of block
				float distY = _y - ty + 0.5f;
				float aDistX = fabsf(distX);
				float aDistY = fabsf(distY);
				if (aDistX < colDist && aDistX > absdx) {
					dx = distX;
					absdx = aDistX;
					cx = true;
				}
				if (aDistY < colDist && aDistY > absdy) {
					dy = distY;
					absdy = aDistY;
					cy = true;
				}
			}

	if (dx > 0) dx -= 0.01f; else dx += 0.01f;
	if (dy > 0) dy -= 0.01f; else dy += 0.01f;

	return {
		position.x + ((cx && (!cy || (absdx > absdy))) ? ((dx > 0) ? (dx - radius - 0.5f) : (dx + radius + 0.5f)) : 0),
		position.y + ((cy && (!cx || (absdy > absdx))) ? ((dy > 0) ? (dy - radius - 0.5f) : (dy + radius + 0.5f)) : 0)
	};
}

// todo dynamic height?
void moveAndCollide(fvec3 &position, fvec3 &velocity, float delta, float radius, int height) {
	fvec3 displacement {
		velocity.x * delta,
		velocity.y * delta,
		velocity.z * delta
	};

	int baseZ = fastFloor(position.z + 0.01f);

	float above = position.z - baseZ;
	if (above > 0.1f)
		++height;

	vec2 target { position.x + displacement.x, position.y + displacement.y };

	bool cx = false, cy = false, cz = false;

	for (int z = 0; z < height; ++z) {
		bool lcx, lcy;

		target = collide2d(target, z + baseZ, radius, lcx, lcy);

		if (lcx || lcy)
			target = collide2d(target, z + baseZ, radius, lcx, lcy);

		cx |= lcx;
		cy |= lcy;

	}
	position.x = target.x;
	position.y = target.y;
	position.z += displacement.z;
	position.z = collideFloor(position, height, radius, cz);

	if (cx)
		velocity.x = 0;
	if (cy)
		velocity.y = 0;
	if (cz)
		velocity.z = 0;

}

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


bool scheduleChunk(s16 x, s16 y, s16 z);

ChunkMetadata *tryGetChunk(s16 x, s16 y, s16 z) {

	auto it = world.find({x, y, z});

	if (it != world.end())
		return &it->second;

	else {
		scheduleChunk(x, y, z); // todo: use this bool somehow
		return nullptr;
	}
}

Block tryGetBlock(int x, int y, int z) {

	auto *ch = tryGetChunk(x >> chunkBits, y >> chunkBits, z >> chunkBits);

	if (ch != nullptr)
		return (*ch->data)[z & chunkMask][y & chunkMask][x & chunkMask];
	else
		return { 0xffff };
}

bool tryGetSides(int x, int y, int z, std::array<chunk *, 6> &out) {

	auto _g = [&out](int i, int _x, int _y, int _z) -> bool {

		if (_z < -zChunks || _z > zChunks) {
			out[i] = nullptr;
			return true;
		}

		auto *m = tryGetChunk(_x, _y, _z);
		out[i] = m ? m->data : nullptr;

		return out[i] != nullptr;
	};

	return (
		_g(0, x-1, y, z) &&
		_g(1, x+1, y, z) &&
		_g(2, x, y-1, z) &&
		_g(3, x, y+1, z) &&
		_g(4, x, y, z-1) &&
		_g(5, x, y, z+1)
	);
}

bool scheduleMesh(
	ChunkMetadata &meta,
	std::array<chunk *, 6> const &sides,
	s16vec3 idx, bool priority);

// already loaded chunk changed, force remeshing
// take care of the return value, store it in a list?
bool regenerateMesh(ChunkMetadata &meta, s16 x, s16 y, s16 z) {

	std::array<chunk *, 6> sides { nullptr };

	if (!tryGetSides(x, y, z, sides))
		return false;

	return scheduleMesh(meta, sides, {x, y, z}, true);
}

bool isMeshScheduled(s16vec3 idx);

bool tryMakeMesh(ChunkMetadata &meta, s16 x, s16 y, s16 z) {
	if (meta.meshed)
		return true;

	s16vec3 idx { x, y, z };
	if (isMeshScheduled(idx))
		return false;

	std::array<chunk *, 6> sides { nullptr };
	if (!tryGetSides(x, y, z, sides))
		return false;

	scheduleMesh(meta, sides, idx, true);
	return false;
}

bool tryInitChunkAndMesh(int x, int y, int z) {
	auto *ch = tryGetChunk(x, y, z);

	if (ch == nullptr)
		return false;

	return tryMakeMesh(*ch, x, y, z);
}

vec3<s32> playerFocus;
bool drawFocus = false;

std::vector<s16vec3> chunksToRemesh; // todo bounded array?
void markChunkRemesh(s16vec3 loc) {

	for (auto &idx : chunksToRemesh)
		if (idx == loc)
			return;

	chunksToRemesh.push_back(loc);
}

bool canProcessMeshes(bool priority = false);

void scheduleMarkedRemeshes() {

	while (chunksToRemesh.size()) {

		if (!canProcessMeshes(true))
			return;

		auto &idx = chunksToRemesh.back();
		chunksToRemesh.pop_back(); // todo: it explodes without it, investigate
		auto ch = tryGetChunk(idx.x, idx.y, idx.z);

		if (!ch) // chunk is not resident, so just ignore it
			return;

		regenerateMesh(*ch, idx.x, idx.y, idx.z);
	}
}

inline s16vec3 _sv(s16 x, s16 y, s16 z) { return { x, y, z }; }

// we have idx by now anyway...
void markBlockDirty(int sx, int sy, int sz, s16vec3 chunkIdx) {

	markChunkRemesh(chunkIdx);

	if (sx == 0)
		markChunkRemesh(_sv(chunkIdx.x - 1, chunkIdx.y, chunkIdx.z));
	if (sx == chunkSize - 1)
		markChunkRemesh(_sv(chunkIdx.x + 1, chunkIdx.y, chunkIdx.z));
	if (sy == 0)
		markChunkRemesh(_sv(chunkIdx.x, chunkIdx.y - 1, chunkIdx.z));
	if (sy == chunkSize - 1)
		markChunkRemesh(_sv(chunkIdx.x, chunkIdx.y + 1, chunkIdx.z));
	if (sz == 0)
		markChunkRemesh(_sv(chunkIdx.x, chunkIdx.y, chunkIdx.z - 1));
	if (sz == chunkSize - 1)
		markChunkRemesh(_sv(chunkIdx.x, chunkIdx.y, chunkIdx.z + 1));
}

u16 selectedBlock = 0;

void handleTouch() {

	if (hidKeysHeld() & KEY_TOUCH) {

		touchPosition t;
		hidTouchRead(&t);

		int x = ((int)t.px - 160 + 4*20) / 40;
		int y = ((int)t.py - 120 + 3*20) / 40;

		if (x >= 0 && y >= 0 && x < 4 && y < 3) {
			int id = x + y * 4;
			if (id >= 12)
				return;
			selectedBlock = id;
		}
	}
}

void handlePlayer(float delta) {

	// rotation

	circlePosition cp;
	hidCstickRead(&cp);

	float dx = cp.dx * 0.01f;
	float dy = cp.dy * 0.01f;
	float t = 0.1f;

	if (dx > t || dx < -t) dx = dx > 0 ? dx - t : dx + t; else dx = 0;
	if (dy > t || dy < -t) dy = dy > 0 ? dy - t : dy + t; else dy = 0;

	// citra look hack, comment for HW
	//dx /= 2; dy /= 2;

	angleX += dx * delta * 4;
	angleY -= dy * delta * 4;

	if (angleY > M_PI_2) angleY = M_PI_2;
	if (angleY < -M_PI_2) angleY = -M_PI_2;

	// movement

	u32 kDown = hidKeysDown();

	float walkSpeed = (kDown & KEY_B) ? 20 : 10;
	float walkAcc = 100;

	hidCircleRead(&cp);
	float angleM = atan2f(cp.dx, cp.dy) + angleX;
	float mag2 = cp.dx*cp.dx + cp.dy*cp.dy;

	float targetX = 0;
	float targetY = 0;

	if (mag2 > 40*40) {
		if (mag2 > 140*140)
			mag2 = 140*140;

		float mag = sqrtf(mag2);
		mag -= 40;
		float targetMag = walkSpeed * mag / 100;

		targetX = sinf(angleM) * targetMag;
		targetY = cosf(angleM) * targetMag;
	}

	float vdx = targetX - player.velocity.x;
	float vdy = targetY - player.velocity.y;
	float maxDelta = walkAcc * delta;
	float vmag2 = vdx*vdx+vdy*vdy;

	if (maxDelta*maxDelta < vmag2) {
		float imag = maxDelta / sqrtf(vmag2);
		vdx *= imag;
		vdy *= imag;
	}
	player.velocity.x += vdx;
	player.velocity.y += vdy;

	if ((kDown & KEY_A) && player.velocity.z < 0.1f)
		// todo raycast down
		player.velocity.z = 4.8f; // adjust until it feels ok
	else
		player.velocity.z -= 9.81f * delta;

	// velocity > 20 will break 1-iteration collision
	if (player.velocity.z < -15)
		player.velocity.z = -15;

	moveAndCollide(player.pos, player.velocity, delta, 0.4f, 2);

	if (kDown & KEY_DLEFT) --selectedBlock;
	if (kDown & KEY_DRIGHT) ++selectedBlock;
	selectedBlock %= 12;

	fvec3 dir {
		sinf(angleX) * cosf(angleY),
		cosf(angleX) * cosf(angleY),
		-sinf(angleY)
	};
	drawFocus = false;
	vec3<s32> normal;

	if (raycast(
		{player.pos.x, player.pos.y, player.pos.z + 1.5f},
		dir, 3, playerFocus, normal)
	) {
		if (selectedBlock && (kDown & (KEY_Y | KEY_R))) {
			int nx = playerFocus.x + normal.x;
			int ny = playerFocus.y + normal.y;
			int nz = playerFocus.z + normal.z;

			auto idx = _sv(nx >> chunkBits, ny >> chunkBits, nz >> chunkBits);
			auto *ch = tryGetChunk(idx.x, idx.y, idx.z);

			if (ch) {
				int lx = nx & chunkMask, ly = ny & chunkMask, lz = nz & chunkMask;
				(*ch->data)[lz][ly][lx] = Block::solid(selectedBlock - 1);
				markBlockDirty(lx, ly, lz, idx);
			}
		}

		if (kDown & (KEY_X | KEY_L)) {
			int nx = playerFocus.x;
			int ny = playerFocus.y;
			int nz = playerFocus.z;

			auto idx = _sv(nx >> chunkBits, ny >> chunkBits, nz >> chunkBits);
			auto *ch = tryGetChunk(idx.x, idx.y, idx.z);

			if (ch) {
				int lx = nx & chunkMask, ly = ny & chunkMask, lz = nz & chunkMask;
				(*ch->data)[lz][ly][lx] = { 0 };
				markBlockDirty(lx, ly, lz, idx);
			}
		}
		drawFocus = true;
	}
}

constexpr int maxScheduledMeshes = 8;
constexpr int scheduledMeshesPrioritySpace = 2;
std::vector<s16vec3> scheduledMeshes; // todo array

inline bool canProcessMeshes(bool priority) {

	return scheduledMeshes.size() < (priority ?
		maxScheduledMeshes + scheduledMeshesPrioritySpace :
		maxScheduledMeshes);
}

bool isMeshScheduled(s16vec3 idx) {

	for (auto &c: scheduledMeshes)
		if (c == idx)
			return true;

	return false;
}

bool scheduleMesh(
	ChunkMetadata &meta,
	std::array<chunk *, 6> const &sides,
	s16vec3 idx, bool priority
) {
	// todo: caller could check for this to save time on gathering sides
	if (!canProcessMeshes(priority))
		return false;

	// no check for scheduled since they can have different chunk data;
	// call to force remeshing, or check separately to avoid moving data

	scheduledMeshes.push_back(idx);

	Task task;
	task.chunk.x = idx.x;
	task.chunk.y = idx.y;
	task.chunk.z = idx.z;

	task.chunk.exdata = new expandedChunk{0}; // todo cache allocations
	task.type = Task::Type::MeshChunk;

	expandChunk(*meta.data, sides, *task.chunk.exdata);

	return postTask(task, priority);
}

constexpr int maxScheduledChunks = 8;
std::vector<s16vec3> scheduledChunks;

inline bool canProcessChunks() {
	return scheduledChunks.size() < maxScheduledChunks;
}

bool scheduleChunk(s16 x, s16 y, s16 z) {

	if (!canProcessChunks())
		return false;

	s16vec3 idx {x, y, z};

	for (auto &c: scheduledChunks)
		if (c == idx)
			return true;

	scheduledChunks.push_back(idx);

	Task task;
	task.chunk.x = x;
	task.chunk.y = y;
	task.chunk.z = z;
	task.type = Task::Type::GenerateChunk;
	return postTask(task);
}

void invalidateWorldIterator();

void processWorkerResults() {

	TaskResult r;

	while (getResult(r))
		switch (r.type) {
			case TaskResult::Type::ChunkData: {

				s16vec3 idx = { r.chunk.x, r.chunk.y, r.chunk.z };
				auto oldBuckets = world.bucket_count();
				auto &meta = world[idx];

				// this will cancel iterative unloading if it is progress
				if (world.bucket_count() != oldBuckets)
					invalidateWorldIterator();

				meta.data = r.chunk.data;
				for (size_t i = 0; i < scheduledChunks.size(); ++i)
					if (scheduledChunks[i] == idx) {
						scheduledChunks[i] = scheduledChunks.back();
						scheduledChunks.pop_back();
					}
				meta.visibility = r.chunk.visibility;
			} break;

			case TaskResult::Type::ChunkMesh: {
				s16vec3 idx = { r.chunk.x, r.chunk.y, r.chunk.z };
				auto &meta = world[idx];

				if (meta.allocation.vertexCount)
					freeMesh(meta.allocation);

				meta.allocation = std::move(*r.chunk.alloc);

				if (r.flags & TaskResult::RESULT_VISIBILITY)
					meta.visibility = r.chunk.visibility;

				if (meta.allocation.vertexCount) {
					BufInfo_Init(&meta.vertexBuffer);
					BufInfo_Add(&meta.vertexBuffer, meta.allocation.vertices, sizeof(vertex), 4, 0x3210);
				}

				meta.meshed = true;
				delete r.chunk.alloc;

				for (size_t i = 0; i < scheduledMeshes.size(); ++i)
					if (scheduledMeshes[i] == idx) {
						scheduledMeshes[i] = scheduledMeshes.back();
						scheduledMeshes.pop_back();
					}
			} break;

			default: break;
		}
}

// note that a 1-chunk thick shell will generate outside the render cage since it is needed for meshing
static constexpr int distanceLoad = 6; // blocks to load, cage size 2n+1
static constexpr int distanceUnload = 8; // blocks to unload, keep blocks in cage of 2n+1

enum class WMStatus {
	Idle,
	UnloadDistance,
	LoadVisible
};

struct {
	int chX, chY, chZ;

	std::vector<Result> pendingResults;
	WorldMap::iterator checkToErase;
	std::vector<s16vec3> checkToLoad;

	WMStatus status = WMStatus::Idle;

} wm;

void invalidateWorldIterator() {
	if (wm.status == WMStatus::UnloadDistance)
		wm.status = WMStatus::LoadVisible;
}

constexpr u32 maxWMTime = SYSCLOCK_ARM11 / 1000; // 1ms per frame

void wmSchedule(fvec3 focus) {

	wm.chX = static_cast<int>(focus.x) >> chunkBits;
	wm.chY = static_cast<int>(focus.y) >> chunkBits;
	wm.chZ = static_cast<int>(focus.z) >> chunkBits;

	wm.checkToLoad.clear();

	wm.checkToErase = world.begin();

	for (int z = wm.chZ - distanceLoad; z <= wm.chZ + distanceLoad; ++z)
		for (int y = wm.chY - distanceLoad; y <= wm.chY + distanceLoad; ++y)
			for (int x = wm.chX - distanceLoad; x <= wm.chX + distanceLoad; ++x)
				if (z >= -zChunks && z <= zChunks)
					wm.checkToLoad.push_back(_sv(x, y, z));

	wm.status = WMStatus::UnloadDistance;
}


inline void wmUnload() {
	auto &it = wm.checkToErase;
	if (wm.checkToErase != world.end()) {
		auto &idx = it->first;
		if (
			idx.x < wm.chX - distanceUnload ||
			idx.x > wm.chX + distanceUnload ||
			idx.y < wm.chY - distanceUnload ||
			idx.y > wm.chY + distanceUnload ||
			idx.y < wm.chY - distanceUnload ||
			idx.z > wm.chZ + distanceUnload
		)
			it = destroyChunk(it);
		else
			++it;
	}
	else
		wm.status = WMStatus::LoadVisible;
}

inline void wmLoad() {

	if (wm.checkToLoad.size()) {

		auto idx = wm.checkToLoad.back();
		wm.checkToLoad.pop_back();

		tryInitChunkAndMesh(idx.x, idx.y, idx.z);
	} else
		wm.status = WMStatus::Idle;
}

// block management only gets a small time-slice from the budget
void manageWorld(fvec3 focus) {

	PROFILE_SCOPE;

	// todo maybe batch the updates a bit, check cost of the tick call
	u32 startTime = svcGetSystemTick();

	while (((u32)svcGetSystemTick() - startTime) < maxWMTime)
		switch (wm.status) {
			case WMStatus::Idle:
				wmSchedule(focus);
				break;
			case WMStatus::UnloadDistance:
				wmUnload();
				break;
			case WMStatus::LoadVisible:
				wmLoad();
				break;
		}
}

enum class RunMode {
	Loading,
	Running,
	Closing
};

RunMode runMode;

u64 tick;

void loadMapLoop() {

	processWorkerResults();

	bool ready = true;
	for (int z = -1; z <= 1; ++z)
		for (int y = -2; y <= 2; ++y)
			for (int x = -2; x <= 2; ++x)
				if (!tryInitChunkAndMesh(x, y, z))
					ready = false;

	renderLoading();
	if (ready)
		runMode = RunMode::Running;
}

void mainLoop() {
// do it before input and vsync wait, consider player input
	processWorkerResults();
	scheduleMarkedRemeshes();
	manageWorld(player.pos);

	hidScanInput();

	float slider = osGet3DSliderState();

	handleTouch();

	// Respond to user input
	u32 kDown = hidKeysDown();
	if (kDown & KEY_START)
		runMode = RunMode::Closing;

	u64 newTick = svcGetSystemTick();
	u32 tickDelta = newTick - tick;
	tick = newTick;

	float delta = tickDelta * invTickRate;
	if (delta > 0.05f) // aim for minimum 20 fps, anything less is a spike
		delta = 0.05f;

	handlePlayer(delta);

	render(player.pos, angleX, angleY, drawFocus ? &playerFocus : nullptr, slider);
}

// #define CITRA_PROFILE

int main() {

	bool console = false;

	hidScanInput();
	u32 keys = hidKeysHeld();

	#if (defined(PROFILER) && defined(CITRA_PROFILE))
	// Enter debug mode when holding dpad-up or running on JPN/"wonky" system.
	// Citra seems to return/fail to JPN so check for that;
	// While I doubt many Japanese people would be interested in playing this,
	// it would be best to disable it when not profiling.

	cfguInit();
	u8 region = 0;
	CFGU_SecureInfoGetRegion(&region);
	cfguExit();

	if (region == 0)
		console = true;
	#endif

	if (keys & KEY_DUP)
		console = true;

	renderInit(!console);
	startWorker();

	tick = svcGetSystemTick();

	player.pos.z = 12;
	player.pos.x = 0;
	player.pos.y = 0;

	runMode = RunMode::Loading;
	while (aptMainLoop() && runMode != RunMode::Closing)
		switch (runMode) {
			case RunMode::Loading:
				loadMapLoop(); break;
			case RunMode::Running:
				mainLoop(); break;
			default: runMode = RunMode::Closing;
		}

	stopWorker(); // halt processing, submit leftover jobs as results
	processWorkerResults(); // accept all results so they do not leak

	for (auto &[idx, meta]: world)
		if (meta.meshed)
			freeMesh(meta.allocation);

	renderExit();

	return 0;
}
