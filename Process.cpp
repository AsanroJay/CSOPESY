#include "Process.h"

#include <iostream>

#include "Config.h"
#include "Utils.h"

Process::Process(int pid, const std::string& name)
    : pid(pid),
      name(name),
      createdAt(currentTimestamp()),
      commandCounter(0),
      coreId(-1),
      currentState(READY),
      sleepTicks(0),
      fileOpened(false),
      screenSessionExists(false),
      screenAttached(false) {}

void Process::addCommand(std::shared_ptr<ICommand> command) {
    commandList.push_back(std::move(command));
}

void Process::executeCurrentCommand(int coreId) {
    if (isFinished()) {
        return;
    }
    int index = commandCounter.load();
    bool finished = commandList[index]->execute(coreId, *this);
    if (finished) {
        commandCounter.fetch_add(1);
        // delay-per-exec: after completing an instruction, busy-wait the
        // configured number of CPU cycles before the next one. The process
        // stays on its core during this wait (busy-waiting scheme per spec).
        if (Config::delayPerExec > 0) {
            startBusyWait(Config::delayPerExec);
        }
    }
}

void Process::finishExecution() {
    currentState.store(FINISHED);

    {
        std::lock_guard<std::mutex> lock(screenMutex);
        screenLogs.push_back("Finished!");
    }

    if (fileOpened) {
        outFile.close();
        fileOpened = false;
    }
}

bool Process::isFinished() const {
    return commandCounter.load() >= static_cast<int>(commandList.size());
}

void Process::logPrint(int coreId, const std::string& message) {
    std::string timestamp = currentTimestamp();
    std::string entry = "(" + timestamp + ") Core:" + std::to_string(coreId) + " \"" + message + "\"";

    {
        std::lock_guard<std::mutex> lock(screenMutex);
        screenLogs.push_back(entry);
    }

    if (!Config::WRITE_PRINT_FILES) {
        return;
    }

    if (!fileOpened) {
        outFile.open(name + ".txt");
        outFile << "Process name: " << name << "\n";
        outFile << "Logs:\n\n";
        fileOpened = true;
    }
    outFile << "(" << currentTimestamp() << ") Core:" << coreId
            << " \"" << message << "\"\n";
}

void Process::setState(ProcessState state) {
    currentState.store(state);
}

void Process::setCoreId(int coreId) {
    this->coreId.store(coreId);
}

void Process::declareVariable(const std::string& name, uint16_t value) {
    symbolTable.declareVariable(name, value);
}

uint16_t Process::getVariable(const std::string& name) {
    return symbolTable.getVariable(name);
}

void Process::setVariable(const std::string& name, uint32_t value) {
    symbolTable.setVariable(name, value);
}

bool Process::isSleeping() const {
    return sleepTicks.load() > 0;
}

int Process::getSleepTicks() const {
    return sleepTicks.load();
}

void Process::sleepFor(int ticks) {
    sleepTicks.store(ticks);
}

bool Process::tickSleep() {
    int remaining = sleepTicks.load();
    if (remaining <= 0) {
        return true;
    }
    remaining = std::max(0, remaining - 1);
    sleepTicks.store(remaining);
    return remaining == 0;
}

int Process::getPID() const {
    return pid;
}

std::string Process::getName() const {
    return name;
}

std::string Process::getCreatedAt() const {
    return createdAt;
}

Process::ProcessState Process::getState() const {
    return currentState.load();
}

int Process::getCoreId() const {
    return coreId.load();
}

int Process::getCurrentLine() const {
    return commandCounter.load();
}

int Process::getTotalLines() const {
    return static_cast<int>(commandList.size());
}

void Process::attachScreen() {
    std::lock_guard<std::mutex> lock(screenMutex);
    screenSessionExists = true;
    screenAttached = true;
}

void Process::detachScreen() {
    std::lock_guard<std::mutex> lock(screenMutex);
    screenAttached = false;
}

bool Process::hasScreenSession() const {
    std::lock_guard<std::mutex> lock(screenMutex);
    return screenSessionExists;
}

bool Process::isScreenAttached() const {
    std::lock_guard<std::mutex> lock(screenMutex);
    return screenAttached;
}

std::vector<std::string> Process::getScreenLogs() const {
    std::lock_guard<std::mutex> lock(screenMutex);
    return screenLogs;
}

// Inside Process.cpp
bool Process::isBusyWaiting() const {
    return currentBusyTicks.load() > 0;
}

void Process::startBusyWait(int cycles) {
    currentBusyTicks.store(cycles);
}

void Process::tickBusyWait() {
    int remaining = currentBusyTicks.load();
    if (remaining > 0) {
        currentBusyTicks.store(remaining - 1);
    }
}
