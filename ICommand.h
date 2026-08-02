#pragma once

class Process;  // forward declaration (a command executes against its owning process)

// Interface implemented by every process instruction. For this homework the only
// instruction is PRINT, but the same interface will later carry ADD, SUBTRACT,
// SLEEP, FOR, etc. for the full machine project.
class ICommand {
public:
    enum CommandType {
        PRINT,
        DECLARE,
        ADD,
        SUBTRACT,
        SLEEP,
        FOR
    };

    explicit ICommand(CommandType type) : commandType(type) {}
    virtual ~ICommand() = default;

    CommandType getCommandType() const { return commandType; }

    // Executes this instruction on the given CPU core.
    // Returns true when the instruction is complete and the process can
    // advance to the next top-level command.
    virtual bool execute(int coreId, Process& process) = 0;

protected:
    CommandType commandType;
};
