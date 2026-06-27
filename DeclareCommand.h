#pragma once

#include <string>
#include "ICommand.h"
#include <cstdint>

// DECLARE(var, value) creates a uint16 variable in the process memory.
class DeclareCommand : public ICommand {
public:
    DeclareCommand(const std::string& variableName, uint16_t initialValue);
    bool execute(int coreId, Process& process) override;

private:
    std::string variableName;
    uint16_t initialValue;
};
