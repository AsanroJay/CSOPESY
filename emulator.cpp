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

// The central background continuous execution loop engine
void runExecutionEngine(std::shared_ptr<std::atomic<uint64_t>> cpuCycles, FCFSScheduler& scheduler, std::shared_ptr<std::atomic<bool>> systemRunning);

int main() {
    printHeader();

    // Setup the shared memory structures for the driver layout
    auto cpuCycles = std::make_shared<std::atomic<uint64_t>>(0);
    auto systemRunning = std::make_shared<std::atomic<bool>>(true);

    // Pass the shared pointer to the clock into the scheduler initialization
    FCFSScheduler scheduler(Config::NUM_CORES, cpuCycles);
    scheduler.start();
    
    // Spin up the background step engine thread
    std::thread engineThread(runExecutionEngine, cpuCycles, std::ref(scheduler), systemRunning);

    string line;
    while (true) {
        cout << "\nEnter command: ";
        if (!getline(cin, line)) {
            break;  // EOF (e.g. input redirected) -> shut down cleanly
        }

        // Strip a leading UTF-8 BOM, which can appear on the first line when
        // input is piped/redirected rather than typed.
        if (line.rfind("\xEF\xBB\xBF", 0) == 0) {
            line.erase(0, 3);
        }

        istringstream iss(line);
        string command;
        string argument;
        iss >> command >> argument;

        // initialize should be called first
        if (!Config::initialized && command != "initialize" && command != "exit" && !command.empty()) {
            std::cout << "Please run \"initialize\" first.\n";
            continue;
        }

        if (command.empty()) {
            // ignore blank input
        }
        else if (command == "initialize") {
            if (Config::loadFromFile()) {
                std::cout << "Configuration loaded:\n";
                std::cout << "  Scheduler        : " << Config::scheduler << "\n";
                std::cout << "  CPU cores        : " << Config::numCpu << "\n";
                std::cout << "  Batch freq       : " << Config::batchProcessFreq << " CPU cycle(s)\n";
                std::cout << "  Min instructions : " << Config::minIns << "\n";
                std::cout << "  Max instructions : " << Config::maxIns << "\n";
                std::cout << "  Delay per exec   : " << Config::delayPerExec << "\n";
                std::cout << "Type scheduler-start to begin processes";
            }
        }
        else if (command == "scheduler-start") {
            scheduler.startGeneration();
            cout << "Continuous background process generation started.\n";
            cout << "Generating processes every " << Config::batchProcessFreq << " CPU cycle(s).\n";
        }
        else if (command == "scheduler-stop") {
            scheduler.stopGeneration();
            cout << "Process generation stopped. Active processes will run to completion.\n";
        }
        else if (command == "screen") {
            if (argument == "-ls") {
                scheduler.printStatus(cout);
            }
            else if (argument == "-s" || argument == "-r") {
                cout << "screen " << argument << " is not used in this homework.\n";
            }
            else {
                cout << "Usage: screen -ls\n";
            }
        }
        else if (command == "report-util") {
            scheduler.writeReport("csopesy-log.txt");
            cout << "Report generated at csopesy-log.txt!\n";
        }
        else if (command == "clear") {
            system("cls");
            printHeader();
        }
        else if (command == "exit") {
            systemRunning->store(false); // Stop the background execution loop cleanly
            if (engineThread.joinable()) {
                engineThread.join();
            }
            scheduler.shutdown();
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
    std::cout << "Welcome to CSOPESY command line! Type \"exit\" to quit the terminal or type \"clear\" to clear the screen. \n";
    std::cout << "Type \"initialize\" to load system configuration\n";
}

// The central background continuous execution loop engine
void runExecutionEngine(std::shared_ptr<std::atomic<uint64_t>> cpuCycles, FCFSScheduler& scheduler, std::shared_ptr<std::atomic<bool>> systemRunning) {
    while (systemRunning->load()) {
        // 1.) Clock cycle increments
        cpuCycles->fetch_add(1);

        // 2.) Everything executes one step and waits for completion
        scheduler.runSingleCycleStep();

        // 3.) Loop Back pacing delay
        int delay = (Config::delayPerExec > 0) ? Config::delayPerExec : 1;
        std::this_thread::sleep_for(std::chrono::milliseconds(delay));
    }
}