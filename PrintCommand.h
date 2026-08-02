#pragma once

#include <string>

#include "ICommand.h"

// A "print" instruction. When executed it writes a single timestamped,
// core-tagged line to the owning process's log file.
class PrintCommand : public ICommand {
public:
    explicit PrintCommand(const std::string& messagePrefix,
                          const std::string& variableName = "");

    bool execute(int coreId, Process& process) override;

private:
    std::string messagePrefix;
    std::string variableName;
};
