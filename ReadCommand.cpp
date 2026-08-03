#include "ReadCommand.h"

#include "Process.h"
#include "Utils.h"

ReadCommand::ReadCommand(const std::string& variableName, size_t address)
    : ICommand(ICommand::READ),
      variableName(variableName),
      address(address) {}

bool ReadCommand::execute(int coreId, Process& process) {
    uint16_t value = 0;

    MemoryStatus status = process.readMemory(address, value);
    if (status == MemoryStatus::PAGE_FAULT) return false;   // restart the instruction
    if (status == MemoryStatus::VIOLATION)  return true;    // process has been shut down

    status = process.writeVariable(variableName, value);
    if (status == MemoryStatus::PAGE_FAULT) return false;
    if (status == MemoryStatus::VIOLATION)  return true;

    if (status == MemoryStatus::IGNORED) {
        process.logPrint(coreId, "READ: " + variableName + " ignored (symbol table full)");
        return true;
    }

    process.logPrint(coreId, "READ: " + variableName + " = " + std::to_string(value) +
                                 " from " + toHexAddress(address));
    return true;
}
