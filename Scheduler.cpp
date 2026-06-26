#include "Scheduler.h"

#include <chrono>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <random>

#include "Config.h"
#include "PrintCommand.h"
#include "Utils.h"

Scheduler::Scheduler(int numCores, std::shared_ptr<std::atomic<uint64_t>> externalClock)
    : numCores(numCores),
      shuttingDown(false),
      isGenerating(false),
      nextPid(1),
      globalCpuCycles(externalClock) {
    for (int i = 0; i < numCores; ++i) {
        cores.push_back(std::make_unique<CoreSlot>());
    }
}

Scheduler::~Scheduler() {
    shutdown();
}

void Scheduler::start() {
    for (int i = 0; i < numCores; ++i) {
        workerThreads.emplace_back(&Scheduler::workerLoop, this, i);
    }
}

void Scheduler::startGeneration() {
    if (!isGenerating.load()) {
        lastGeneratedCycle.store(globalCpuCycles->load());
        isGenerating.store(true);
    }
}

void Scheduler::stopGeneration() {
    isGenerating.store(false);
}

void Scheduler::shutdown() {
    bool expected = false;
    if (!shuttingDown.compare_exchange_strong(expected, true)) return;

    isGenerating.store(false);

    for (auto& core : cores) {
        std::lock_guard<std::mutex> lock(core->mutex);
        core->tickSignal = true;
        core->cv.notify_all();
    }

    for (auto& worker : workerThreads) {
        if (worker.joinable()) worker.join();
    }
}

void Scheduler::runSingleCycleStep() {
    uint64_t currentCycle = globalCpuCycles->load();
    bool isRR = (Config::scheduler == "rr");

    // A. Generation Check
    if (isGenerating.load() &&
        (currentCycle - lastGeneratedCycle.load() >= (uint64_t)Config::batchProcessFreq)) {
        lastGeneratedCycle.store(currentCycle);

        int pid = nextPid.fetch_add(1);
        std::string name = "p" + zeroPad(pid, 2);
        auto process = std::make_shared<Process>(pid, name);

        // Randomize instruction count between min-ins and max-ins
        static std::mt19937 rng(std::random_device{}());
        std::uniform_int_distribution<int> dist(Config::minIns, Config::maxIns);
        int totalIns = dist(rng);

        std::string message = "Hello world from " + name + "!";
        for (int j = 0; j < totalIns; ++j) {
            process->addCommand(std::make_shared<PrintCommand>(message));
        }

        {
            std::lock_guard<std::mutex> lock(allProcMutex);
            allProcesses.push_back(process);
        }
        {
            std::lock_guard<std::mutex> lock(queueMutex);
            readyQueue.push(process);
        }
    }

    // B. Dispatcher: assign ready processes to free cores
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
                core.current   = process;
                core.quantumTicks  = 0;
                core.stepCompleted = false;
            }
        }
    }

    // C. RR preemption check — before ticking, check if quantum expired
    if (isRR) {
        for (int i = 0; i < numCores; ++i) {
            CoreSlot& core = *cores[i];
            std::lock_guard<std::mutex> coreLock(core.mutex);
            if (core.current != nullptr && core.quantumTicks >= Config::quantumCycles) {
                // Preempt: send back to rear of ready queue
                auto process = core.current;
                process->setState(Process::READY);
                process->setCoreId(-1);
                {
                    std::lock_guard<std::mutex> queueLock(queueMutex);
                    readyQueue.push(process);
                }
                core.current      = nullptr;
                core.quantumTicks = 0;
            }
        }
    }

    // D. Tick Broadcast
    for (int i = 0; i < numCores; ++i) {
        CoreSlot& core = *cores[i];
        std::lock_guard<std::mutex> coreLock(core.mutex);
        core.tickSignal    = true;
        core.stepCompleted = false;
        core.cv.notify_one();
    }

    // E. Barrier Sync
    for (int i = 0; i < numCores; ++i) {
        CoreSlot& core = *cores[i];
        std::unique_lock<std::mutex> coreLock(core.mutex);
        if (core.current != nullptr && !core.stepCompleted) {
            core.cv.wait(coreLock, [&] {
                return core.stepCompleted || shuttingDown.load();
            });
        }
        // Increment quantum tick counter for RR
        if (isRR && core.current != nullptr) {
            core.quantumTicks++;
        }
    }
}

void Scheduler::workerLoop(int coreId) {
    CoreSlot& core = *cores[coreId];
    while (!shuttingDown.load()) {
        {
            std::unique_lock<std::mutex> coreLock(core.mutex);
            core.cv.wait(coreLock, [&] {
                return core.tickSignal || shuttingDown.load();
            });
            core.tickSignal = false;
            if (shuttingDown.load()) return;
        }

        auto process = core.current;
        if (process != nullptr) {
            if (!process->isFinished()) {
                process->executeCurrentCommand(coreId);
            }
            if (process->isFinished()) {
                process->finishExecution();
                std::lock_guard<std::mutex> coreLock(core.mutex);
                core.current      = nullptr;
                core.quantumTicks = 0;
            }
        }

        {
            std::lock_guard<std::mutex> coreLock(core.mutex);
            core.stepCompleted = true;
            core.cv.notify_all();
        }
    }
}

std::shared_ptr<Process> Scheduler::findProcess(const std::string& name) {
    std::lock_guard<std::mutex> lock(allProcMutex);
    for (const auto& p : allProcesses) {
        if (p->getName() == name) return p;
    }
    return nullptr;
}

void Scheduler::printStatus(std::ostream& os) {
    std::lock_guard<std::mutex> lock(allProcMutex);

    int coresUsed = 0;
    for (int i = 0; i < numCores; ++i) {
        if (cores[i]->current != nullptr) coresUsed++;
    }
    int coresAvailable  = numCores - coresUsed;
    int cpuUtilization  = (numCores > 0) ? (coresUsed * 100) / numCores : 0;

    os << "CPU utilization: " << cpuUtilization << "%\n";
    os << "Cores used: "      << coresUsed      << "\n";
    os << "Cores available: " << coresAvailable  << "\n";
    os << "------------------------------------\n";

    os << "Running processes:\n";
    for (const auto& process : allProcesses) {
        if (process->getState() == Process::RUNNING) {
            os << std::left << std::setw(12) << process->getName()
               << "(" << process->getCreatedAt() << ")  "
               << "Core: " << process->getCoreId() << "   "
               << process->getCurrentLine() << " / " << process->getTotalLines()
               << "\n";
        }
    }

    os << "\nFinished processes:\n";
    for (const auto& process : allProcesses) {
        if (process->getState() == Process::FINISHED) {
            os << std::left << std::setw(12) << process->getName()
               << "(" << process->getCreatedAt() << ")  "
               << "Finished   "
               << process->getTotalLines() << " / " << process->getTotalLines()
               << "\n";
        }
    }

    os << "------------------------------------\n";
}

void Scheduler::writeReport(const std::string& path) {
    std::ofstream file(path);
    printStatus(file);
}