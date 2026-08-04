#pragma once

#include <cstddef>
#include <mutex>
#include <string>
#include <unordered_map>
#include <vector>
#include <cstdint>

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


    size_t getNumPagedIn() const override;
    size_t getNumPagedOut() const override;

    // Derived from the free list under the lock. The running counter dips
    // transiently mid-eviction, so reading it unsynchronised can report 0 used
    // memory while the system is actually full.
    size_t getCurrentAllocatedSize() const override;


    // The access seam for demand paging.
    // Returns true if the page was already resident (a hit).
    // Returns false if a page fault occurred (the page was just loaded into a frame).
    bool accessPage(void* handle, size_t pageIndex) override;

    size_t getProcessResidentMemory(void* handle) const override;

    size_t getFrameCount() const override { return numFrames; }

private:
    struct Allocation {
        int                         ownerId;    // synthetic id standing in for a PID
        size_t                      size;       // requested bytes
        std::vector<PageTableEntry> pageTable;  // one PTE per page of this allocation
    };

    struct FrameOwner {
        int      allocId   = -1;   // -1 == free frame
        size_t   pageIndex = 0;
        uint64_t lastAccess = 0;   // tracks LRU via an internal clock tick
    };

    size_t frameSize;
    size_t numFrames;

    std::vector<char>       physicalMemory;  // simulated RAM (maximumSize bytes)
    std::vector<size_t>     freeFrameList;   // indices of currently-free frames
    std::vector<FrameOwner> frameOwners;     // numFrames entries, frame -> owner

    std::unordered_map<void*, Allocation> allocations;  
    int nextAllocId;
    
    uint64_t internalClock; // Increments on every page access to determine LRU

    size_t numPagedIn;
    size_t numPagedOut;

    mutable std::mutex mutex;

    void pageOutFrame(size_t frameIndex, int allocId, size_t pageIndex);
    void pageInFrame(size_t frameIndex, int allocId, size_t pageIndex);
};