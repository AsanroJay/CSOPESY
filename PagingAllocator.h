#pragma once

#include <cstddef>
#include <mutex>
#include <string>
#include <unordered_map>
#include <vector>

#include "IMemoryAllocator.h"

// A virtual page's status. `frameNumber` is valid only when `valid` is true;
// when a page is evicted, `valid` flips to false and its bytes live in the
// backing store (csopesy-backing-store.txt) instead of a physical frame.
struct PageTableEntry {
    int  frameNumber = -1;    // physical frame holding this page, or -1
    bool valid       = false; // true = resident in RAM, false = in backing store
    bool dirty       = false; // true = modified since paged in (needs write-back)
};

// PagingAllocator
// ---------------
// A paging implementation of IMemoryAllocator. Physical memory is a flat byte
// array carved into fixed-size frames (Config::memPerFrame). Each allocation is
// broken into ceil(size / frameSize) pages; pages are mapped to any free frames
// (non-contiguous allocation is fine, which is the whole point of paging).
//
// The returned void* handle is the address of the frame backing page 0. That
// pointer can always be mapped back to a Page Table Entry and physical frame
// via pointer arithmetic (see deallocate()), which is how deallocate() frees the
// right frames given only the void*.
class PagingAllocator : public IMemoryAllocator {
public:
    PagingAllocator(size_t maxMemory, size_t frameSize);

    // IMemoryAllocator
    void*  allocate(size_t size) override;
    void   deallocate(void* ptr) override;
    String visualizeMemory() override;

    // vmstat counters: total pages read in from / written out to the backing store.
    size_t getNumPagedIn() const;
    size_t getNumPagedOut() const;

private:
    // One record per live allocation, keyed by the void* handle we returned.
    struct Allocation {
        int                         ownerId;    // synthetic id standing in for a PID
        size_t                      size;       // requested bytes
        std::vector<PageTableEntry> pageTable;  // one PTE per page of this allocation
    };

    // Reverse map: which allocation/page a physical frame currently holds. Lets
    // us walk a raw frame index back to its owner + PageTableEntry (Question 5).
    struct FrameOwner {
        int    allocId  = -1;   // -1 == free frame
        size_t pageIndex = 0;
    };

    size_t frameSize;
    size_t numFrames;

    std::vector<char>       physicalMemory;  // simulated RAM (maximumSize bytes)
    std::vector<size_t>     freeFrameList;   // indices of currently-free frames
    std::vector<FrameOwner> frameOwners;     // numFrames entries, frame -> owner

    std::unordered_map<void*, Allocation> allocations;  // handle -> allocation
    int nextAllocId;

    // Backing-store bookkeeping surfaced by the vmstat command.
    size_t numPagedIn;
    size_t numPagedOut;

    std::mutex mutex;

    // Helpers (caller holds `mutex`).
    void*  frameToPointer(size_t frameIndex);
    size_t pointerToFrame(void* ptr) const;

    // Backing-store I/O. Every read bumps num-paged-in, every write bumps
    // num-paged-out. This is the single place the backing store is touched.
    void pageOutFrame(size_t frameIndex, int allocId, size_t pageIndex);
    void pageInFrame(size_t frameIndex, int allocId, size_t pageIndex);
};
