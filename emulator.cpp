#include <cstdlib>
#include <iostream>
#include <sstream>
#include <string>

#include "Config.h"
#include "Scheduler.h"

using namespace std;

void printHeader();

int main() {
    printHeader();

    // The scheduler + 4 worker threads start now but stay idle until
    // "scheduler-start" generates processes.
    FCFSScheduler scheduler(Config::NUM_CORES);
    scheduler.start();

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

        if (command.empty()) {
            // ignore blank input
        }
        else if (command == "initialize") {
            cout << "Configuration:\n";
            cout << "  Scheduler       : FCFS (non-preemptive)\n";
            cout << "  CPU cores       : " << Config::NUM_CORES << "\n";
            cout << "  Processes       : " << Config::NUM_PROCESSES
                 << " (type 'scheduler-start' to generate)\n";
            cout << "  Prints/process  : " << Config::PRINTS_PER_PROCESS << "\n";
        }
        else if (command == "scheduler-start") {
            scheduler.generateProcesses(Config::NUM_PROCESSES, Config::PRINTS_PER_PROCESS);
            cout << "Generated " << Config::NUM_PROCESSES << " processes, each with "
                 << Config::PRINTS_PER_PROCESS << " print commands.\n";
        }
        else if (command == "scheduler-stop") {
            scheduler.stopDispatch();
            cout << "Scheduler stopped accepting new processes.\n";
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
    std::cout << "Type \"scheduler-start\" to create the processes and run the FCFS scheduler.\n";
}
