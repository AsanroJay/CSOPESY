#include "SleepCommand.h"
#include "Process.h"

SleepCommand::SleepCommand(int ticks)
    : ICommand(ICommand::SLEEP),
      ticks(ticks) {}

bool SleepCommand::execute(int coreId, Process& process) {
    // Initiate sleep phase
    process.sleepFor(ticks);
    process.setState(Process::SLEEPING); // Change state so scheduler pulls it off the core
    process.logPrint(coreId, "SLEEP: " + std::to_string(ticks) + " ticks");
    
    // Return true to indicate this command has completed its execution turn
    return true; 
}