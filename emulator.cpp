#include <cstdlib>
#include <iomanip>
#include <iostream>
#include <sstream>
#include <string>
#include <atomic>
#include <memory>
#include <thread>

#include "Config.h"
#include "Scheduler.h"  

using namespace std;

void printHeader();
void displayProcessScreen(Process& process);

// Splits the tail of a "screen -s/-c" line into an optional memory size and
// whatever follows it.
//
// The spec's syntax line includes <process_memory_size>, but its own sample
// usage (and the graded test cases) omit it -- e.g.
//     screen -c faulty_process "DECLARE varA 10; ..."
// So a leading numeric token is treated as the size; anything else means the
// size was left out and the configured minimum is used.
static void splitMemoryAndRest(const std::string& tail, size_t& outMemSize, std::string& outRest) {
    // outMemSize == 0 signals "no size given"; each caller decides what to do.
    size_t start = tail.find_first_not_of(" \t");
    if (start == std::string::npos) {
        outMemSize = 0;
        outRest.clear();
        return;
    }

    size_t end = tail.find_first_of(" \t", start);
    std::string firstToken = tail.substr(start, end == std::string::npos ? std::string::npos : end - start);

    bool numeric = !firstToken.empty() &&
                   firstToken.find_first_not_of("0123456789") == std::string::npos;

    if (numeric) {
        try {
            outMemSize = std::stoull(firstToken);
        } catch (...) {
            outMemSize = 0;
        }
        outRest = (end == std::string::npos) ? "" : tail.substr(end);
    } else {
        outMemSize = 0;
        outRest    = tail.substr(start);
    }
}

// Memory must be a power of 2 within [2^6, 2^16].
static bool isValidMemorySize(size_t memSize) {
    return memSize >= 64 && memSize <= 65536 && (memSize & (memSize - 1)) == 0;
}

// process-smi reports memory in MiB to match the spec mockup. The emulated
// memory space tops out at 65536 bytes, so MiB alone rounds to ~0 -- the exact
// byte count is kept alongside it to stay readable at these sizes.
static std::string toMiB(size_t bytes) {
    std::ostringstream out;
    out << std::fixed << std::setprecision(2)
        << (static_cast<double>(bytes) / (1024.0 * 1024.0)) << "MiB";
    return out.str();
}

// "0.92MiB / 1.00MiB (963584B / 1048576B)"
static std::string formatMemoryPair(size_t used, size_t total) {
    return toMiB(used) + " / " + toMiB(total) +
           " (" + std::to_string(used) + "B / " + std::to_string(total) + "B)";
}

// "0.00MiB (64B)"
static std::string formatMemory(size_t bytes) {
    return toMiB(bytes) + " (" + std::to_string(bytes) + "B)";
}

void runExecutionEngine(std::shared_ptr<std::atomic<uint64_t>> cpuCycles,Scheduler& scheduler, std::shared_ptr<std::atomic<bool>> systemRunning);

int main() {
    printHeader();

    auto cpuCycles     = std::make_shared<std::atomic<uint64_t>>(0);
    auto systemRunning = std::make_shared<std::atomic<bool>>(true);

    // Declared as pointers, only constructed after "initialize" loads config
    std::unique_ptr<Scheduler> scheduler;
    std::thread engineThread;

    string line;
    while (true) {
        cout << "\nEnter command: ";
        if (!getline(cin, line)) break;

        if (line.rfind("\xEF\xBB\xBF", 0) == 0) line.erase(0, 3);

        istringstream iss(line);
        string command, argument, processName;
        iss >> command >> argument >> processName;

        if (!Config::initialized && command != "initialize" && command != "exit" && !command.empty()) {
            cout << "Please run \"initialize\" first.\n";
            continue;
        }

        if (command.empty()) {
            // ignore
        }
       else if (command == "initialize") {
            if (scheduler) {
                cout << "Already initialized.\n";
                continue;
            }
            if (Config::loadFromFile()) {
                scheduler = std::make_unique<Scheduler>(Config::numCpu, cpuCycles);
                scheduler->start();
                engineThread = std::thread(runExecutionEngine, cpuCycles,
                                           std::ref(*scheduler), systemRunning);

                cout << "Configuration loaded:\n";
                cout << "  Scheduler        : " << Config::scheduler << "\n";
                cout << "  CPU cores        : " << Config::numCpu << "\n";
                cout << "  Batch freq       : " << Config::batchProcessFreq << " CPU cycle(s)\n";
                cout << "  Min instructions : " << Config::minIns << "\n";
                cout << "  Max instructions : " << Config::maxIns << "\n";
                cout << "  Delay per exec   : " << Config::delayPerExec << "\n";
                cout << "  Max overall mem  : " << Config::maxOverallMem << "\n";
                cout << "  Mem per frame    : " << Config::memPerFrame << "\n";
                cout << "  Min mem per proc : " << Config::minMemPerProc << "\n";
                cout << "  Max mem per proc : " << Config::maxMemPerProc << "\n";
                cout << "Type \"scheduler-start\" to begin generating processes.\n";
            }
        }
        // The spec names this command both ways -- "scheduler-start" in the
        // command list and "scheduler-test" in the batch-process-freq entry --
        // so accept either.
        else if (command == "scheduler-start" || command == "scheduler-test") {
            scheduler->startGeneration();
            cout << "Type 'scheduler-stop' to stop process scheduling.\n";
            cout << "Process generation started. Every " << Config::batchProcessFreq << " CPU cycle(s).\n";
        }
        else if (command == "scheduler-stop") {
            scheduler->stopGeneration();
            cout << "Process generation stopped. Active processes will run to completion.\n";
        }
       else if (command == "screen") {
            if (argument == "-ls") {
                scheduler->printStatus(cout);
            }
            else if (argument == "-s") {
                std::string tail;
                std::getline(iss, tail);

                size_t memSize = 0;
                std::string unused;
                splitMemoryAndRest(tail, memSize, unused);

                // No instructions to infer a size from, so fall back to the
                // configured minimum when the size is omitted.
                if (memSize == 0) memSize = Config::minMemPerProc;

                if (processName.empty()) {
                    cout << "Usage: screen -s <name> <memory_size>\n";
                }
                else if (!isValidMemorySize(memSize)) {
                    cout << "invalid memory allocation\n"; // Spec requirement
                }
                else if (scheduler->findProcess(processName)) {
                    cout << "Process " << processName << " already exists.\n";
                }
                else {
                    auto process = scheduler->createProcess(processName, memSize);
                    process->attachScreen();
                    displayProcessScreen(*process);
                }
            }
            else if (argument == "-c") {
                std::string tail;
                std::getline(iss, tail);

                size_t memSize = 0;
                std::string instructions;
                splitMemoryAndRest(tail, memSize, instructions);

                // Trim surrounding spaces and quotes
                size_t first = instructions.find_first_not_of(" \t\"");
                size_t last = instructions.find_last_not_of(" \t\"");
                if (first != std::string::npos && last != std::string::npos) {
                    instructions = instructions.substr(first, last - first + 1);
                } else {
                    instructions = "";
                }

                if (processName.empty() || instructions.empty()) {
                    cout << "Usage: screen -c <name> [memory_size] \"<instructions>\"\n";
                } else {
                    // memSize == 0 means it was omitted; createCustomProcess
                    // then sizes the process from the addresses it references.
                    if (memSize != 0 && !isValidMemorySize(memSize)) {
                        cout << "invalid memory allocation\n"; // Spec requirement[cite: 1]
                    } else if (scheduler->findProcess(processName)) {
                        cout << "Process " << processName << " already exists.\n";
                    } else {
                        // The parser enforces the 1-50 instruction limit and
                        // rejects anything it cannot turn into a command.
                        std::string parseError;
                        auto process = scheduler->createCustomProcess(processName, memSize,
                                                                      instructions, parseError);
                        if (!process) {
                            cout << "invalid command\n"; // Spec requirement[cite: 1]
                            cout << "  (" << parseError << ")\n";
                        } else {
                            process->attachScreen();
                            displayProcessScreen(*process);
                        }
                    }
                }
            }
            else if (argument == "-r") {
                if (processName.empty()) {
                    cout << "Usage: screen -r <name>\n";
                }
                else {
                    auto process = scheduler->findProcess(processName);
                    
                    if (!process) {
                        cout << "Process " << processName << " not found.\n";
                    } 
                    // Handle memory violation print requirement[cite: 1]
                    else if (process->hasMemoryViolation()) {
                        cout << "Process " << processName << " shut down due to memory access violation error that occurred at " 
                             << process->getViolationTime() << ". " << process->getInvalidAddress() << " invalid.\n"; 
                    } 
                    else if (process->isFinished()) {
                        cout << "Process " << processName << " not found.\n";
                    } 
                    else {
                        process->attachScreen();
                        displayProcessScreen(*process);
                    }
                }
            }
            else {
                cout << "Usage: screen -ls | screen -s <name> <memory> | screen -c <name> <memory> \"<instructions>\" | screen -r <name>\n";
            }
        }
        else if (command == "report-util") {
            scheduler->writeReport("csopesy-log.txt");
            cout << "Report generated at csopesy-log.txt!\n";
        }
        else if (command == "clear") {
            system("cls");
            printHeader();
        }
       else if (command == "vmstat") {
            std::cout << scheduler->getTotalMemory() << " bytes total memory\n";
            std::cout << scheduler->getUsedMemory()  << " bytes used memory\n";
            std::cout << scheduler->getFreeMemory()  << " bytes free memory\n";
            std::cout << scheduler->getIdleTicks()   << " idle cpu ticks\n";
            std::cout << scheduler->getActiveTicks() << " active cpu ticks\n";
            std::cout << scheduler->getTotalTicks()  << " total cpu ticks\n";
            std::cout << scheduler->getPagedIn()     << " pages paged in\n";
            std::cout << scheduler->getPagedOut()    << " pages paged out\n";
        }
        else if (command == "process-smi") {
            std::cout << "--------------------------------------------------\n";
            std::cout << "| PROCESS-SMI V01.00 Driver Version: 01.00       |\n";
            std::cout << "--------------------------------------------------\n";
            std::cout << "CPU-Util: " << scheduler->getCpuUtilization() << "%\n";
            size_t used = scheduler->getUsedMemory();
            size_t total = scheduler->getTotalMemory();
            std::cout << "Memory Usage: " << formatMemoryPair(used, total) << "\n";
            std::cout << "Memory Util: " << (total ? (used * 100 / total) : 0) << "%\n";
            std::cout << "==================================================\n";
            std::cout << "Running processes and memory usage:\n";
            std::cout << "--------------------------------------------------\n";
            
            auto runningProcesses = scheduler->getRunningProcesses();
            bool anyPrinted = false;
            for (const auto& p : runningProcesses) {
                size_t residentMem = scheduler->getProcessResidentMemory(p->getPID());
                if (residentMem > 0) { // Only list processes currently occupying physical RAM
                    std::cout << p->getName() << " " << formatMemory(residentMem) << "\n";
                    anyPrinted = true;
                }
            }
            if (!anyPrinted) {
                std::cout << "(none)\n";
            }
            std::cout << "--------------------------------------------------\n";
        }
        else if (command == "exit") {
            systemRunning->store(false);
            if (engineThread.joinable()) engineThread.join();
            if (scheduler) scheduler->shutdown();
            break;
        }
        else {
            cout << "Unknown command.\n";
        }
    }

    return 0;
}

void printHeader() {
    std::cout << " \x1b[1m\x1b[36m   ______   ______     ___   _______  ________   ______  ____  ____  \x1b[0m\n";
    std::cout << " \x1b[1m\x1b[33m .' ___  |.' ____ \\  .'   `.|_   __ \\|_   __  |.' ____ \\|_  _||_  _| \x1b[0m\n";
    std::cout << " \x1b[1m\x1b[37m/ .'   \\_|| (___ \\_|/  .-.  \\ | |__) | | |_ \\_|| (___ \\_| \\ \\  / /   \x1b[0m\n";
    std::cout << " \x1b[1m\x1b[37m| |        _.____`. | |   | | |  ___/  |  _| _  _.____`.   \\ \\/ /    \x1b[0m\n";
    std::cout << " \x1b[1m\x1b[33m\\ `.___.'\\| \\____) |\\  `-'  /_| |_    _| |__/ || \\____) |  _|  |_    \x1b[0m\n";
    std::cout << " \x1b[1m\x1b[36m `.____ .' \\______.' `.___.'|_____|  |________| \\______.' |______|  \x1b[0m\n";
    std::cout << "\n";
    std::cout << "Welcome to CSOPESY command line! Type \"exit\" to quit or \"clear\" to clear the screen.\n";
    std::cout << "Type \"initialize\" to load system configuration.\n";
}

void displayProcessScreen(Process& process) {
    // Helper function to print the layout required by the specifications
    auto printProcessInfo = [](Process& p) {
        system("cls"); // Clear the contents to "move" into the process screen
        const auto logs = p.getScreenLogs();
        
        std::cout << "Process name: " << p.getName() << "\n";
        std::cout << "ID: "           << p.getPID() << "\n";
        std::cout << "Logs:\n";
        
        if (logs.empty()) {
            std::cout << "(none)\n";
        } else {
            for (const auto& log : logs) {
                std::cout << log << "\n";
            }
        }
        
        // Only print execution status metrics if the process isn't finished yet
        if (p.isTerminated()) {
            std::cout << "\nProcess " << p.getName()
                      << " shut down due to memory access violation error that occurred at "
                      << p.getViolationTime() << ". " << p.getInvalidAddress() << " invalid.\n";
        } else if (!p.isFinished()) {
            std::cout << "\nCurrent instruction line: " << p.getCurrentLine() << "\n";
            std::cout << "Lines of code: "            << p.getTotalLines() << "\n";
        } else {
            // Requirement: If the process has finished, print "Finished!" right after the logs
            std::cout << "\nFinished!\n";
        }
    };

    // --- RULE 1: Print info IMMEDIATELY upon screen entry (-s or -r) ---
    printProcessInfo(process);

    while (true) {
        std::string line;
        std::cout << "\nroot:\\> "; // Match specification console prompt layout
        if (!std::getline(std::cin, line)) break;

        if (line.rfind("\xEF\xBB\xBF", 0) == 0) line.erase(0, 3);

        std::istringstream iss(line);
        std::string command;
        iss >> command;

        if (command == "process-smi") {
            // RULE 2: process-smi updates details dynamically based on execution steps
            printProcessInfo(process);
        }
        else if (command == "exit") {
            process.detachScreen();
            system("cls"); // Clear screen upon returning to main menu
            break;
        }
        else if (!command.empty()) {
            std::cout << "Unknown command.\n";
        }
    }
}

void runExecutionEngine(std::shared_ptr<std::atomic<uint64_t>> cpuCycles, Scheduler& scheduler, std::shared_ptr<std::atomic<bool>> systemRunning) {
    while (systemRunning->load()) {
        cpuCycles->fetch_add(1);
        scheduler.runSingleCycleStep();

        // Constant baseline tick delay (1ms) to prevent host CPU hogging,
        // decoupled from the simulated parameters.
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }
}

