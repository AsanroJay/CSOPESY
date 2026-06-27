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
    uint32_t leftValue = resolveOperand(leftOperand, process);
    uint32_t rightValue = resolveOperand(rightOperand, process);
    uint32_t result = 0;

    if (commandType == ICommand::ADD) {
        result = leftValue + rightValue;
    } else {
        result = leftValue > rightValue ? leftValue - rightValue : 0;
    }

    process.setVariable(destination, result);
    // --- ARITHMETIC: log the operation performed
    auto operandToString = [&](const ArithmeticCommand::Operand& op, uint16_t val) {
        if (op.mode == ArithmeticCommand::VARIABLE) return op.name + "(" + std::to_string(val) + ")";
        return std::to_string(val);
    };

    std::string opName = (commandType == ICommand::ADD) ? "ADD" : "SUBTRACT";
    std::string leftStr = operandToString(leftOperand, static_cast<uint16_t>(leftValue));
    std::string rightStr = operandToString(rightOperand, static_cast<uint16_t>(rightValue));
    std::string message = opName + ": " + destination + " = " + leftStr + " " + ((opName=="ADD")?"+":"-") + " " + rightStr + " -> " + std::to_string(result);
    process.logPrint(coreId, message);
    return true;
}
