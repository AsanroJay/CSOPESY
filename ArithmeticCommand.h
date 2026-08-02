#pragma once

#include <string>
#include "ICommand.h"
#include <cstdint>

// ADD and SUBTRACT instructions operate on 3 operands:
// commandType (destination, left, right) -> destination = left op right
class ArithmeticCommand : public ICommand {
public:
    enum OperandMode {
        VARIABLE,
        LITERAL
    };

    struct Operand {
        OperandMode mode;
        std::string name;
        uint16_t literalValue;
    };

    // Updated constructor to explicitly separate destination from left and right operands
    ArithmeticCommand(CommandType type,
                      const std::string& destination,
                      Operand left,
                      Operand right);

    bool execute(int coreId, Process& process) override;

private:
    std::string destination;
    Operand leftOperand;
    Operand rightOperand;
};