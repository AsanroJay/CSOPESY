#include "Process.h"

#include <filesystem>
#include <iostream>
#include <limits>

#include "Config.h"
#include "Utils.h"

Process::Process(int pid, const std::string& name, size_t memorySize)
    : pid(pid),
      name(name),
      createdAt(currentTimestamp()),
      commandCounter(0),
      coreId(-1),
      currentState(READY),
      sleepTicks(0),
      fileOpened(false),
      screenSessionExists(false),
      screenAttached(false),
      processMemory(std::max<size_t>(memorySize, ProcessMemory::SYMBOL_TABLE_BYTES)) {}

void Process::addCommand(std::shared_ptr<ICommand> command) {
    commandList.push_back(std::move(command));
}

void Process::executeCurrentCommand(int coreId) {
    if (isFinished()) {
        return;
    }
    int index = commandCounter.load();
    if (index < 0 || index >= static_cast<int>(commandList.size())) {
        return;
    }

    bool completed = commandList[index]->execute(coreId, *this);

    // A command that reports "not yet" took a page fault, so the program
    // counter stays put and the instruction is restarted on the next tick.
    if (completed) {
        commandCounter.fetch_add(1);
    }
}

void Process::finishExecution() {
    if (isTerminated()) {
        if (fileOpened) {
            outFile.close();
            fileOpened = false;
        }
        return;
    }

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
    if (memoryViolation.load()) return true;
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
        std::filesystem::create_directories("process_logs");
        std::string path = "process_logs/" + name + ".txt";
        outFile.open(path);
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

// --- Variables ------------------------------------------------------------

bool Process::resolveVariableAddress(const std::string& name, size_t& outAddress, bool allowCreate) {
    if (allowCreate) {
        return symbolTable.addressOf(name, outAddress);
    }
    return symbolTable.find(name, outAddress);
}

MemoryStatus Process::declareVariable(const std::string& name, uint16_t value) {
    size_t address = 0;
    if (!resolveVariableAddress(name, address, true)) {
        // 32-variable limit reached: the spec says to ignore the declaration.
        return MemoryStatus::IGNORED;
    }
    return writeMemory(address, value);
}

MemoryStatus Process::readVariable(const std::string& name, uint16_t& outValue) {
    size_t address = 0;
    if (!resolveVariableAddress(name, address, true)) {
        // Undeclared and no slot left: reads fall back to 0 rather than failing.
        outValue = 0;
        return MemoryStatus::OK;
    }
    return readMemory(address, outValue);
}

MemoryStatus Process::writeVariable(const std::string& name, uint32_t value) {
    size_t address = 0;
    if (!resolveVariableAddress(name, address, true)) {
        return MemoryStatus::IGNORED;
    }

    // uint16 values are clamped to [0, 65535].
    uint32_t clamped = std::min<uint32_t>(value, std::numeric_limits<uint16_t>::max());
    return writeMemory(address, static_cast<uint16_t>(clamped));
}

// --- Raw memory -----------------------------------------------------------

// These two functions are the seam for demand paging. Both pages a uint16 can
// span are known here, so making them resident (and returning PAGE_FAULT so the
// instruction restarts) is a local change -- no command needs to be touched.
void Process::setPageAccessHandler(PageAccessHandler handler) {
    pageAccessHandler = std::move(handler);
}

MemoryStatus Process::readMemory(size_t address, uint16_t& outValue) {
    if (!processMemory.isValidWordAddress(address)) {
        raiseAccessViolation(address);
        return MemoryStatus::VIOLATION;
    }

    // --- DEMAND PAGING SEAM ---
    // Check if the page is resident. If not, fault!
    if (pageAccessHandler && !pageAccessHandler(address)) {
        return MemoryStatus::PAGE_FAULT; 
    }

    outValue = processMemory.readWord(address);
    return MemoryStatus::OK;
}

MemoryStatus Process::writeMemory(size_t address, uint16_t value) {
    if (!processMemory.isValidWordAddress(address)) {
        raiseAccessViolation(address);
        return MemoryStatus::VIOLATION;
    }

    // --- DEMAND PAGING SEAM ---
    // Check if the page is resident. If not, fault!
    if (pageAccessHandler && !pageAccessHandler(address)) {
        return MemoryStatus::PAGE_FAULT;
    }

    processMemory.writeWord(address, value);
    return MemoryStatus::OK;
}

// --- Scheduling state -----------------------------------------------------

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

bool Process::isBusyWaiting(uint64_t currentGlobalClock) const {
    return currentGlobalClock < busyWaitDeadline.load();
}

void Process::startBusyWait(uint64_t currentGlobalClock, int delayCycles) {
    busyWaitDeadline.store(currentGlobalClock + static_cast<uint64_t>(delayCycles));
}

size_t Process::getMemorySize() const {
    return processMemory.getSize();
}

ProcessMemory& Process::memory() {
    return processMemory;
}

// --- Access violation -----------------------------------------------------

void Process::raiseAccessViolation(size_t address) {
    {
        std::lock_guard<std::mutex> lock(violationMutex);
        if (memoryViolation.load()) return;  // keep the first violation
        violationTime  = currentTimeOfDay();
        invalidAddress = toHexAddress(address);
    }

    currentState.store(TERMINATED);
    memoryViolation.store(true);  // set last, after the strings are written

    logPrint(coreId.load(),
             "ACCESS VIOLATION at " + toHexAddress(address) + " - process shut down");
}

bool Process::hasMemoryViolation() const {
    return memoryViolation.load();
}

bool Process::isTerminated() const {
    return memoryViolation.load();
}

std::string Process::getViolationTime() const {
    std::lock_guard<std::mutex> lock(violationMutex);
    return violationTime;
}

std::string Process::getInvalidAddress() const {
    std::lock_guard<std::mutex> lock(violationMutex);
    return invalidAddress;
}
