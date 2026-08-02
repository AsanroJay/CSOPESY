#pragma once

#include "ICommand.h"

// SLEEP(X) blocks the process for X CPU ticks.
class SleepCommand : public ICommand {
public:
    explicit SleepCommand(int ticks);
    bool execute(int coreId, Process& process) override;

private:
    int ticks;
};
