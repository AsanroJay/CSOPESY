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

#include "MemoryManager.h"
#include "Process.h"

class Scheduler {
public:
    explicit Scheduler(int numCores, std::shared_ptr<std::atomic<uint64_t>> externalClock);
    ~Scheduler();

    void start();
    void startGeneration();
    void stopGeneration();
    void shutdown();
    void printStatus(std::ostream& os);
    void writeReport(const std::string& path);
    void runSingleCycleStep();

    // For screen -s / -r lookups
    std::shared_ptr<Process> findProcess(const std::string& name);
    std::shared_ptr<Process> createProcess(const std::string& name, size_t memorySize = 64);

    // Returns nullptr when the instruction string cannot be parsed; `outError`
    // then explains why. The caller reports "invalid command".
    std::shared_ptr<Process> createCustomProcess(const std::string& name,
                                                 size_t memorySize,
                                                 const std::string& instructions,
                                                 std::string& outError);

    // Getters for process-smi and vmstat outputs[cite: 1]
    int getCpuUtilization();
    size_t getActiveTicks() const;
    size_t getIdleTicks() const;
    size_t getTotalTicks() const;
    size_t getTotalMemory() const;
    size_t getUsedMemory() const;
    size_t getFreeMemory() const;
    size_t getPagedIn() const;
    size_t getPagedOut() const;
    std::vector<std::shared_ptr<Process>> getRunningProcesses();

    size_t getProcessResidentMemory(int pid);

private:
    void workerLoop(int coreId);

    int numCores;

    std::vector<std::shared_ptr<Process>> allProcesses;
    std::mutex allProcMutex;

    std::queue<std::shared_ptr<Process>> readyQueue;
    std::mutex queueMutex;

    struct CoreSlot {
        std::mutex mutex;
        std::condition_variable cv;
        std::shared_ptr<Process> current = nullptr;
        bool tickSignal    = false;
        bool stepCompleted = false;
        int  quantumTicks  = 0;  // ticks used by current process this quantum
    };

    std::vector<std::unique_ptr<CoreSlot>> cores;
    std::vector<std::thread> workerThreads;

    std::atomic<bool>     shuttingDown;
    std::atomic<bool>     isGenerating;
    std::atomic<int>      nextPid;
    std::atomic<uint64_t> lastGeneratedCycle{0};

    // Atomic counters to track CPU ticks for vmstat[cite: 1]
    std::atomic<size_t> activeTicks{0};
    std::atomic<size_t> idleTicks{0};

    std::shared_ptr<std::atomic<uint64_t>> globalCpuCycles;

    // Demand pager shared by the dispatcher (admission) and the CPU workers
    // (page faults, release on exit). Internally thread-safe.
    MemoryManager memory;

    // Builds the instruction stream for a generated process.
    void populateRandomInstructions(const std::shared_ptr<Process>& process);

    // Memory-snapshot bookkeeping. Snapshots begin once the scheduler starts
    // generating processes; one file is emitted every `quantum-cycles`.
    std::atomic<bool>     snapshotsEnabled{false};
    std::atomic<uint64_t> quantumCounter{0};
    std::atomic<uint64_t> lastSnapshotCycle{0};
    void maybeWriteMemorySnapshot(uint64_t currentCycle);
};