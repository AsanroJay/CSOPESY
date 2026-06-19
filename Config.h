#pragma once

// Central place for the emulator's tunable settings for this homework.
namespace Config {
    constexpr int  NUM_CORES = 4;             // CPU cores = worker threads
    constexpr int  NUM_PROCESSES = 10;        // processes created by "scheduler-start"
    constexpr int  PRINTS_PER_PROCESS = 100;  // print instructions per process

    // Delay applied after each executed instruction.
    // 0 = near-instant (current choice). Set to ~30 to make the running -> finished
    // transition visible when typing "screen -ls" during the recording.
    constexpr int  PER_INSTRUCTION_DELAY_MS = 0;

    // Whether the "print" instruction writes a .txt file per process.
    // The homework PDF requires this; set to false for the machine-project
    // submission so file I/O does not slow the scheduler.
    constexpr bool WRITE_PRINT_FILES = true;
}
