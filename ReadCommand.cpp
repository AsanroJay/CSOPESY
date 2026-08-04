#include "ReadCommand.h"

#include "Process.h"
#include "Utils.h"

ReadCommand::ReadCommand(const std::string& variableName, size_t address)
    : ICommand(ICommand::READ),
      variableName(variableName),
      address(address) {}

bool ReadCommand::execute(int coreId, Process& process) {
    // Stage 1: fetch the value from memory.
    if (!valueCached) {
        uint16_t value = 0;
        MemoryStatus status = process.readMemory(address, value);
        if (status == MemoryStatus::PAGE_FAULT) return false;  // restart the instruction
        if (status == MemoryStatus::VIOLATION)  return true;    // process has been shut down
        cachedValue = value;
        valueCached = true;
    }

    // Stage 2: store it into the variable, which lives on the symbol table page.
    MemoryStatus status = process.writeVariable(variableName, cachedValue);
    if (status == MemoryStatus::PAGE_FAULT) return false;

    uint16_t value = cachedValue;
    valueCached = false;
    if (status == MemoryStatus::VIOLATION)  return true;

    if (status == MemoryStatus::IGNORED) {
        process.logPrint(coreId, "READ: " + variableName + " ignored (symbol table full)");
        return true;
    }

    process.logPrint(coreId, "READ: " + variableName + " = " + std::to_string(value) +
                                 " from " + toHexAddress(address));
    return true;
}
