#include <3ds.h>
#include <cmath>
#include <stdio.h>

#include "common.hpp"
#include "mesher.hpp"
#include "render.hpp"

#include "scheduler.hpp"
#include "player.hpp"

Player player;

vec3<s32> playerFocus;
bool drawFocus = false;

std::vector<s16vec3> chunksToRemesh; // todo bounded array?
void markChunkRemesh(s16vec3 loc) {

	for (auto &idx : chunksToRemesh)
		if (idx == loc)
			return;

	chunksToRemesh.push_back(loc);
}

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

		int x = ((int)t.px - 160 + 6*20) / 40;
		int y = ((int)t.py - 120 + 3*20) / 40;

		if (x >= 0 && y >= 0 && x < 6 && y < 5) {
			int id = x + y * 8;

			selectedBlock = id;
		}
	}
}

void handlePlayer(float delta) {

	player.moveInput(delta);

	u32 kDown = hidKeysDown();

	// if (kDown & KEY_DLEFT) --selectedBlock;
	// if (kDown & KEY_DRIGHT) ++selectedBlock;
	// selectedBlock %= 12;

	fvec3 dir {
		sinf(player.rx) * cosf(player.ry),
		cosf(player.rx) * cosf(player.ry),
		-sinf(player.ry)
	};

	drawFocus = false;
	vec3<s32> normal;

	if (raycast(
		{player.pos.x, player.pos.y, player.pos.z + 1.5f},
		dir, 4, playerFocus, normal)
	) {
		if (selectedBlock && (kDown & (KEY_Y | KEY_R))) {
			int nx = playerFocus.x + normal.x;
			int ny = playerFocus.y + normal.y;
			int nz = playerFocus.z + normal.z;

			auto idx = _sv(nx >> chunkBits, ny >> chunkBits, nz >> chunkBits);
			auto *ch = tryGetChunk(idx.x, idx.y, idx.z);

			if (ch) {
				int lx = nx & chunkMask, ly = ny & chunkMask, lz = nz & chunkMask;
				(*ch->data)[lz][ly][lx] = Block::solid(selectedBlock);
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

				scheduledChunkReceived(idx);
				meta.data = r.chunk.data;
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

				scheduledMeshReceived(idx);

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

	render(player.pos, player.rx, player.ry, drawFocus ? &playerFocus : nullptr, slider);
}

// #define CITRA_CONSOLE

int main() {

	romfsInit();

	bool console = false;

	hidScanInput();
	u32 keys = hidKeysHeld();

	#ifdef CITRA_CONSOLE
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

	romfsExit();

	return 0;
}
