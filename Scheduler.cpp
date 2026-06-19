#include "Scheduler.h"

#include <chrono>
#include <fstream>
#include <iomanip>

#include "Config.h"
#include "PrintCommand.h"
#include "Utils.h"

FCFSScheduler::FCFSScheduler(int numCores)
    : numCores(numCores),
      shuttingDown(false),
      dispatching(true),
      nextPid(1) {
    for (int i = 0; i < numCores; ++i) {
        cores.push_back(std::make_unique<CoreSlot>());
    }
}

FCFSScheduler::~FCFSScheduler() {
    shutdown();
}

void FCFSScheduler::start() {
    schedulerThread = std::thread(&FCFSScheduler::schedulerLoop, this);
    for (int i = 0; i < numCores; ++i) {
        workerThreads.emplace_back(&FCFSScheduler::workerLoop, this, i);
    }
}

void FCFSScheduler::generateProcesses(int count, int printsEach) {
    for (int i = 0; i < count; ++i) {
        int pid = nextPid.fetch_add(1);
        std::string name = "process" + zeroPad(pid, 2);
        auto process = std::make_shared<Process>(pid, name);

        std::string message = "Hello world from " + name + "!";
        for (int j = 0; j < printsEach; ++j) {
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
}

void FCFSScheduler::stopDispatch() {
    dispatching.store(false);
}

void FCFSScheduler::shutdown() {
    bool expected = false;
    if (!shuttingDown.compare_exchange_strong(expected, true)) {
        return;  // already shut down / joined
    }
    for (auto& core : cores) {
        std::lock_guard<std::mutex> lock(core->mutex);
        core->cv.notify_all();
    }
    if (schedulerThread.joinable()) {
        schedulerThread.join();
    }
    for (auto& worker : workerThreads) {
        if (worker.joinable()) {
            worker.join();
        }
    }
}

// Dispatcher: assign the front of the ready queue to any free core, FIFO order
// (true FCFS -- no sorting by burst length).
void FCFSScheduler::schedulerLoop() {
    while (!shuttingDown.load()) {
        if (dispatching.load()) {
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
                    coreLock.unlock();
                    core.cv.notify_one();
                }
            }
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(1));  // light poll
    }
}

// Worker: wait for an assignment, run that process to completion, free the core.
void FCFSScheduler::workerLoop(int coreId) {
    CoreSlot& core = *cores[coreId];
    while (!shuttingDown.load()) {
        std::shared_ptr<Process> process;
        {
            std::unique_lock<std::mutex> coreLock(core.mutex);
            core.cv.wait(coreLock, [&] {
                return core.current != nullptr || shuttingDown.load();
            });
            if (shuttingDown.load()) {
                return;
            }
            process = core.current;
        }

        while (!process->isFinished() && !shuttingDown.load()) {
            process->executeCurrentCommand(coreId);
            if (Config::PER_INSTRUCTION_DELAY_MS > 0) {
                std::this_thread::sleep_for(
                    std::chrono::milliseconds(Config::PER_INSTRUCTION_DELAY_MS));
            }
        }

        process->finishExecution();

        {
            std::lock_guard<std::mutex> coreLock(core.mutex);
            core.current = nullptr;
        }
    }
}

void FCFSScheduler::printStatus(std::ostream& os) {
    std::lock_guard<std::mutex> lock(allProcMutex);

    os << "------------------------------------\n";
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
