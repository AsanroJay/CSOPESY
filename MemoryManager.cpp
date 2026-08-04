#include "MemoryManager.h"
#include <fstream>
#include <iostream>

MemoryManager::MemoryManager(int maxOverallMem, int memPerFrame, int memPerProc)
    : maxOverallMem(maxOverallMem), memPerFrame(memPerFrame), memPerProc(memPerProc) {
    
    // Initialize the new PagingAllocator here instead of using the old flat logic
    allocator = std::make_unique<PagingAllocator>(maxOverallMem, memPerFrame);
}

int MemoryManager::allocate(int pid, size_t processSize) {
    std::lock_guard<std::mutex> lock(mutex);
    
    // Idempotent check: return success if already allocated
    if (processBlocks.find(pid) != processBlocks.end()) {
        return 1; 
    }

    // Call the PagingAllocator to handle the memory distribution
    void* handle = allocator->allocate(processSize);
    if (!handle) {
        return -1; // Allocation failed (memory full)
    }

    processBlocks[pid] = handle;
    return 1;
}

void MemoryManager::deallocate(int pid) {
    std::lock_guard<std::mutex> lock(mutex);
    auto it = processBlocks.find(pid);
    if (it != processBlocks.end()) {
        allocator->deallocate(it->second);
        processBlocks.erase(it);
    }
}

bool MemoryManager::isAllocated(int pid) {
    std::lock_guard<std::mutex> lock(mutex);
    return processBlocks.find(pid) != processBlocks.end();
}

int MemoryManager::processCount() {
    std::lock_guard<std::mutex> lock(mutex);
    return processBlocks.size();
}

int MemoryManager::externalFragmentationBytes() {
    return getMaximumSize() - getCurrentAllocatedSize(); 
}

void MemoryManager::writeSnapshot(const std::string& path) {
    std::lock_guard<std::mutex> lock(mutex);
    std::ofstream file(path);
    if (file.is_open()) {
        file << allocator->visualizeMemory();
    }
}

size_t MemoryManager::getMaximumSize() const {
    if (allocator) return allocator->getMaximumSize();
    return 0;
}

size_t MemoryManager::getCurrentAllocatedSize() const {
    if (allocator) return allocator->getCurrentAllocatedSize();
    return 0;
}

size_t MemoryManager::getNumPagedIn() const {
    if (allocator) return allocator->getNumPagedIn();
    return 0;
}

size_t MemoryManager::getNumPagedOut() const {
    if (allocator) return allocator->getNumPagedOut();
    return 0;
}

size_t MemoryManager::getFrameCount() const {
    return allocator ? allocator->getFrameCount() : 0;
}

bool MemoryManager::accessPage(int pid, size_t virtualAddress) {
    std::lock_guard<std::mutex> lock(mutex);
    
    auto it = processBlocks.find(pid);
    if (it == processBlocks.end()) {
        return false; // Process memory not allocated
    }

    // Convert the raw byte address into a page index
    size_t pageIndex = virtualAddress / memPerFrame;
    
    // Call it directly - no dynamic_cast needed!
    if (allocator) {
        return allocator->accessPage(it->second, pageIndex);
    }
    
    return true; 
}

size_t MemoryManager::getProcessResidentMemory(int pid) const {
    std::lock_guard<std::mutex> lock(mutex);
    auto it = processBlocks.find(pid);
    if (it == processBlocks.end()) return 0;

    if (allocator) {
        return allocator->getProcessResidentMemory(it->second);
    }
    return 0;
}