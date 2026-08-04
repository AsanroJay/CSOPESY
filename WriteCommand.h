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

    // The operand and the target address can sit on different pages. When
    // physical memory holds fewer frames than that, restarting the whole
    // instruction after every fault would oscillate forever -- each retry
    // evicting the page the previous one just loaded. Holding the resolved
    // operand across retries lets the instruction advance one page at a time.
    // Cleared once the write lands, so a FOR loop re-resolves on its next pass.
    bool     valueCached = false;
    uint16_t cachedValue = 0;
};
