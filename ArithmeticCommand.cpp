#include "ArithmeticCommand.h"
#include "Process.h"

ArithmeticCommand::ArithmeticCommand(CommandType type,
                                     const std::string& destination,
                                     Operand left,
                                     Operand right)
    : ICommand(type),
      destination(destination),
      leftOperand(std::move(left)),
      rightOperand(std::move(right)) {}

static uint16_t resolveOperand(const ArithmeticCommand::Operand& operand, Process& process) {
    if (operand.mode == ArithmeticCommand::VARIABLE) {
        return process.getVariable(operand.name);
    }
    return operand.literalValue;
}

bool ArithmeticCommand::execute(int coreId, Process& process) {
    // 1. Resolve both operands (implicitly handles fallback initialization to 0 if variable is new)
    uint32_t leftValue  = resolveOperand(leftOperand, process);
    uint32_t rightValue = resolveOperand(rightOperand, process);
    uint32_t result     = 0;

    // 2. Perform operation
    if (commandType == ICommand::ADD) {
        result = leftValue + rightValue;
    } else {
        // Underflow protection: specification implies clamping between (0, max(uint16))
        result = leftValue > rightValue ? leftValue - rightValue : 0;
    }

    // 3. Save result to destination (setVariable automatically handles uint16 bounds clamping)
    process.setVariable(destination, result);

    // 4. Construct log layout
    auto operandToString = [&](const ArithmeticCommand::Operand& op, uint16_t val) {
        if (op.mode == ArithmeticCommand::VARIABLE) return op.name + "(" + std::to_string(val) + ")";
        return std::to_string(val);
    };

    std::string opName   = (commandType == ICommand::ADD) ? "ADD" : "SUBTRACT";
    std::string sign     = (commandType == ICommand::ADD) ? "+" : "-";
    std::string leftStr  = operandToString(leftOperand, static_cast<uint16_t>(leftValue));
    std::string rightStr = operandToString(rightOperand, static_cast<uint16_t>(rightValue));
    
    // Format: ADD: x = y(10) + z(5) -> 15
    std::string message = opName + ": " + destination + " = " + leftStr + " " + sign + " " + rightStr + " -> " + std::to_string(process.getVariable(destination));
    process.logPrint(coreId, message);
    
    return true;
}