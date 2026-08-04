#pragma once

#include <memory>
#include <string>
#include <vector>

#include "ICommand.h"

namespace InstructionParser {
    constexpr size_t MIN_INSTRUCTIONS = 1;
    constexpr size_t MAX_INSTRUCTIONS = 50;

    // `outHighestAddress`, when supplied, receives one past the last byte any
    // READ/WRITE in the program touches (0 if it touches none). That is the
    // smallest address space the program can run in, which is how a "screen -c"
    // given no explicit memory size gets sized.
    bool parse(const std::string& text,
               std::vector<std::shared_ptr<ICommand>>& outCommands,
               std::string& outError,
               size_t* outHighestAddress = nullptr);
}
