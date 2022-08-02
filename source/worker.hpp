#pragma once
#include "common.hpp"

#include "mesher.hpp"

struct Task {
    enum class Type: u8 {
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
    u8 flags; 

    static constexpr int TASK_VISIBILITY = 1;
};

struct TaskResult {
    enum class Type: u8 {
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
            u8 visibility;
        } chunk;
    };
    Type type;
    u8 flags;
    static constexpr int RESULT_VISIBILITY = 1;
};

void startWorker();
void stopWorker();
bool postTask(Task task, bool priority = false);
bool getResult(TaskResult &result);
