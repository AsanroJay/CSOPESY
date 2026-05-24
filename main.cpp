#include <cstdio>
#include <iostream>
#include <string>

using namespace std;

void printHeader();
void initializeCommand();
void screenCommand();
void schedulerStartCommand();
void schedulerStopCommand();
void reportUtilCommand();
void clearCommand();
void exitCommand(bool* running);

int main() {
        string command;
        printHeader();
		
		bool running = false;
		
        while (true) {
            cout << "\nEnter command: ";
            getline(cin, command);

            if (command == "initialize") {
                initializeCommand();
            }
            else if (command == "screen") {
                screenCommand();
            }
            else if (command == "scheduler-start") {
                schedulerStartCommand();
            }
            else if (command == "scheduler-stop") {
                schedulerStopCommand();
            }
            else if (command == "report-util") {
                reportUtilCommand();
            }
            else if (command == "clear") {
                clearCommand();
            }
            else if (command == "exit") {
                exitCommand(&running);
                break;
            }
            else {
                cout << "Unknown command." << endl;
            }
    }
    return 0;
}

void printHeader() {
    std::cout << "  \x1b[1m\x1b[36m ▄████▄  ▄████▄   ▄█████▄  ▀█████▄  ██▀▀▀▀██  ▄████▄  ██      ██ \x1b[0m\n";
    std::cout << "  \x1b[1m\x1b[33m███   ██ ██       ██   ██   ██   ██ ██        ██       ██    ██    \x1b[0m\n";
    std::cout << "  \x1b[1m\x1b[37m██        ▀███▄   ██   ██   ██   ██ ██▀▀▀▀     ▀███▄   ▀██  ██▀    \x1b[0m\n";
    std::cout << "  \x1b[1m\x1b[37m██            ██  ██   ██   █████   ██             ██    ▀██▀      \x1b[0m\n";
    std::cout << "  \x1b[1m\x1b[33m██▄   ██ ▄    ██  ██   ██   ██      ██        ▄    ██     ██     \x1b[0m\n";
    std::cout << "  \x1b[1m\x1b[36m ▀█████▀  ▀███▀   ▀█████▀  ▄██▄     ██▄▄▄▄██   ▀███▀      ██       \x1b[0m\n";
    std::cout << "\n";
}

void initializeCommand() {
    cout << "initialize command recognized. Doing something." << endl;
}

void screenCommand() {
    cout << "screen command recognized. Doing something." << endl;
}

void schedulerStartCommand() {
    cout << "scheduler-start command recognized. Doing something." << endl;
}

void schedulerStopCommand() {
    cout << "scheduler-stop command recognized. Doing something." << endl;
}

void reportUtilCommand() {
    cout << "report-util command recognized. Doing something." << endl;
}

void clearCommand() {
    cout << "clear command recognized. Doing something." << endl;
    
    system("CLS");
    printHeader();
}

void exitCommand(bool* running) {
    cout << "exit command recognized. Doing something." << endl;
    
    *running = false;
}
