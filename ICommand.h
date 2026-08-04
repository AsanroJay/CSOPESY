#pragma once

class Process;  // forward declaration (a command executes against its owning process)


class ICommand {
public:
    enum CommandType {
        PRINT,
        DECLARE,
        ADD,
        SUBTRACT,
        SLEEP,
        FOR,
        READ,
        WRITE
    };

    explicit ICommand(CommandType type) : commandType(type) {}
    virtual ~ICommand() = default;

    CommandType getCommandType() const { return commandType; }

    // Executes this instruction on the given CPU core.
    // Returns true when the instruction is complete and the process can
    // advance to the next top-level command. Returning false means the
    // instruction took a page fault and must be restarted on the next tick.
    virtual bool execute(int coreId, Process& process) = 0;

protected:
    CommandType commandType;
};
