#include "DeclareCommand.h"
#include "Process.h"

DeclareCommand::DeclareCommand(const std::string& variableName, uint16_t initialValue)
    : ICommand(ICommand::DECLARE),
      variableName(variableName),
      initialValue(initialValue) {}

bool DeclareCommand::execute(int coreId, Process& process) {
    // --- DECLARE: create a process-local uint16 variable
    process.declareVariable(variableName, initialValue);
    process.logPrint(coreId, "DECLARE: " + variableName + " = " + std::to_string(initialValue));
    return true;
}
