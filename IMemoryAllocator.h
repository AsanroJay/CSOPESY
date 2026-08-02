#pragma once

#include <cstddef>
#include <string>

using String = std::string;

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

    // TWO GETTERS for new stats:
    virtual size_t getMaximumSize() const { return maximumSize; }
    virtual size_t getCurrentAllocatedSize() const { return currentAllocatedSize; }

protected:
    MemoryAllocatorType memoryAllocatorType;
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