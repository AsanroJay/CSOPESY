#pragma once

#include <string>
#include "ICommand.h"
#include <cstdint>

// ADD and SUBTRACT instructions operate on process-local uint16 variables.
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
