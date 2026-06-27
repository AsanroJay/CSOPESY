#pragma once

#include <memory>
#include <string>
#include <vector>

#include "ICommand.h"

// FOR([instructions], repeats) executes the nested instruction block multiple times.
class ForCommand : public ICommand {
public:
    ForCommand(std::vector<std::shared_ptr<ICommand>> innerCommands, int repeatCount);
    bool execute(int coreId, Process& process) override;

private:
    std::vector<std::shared_ptr<ICommand>> innerCommands;
    int repeatCount;
    int currentIteration;
    int currentIndex;
};
