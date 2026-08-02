#pragma once

#include <algorithm>
#include <atomic>
#include <fstream>
#include <memory>
#include <mutex>
#include <string>
#include <unordered_map>
#include <vector>
#include "SymbolTable.h"

#include "ICommand.h"

// The emulator's Process Control Block (PCB). Holds a process's identity, its
// list of instructions, and its execution state. A process is run to completion
// by a single CPU worker thread (non-preemptive FCFS), so its output file needs
// no locking. Fields read by the console thread (for "screen -ls") are atomic.
class Process {
public:
    enum ProcessState {
        READY,
        RUNNING,
        SLEEPING,
        FINISHED
    };

    Process(int pid, const std::string& name);

    // Build-up (called before the process is scheduled).
    void addCommand(std::shared_ptr<ICommand> command);

    // Execution (called by the owning CPU worker thread).
    void executeCurrentCommand(int coreId);
    void finishExecution();          // mark FINISHED and flush/close the log file
    bool isFinished() const;

    // Writes one print line to this process's log file. Called from PrintCommand.
    void logPrint(int coreId, const std::string& message);

    // Prints the provided message to this process's attached console.
    void printToScreen(const std::string& message);

    // State mutators (used by the scheduler/worker).
    void setState(ProcessState state);
    void setCoreId(int coreId);

    // Variable support for process-local uint16 registers.
    void declareVariable(const std::string& name, uint16_t value);
    uint16_t getVariable(const std::string& name);
    void setVariable(const std::string& name, uint32_t value);

    // Sleep support for SLEEP instructions.
    bool isSleeping() const;
    int getSleepTicks() const;
    void sleepFor(int ticks);
    bool tickSleep();

    // Busy-wait support for delay-per-exec parameters
    bool isBusyWaiting(uint64_t currentGlobalClock) const;
    void startBusyWait(uint64_t currentGlobalClock, int delayCycles);
    void tickBusyWait();

    // Accessors (read by the console thread for "screen -ls").
    int getPID() const;
    std::string getName() const;
    std::string getCreatedAt() const;
    ProcessState getState() const;
    int getCoreId() const;
    int getCurrentLine() const;      // instructions executed so far
    int getTotalLines() const;       // total instructions

    void attachScreen();
    void detachScreen();
    bool hasScreenSession() const;
    bool isScreenAttached() const;
    std::vector<std::string> getScreenLogs() const;

    

private:
    int pid;
    std::string name;
    std::string createdAt;           // timestamp captured at construction

    std::vector<std::shared_ptr<ICommand>> commandList;

    std::atomic<int> commandCounter; // next instruction index = lines executed
    std::atomic<int> coreId;         // -1 until assigned to a core
    std::atomic<ProcessState> currentState;
    std::atomic<int> sleepTicks;

    std::ofstream outFile;           // per-process log; opened lazily on first print
    bool fileOpened;

    std::vector<std::string> screenLogs;
    mutable std::mutex screenMutex;
    std::unordered_map<std::string, uint16_t> variables;
    mutable std::mutex variableMutex;
    bool screenSessionExists;
    bool screenAttached;
    SymbolTable symbolTable;
    // Inside Process.h
    std::atomic<uint64_t> busyWaitDeadline{0}; // Tracks cycles spent busy-waiting
};
