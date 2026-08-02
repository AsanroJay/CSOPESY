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
        uint16_t value = process.getVariable(variableName);
        output += std::to_string(value);
    }

    process.logPrint(coreId, output);
    return true;
}
