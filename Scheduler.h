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
    explicit FCFSScheduler(int numCores);
    ~FCFSScheduler();

    // Spawns the scheduler thread and the per-core worker threads. They stay
    // idle until processes are generated.
    void start();

    // Creates `count` processes (process01, process02, ...), each with
    // `printsEach` identical print instructions, and enqueues them.
    void generateProcesses(int count, int printsEach);

    // Stops the dispatcher from assigning any further processes.
    void stopDispatch();

    // Signals all threads to stop and joins them. Safe to call more than once.
    void shutdown();

    // Renders the "screen -ls" status (running + finished processes).
    void printStatus(std::ostream& os);

    // Writes the same status to a file (used by "report-util").
    void writeReport(const std::string& path);

private:
    // One per CPU core. mutex/cv guard the handshake between the dispatcher
    // (which fills `current`) and the worker (which drains it).
    struct CoreSlot {
        std::mutex mutex;
        std::condition_variable cv;
        std::shared_ptr<Process> current;  // assigned process (nullptr = core free)
    };

    void schedulerLoop();          // dispatcher thread body
    void workerLoop(int coreId);   // per-core worker thread body

    int numCores;

    std::vector<std::shared_ptr<Process>> allProcesses;  // master list for screen -ls
    std::mutex allProcMutex;

    std::queue<std::shared_ptr<Process>> readyQueue;
    std::mutex queueMutex;

    std::vector<std::unique_ptr<CoreSlot>> cores;  // unique_ptr: CoreSlot is non-movable

    std::thread schedulerThread;
    std::vector<std::thread> workerThreads;

    std::atomic<bool> shuttingDown;
    std::atomic<bool> dispatching;
    std::atomic<int> nextPid;
};
