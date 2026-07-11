#pragma once

#include <mutex>
#include <string>
#include <vector>

// A flat, first-fit memory allocator for the round-robin scheduler homework.
//
// Main memory is a single contiguous span of `maxOverallMem` bytes. Every
// process needs a fixed `memPerProc` bytes and keeps that region for its entire
// lifetime, releasing it only when it finishes (no backing store / no paging).
//
// allocate() scans from address 0 upward and places the process in the FIRST
// free hole large enough to hold it (first-fit). If no hole fits, allocation
// fails and the scheduler must send the process back to the ready queue.
class MemoryManager {
public:
    MemoryManager(int maxOverallMem, int memPerFrame, int memPerProc);

    // Reserves memPerProc bytes for `pid` using first-fit. Returns the base
    // address on success, or -1 if memory is full. If `pid` already owns a
    // block, returns its existing base (idempotent for a running process).
    int allocate(int pid, const std::string& name);

    // Releases the block held by `pid`, if any (called when a process finishes).
    void deallocate(int pid);

    // True if `pid` currently holds a memory block.
    bool isAllocated(int pid);

    // Number of processes currently resident in memory.
    int processCount();

    // Total free memory in bytes (external fragmentation for this flat model).
    int externalFragmentationBytes();

    // Writes a "memory_stamp_<qq>.txt" style snapshot to `path`:
    // timestamp, process count, external fragmentation (KB), and an ASCII map.
    void writeSnapshot(const std::string& path);

private:
    struct MemoryBlock {
        int base;          // inclusive lower address
        int limit;         // exclusive upper address (base + memPerProc)
        int pid;
        std::string name;
    };

    int maxOverallMem;
    int memPerFrame;
    int memPerProc;

    std::vector<MemoryBlock> blocks;  // allocated blocks, kept sorted by base
    std::mutex mutex;

    // Non-locking helpers (caller must hold `mutex`).
    int firstFitBaseLocked() const;
    int externalFragmentationBytesLocked() const;
};
