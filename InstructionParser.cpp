#include "InstructionParser.h"

#include <algorithm>
#include <cctype>
#include <cstdint>
#include <sstream>

#include "ArithmeticCommand.h"
#include "DeclareCommand.h"
#include "PrintCommand.h"
#include "ReadCommand.h"
#include "SleepCommand.h"
#include "WriteCommand.h"

namespace {

std::string trim(const std::string& text) {
    size_t first = text.find_first_not_of(" \t\r\n");
    if (first == std::string::npos) return "";
    size_t last = text.find_last_not_of(" \t\r\n");
    return text.substr(first, last - first + 1);
}

std::string toUpper(const std::string& text) {
    std::string result = text;
    std::transform(result.begin(), result.end(), result.begin(),
                   [](unsigned char c) { return static_cast<char>(std::toupper(c)); });
    return result;
}

// The spec writes escaped quotes inside the instruction string, e.g.
// PRINT(\"Result: \" + varC). Drop the backslashes so quotes are plain.
std::string unescapeQuotes(const std::string& text) {
    std::string result;
    result.reserve(text.size());
    for (size_t i = 0; i < text.size(); ++i) {
        if (text[i] == '\\' && i + 1 < text.size() && text[i + 1] == '"') continue;
        result.push_back(text[i]);
    }
    return result;
}

// Splits on ';' but ignores separators that sit inside a quoted string.
std::vector<std::string> splitInstructions(const std::string& text) {
    std::vector<std::string> parts;
    std::string current;
    bool inQuotes = false;

    for (char c : text) {
        if (c == '"') {
            inQuotes = !inQuotes;
            current.push_back(c);
        } else if (c == ';' && !inQuotes) {
            parts.push_back(current);
            current.clear();
        } else {
            current.push_back(c);
        }
    }
    parts.push_back(current);

    std::vector<std::string> cleaned;
    for (const std::string& part : parts) {
        std::string trimmed = trim(part);
        if (!trimmed.empty()) cleaned.push_back(trimmed);
    }
    return cleaned;
}

bool parseNumber(const std::string& token, uint64_t& outValue) {
    if (token.empty()) return false;

    int base = 10;
    size_t start = 0;
    if (token.size() > 2 && token[0] == '0' && (token[1] == 'x' || token[1] == 'X')) {
        base  = 16;
        start = 2;
    }

    uint64_t value = 0;
    for (size_t i = start; i < token.size(); ++i) {
        int digit;
        char c = token[i];
        if (c >= '0' && c <= '9')      digit = c - '0';
        else if (c >= 'a' && c <= 'f') digit = c - 'a' + 10;
        else if (c >= 'A' && c <= 'F') digit = c - 'A' + 10;
        else return false;

        if (digit >= base) return false;
        value = value * base + static_cast<uint64_t>(digit);
        if (value > 0xFFFFFFFFull) return false;  // far past any legal address
    }

    outValue = value;
    return true;
}

bool parseUint16(const std::string& token, uint16_t& outValue) {
    uint64_t raw = 0;
    if (!parseNumber(token, raw)) return false;
    outValue = static_cast<uint16_t>(std::min<uint64_t>(raw, 65535));
    return true;
}

bool isValidIdentifier(const std::string& token) {
    if (token.empty()) return false;
    if (!std::isalpha(static_cast<unsigned char>(token[0])) && token[0] != '_') return false;
    for (char c : token) {
        if (!std::isalnum(static_cast<unsigned char>(c)) && c != '_') return false;
    }
    return true;
}

ArithmeticCommand::Operand makeArithmeticOperand(const std::string& token, bool& ok) {
    ArithmeticCommand::Operand operand{};
    uint16_t literal = 0;

    if (isValidIdentifier(token)) {
        operand.mode = ArithmeticCommand::VARIABLE;
        operand.name = token;
        operand.literalValue = 0;
        ok = true;
    } else if (parseUint16(token, literal)) {
        operand.mode = ArithmeticCommand::LITERAL;
        operand.literalValue = literal;
        ok = true;
    } else {
        ok = false;
    }
    return operand;
}

// PRINT("text"), PRINT("text" + var), or PRINT(var).
std::shared_ptr<ICommand> parsePrint(const std::string& instruction, std::string& error) {
    size_t open  = instruction.find('(');
    size_t close = instruction.rfind(')');
    if (open == std::string::npos || close == std::string::npos || close < open) {
        error = "PRINT requires parentheses";
        return nullptr;
    }

    std::string inner = trim(instruction.substr(open + 1, close - open - 1));
    if (inner.empty()) return std::make_shared<PrintCommand>("");

    if (inner.front() == '"') {
        size_t closingQuote = inner.find('"', 1);
        if (closingQuote == std::string::npos) {
            error = "PRINT has an unterminated string";
            return nullptr;
        }

        std::string prefix = inner.substr(1, closingQuote - 1);
        std::string rest   = trim(inner.substr(closingQuote + 1));
        if (rest.empty()) return std::make_shared<PrintCommand>(prefix);

        if (rest.front() != '+') {
            error = "PRINT expects '+' between the string and a variable";
            return nullptr;
        }

        std::string variableName = trim(rest.substr(1));
        if (!isValidIdentifier(variableName)) {
            error = "PRINT refers to an invalid variable name";
            return nullptr;
        }
        return std::make_shared<PrintCommand>(prefix, variableName);
    }

    if (!isValidIdentifier(inner)) {
        error = "PRINT refers to an invalid variable name";
        return nullptr;
    }
    return std::make_shared<PrintCommand>("", inner);
}

std::shared_ptr<ICommand> parseOne(const std::string& instruction, std::string& error) {
    // PRINT carries parentheses, so key off the text before '(' when present.
    size_t keywordEnd = instruction.find_first_of(" \t(");
    std::string keyword = toUpper(keywordEnd == std::string::npos ? instruction
                                                                 : instruction.substr(0, keywordEnd));

    if (keyword == "PRINT") {
        return parsePrint(instruction, error);
    }

    std::istringstream iss(instruction.substr(keyword.size()));
    std::vector<std::string> tokens;
    std::string token;
    while (iss >> token) tokens.push_back(token);

    if (keyword == "DECLARE") {
        uint16_t value = 0;
        if (tokens.size() != 2 || !isValidIdentifier(tokens[0]) || !parseUint16(tokens[1], value)) {
            error = "DECLARE expects <var> <value>";
            return nullptr;
        }
        return std::make_shared<DeclareCommand>(tokens[0], value);
    }

    if (keyword == "ADD" || keyword == "SUBTRACT") {
        if (tokens.size() != 3 || !isValidIdentifier(tokens[0])) {
            error = keyword + " expects <dest> <lhs> <rhs>";
            return nullptr;
        }

        bool leftOk = false, rightOk = false;
        ArithmeticCommand::Operand left  = makeArithmeticOperand(tokens[1], leftOk);
        ArithmeticCommand::Operand right = makeArithmeticOperand(tokens[2], rightOk);
        if (!leftOk || !rightOk) {
            error = keyword + " has an invalid operand";
            return nullptr;
        }

        ICommand::CommandType type = (keyword == "ADD") ? ICommand::ADD : ICommand::SUBTRACT;
        return std::make_shared<ArithmeticCommand>(type, tokens[0], left, right);
    }

    if (keyword == "SLEEP") {
        uint16_t ticks = 0;
        if (tokens.size() != 1 || !parseUint16(tokens[0], ticks)) {
            error = "SLEEP expects <ticks>";
            return nullptr;
        }
        return std::make_shared<SleepCommand>(static_cast<int>(ticks));
    }

    if (keyword == "READ") {
        uint64_t address = 0;
        if (tokens.size() != 2 || !isValidIdentifier(tokens[0]) || !parseNumber(tokens[1], address)) {
            error = "READ expects <var> <address>";
            return nullptr;
        }
        return std::make_shared<ReadCommand>(tokens[0], static_cast<size_t>(address));
    }

    if (keyword == "WRITE") {
        uint64_t address = 0;
        if (tokens.size() != 2 || !parseNumber(tokens[0], address)) {
            error = "WRITE expects <address> <value|var>";
            return nullptr;
        }

        WriteCommand::Operand operand{};
        uint16_t literal = 0;
        if (isValidIdentifier(tokens[1])) {
            operand.isVariable = true;
            operand.name       = tokens[1];
        } else if (parseUint16(tokens[1], literal)) {
            operand.isVariable  = false;
            operand.literalValue = literal;
        } else {
            error = "WRITE has an invalid value";
            return nullptr;
        }
        return std::make_shared<WriteCommand>(static_cast<size_t>(address), operand);
    }

    error = "unknown instruction \"" + keyword + "\"";
    return nullptr;
}

}  // namespace

bool InstructionParser::parse(const std::string& text,
                              std::vector<std::shared_ptr<ICommand>>& outCommands,
                              std::string& outError) {
    outCommands.clear();
    outError.clear();

    std::vector<std::string> instructions = splitInstructions(unescapeQuotes(text));

    if (instructions.size() < MIN_INSTRUCTIONS || instructions.size() > MAX_INSTRUCTIONS) {
        outError = "instruction count must be between 1 and 50";
        return false;
    }

    for (const std::string& instruction : instructions) {
        std::shared_ptr<ICommand> command = parseOne(instruction, outError);
        if (command == nullptr) {
            if (outError.empty()) outError = "could not parse \"" + instruction + "\"";
            outCommands.clear();
            return false;
        }
        outCommands.push_back(command);
    }

    return true;
}
