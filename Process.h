#pragma once

#include <algorithm>
#include <atomic>
#include <cstdint>
#include <fstream>
#include <memory>
#include <mutex>
#include <string>
#include <vector>

#include "ICommand.h"
#include "ProcessMemory.h"
#include "SymbolTable.h"

// Outcome of any instruction step that touches process memory.
//
// PAGE_FAULT is never returned today -- Process owns only the virtual address
// space, not physical frames. It exists because every instruction already
// handles it by restarting, so whoever wires up demand paging only has to
// return it from Process::readMemory / writeMemory.
enum class MemoryStatus {
    OK,          // the access completed
    PAGE_FAULT,  // a page fault was serviced; the instruction must restart
    VIOLATION,   // address outside the process's memory; the process is dead
    IGNORED,     // symbol table segment is full, so the declaration is skipped
};

class Process {
public:
    enum ProcessState {
        READY,
        RUNNING,
        SLEEPING,
        FINISHED,
        TERMINATED
    };

    Process(int pid, const std::string& name, size_t memorySize);

    void addCommand(std::shared_ptr<ICommand> command);

    void executeCurrentCommand(int coreId);
    void finishExecution();
    bool isFinished() const;

    void logPrint(int coreId, const std::string& message);

    void setState(ProcessState state);
    void setCoreId(int coreId);

    // --- Memory-backed variables ------------------------------------------
    // Every one of these can page-fault, because variables live in the 64-byte
    // symbol table segment at the base of the process's address space.
    MemoryStatus declareVariable(const std::string& name, uint16_t value);
    MemoryStatus readVariable(const std::string& name, uint16_t& outValue);
    MemoryStatus writeVariable(const std::string& name, uint32_t value);

    // --- Raw memory access (READ / WRITE instructions) ---------------------
    MemoryStatus readMemory(size_t address, uint16_t& outValue);
    MemoryStatus writeMemory(size_t address, uint16_t value);

    bool isSleeping() const;
    int getSleepTicks() const;
    void sleepFor(int ticks);
    bool tickSleep();

    bool isBusyWaiting(uint64_t currentGlobalClock) const;
    void startBusyWait(uint64_t currentGlobalClock, int delayCycles);

    int getPID() const;
    std::string getName() const;
    std::string getCreatedAt() const;
    ProcessState getState() const;
    int getCoreId() const;
    int getCurrentLine() const;
    int getTotalLines() const;

    void attachScreen();
    void detachScreen();
    bool hasScreenSession() const;
    bool isScreenAttached() const;
    std::vector<std::string> getScreenLogs() const;

    size_t getMemorySize() const;
    ProcessMemory& memory();

    // --- Access violation --------------------------------------------------
    bool hasMemoryViolation() const;
    bool isTerminated() const;
    std::string getViolationTime() const;
    std::string getInvalidAddress() const;
    void raiseAccessViolation(size_t address);

private:
    // Resolves `name` to its address in the symbol table segment, claiming a
    // slot if needed. False when the 32-variable limit is already reached.
    bool resolveVariableAddress(const std::string& name, size_t& outAddress, bool allowCreate);

    int pid;
    std::string name;
    std::string createdAt;

    std::vector<std::shared_ptr<ICommand>> commandList;

    std::atomic<int> commandCounter;
    std::atomic<int> coreId;
    std::atomic<ProcessState> currentState;
    std::atomic<int> sleepTicks;

    std::ofstream outFile;
    bool fileOpened;

    std::vector<std::string> screenLogs;
    mutable std::mutex screenMutex;

    bool screenSessionExists;
    bool screenAttached;

    SymbolTable   symbolTable;
    ProcessMemory processMemory;

    std::atomic<uint64_t> busyWaitDeadline{0};

    mutable std::mutex violationMutex;
    std::atomic<bool> memoryViolation{false};
    std::string violationTime;
    std::string invalidAddress;
};
