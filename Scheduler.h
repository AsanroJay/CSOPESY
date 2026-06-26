#pragma once

#include <atomic>
#include <condition_variable>
#include <memory>
#include <mutex>
#include <ostream>
#include <queue>
#include <string>
#include <thread>
#include <vector>

#include "Process.h"

// A multi-threaded first-come-first-serve CPU scheduler.
//
// Threading model (per the course lecture, p.22):
//   * 1 scheduler ("dispatcher") thread pulls processes from a single shared
//     ready queue in FIFO order and assigns each to a free CPU core.
//   * 1 worker thread per CPU core. A worker runs its assigned process to
//     completion (non-preemptive), then frees the core for the next process.
class FCFSScheduler {
public:
    // Constructor accepts the shared pointer to the atomic clock
    explicit FCFSScheduler(int numCores, std::shared_ptr<std::atomic<uint64_t>> externalClock);
    ~FCFSScheduler();

    // Spawns the per-core worker threads only (Master scheduler thread removed)
    void start();

    void startGeneration();
    void stopGeneration();
    void shutdown();
    void printStatus(std::ostream& os);
    void writeReport(const std::string& path);

    // EXPLICIT TICK STEP: Called by main() on every frame pass
    void runSingleCycleStep();

private:
    void workerLoop(int coreId);   // per-core worker thread body (Regulated single-step worker)

    int numCores;

    std::vector<std::shared_ptr<Process>> allProcesses;  // master list for screen -ls
    std::mutex allProcMutex;

    std::queue<std::shared_ptr<Process>> readyQueue;
    std::mutex queueMutex;

    struct CoreSlot {
        std::mutex mutex;
        std::condition_variable cv;
        std::shared_ptr<Process> current = nullptr;  // assigned process (nullptr = core free)
        
        // Coordination flags to ensure strict 1-instruction-per-tick behavior
        bool tickSignal = false;
        bool stepCompleted = false;
    };

    std::vector<std::unique_ptr<CoreSlot>> cores;  // unique_ptr: CoreSlot is non-movable
    std::vector<std::thread> workerThreads;

    std::atomic<bool> shuttingDown;
    std::atomic<bool> isGenerating; 
    std::atomic<int> nextPid;
    
    // Shared pointer to the atomic master clock residing in emulator.cpp
    std::shared_ptr<std::atomic<uint64_t>> globalCpuCycles; 

    std::atomic<uint64_t> lastGeneratedCycle{0};
};