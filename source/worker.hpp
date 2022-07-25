#pragma once
#include "common.hpp"

struct Task {
    enum class Type {
        Dummy,
        GenerateChunk,
    };
    union {
        void *ptr;
        u32 value;
        struct {
            s16 x, y, z;
        } chunkId;
    };
    Type type;
};

struct TaskResult {
    enum class Type {
        Dummy,
        ChunkData,
    };
    union {
        void *ptr;
        struct {
            chunk *data;
            s16 x, y, z;
        } chunk;
    };
    Type type;
};

void startWorker();
void stopWorker();
bool postTask(Task task);
bool getResult(TaskResult &result);
