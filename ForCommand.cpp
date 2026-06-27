#include "ForCommand.h"
#include "Process.h"

ForCommand::ForCommand(std::vector<std::shared_ptr<ICommand>> innerCommands, int repeatCount)
    : ICommand(ICommand::FOR),
      innerCommands(std::move(innerCommands)),
      repeatCount(repeatCount),
      currentIteration(0),
      currentIndex(0) {}

bool ForCommand::execute(int coreId, Process& process) {
    if (innerCommands.empty() || repeatCount <= 0) {
        return true;
    }

    if (currentIteration >= repeatCount) {
        return true;
    }

    if (currentIndex >= static_cast<int>(innerCommands.size())) {
        currentIndex = 0;
        currentIteration += 1;
        if (currentIteration >= repeatCount) {
            return true;
        }
    }

    // --- FOR: log loop entry on first iteration
    if (currentIteration == 0 && currentIndex == 0) {
        process.logPrint(coreId, std::string("FOR: iterations=") + std::to_string(repeatCount));
    }

    bool finished = innerCommands[currentIndex]->execute(coreId, process);
    if (finished) {
        currentIndex += 1;
        if (currentIndex >= static_cast<int>(innerCommands.size())) {
            currentIndex = 0;
            currentIteration += 1;
        }
    }

    return currentIteration >= repeatCount;
}
