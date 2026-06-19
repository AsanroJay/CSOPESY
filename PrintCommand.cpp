#include "PrintCommand.h"

#include "Process.h"

PrintCommand::PrintCommand(const std::string& toPrint)
    : ICommand(ICommand::PRINT), toPrint(toPrint) {}

void PrintCommand::execute(int coreId, Process& process) {
    // The process owns the output file + timestamp formatting; keep the command thin.
    process.logPrint(coreId, toPrint);
}
