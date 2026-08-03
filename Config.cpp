#include "Config.h"

#include <algorithm>
#include <cstdint>
#include <fstream>
#include <iostream>
#include <sstream>
#include <string>

namespace {
    template <typename T>
    T clampToRange(T value, T minValue, T maxValue) {
        return std::max(minValue, std::min(maxValue, value));
    }

    int clampToPowerOfTwoRange(int value) {
        constexpr int minValue = 64;
        constexpr int maxValue = 65536;

        if (value < minValue) return minValue;
        if (value > maxValue) return maxValue;
        if ((value & (value - 1)) == 0) return value;

        int roundedUp = 1;
        while (roundedUp < value) {
            roundedUp <<= 1;
        }

        return roundedUp;
    }
}

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
            int parsedValue = 0;
            if (iss >> parsedValue) {
                const int clampedValue = clampToRange(parsedValue, 1, 128);
                if (clampedValue != parsedValue) {
                    std::cerr << "Warning: \"num-cpu\" must be between 1 and 128; using " << clampedValue << " instead.\n";
                }
                Config::numCpu = clampedValue;
            }
        } else if (key == "scheduler") {
            iss >> Config::scheduler;
            // Strip surrounding quotes if present (e.g. "fcfs" -> fcfs)
            auto& s = Config::scheduler;
            if (s.size() >= 2 && s.front() == '"' && s.back() == '"') {
                s = s.substr(1, s.size() - 2);
            }
        } else if (key == "quantum-cycles") {
            uint64_t parsedValue = 0;
            if (iss >> parsedValue) {
                const uint64_t clampedValue = clampToRange(parsedValue, uint64_t{1}, uint64_t{4294967296});
                if (clampedValue != parsedValue) {
                    std::cerr << "Warning: \"quantum-cycles\" must be between 1 and 2^32; using " << clampedValue << " instead.\n";
                }
                Config::quantumCycles = clampedValue;
            }
        } else if (key == "batch-process-freq") {
            uint64_t parsedValue = 0;
            if (iss >> parsedValue) {
                const uint64_t clampedValue = clampToRange(parsedValue, uint64_t{1}, uint64_t{4294967296});
                if (clampedValue != parsedValue) {
                    std::cerr << "Warning: \"batch-process-freq\" must be between 1 and 2^32; using " << clampedValue << " instead.\n";
                }
                Config::batchProcessFreq = clampedValue;
            }
        } else if (key == "min-ins") {
            uint64_t parsedValue = 0;
            if (iss >> parsedValue) {
                const uint64_t clampedValue = clampToRange(parsedValue, uint64_t{1}, uint64_t{4294967296});
                if (clampedValue != parsedValue) {
                    std::cerr << "Warning: \"min-ins\" must be between 1 and 2^32; using " << clampedValue << " instead.\n";
                }
                Config::minIns = clampedValue;
            }
        } else if (key == "max-ins") {
            uint64_t parsedValue = 0;
            if (iss >> parsedValue) {
                const uint64_t clampedValue = clampToRange(parsedValue, uint64_t{1}, uint64_t{4294967296});
                if (clampedValue != parsedValue) {
                    std::cerr << "Warning: \"max-ins\" must be between 1 and 2^32; using " << clampedValue << " instead.\n";
                }
                Config::maxIns = clampedValue;
            }
        } else if (key == "delay-per-exec" || key == "delays-per-exec") {
            uint64_t parsedValue = 0;
            if (iss >> parsedValue) {
                const uint64_t clampedValue = clampToRange(parsedValue, uint64_t{0}, uint64_t{4294967296});
                if (clampedValue != parsedValue) {
                    std::cerr << "Warning: \"delay-per-exec\" must be between 0 and 2^32; using " << clampedValue << " instead.\n";
                }
                Config::delayPerExec = clampedValue;
            }
        } else if (key == "max-overall-mem") {
            int parsedValue = 0;
            if (iss >> parsedValue) {
                const int normalizedValue = clampToPowerOfTwoRange(parsedValue);
                if (normalizedValue != parsedValue) {
                    std::cerr << "Warning: \"max-overall-mem\" must be a power of 2 between 64 and 65536 bytes; using " << normalizedValue << " instead.\n";
                }
                Config::maxOverallMem = normalizedValue;
            }
        } else if (key == "mem-per-frame") {
            int parsedValue = 0;
            if (iss >> parsedValue) {
                const int normalizedValue = clampToPowerOfTwoRange(parsedValue);
                if (normalizedValue != parsedValue) {
                    std::cerr << "Warning: \"mem-per-frame\" must be a power of 2 between 64 and 65536 bytes; using " << normalizedValue << " instead.\n";
                }
                Config::memPerFrame = normalizedValue;
            }
        } else if (key == "min-mem-per-proc") {
            int parsedValue = 0;
            if (iss >> parsedValue) {
                const int normalizedValue = clampToPowerOfTwoRange(parsedValue);
                if (normalizedValue != parsedValue) {
                    std::cerr << "Warning: \"min-mem-per-proc\" must be a power of 2 between 64 and 65536 bytes; using " << normalizedValue << " instead.\n";
                }
                Config::minMemPerProc = static_cast<size_t>(normalizedValue);
            }
        } else if (key == "max-mem-per-proc") {
            int parsedValue = 0;
            if (iss >> parsedValue) {
                const int normalizedValue = clampToPowerOfTwoRange(parsedValue);
                if (normalizedValue != parsedValue) {
                    std::cerr << "Warning: \"max-mem-per-proc\" must be a power of 2 between 64 and 65536 bytes; using " << normalizedValue << " instead.\n";
                }
                Config::maxMemPerProc = static_cast<size_t>(normalizedValue);
            }
        } else {
            std::cerr << "Warning: unknown config key \"" << key << "\" (ignored).\n";
        }
    }

    // Cross-parameter sanity checks. These matter because the page arithmetic
    // (page = address / mem-per-frame, frames = max-overall-mem / mem-per-frame)
    // silently produces nonsense when the values are inconsistent.
    if (Config::memPerFrame > Config::maxOverallMem) {
        std::cerr << "Warning: \"mem-per-frame\" (" << Config::memPerFrame
                  << ") exceeds \"max-overall-mem\" (" << Config::maxOverallMem
                  << "); using " << Config::maxOverallMem << " instead.\n";
        Config::memPerFrame = Config::maxOverallMem;
    }
    // A process larger than all of physical memory could never become fully
    // resident, so it would sit in the ready queue forever. Cap it instead.
    if (Config::maxMemPerProc > static_cast<size_t>(Config::maxOverallMem)) {
        std::cerr << "Warning: \"max-mem-per-proc\" (" << Config::maxMemPerProc
                  << ") exceeds \"max-overall-mem\" (" << Config::maxOverallMem
                  << "); using " << Config::maxOverallMem << " instead.\n";
        Config::maxMemPerProc = static_cast<size_t>(Config::maxOverallMem);
    }
    if (Config::minMemPerProc > static_cast<size_t>(Config::maxOverallMem)) {
        Config::minMemPerProc = static_cast<size_t>(Config::maxOverallMem);
    }
    if (Config::minMemPerProc > Config::maxMemPerProc) {
        std::cerr << "Warning: \"min-mem-per-proc\" (" << Config::minMemPerProc
                  << ") exceeds \"max-mem-per-proc\" (" << Config::maxMemPerProc
                  << "); swapping them.\n";
        std::swap(Config::minMemPerProc, Config::maxMemPerProc);
    }

    Config::initialized = true;
    return true;
}