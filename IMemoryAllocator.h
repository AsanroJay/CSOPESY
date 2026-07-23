#pragma once

#include <cstddef>
#include <string>

// Common alias so the interface reads like the MCO2 spec (which uses `String`).
using String = std::string;

// IMemoryAllocator
// ----------------
// The abstract memory-manager contract given in the Week 11 seatwork. Every
// concrete allocator in the emulator (the existing flat first-fit allocator and
// the new PagingAllocator) derives from this interface, so the Scheduler can be
// written against `IMemoryAllocator&` and swap strategies purely from config
// ("max-overall-mem"/"mem-per-frame" -> FLAT when frame == overall, PAGING
// otherwise).
//
//   allocate(size)      -> reserve `size` bytes, return an opaque handle (void*)
//   deallocate(ptr)     -> release the region previously returned by allocate()
//   visualizeMemory()   -> human-readable snapshot for the memory-stamp files
class IMemoryAllocator {
public:
    enum MemoryAllocatorType {
        FLAT_MEMORY_ALLOCATOR,
        PAGING,
    };

    virtual ~IMemoryAllocator() = default;

    virtual void* allocate(size_t size) = 0;
    virtual void deallocate(void* ptr) = 0;
    virtual String visualizeMemory() = 0;

protected:
    MemoryAllocatorType memoryAllocatorType;

    // A single contiguous run of the address space. In the flat allocator this
    // is a real hole/occupied span; in paging it degenerates to one frame.
    struct MemoryBlock {
        size_t start;
        size_t size;

        bool operator<(const MemoryBlock& other) const {
            return start < other.start;
        }
    };

    size_t maximumSize;           // total bytes of simulated main memory
    size_t currentAllocatedSize;  // bytes currently handed out
};
