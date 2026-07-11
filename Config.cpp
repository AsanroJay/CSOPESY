#include "Config.h"

#include <fstream>
#include <iostream>
#include <sstream>
#include <string>

bool Config::loadFromFile(const std::string& path) {
    std::ifstream file(path);
    if (!file.is_open()) {
        std::cerr << "Error: could not open \"" << path << "\".\n";
        return false;
    }

    std::string line;
    while (std::getline(file, line)) {
        if (line.empty() || line[0] == '#') continue;  // skip blanks and comments

        std::istringstream iss(line);
        std::string key;
        if (!(iss >> key)) continue;

        if (key == "num-cpu") {
            iss >> Config::numCpu;
        } else if (key == "scheduler") {
            iss >> Config::scheduler;
            // Strip surrounding quotes if present (e.g. "fcfs" -> fcfs)
            auto& s = Config::scheduler;
            if (s.size() >= 2 && s.front() == '"' && s.back() == '"') {
                s = s.substr(1, s.size() - 2);
            }
        } else if (key == "quantum-cycles") {
            iss >> Config::quantumCycles;
        } else if (key == "batch-process-freq") {
            iss >> Config::batchProcessFreq;
        } else if (key == "min-ins") {
            iss >> Config::minIns;
        } else if (key == "max-ins") {
            iss >> Config::maxIns;
        } else if (key == "delay-per-exec") {
            iss >> Config::delayPerExec;
        } else if (key == "max-overall-mem") {
            iss >> Config::maxOverallMem;
        } else if (key == "mem-per-frame") {
            iss >> Config::memPerFrame;
        } else if (key == "mem-per-proc") {
            iss >> Config::memPerProc;
        } else {
            std::cerr << "Warning: unknown config key \"" << key << "\" (ignored).\n";
        }
    }

    Config::initialized = true;
    return true;
}