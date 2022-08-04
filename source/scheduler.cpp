#include "scheduler.hpp"

namespace {

    constexpr int maxScheduledMeshes = 8;
    constexpr int scheduledMeshesPrioritySpace = 2;

    std::vector<s16vec3> scheduledMeshes; // todo array


    constexpr int maxScheduledChunks = 8;
    
    std::vector<s16vec3> scheduledChunks;

}

ChunkMetadata *getOrScheduleChunk(s16 x, s16 y, s16 z) {

	auto it = world.find({x, y, z});

	if (it != world.end())
		return &it->second;

	else {
		scheduleChunk(x, y, z); // todo: use this bool somehow
		return nullptr;
	}
}

Block getOrScheduleBlock(int x, int y, int z) {

	auto *ch = getOrScheduleChunk(x >> chunkBits, y >> chunkBits, z >> chunkBits);

	if (ch != nullptr)
		return (*ch->data)[z & chunkMask][y & chunkMask][x & chunkMask];
	else
		return { 0xffff };
}

bool getOrScheduleSides(int x, int y, int z, std::array<chunk *, 6> &out) {

	auto _g = [&out](int i, int _x, int _y, int _z) -> bool {

		if (_z < -zChunks || _z > zChunks) {
			out[i] = nullptr;
			return true;
		}

		auto *m = getOrScheduleChunk(_x, _y, _z);
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

	if (!getOrScheduleSides(x, y, z, sides))
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
	if (!getOrScheduleSides(x, y, z, sides))
		return false;

	scheduleMesh(meta, sides, idx, true);
	return false;
}

bool tryInitChunkAndMesh(int x, int y, int z) {
	auto *ch = getOrScheduleChunk(x, y, z);

	if (ch == nullptr)
		return false;

	return tryMakeMesh(*ch, x, y, z);
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

bool canProcessChunks() {
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

void scheduledChunkReceived(s16vec3 idx) {
    for (size_t i = 0; i < scheduledChunks.size(); ++i)
    if (scheduledChunks[i] == idx) {
        scheduledChunks[i] = scheduledChunks.back();
        scheduledChunks.pop_back();
    }

}

bool canProcessMeshes(bool priority) {

	return scheduledMeshes.size() < (priority ?
		maxScheduledMeshes + scheduledMeshesPrioritySpace :
		maxScheduledMeshes);
}

void scheduledMeshReceived(s16vec3 idx) {
    for (size_t i = 0; i < scheduledMeshes.size(); ++i)

    if (scheduledMeshes[i] == idx) {
        scheduledMeshes[i] = scheduledMeshes.back();
        scheduledMeshes.pop_back();
    }
}

bool isMeshScheduled(s16vec3 idx) {

	for (auto &c: scheduledMeshes)
		if (c == idx)
			return true;

	return false;
}
