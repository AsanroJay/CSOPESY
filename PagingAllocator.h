#pragma once

#include <cstddef>
#include <mutex>
#include <string>
#include <unordered_map>
#include <vector>

#include "IMemoryAllocator.h"

struct PageTableEntry {
    int  frameNumber = -1;    // physical frame holding this page, or -1
    bool valid       = false; // true = resident in RAM, false = in backing store
    bool dirty       = false; // true = modified since paged in (needs write-back)
};

class PagingAllocator : public IMemoryAllocator {
public:
    PagingAllocator(size_t maxMemory, size_t frameSize);

    // IMemoryAllocator
    void*  allocate(size_t size) override;
    void   deallocate(void* ptr) override;
    String visualizeMemory() override;

    size_t getNumPagedIn() const;
    size_t getNumPagedOut() const;

private:
    struct Allocation {
        int                         ownerId;    // synthetic id standing in for a PID
        size_t                      size;       // requested bytes
        std::vector<PageTableEntry> pageTable;  // one PTE per page of this allocation
    };

    struct FrameOwner {
        int    allocId  = -1;   // -1 == free frame
        size_t pageIndex = 0;
    };

    size_t frameSize;
    size_t numFrames;

    std::vector<char>       physicalMemory;  // simulated RAM (maximumSize bytes)
    std::vector<size_t>     freeFrameList;   // indices of currently-free frames
    std::vector<FrameOwner> frameOwners;     // numFrames entries, frame -> owner

    std::unordered_map<void*, Allocation> allocations;  
    int nextAllocId;

    size_t numPagedIn;
    size_t numPagedOut;

    std::mutex mutex;

    void*  frameToPointer(size_t frameIndex);
    size_t pointerToFrame(void* ptr) const;

    void pageOutFrame(size_t frameIndex, int allocId, size_t pageIndex);
    void pageInFrame(size_t frameIndex, int allocId, size_t pageIndex);
};
