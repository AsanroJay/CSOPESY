#include "WriteCommand.h"

#include "Process.h"
#include "Utils.h"

WriteCommand::WriteCommand(size_t address, Operand value)
    : ICommand(ICommand::WRITE),
      address(address),
      value(std::move(value)) {}

bool WriteCommand::execute(int coreId, Process& process) {
    uint16_t resolved = value.literalValue;

    // Reading the source variable can itself fault, since variables live in the
    // symbol table segment of this process's memory.
    if (value.isVariable) {
        MemoryStatus status = process.readVariable(value.name, resolved);
        if (status == MemoryStatus::PAGE_FAULT) return false;  // restart
        if (status == MemoryStatus::VIOLATION)  return true;
    }

    MemoryStatus status = process.writeMemory(address, resolved);
    if (status == MemoryStatus::PAGE_FAULT) return false;
    if (status == MemoryStatus::VIOLATION)  return true;

    process.logPrint(coreId, "WRITE: " + toHexAddress(address) + " = " + std::to_string(resolved));
    return true;
}
