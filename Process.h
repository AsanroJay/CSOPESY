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

class Process {
public:
    enum ProcessState {
        READY,
        RUNNING,
        SLEEPING,
        FINISHED
    };

    Process(int pid, const std::string& name);

    void addCommand(std::shared_ptr<ICommand> command);

    void executeCurrentCommand(int coreId);
    void finishExecution();
    bool isFinished() const;

    void logPrint(int coreId, const std::string& message);
    void printToScreen(const std::string& message);

    void setState(ProcessState state);
    void setCoreId(int coreId);

    void declareVariable(const std::string& name, uint16_t value);
    uint16_t getVariable(const std::string& name);
    void setVariable(const std::string& name, uint32_t value);

    bool isSleeping() const;
    int getSleepTicks() const;
    void sleepFor(int ticks);
    bool tickSleep();

    bool isBusyWaiting(uint64_t currentGlobalClock) const;
    void startBusyWait(uint64_t currentGlobalClock, int delayCycles);
    void tickBusyWait();

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

    // Memory size and violation accessors
    void setMemorySize(size_t size);
    size_t getMemorySize() const;

    bool hasMemoryViolation() const;
    std::string getViolationTime() const;
    std::string getInvalidAddress() const;
    void setMemoryViolation(const std::string& timestamp, const std::string& address);

private:
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
    std::unordered_map<std::string, uint16_t> variables;
    mutable std::mutex variableMutex;
    bool screenSessionExists;
    bool screenAttached;
    SymbolTable symbolTable;
    std::atomic<uint64_t> busyWaitDeadline{0};

    size_t memorySize = 0;
    bool memoryViolation = false;
    std::string violationTime = "";
    std::string invalidAddress = "";
};