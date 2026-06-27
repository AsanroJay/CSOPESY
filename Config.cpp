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
        } else if (key == "instruction-mode") {
            iss >> Config::instructionMode;
            // Strip surrounding quotes if present (e.g. "alternating" -> alternating)
            auto& m = Config::instructionMode;
            if (m.size() >= 2 && m.front() == '"' && m.back() == '"') {
                m = m.substr(1, m.size() - 2);
            }
        } else {
            std::cerr << "Warning: unknown config key \"" << key << "\" (ignored).\n";
        }
    }

    // Validate parameters against the spec's accepted ranges before committing.
    if (Config::numCpu < 1 || Config::numCpu > 128) {
        std::cerr << "Error: num-cpu must be in [1, 128] (got " << Config::numCpu << ").\n";
        return false;
    }
    if (Config::scheduler != "fcfs" && Config::scheduler != "rr") {
        std::cerr << "Error: scheduler must be \"fcfs\" or \"rr\" (got \"" << Config::scheduler << "\").\n";
        return false;
    }
    if (Config::quantumCycles < 1) {
        std::cerr << "Error: quantum-cycles must be >= 1 (got " << Config::quantumCycles << ").\n";
        return false;
    }
    if (Config::batchProcessFreq < 1) {
        std::cerr << "Error: batch-process-freq must be >= 1 (got " << Config::batchProcessFreq << ").\n";
        return false;
    }
    if (Config::minIns < 1 || Config::maxIns < 1 || Config::minIns > Config::maxIns) {
        std::cerr << "Error: require 1 <= min-ins <= max-ins (got min-ins "
                  << Config::minIns << ", max-ins " << Config::maxIns << ").\n";
        return false;
    }
    if (Config::delayPerExec < 0) {
        std::cerr << "Error: delay-per-exec must be >= 0 (got " << Config::delayPerExec << ").\n";
        return false;
    }
    if (Config::instructionMode != "random" && Config::instructionMode != "alternating") {
        std::cerr << "Error: instruction-mode must be \"random\" or \"alternating\" (got \""
                  << Config::instructionMode << "\").\n";
        return false;
    }

    Config::initialized = true;
    return true;
}