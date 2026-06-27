#include "SleepCommand.h"
#include "Process.h"

SleepCommand::SleepCommand(int ticks)
    : ICommand(ICommand::SLEEP),
      ticks(ticks) {}

bool SleepCommand::execute(int coreId, Process& process) {
    // --- SLEEP: initiate sleep on first encounter and tick down each cycle
    if (!process.isSleeping()) {
        process.sleepFor(ticks);
        process.logPrint(coreId, std::string("SLEEP: ") + std::to_string(ticks) + " ticks");
    }
    return process.tickSleep();
}
