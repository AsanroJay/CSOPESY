#pragma once

#include <cstddef>
#include <cstdint>
#include <string>

#include "ICommand.h"

// READ(var, address) retrieves a uint16 from the process's memory and stores it
// in `var`. Memory that was never written reads back as 0.
class ReadCommand : public ICommand {
public:
    ReadCommand(const std::string& variableName, size_t address);

    bool execute(int coreId, Process& process) override;

private:
    std::string variableName;
    size_t      address;
};
