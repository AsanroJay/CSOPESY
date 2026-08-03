#pragma once
#include <cstdint>
#include <string>

namespace Config {
    // Populated by loadFromFile();
    inline bool initialized = false;

    // Config values 
    inline int         numCpu            = 4;
    inline std::string scheduler         = "fcfs";
    inline uint64_t    quantumCycles     = 5;
    inline uint64_t    batchProcessFreq  = 1;
    inline uint64_t    minIns            = 1000;
    inline uint64_t    maxIns            = 2000;
    inline uint64_t    delayPerExec      = 0;

    // Memory manager parameters (first-fit flat allocator).
    inline int         maxOverallMem     = 16384;  // total bytes of main memory
    inline int         memPerFrame       = 16;     // bytes per frame
    inline size_t      minMemPerProc     = 64;     // default minimum
    inline size_t      maxMemPerProc     = 4096;   // default maximum

    // Legacy aliases so existing Scheduler/Process code compiles unchanged
    inline int& NUM_CORES             = numCpu;
    inline uint64_t& PRINTS_PER_PROCESS = maxIns;   // temporary; replace later
    inline uint64_t& PER_INSTRUCTION_DELAY_MS = delayPerExec;
    inline bool WRITE_PRINT_FILES     = true;

    // Reads config.txt from the working directory.
    // Returns true on success, prints an error and returns false on failure.
    bool loadFromFile(const std::string& path = "config.txt");
}