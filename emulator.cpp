#include <cstdlib>
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
        else if (command == "scheduler-start") {
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
                std::string memStr;
                iss >> memStr;
                
                if (processName.empty() || memStr.empty()) {
                    cout << "Usage: screen -s <name> <memory_size>\n";
                }
                else {
                    size_t memSize = 0;
                    bool validMem = false;
                    try {
                        memSize = std::stoull(memStr);
                        // Memory must be between 2^6 (64) and 2^16 (65536) and a power of 2
                        if (memSize >= 64 && memSize <= 65536 && (memSize & (memSize - 1)) == 0) {
                            validMem = true;
                        }
                    } catch (...) {}

                    if (!validMem) {
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
            }
            else if (argument == "-c") {
                std::string memStr;
                iss >> memStr;
                std::string instructions;
                std::getline(iss, instructions);
                
                // Trim leading spaces and quotes
                size_t first = instructions.find_first_not_of(" \t\"");
                size_t last = instructions.find_last_not_of(" \t\"");
                if (first != std::string::npos && last != std::string::npos) {
                    instructions = instructions.substr(first, last - first + 1);
                } else {
                    instructions = "";
                }

                if (processName.empty() || memStr.empty() || instructions.empty()) {
                    cout << "Usage: screen -c <name> <memory_size> \"<instructions>\"\n";
                } else {
                    size_t memSize = 0;
                    bool validMem = false;
                    try {
                        memSize = std::stoull(memStr);
                        if (memSize >= 64 && memSize <= 65536 && (memSize & (memSize - 1)) == 0) {
                            validMem = true;
                        }
                    } catch (...) {}
                    
                    if (!validMem) {
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
            std::cout << "PROCESS-SMI\n";
            std::cout << "--------------------------------------------------\n";
            std::cout << "CPU-Util: " << scheduler->getCpuUtilization() << "%\n";
            size_t used = scheduler->getUsedMemory();
            size_t total = scheduler->getTotalMemory();
            std::cout << "Memory Usage: " << used << "B / " << total << "B\n";
            std::cout << "Memory Util: " << (total ? (used * 100 / total) : 0) << "%\n";
            std::cout << "--------------------------------------------------\n";
            std::cout << "Running Processes:\n";
            
            auto runningProcesses = scheduler->getRunningProcesses();
            if (runningProcesses.empty()) {
                std::cout << " (none)\n";
            } else {
                for (const auto& p : runningProcesses) {

                    std::cout << p->getName() 
                              << " [PID: " << p->getPID() << "]"
                              << " Mem: " << p->getMemorySize() << "B\n";
                }
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

