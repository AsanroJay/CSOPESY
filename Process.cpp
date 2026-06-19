#include "Process.h"

#include "Config.h"
#include "Utils.h"

Process::Process(int pid, const std::string& name)
    : pid(pid),
      name(name),
      createdAt(currentTimestamp()),
      commandCounter(0),
      coreId(-1),
      currentState(READY),
      fileOpened(false) {}

void Process::addCommand(std::shared_ptr<ICommand> command) {
    commandList.push_back(std::move(command));
}

void Process::executeCurrentCommand(int coreId) {
    if (isFinished()) {
        return;
    }
    int index = commandCounter.load();
    commandList[index]->execute(coreId, *this);
    commandCounter.fetch_add(1);
}

void Process::finishExecution() {
    currentState.store(FINISHED);
    if (fileOpened) {
        outFile.close();
        fileOpened = false;
    }
}

bool Process::isFinished() const {
    return commandCounter.load() >= static_cast<int>(commandList.size());
}

void Process::logPrint(int coreId, const std::string& message) {
    if (!Config::WRITE_PRINT_FILES) {
        return;
    }
    if (!fileOpened) {
        outFile.open(name + ".txt");
        outFile << "Process name: " << name << "\n";
        outFile << "Logs:\n\n";
        fileOpened = true;
    }
    outFile << "(" << currentTimestamp() << ") Core:" << coreId
            << " \"" << message << "\"\n";
}

void Process::setState(ProcessState state) {
    currentState.store(state);
}

void Process::setCoreId(int coreId) {
    this->coreId.store(coreId);
}

int Process::getPID() const {
    return pid;
}

std::string Process::getName() const {
    return name;
}

std::string Process::getCreatedAt() const {
    return createdAt;
}

Process::ProcessState Process::getState() const {
    return currentState.load();
}

int Process::getCoreId() const {
    return coreId.load();
}

int Process::getCurrentLine() const {
    return commandCounter.load();
}

int Process::getTotalLines() const {
    return static_cast<int>(commandList.size());
}
