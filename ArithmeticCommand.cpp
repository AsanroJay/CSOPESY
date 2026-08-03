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

// Resolves one operand. Variable operands read from the symbol table segment,
// which can page-fault; `status` reports that back to the caller.
static uint16_t resolveOperand(const ArithmeticCommand::Operand& operand,
                               Process& process,
                               MemoryStatus& status) {
    if (operand.mode == ArithmeticCommand::VARIABLE) {
        uint16_t value = 0;
        status = process.readVariable(operand.name, value);
        return value;
    }
    status = MemoryStatus::OK;
    return operand.literalValue;
}

bool ArithmeticCommand::execute(int coreId, Process& process) {
    MemoryStatus status = MemoryStatus::OK;

    // Nothing is written until both operands resolve, so restarting after a
    // fault simply re-reads them -- the instruction stays idempotent.
    uint32_t leftValue = resolveOperand(leftOperand, process, status);
    if (status == MemoryStatus::PAGE_FAULT) return false;
    if (status == MemoryStatus::VIOLATION)  return true;

    uint32_t rightValue = resolveOperand(rightOperand, process, status);
    if (status == MemoryStatus::PAGE_FAULT) return false;
    if (status == MemoryStatus::VIOLATION)  return true;

    uint32_t result = 0;
    if (commandType == ICommand::ADD) {
        result = leftValue + rightValue;
    } else {
        // Underflow protection: uint16 values are clamped to [0, 65535].
        result = leftValue > rightValue ? leftValue - rightValue : 0;
    }

    status = process.writeVariable(destination, result);
    if (status == MemoryStatus::PAGE_FAULT) return false;
    if (status == MemoryStatus::VIOLATION)  return true;

    std::string opName = (commandType == ICommand::ADD) ? "ADD" : "SUBTRACT";

    if (status == MemoryStatus::IGNORED) {
        process.logPrint(coreId, opName + ": " + destination + " ignored (symbol table full)");
        return true;
    }

    auto operandToString = [&](const ArithmeticCommand::Operand& op, uint16_t val) {
        if (op.mode == ArithmeticCommand::VARIABLE) return op.name + "(" + std::to_string(val) + ")";
        return std::to_string(val);
    };

    std::string sign     = (commandType == ICommand::ADD) ? "+" : "-";
    std::string leftStr  = operandToString(leftOperand, static_cast<uint16_t>(leftValue));
    std::string rightStr = operandToString(rightOperand, static_cast<uint16_t>(rightValue));

    uint16_t clamped = static_cast<uint16_t>(result > 65535u ? 65535u : result);

    // Format: ADD: x = y(10) + z(5) -> 15
    process.logPrint(coreId, opName + ": " + destination + " = " + leftStr + " " + sign + " " +
                                 rightStr + " -> " + std::to_string(clamped));
    return true;
}
