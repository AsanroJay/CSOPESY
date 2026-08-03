#pragma once

#include <cstddef>
#include <cstdint>
#include <string>

#include "ICommand.h"

// WRITE(address, value) stores a uint16 at the given address. The value may be
// a literal or a variable, e.g. "WRITE 0x500 42" or "WRITE 0x500 varA".
class WriteCommand : public ICommand {
public:
    struct Operand {
        bool        isVariable = false;
        std::string name;
        uint16_t    literalValue = 0;
    };

    WriteCommand(size_t address, Operand value);

    bool execute(int coreId, Process& process) override;

private:
    size_t  address;
    Operand value;
};
