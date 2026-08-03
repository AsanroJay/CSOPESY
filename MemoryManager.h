#pragma once

#include <memory>
#include <mutex>
#include <string>
#include <unordered_map>

#include "IMemoryAllocator.h"
#include "PagingAllocator.h"

class MemoryManager {
public:
    MemoryManager(int maxOverallMem, int memPerFrame, int memPerProc);

    // Reserves memory for `pid`. Returns 1 on success, or -1 if memory is full.
    // Updated to accept the specific size required by the process instead of the name.
    int allocate(int pid, size_t processSize);

    // Releases the block held by `pid`
    void deallocate(int pid);

    // True if `pid` currently holds a memory block.
    bool isAllocated(int pid);

    // Number of processes currently resident in memory.
    int processCount();

    int externalFragmentationBytes();

    void writeSnapshot(const std::string& path);

    // --- NEW GETTERS FOR VMSTAT ---
    size_t getMaximumSize() const;
    size_t getCurrentAllocatedSize() const;
    size_t getNumPagedIn() const;
    size_t getNumPagedOut() const;

private:
    int maxOverallMem;
    int memPerFrame;
    int memPerProc;

    // Pointer to your new PagingAllocator
    std::unique_ptr<IMemoryAllocator> allocator;

    // Map PID to the void* handle returned by the PagingAllocator
    std::unordered_map<int, void*> processBlocks;
    
    mutable std::mutex mutex;
};