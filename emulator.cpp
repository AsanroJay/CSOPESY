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
        string command, argument;
        iss >> command >> argument;

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
                cout << "Type \"scheduler-start\" to begin generating processes.\n";
            }
        }
        else if (command == "scheduler-start") {
            scheduler->startGeneration();
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
            else if (argument == "-s" || argument == "-r") {
                cout << "screen " << argument << " not yet implemented.\n";
            }
            else {
                cout << "Usage: screen -ls | screen -s <name> | screen -r <name>\n";
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

void runExecutionEngine(std::shared_ptr<std::atomic<uint64_t>> cpuCycles, Scheduler& scheduler, std::shared_ptr<std::atomic<bool>> systemRunning) {
    while (systemRunning->load()) {
        cpuCycles->fetch_add(1);
        scheduler.runSingleCycleStep();

        int delay = (Config::delayPerExec > 0) ? Config::delayPerExec : 1;
        std::this_thread::sleep_for(std::chrono::milliseconds(delay));
    }
}