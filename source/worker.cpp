#include "worker.hpp"

#include "worldgen.hpp"
#include "mesher.hpp"

namespace {
    
    volatile bool runWorker = true;

    Thread workerThread;

    CondVar signalNewTask;
    CondVar signalNewResult;

    constexpr int queuesize = 32;
        
        // todo a common class maybe?
    struct {
        std::array<Task, queuesize> data; 

        int start = 0; 
        int count = 0;

        LightLock lock;
    } tasks;

    struct {
        std::array<TaskResult, queuesize> data; 

        int start = 0; 
        int count = 0;

        LightLock lock;
    } results;

}

bool postResult(TaskResult result);
void processTask(Task &t) {	

    TaskResult r;

    switch (t.type) {

        // todo: we might end up with symmetrical enums, monitor it
        case Task::Type::GenerateChunk:
            r.type = TaskResult::Type::ChunkData;
            r.chunk.data = generateChunk(t.chunk.x, t.chunk.y, t.chunk.z);
            r.chunk.x = t.chunk.x; r.chunk.y = t.chunk.y; r.chunk.z = t.chunk.z;
            postResult(r);
            break;
        
        case Task::Type::MeshChunk:
            r.type = TaskResult::Type::ChunkMesh;
            // todo make these allocs saner
            r.chunk.alloc = new MesherAllocation;
            *r.chunk.alloc = meshChunk(*t.chunk.exdata);
            delete t.chunk.exdata;
            r.chunk.x = t.chunk.x; r.chunk.y = t.chunk.y; r.chunk.z = t.chunk.z;
            postResult(r);
            break;
    
        case Task::Type::Tag: 
            r.type = TaskResult::Type::Tag;
            r.value = t.value;
            postResult(r);
            break;

        default: break;
    }    
}

void workerMain(void *arg) {
    
    LightLock_Lock(&tasks.lock);

    while (runWorker) {
        if (tasks.count) {
            Task task = tasks.data[tasks.start];
            tasks.start = (tasks.start + 1) % queuesize;
            --tasks.count;
            LightLock_Unlock(&tasks.lock);
            processTask(task);
            svcSleepThread(1);
            LightLock_Lock(&tasks.lock);
        } else
            CondVar_Wait(&signalNewTask, &tasks.lock);
	}

    LightLock_Unlock(&tasks.lock);
}

void startWorker() {

    s32 prio = 0;
	svcGetThreadPriority(&prio, CUR_THREAD_HANDLE);

    bool new3DS = false;
    APT_CheckNew3DS(&new3DS);
    int affinity = new3DS ? 2 : 1;

    APT_SetAppCpuTimeLimit(30);
    LightLock_Init(&tasks.lock);
    LightLock_Init(&results.lock);
    CondVar_Init(&signalNewTask);

    workerThread = threadCreate(workerMain, nullptr, 4*1024, prio-1, 1, false);
}

void stopWorker() {
    threadJoin(workerThread, U64_MAX);
    threadFree(workerThread);
}

// todo: maybe have separate queues or limits per task type?
bool postTask(Task task, bool priority) { 

    LightLock_Lock(&tasks.lock);

    if (tasks.count < queuesize) {
        if (priority) { // important task; put it on the front of the queue
            tasks.start = (tasks.start - 1 + queuesize) % queuesize;
            tasks.data[tasks.start] = task;
            ++tasks.count;
        } else { // put it on the end of queue after other work
            tasks.data[(tasks.start + tasks.count) % queuesize] = task;
            ++tasks.count;
        }
        LightLock_Unlock(&tasks.lock);
        CondVar_Signal(&signalNewTask);
        return true;
    } else {
        LightLock_Unlock(&tasks.lock);
        return false;
    }
}

bool postResult(TaskResult result) {

    LightLock_Lock(&results.lock);

    if (results.count < queuesize) {
        results.data[(results.start + results.count) % queuesize] = result;
        ++results.count;

        LightLock_Unlock(&results.lock);
        // we might not need a result condvar; 
        // main thread only checks it once per frame
        //CondVar_Signal(&signalNewResult); 
        return true;
    } else {
        LightLock_Unlock(&results.lock);
        return false;
    }
}

bool getResult(TaskResult &result) {
    LightLock_Lock(&results.lock);
    if (results.count) {
        result = results.data[results.start];
        results.start = (results.start + 1) % queuesize;
        --results.count;
        LightLock_Unlock(&results.lock);
        return true;
    } else {
        LightLock_Unlock(&results.lock);
        return false;
    }
}
