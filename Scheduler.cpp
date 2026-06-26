#include "Scheduler.h"

#include <chrono>
#include <fstream>
#include <iomanip>
#include <iostream>

#include "Config.h"
#include "PrintCommand.h"
#include "Utils.h"

FCFSScheduler::FCFSScheduler(int numCores, std::shared_ptr<std::atomic<uint64_t>> externalClock)
    : numCores(numCores),
      shuttingDown(false),
      isGenerating(false),
      nextPid(1),
      globalCpuCycles(externalClock) {
    for (int i = 0; i < numCores; ++i) {
        cores.push_back(std::make_unique<CoreSlot>());
    }
}

FCFSScheduler::~FCFSScheduler() {
    shutdown();
}

void FCFSScheduler::start() {
    // We only spawn the core worker threads now. The main loop controls the scheduling ticks.
    for (int i = 0; i < numCores; ++i) {
        workerThreads.emplace_back(&FCFSScheduler::workerLoop, this, i);
    }
}

void FCFSScheduler::startGeneration() {
    // Capture exactly where the global clock is right now when starting
    lastGeneratedCycle.store(globalCpuCycles->load()); 
    isGenerating.store(true);
}

void FCFSScheduler::stopGeneration() {
    isGenerating.store(false);
    std::lock_guard<std::mutex> lock(queueMutex);
    while (!readyQueue.empty()) readyQueue.pop();
}

void FCFSScheduler::shutdown() {
    bool expected = false;
    if (!shuttingDown.compare_exchange_strong(expected, true)) {
        return;  // already shut down / joined
    }

    isGenerating.store(false);

    // Wake up all workers so they can read shuttingDown and exit cleanly
    for (auto& core : cores) {
        std::lock_guard<std::mutex> lock(core->mutex);
        core->tickSignal = true; 
        core->cv.notify_all();
    }

    for (auto& worker : workerThreads) {
        if (worker.joinable()) {
            worker.join();
        }
    }
}

// Called explicitly by main() on every clock tick iteration
void FCFSScheduler::runSingleCycleStep() {
    uint64_t currentCycle = globalCpuCycles->load();

    // A. Generation Check: Trigger a batch every X cycles
    if (isGenerating.load() && (currentCycle - lastGeneratedCycle.load() >= Config::batchProcessFreq)) {
        lastGeneratedCycle.store(currentCycle); // Reset our timeline anchor for the next interval   
        int pid = nextPid.fetch_add(1);
        std::string name = "process" + zeroPad(pid, 2);
        auto process = std::make_shared<Process>(pid, name);

        // // --- ADD THIS TEMPORARY DEBUG PRINT ---
        // std::cout << "\n[DEBUG CLOCK] " << name << " created precisely at Cycle: " << currentCycle << "\n";
        // // --------------------------------------

        int totalIns = Config::minIns; 
        std::string message = "Hello world from " + name + "!";
        for (int j = 0; j < totalIns; ++j) {
            process->addCommand(std::make_shared<PrintCommand>(message));
        }

        // Fully build the process before sharing it with the worker threads.
        {
            std::lock_guard<std::mutex> lock(allProcMutex);
            allProcesses.push_back(process);
        }
        {
            std::lock_guard<std::mutex> lock(queueMutex);
            readyQueue.push(process);
        }
    }

    // B. Dispatcher Step: Assign the front of the ready queue to any free core, FIFO order
    {
        std::lock_guard<std::mutex> queueLock(queueMutex);
        for (int i = 0; i < numCores && !readyQueue.empty(); ++i) {
            CoreSlot& core = *cores[i];
            std::unique_lock<std::mutex> coreLock(core.mutex);
            if (core.current == nullptr) {
                auto process = readyQueue.front();
                readyQueue.pop();
                process->setCoreId(i);
                process->setState(Process::RUNNING);
                core.current = process;
                core.stepCompleted = false;
            }
        }
    }

    // C. Tick Broadcast: Signal active cores to advance exactly 1 instruction
    for (int i = 0; i < numCores; ++i) {
        CoreSlot& core = *cores[i];
        std::lock_guard<std::mutex> coreLock(core.mutex);
        core.tickSignal = true;
        core.stepCompleted = false;
        core.cv.notify_one();
    }

    // D. Barrier Sync: Wait until all active cores complete their cycle instruction
    for (int i = 0; i < numCores; ++i) {
        CoreSlot& core = *cores[i];
        std::unique_lock<std::mutex> coreLock(core.mutex);
        if (core.current != nullptr && !core.stepCompleted) {
            core.cv.wait(coreLock, [&] { 
                return core.stepCompleted || shuttingDown.load(); 
            });
        }
    }
}

// Worker: wait for an assignment, run that process to completion, free the core.
// Regulated to run exactly one instruction step per cycle notification.
void FCFSScheduler::workerLoop(int coreId) {
    CoreSlot& core = *cores[coreId];
    while (!shuttingDown.load()) {
        std::shared_ptr<Process> process;
        {
            std::unique_lock<std::mutex> coreLock(core.mutex);
            // Wait until the master clock drops a tick token
            core.cv.wait(coreLock, [&] {
                return core.tickSignal || shuttingDown.load();
            });

            core.tickSignal = false; // Consume token
            if (shuttingDown.load()) return;
            
            process = core.current;
        }

        // If this core has an assigned process, advance it by EXACTLY one instruction step
        if (process != nullptr) {
            if (!process->isFinished()) {
                process->executeCurrentCommand(coreId);
            }

            if (process->isFinished()) {
                process->finishExecution();
                std::lock_guard<std::mutex> coreLock(core.mutex);
                core.current = nullptr; // Free the core
            }
        }

        // Notify Master Clock that this core has completed its single-cycle obligations
        {
            std::lock_guard<std::mutex> coreLock(core.mutex);
            core.stepCompleted = true;
            core.cv.notify_all(); // Wake master loop out of barrier wait
        }
    }
}

void FCFSScheduler::printStatus(std::ostream& os) {
    std::lock_guard<std::mutex> lock(allProcMutex);

    // 1. Calculate Core Statistics
    int coresUsed = 0;
    for (int i = 0; i < numCores; ++i) {
        if (cores[i]->current != nullptr) {
            coresUsed++;
        }
    }
    int coresAvailable = numCores - coresUsed;

    // Calculate utilization percentage (avoid division by zero)
    int cpuUtilization = (numCores > 0) ? (coresUsed * 100) / numCores : 0;

    os << "CPU utilization: " << cpuUtilization << "%\n";
    os << "Cores used: " << coresUsed << "\n";
    os << "Cores available: " << coresAvailable << "\n";

    os << "------------------------------------\n";
    // os << "Global Clock Cycles: " << globalCpuCycles->load() << "\n";
    os << "Running processes:\n";
    for (const auto& process : allProcesses) {
        if (process->getState() == Process::RUNNING) {
            os << std::left << std::setw(11) << process->getName()
               << "(" << process->getCreatedAt() << ")  "
               << "Core: " << process->getCoreId() << "   "
               << process->getCurrentLine() << " / " << process->getTotalLines()
               << "\n";
        }
    }

    os << "\nFinished processes:\n";
    for (const auto& process : allProcesses) {
        if (process->getState() == Process::FINISHED) {
            os << std::left << std::setw(11) << process->getName()
               << "(" << process->getCreatedAt() << ")  "
               << "Finished  "
               << process->getTotalLines() << " / " << process->getTotalLines()
               << "\n";
        }
    }

    os << "------------------------------------\n";
}

void FCFSScheduler::writeReport(const std::string& path) {
    std::ofstream file(path);
    printStatus(file);
}