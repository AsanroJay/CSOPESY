#include "Scheduler.h"

#include <chrono>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <limits>
#include <random>
#include <cmath>
#include "ArithmeticCommand.h"
#include "Config.h"
#include "DeclareCommand.h"
#include "ForCommand.h"
#include "PrintCommand.h"
#include "SleepCommand.h"
#include "Utils.h"
#include <filesystem>

static std::mt19937& getRng() {
    static std::mt19937 rng(std::random_device{}());
    return rng;
}

static int randomInt(int minValue, int maxValue) {
    return std::uniform_int_distribution<int>(minValue, maxValue)(getRng());
}

static bool randomChance(int percent) {
    return randomInt(1, 100) <= percent;
}

static std::string chooseVarName(const std::vector<std::string>& names) {
    return names[randomInt(0, static_cast<int>(names.size()) - 1)];
}

static ArithmeticCommand::Operand makeRandomOperand(const std::vector<std::string>& variableNames) {
    if (!variableNames.empty() && randomChance(60)) {
        return {ArithmeticCommand::VARIABLE, chooseVarName(variableNames), 0};
    }
    return {ArithmeticCommand::LITERAL, std::string(), static_cast<uint16_t>(randomInt(0, 100))};
}

static std::shared_ptr<ICommand> makeRandomInstruction(const std::string& processName,
                                                      const std::vector<std::string>& variableNames,
                                                      int depth);

static std::vector<std::shared_ptr<ICommand>> makeRandomCommandBlock(const std::string& processName,
                                                                     const std::vector<std::string>& variableNames,
                                                                     int depth,
                                                                     size_t count) {
    std::vector<std::shared_ptr<ICommand>> commands;
    commands.reserve(count);
    for (size_t i = 0; i < count; ++i) {
        commands.push_back(makeRandomInstruction(processName, variableNames, depth));
    }
    return commands;
}

static std::shared_ptr<ICommand> makeRandomInstruction(const std::string& processName,
                                                      const std::vector<std::string>& variableNames,
                                                      int depth) {
    constexpr int maxForNestingDepth = 3;

    if (depth >= maxForNestingDepth) {
        int choice = randomInt(1, 90);
        if (choice <= 30) {
            return std::make_shared<PrintCommand>("Hello world from " + processName + "!");
        }
        if (choice <= 45) {
            return std::make_shared<DeclareCommand>(chooseVarName(variableNames), static_cast<uint16_t>(randomInt(0, 65535)));
        }
        if (choice <= 65) {
            return std::make_shared<ArithmeticCommand>(ICommand::ADD,
                                                       chooseVarName(variableNames),
                                                       makeRandomOperand(variableNames),
                                                       makeRandomOperand(variableNames));
        }
        if (choice <= 80) {
            return std::make_shared<ArithmeticCommand>(ICommand::SUBTRACT,
                                                       chooseVarName(variableNames),
                                                       makeRandomOperand(variableNames),
                                                       makeRandomOperand(variableNames));
        }
        return std::make_shared<SleepCommand>(randomInt(1, 5));
    }

    int choice = randomInt(1, depth >= 2 ? 85 : 100);
    if (choice <= 30) {
        return std::make_shared<PrintCommand>("Hello world from " + processName + "!");
    }
    if (choice <= 45) {
        return std::make_shared<DeclareCommand>(chooseVarName(variableNames), static_cast<uint16_t>(randomInt(0, 65535)));
    }
    if (choice <= 65) {
        return std::make_shared<ArithmeticCommand>(ICommand::ADD,
                                                   chooseVarName(variableNames),
                                                   makeRandomOperand(variableNames),
                                                   makeRandomOperand(variableNames));
    }
    if (choice <= 80) {
        return std::make_shared<ArithmeticCommand>(ICommand::SUBTRACT,
                                                   chooseVarName(variableNames),
                                                   makeRandomOperand(variableNames),
                                                   makeRandomOperand(variableNames));
    }
    if (choice <= 90) {
        return std::make_shared<SleepCommand>(randomInt(1, 5));
    }

    int innerCount = randomInt(1, 3);
    int repeats = randomInt(2, 5);
    return std::make_shared<ForCommand>(makeRandomCommandBlock(processName, variableNames, depth + 1, innerCount), repeats);
}

Scheduler::Scheduler(int numCores, std::shared_ptr<std::atomic<uint64_t>> externalClock)
    : numCores(numCores),
      shuttingDown(false),
      isGenerating(false),
      nextPid(1),
      globalCpuCycles(externalClock),
      memory(Config::maxOverallMem, Config::memPerFrame, static_cast<int>(Config::minMemPerProc)) {
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
    // Begin emitting periodic memory snapshots (persists after scheduler-stop
    // so we keep capturing memory while the remaining processes drain).
    if (!snapshotsEnabled.load()) {
        lastSnapshotCycle.store(globalCpuCycles->load());
        snapshotsEnabled.store(true);
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

    // --- NEW: Global Sleep Cycle Management ---
    {
        std::lock_guard<std::mutex> lock(allProcMutex);
        for (auto& process : allProcesses) {
            if (process->getState() == Process::SLEEPING) {
                // tickSleep decrements sleepTicks and returns true if remaining == 0
                if (process->tickSleep()) { 
                    process->setState(Process::READY);
                    
                    // Put it back in the ready queue to pick up where it left off
                    std::lock_guard<std::mutex> queueLock(queueMutex);
                    readyQueue.push(process);
                }
            }
        }
    }

    // A. Generation Check
    if (isGenerating.load() &&
        (currentCycle - lastGeneratedCycle.load() >= (uint64_t)Config::batchProcessFreq)) {
        lastGeneratedCycle.store(currentCycle);

        int pid = nextPid.fetch_add(1);
        std::string name = "p" + zeroPad(pid, 2);
        auto process = std::make_shared<Process>(pid, name);

        // --- POWER OF 2 MEMORY GENERATOR ---
        static std::mt19937 rng(std::random_device{}());
        
        // Calculate the base-2 exponents for the min and max bounds
        int minExp = static_cast<int>(std::log2(Config::minMemPerProc));
        int maxExp = static_cast<int>(std::log2(Config::maxMemPerProc));
        
        // Roll a random exponent and convert it back to a power of 2
        std::uniform_int_distribution<int> memDist(minExp, maxExp);
        size_t rolledMem = 1ULL << memDist(rng); 
        
        process->setMemorySize(rolledMem);

        // --- RANDOMIZED INSTRUCTIONS ---
        std::uniform_int_distribution<uint64_t> dist(Config::minIns, Config::maxIns);
        size_t totalIns = static_cast<size_t>(dist(rng));

        // Build a list of candidate variable names for randomized instructions
        std::vector<std::string> variableNames = {"x", "y", "z", "i", "j", "k"};

        // Populate the process with a randomized block of instructions
        auto commands = makeRandomCommandBlock(name, variableNames, 0, totalIns);
        for (auto& cmd : commands) {
            process->addCommand(cmd);
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

    // B. Dispatcher: assign ready processes to free cores.
    //
    // A process must hold memory before it can run. If it isn't resident yet we
    // attempt a first-fit allocation; when memory is full the process reverts to
    // the TAIL of the ready queue (no backing store) and the core is left idle
    // for this cycle. We bound the retries per core to the queue length so a
    // fully-occupied memory doesn't spin.
    {
        std::lock_guard<std::mutex> queueLock(queueMutex);
        for (int i = 0; i < numCores; ++i) {
            CoreSlot& core = *cores[i];
            std::unique_lock<std::mutex> coreLock(core.mutex);
            if (core.current != nullptr) continue;

            int attempts = static_cast<int>(readyQueue.size());
            while (attempts-- > 0 && !readyQueue.empty()) {
                auto process = readyQueue.front();
                readyQueue.pop();

                if (!memory.isAllocated(process->getPID())) {
                    int base = memory.allocate(process->getPID(), process->getMemorySize());
                    if (base < 0) {
                        // Memory full: send back to the rear of the ready queue.
                        readyQueue.push(process);
                        continue;
                    }
                }

                process->setCoreId(i);
                process->setState(Process::RUNNING);
                core.current       = process;
                core.quantumTicks  = 0;
                core.stepCompleted = false;
                break;
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

   // ... inside Scheduler::runSingleCycleStep() ...

    // E. Barrier Sync
    int activeCoresThisTick = 0; // NEW: Track active cores
    int idleCoresThisTick = 0;   // NEW: Track idle cores

    for (int i = 0; i < numCores; ++i) {
        CoreSlot& core = *cores[i];
        std::unique_lock<std::mutex> coreLock(core.mutex);
        if (core.current != nullptr && !core.stepCompleted) {
            core.cv.wait(coreLock, [&] {
                return core.stepCompleted || shuttingDown.load();
            });
        }
        
        // NEW: Tally core activity for this cycle
        if (core.current != nullptr) {
            activeCoresThisTick++;
            // Increment quantum tick counter for RR
            if (isRR) core.quantumTicks++;
        } else {
            idleCoresThisTick++;
        }
    }

    // NEW: Safely add to our atomic counters
    activeTicks.fetch_add(activeCoresThisTick);
    idleTicks.fetch_add(idleCoresThisTick);


    // F. Memory snapshot: dump the memory map once per quantum-cycles.
    maybeWriteMemorySnapshot(currentCycle);
}

void Scheduler::maybeWriteMemorySnapshot(uint64_t currentCycle) {
    if (!snapshotsEnabled.load()) return;

    int quantum = Config::quantumCycles > 0 ? Config::quantumCycles : 1;
    if (currentCycle - lastSnapshotCycle.load() < static_cast<uint64_t>(quantum)) {
        return;
    }
    lastSnapshotCycle.store(currentCycle);

    std::filesystem::create_directories("memory_snapshots");
    uint64_t qq = quantumCounter.fetch_add(1);
    std::string path = "memory_snapshots/memory_stamp_" + zeroPad(static_cast<int>(qq), 2) + ".txt";
    memory.writeSnapshot(path);
}

void Scheduler::workerLoop(int coreId) {
    CoreSlot& core = *cores[coreId];
    while (!shuttingDown.load()) {
        // 1. Wait for the main scheduler thread to signal a clock cycle tick
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
            // 2. Fetch the current global clock cycle value safely
            uint64_t currentGlobalClock = globalCpuCycles->load();

            // 3. Deadline Check: If the process is still waiting out its instruction delay, skip execution
            if (process->isBusyWaiting(currentGlobalClock)) {
                // Do nothing this cycle tick! Just hold the core slot and wait for time to advance
            }
            // 4. Deadline Passed: Execute the next instruction line if it's not finished
            else if (!process->isFinished()) {
                process->executeCurrentCommand(coreId);
                
                // 5. If a delay modifier is configured, lock in the new deadline right now
                if (Config::delayPerExec > 0 && !process->isFinished()) {
                    process->startBusyWait(currentGlobalClock, Config::delayPerExec);
                }
            }
            
            // 6. Post-execution Lifecycle Clean up
            if (process->isFinished()) {
                // Wrap up file handles and update state to FINISHED
                process->finishExecution();

                // Release its memory back to the allocator (only finished
                // processes free memory; preempted ones stay resident).
                memory.deallocate(process->getPID());

                // Relinquish the core slot instantly so the dispatcher can cycle in a fresh process
                std::lock_guard<std::mutex> coreLock(core.mutex);
                core.current      = nullptr;
                core.quantumTicks = 0;
            }
            else if (process->getState() == Process::SLEEPING) {
                // Relinquish the core slot immediately if the command forced a process sleep
                std::lock_guard<std::mutex> coreLock(core.mutex);
                process->setCoreId(-1); 
                core.current      = nullptr;
                core.quantumTicks = 0;
            }
        }

        // 7. Core Thread Barrier Synchronization Sync-Back
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

std::shared_ptr<Process> Scheduler::createProcess(const std::string& name, size_t memorySize) {
    int pid = nextPid.fetch_add(1);
    auto process = std::make_shared<Process>(pid, name);
    
    // Ensure memory size is set to the process
    process->setMemorySize(memorySize);

    // RANDOMIZED INSTRUCTIONS
    static std::mt19937 rng(std::random_device{}());
    std::uniform_int_distribution<uint64_t> dist(Config::minIns, Config::maxIns);
    size_t totalIns = static_cast<size_t>(dist(rng));

    std::vector<std::string> variableNames = {"x", "y", "z", "i", "j", "k"};
    auto commands = makeRandomCommandBlock(name, variableNames, 0, totalIns);
    for (auto& cmd : commands) {
        process->addCommand(cmd);
    }

    {
        std::lock_guard<std::mutex> lock(allProcMutex);
        allProcesses.push_back(process);
    }
    {
        std::lock_guard<std::mutex> lock(queueMutex);
        readyQueue.push(process);
    }

    return process;
}

std::shared_ptr<Process> Scheduler::createCustomProcess(const std::string& name, size_t memorySize, const std::string& instructions) {
    int pid = nextPid.fetch_add(1);
    auto process = std::make_shared<Process>(pid, name);
    process->setMemorySize(memorySize);
    
    // TODO: Parse the `instructions` string (split by ';') and convert them 
    // into actual ICommand objects via your Command parsing logic, then call:
    // process->addCommand(parsedCommand);

    {
        std::lock_guard<std::mutex> lock(allProcMutex);
        allProcesses.push_back(process);
    }
    {
        std::lock_guard<std::mutex> lock(queueMutex);
        readyQueue.push(process);
    }

    return process;
}
void Scheduler::printStatus(std::ostream& os) {
    std::lock_guard<std::mutex> lock(allProcMutex);

    int coresUsed = 0;
    for (int i = 0; i < numCores; ++i) {
        std::lock_guard<std::mutex> coreLock(cores[i]->mutex);
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

int Scheduler::getCpuUtilization() {
    int coresUsed = 0;
    for (int i = 0; i < numCores; ++i) {
        std::lock_guard<std::mutex> coreLock(cores[i]->mutex);
        if (cores[i]->current != nullptr) coresUsed++;
    }
    return (numCores > 0) ? (coresUsed * 100) / numCores : 0;
}

size_t Scheduler::getActiveTicks() const {
    return activeTicks.load();
}

size_t Scheduler::getIdleTicks() const {
    return idleTicks.load();
}

size_t Scheduler::getTotalTicks() const {
    return activeTicks.load() + idleTicks.load();
}

size_t Scheduler::getTotalMemory() const {
    return memory.getMaximumSize(); 
}

size_t Scheduler::getUsedMemory() const {
    return memory.getCurrentAllocatedSize(); 
}

size_t Scheduler::getFreeMemory() const {
    return getTotalMemory() - getUsedMemory();
}

size_t Scheduler::getPagedIn() const {
    return memory.getNumPagedIn(); 
}

size_t Scheduler::getPagedOut() const {
    return memory.getNumPagedOut(); 
}

std::vector<std::shared_ptr<Process>> Scheduler::getRunningProcesses() {
    std::lock_guard<std::mutex> lock(allProcMutex);
    std::vector<std::shared_ptr<Process>> running;
    for (const auto& p : allProcesses) {
        // Capture both running and ready processes so they show up in process-smi
        if (p->getState() == Process::RUNNING || p->getState() == Process::READY) {
            running.push_back(p);
        }
    }
    return running;
}