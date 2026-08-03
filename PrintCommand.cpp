#include "PrintCommand.h"

#include "Process.h"

PrintCommand::PrintCommand(const std::string& messagePrefix,
                             const std::string& variableName)
    : ICommand(ICommand::PRINT),
      messagePrefix(messagePrefix),
      variableName(variableName) {}

bool PrintCommand::execute(int coreId, Process& process) {
    std::string output = messagePrefix;

    if (!variableName.empty()) {
        uint16_t value = 0;
        MemoryStatus status = process.readVariable(variableName, value);
        if (status == MemoryStatus::PAGE_FAULT) return false;  // restart
        if (status == MemoryStatus::VIOLATION)  return true;

        output += std::to_string(value);
    }

    process.logPrint(coreId, output);
    return true;
}
