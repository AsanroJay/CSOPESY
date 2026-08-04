#pragma once

#include <cstddef>
#include <cstdint>
#include <string>

#include "ICommand.h"

class ReadCommand : public ICommand {
public:
    ReadCommand(const std::string& variableName, size_t address);

    bool execute(int coreId, Process& process) override;

private:
    std::string variableName;
    size_t      address;

    // The source address and the destination variable can sit on different
    // pages; see WriteCommand for why the fetched value is held across retries.
    bool     valueCached = false;
    uint16_t cachedValue = 0;
};
