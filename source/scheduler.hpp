#pragma once

#include "worker.hpp"
#include "world.hpp"

// todo: organise these functions better

WorldMap::iterator destroyChunk(WorldMap::iterator it);

void destroyChunk(s16vec3 v);

bool scheduleChunk(s16 x, s16 y, s16 z);

ChunkMetadata *getOrScheduleChunk(s16 x, s16 y, s16 z);

Block getOrScheduleBlock(int x, int y, int z);

bool getOrScheduleSides(int x, int y, int z, std::array<chunk *, 6> &out);

bool regenerateMesh(ChunkMetadata &meta, s16 x, s16 y, s16 z);

bool tryMakeMesh(ChunkMetadata &meta, s16 x, s16 y, s16 z);

bool tryInitChunkAndMesh(int x, int y, int z);

bool canProcessChunks();

bool scheduleChunk(s16 x, s16 y, s16 z);

void scheduledChunkReceived(s16vec3 idx);

bool canProcessMeshes(bool priority);

bool scheduleMesh(
	ChunkMetadata &meta,
	std::array<chunk *, 6> const &sides,
	s16vec3 idx, 
    bool priority
);

void scheduledMeshReceived(s16vec3 idx);

bool isMeshScheduled(s16vec3 idx);