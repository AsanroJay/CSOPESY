#include "Scheduler.h"

#include <chrono>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <limits>
#include <random>

#include "ArithmeticCommand.h"
#include "Config.h"
#include "DeclareCommand.h"
#include "ForCommand.h"
#include "PrintCommand.h"
#include "SleepCommand.h"
#include "Utils.h"

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
                                                                     int count) {
    std::vector<std::shared_ptr<ICommand>> commands;
    commands.reserve(count);
    for (int i = 0; i < count; ++i) {
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