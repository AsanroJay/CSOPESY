#pragma once
#include <string>

namespace Config {
    // Populated by loadFromFile();
    inline bool initialized = false;

    // Config values 
    inline int         numCpu            = 4;
    inline std::string scheduler         = "fcfs";
    inline int         quantumCycles     = 5;
    inline int         batchProcessFreq  = 1;
    inline int         minIns            = 1000;
    inline int         maxIns            = 2000;
    inline int         delayPerExec      = 0;

    // Memory manager parameters (first-fit flat allocator).
    inline int         maxOverallMem     = 16384;  // total bytes of main memory
    inline int         memPerFrame       = 16;     // bytes per frame
    inline size_t      minMemPerProc     = 64;     // default minimum
    inline size_t      maxMemPerProc     = 4096;   // default maximum

    // Legacy aliases so existing Scheduler/Process code compiles unchanged
    inline int& NUM_CORES             = numCpu;
    inline int& PRINTS_PER_PROCESS    = maxIns;   // temporary; replace later
    inline int& PER_INSTRUCTION_DELAY_MS = delayPerExec;
    inline bool WRITE_PRINT_FILES     = true;

    // Reads config.txt from the working directory.
    // Returns true on success, prints an error and returns false on failure.
    bool loadFromFile(const std::string& path = "config.txt");
}