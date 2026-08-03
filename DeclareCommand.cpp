#include "DeclareCommand.h"

#include "Process.h"

DeclareCommand::DeclareCommand(const std::string& variableName, uint16_t initialValue)
    : ICommand(ICommand::DECLARE),
      variableName(variableName),
      initialValue(initialValue) {}

bool DeclareCommand::execute(int coreId, Process& process) {
    // The variable lives in the symbol table segment, so this can page-fault
    // exactly like any other memory access.
    MemoryStatus status = process.declareVariable(variableName, initialValue);

    if (status == MemoryStatus::PAGE_FAULT) return false;  // restart the instruction
    if (status == MemoryStatus::VIOLATION)  return true;

    if (status == MemoryStatus::IGNORED) {
        // 32-variable limit reached: the spec says to ignore the declaration.
        process.logPrint(coreId, "DECLARE: " + variableName + " ignored (symbol table full)");
        return true;
    }

    process.logPrint(coreId, "DECLARE: " + variableName + " = " + std::to_string(initialValue));
    return true;
}
