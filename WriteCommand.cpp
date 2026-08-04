#include "WriteCommand.h"

#include "Process.h"
#include "Utils.h"

WriteCommand::WriteCommand(size_t address, Operand value)
    : ICommand(ICommand::WRITE),
      address(address),
      value(std::move(value)) {}

bool WriteCommand::execute(int coreId, Process& process) {
    // Stage 1: resolve the operand. Reading the source variable can itself
    // fault, since variables live in the symbol table segment.
    if (!valueCached) {
        uint16_t resolved = value.literalValue;
        if (value.isVariable) {
            MemoryStatus status = process.readVariable(value.name, resolved);
            if (status == MemoryStatus::PAGE_FAULT) return false;  // restart
            if (status == MemoryStatus::VIOLATION)  return true;
        }
        cachedValue = resolved;
        valueCached = true;
    }

    // Stage 2: store it. The symbol table page may well have been evicted to
    // make room for this one -- that is fine, the value is already in hand.
    MemoryStatus status = process.writeMemory(address, cachedValue);
    if (status == MemoryStatus::PAGE_FAULT) return false;

    uint16_t written = cachedValue;
    valueCached = false;
    if (status == MemoryStatus::VIOLATION)  return true;

    process.logPrint(coreId, "WRITE: " + toHexAddress(address) + " = " + std::to_string(written));
    return true;
}
