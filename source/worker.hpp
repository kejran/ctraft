#pragma once
#include "common.hpp"

#include "mesher.hpp"

struct Task {
    enum class Type {
        GenerateChunk,
        MeshChunk,
        Tag,
    };
    union {
        void *ptr;
        u32 value;
        struct {
            expandedChunk *exdata;
            s16 x, y, z;
        } chunk;
    };
    Type type;
};

struct TaskResult {
    enum class Type {
        ChunkData,
        ChunkMesh,
        Tag,
    };
    union {
        u32 value;
        struct {
            union {
                chunk *data;
                MesherAllocation *alloc;
            };
            s16 x, y, z;
        } chunk;
    };
    Type type;
};

void startWorker();
void stopWorker();
bool postTask(Task task);
bool getResult(TaskResult &result);
