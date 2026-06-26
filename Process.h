#pragma once

#include <atomic>
#include <fstream>
#include <memory>
#include <string>
#include <vector>

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

    // State mutators (used by the scheduler/worker).
    void setState(ProcessState state);
    void setCoreId(int coreId);

    // Accessors (read by the console thread for "screen -ls").
    int getPID() const;
    std::string getName() const;
    std::string getCreatedAt() const;
    ProcessState getState() const;
    int getCoreId() const;
    int getCurrentLine() const;      // instructions executed so far
    int getTotalLines() const;       // total instructions

private:
    int pid;
    std::string name;
    std::string createdAt;           // timestamp captured at construction

    std::vector<std::shared_ptr<ICommand>> commandList;

    std::atomic<int> commandCounter; // next instruction index = lines executed
    std::atomic<int> coreId;         // -1 until assigned to a core
    std::atomic<ProcessState> currentState;

    std::ofstream outFile;           // per-process log; opened lazily on first print
    bool fileOpened;
    
};
