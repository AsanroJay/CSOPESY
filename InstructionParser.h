#pragma once

#include <memory>
#include <string>
#include <vector>

#include "ICommand.h"

// Parses the semicolon-separated instruction string accepted by "screen -c".
//
// Supported forms (keywords are case-insensitive):
//   DECLARE <var> <value>
//   ADD <dest> <lhs> <rhs>
//   SUBTRACT <dest> <lhs> <rhs>
//   PRINT("text")  |  PRINT("text" + var)  |  PRINT(var)
//   SLEEP <ticks>
//   READ <var> <address>
//   WRITE <address> <value|var>
//
// Addresses are hexadecimal (0x500); plain decimal is also accepted. Returns
// false if any instruction fails to parse or the count is outside [1, 50], in
// which case the caller reports "invalid command".
namespace InstructionParser {
    constexpr size_t MIN_INSTRUCTIONS = 1;
    constexpr size_t MAX_INSTRUCTIONS = 50;

    bool parse(const std::string& text,
               std::vector<std::shared_ptr<ICommand>>& outCommands,
               std::string& outError);
}
