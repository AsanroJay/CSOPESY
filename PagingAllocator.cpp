#include "PagingAllocator.h"

#include <algorithm>
#include <fstream>
#include <sstream>

#include "Utils.h"

static const char* kBackingStorePath = "csopesy-backing-store.txt";

PagingAllocator::PagingAllocator(size_t maxMemory, size_t frameSize)
    : frameSize(frameSize),
      numFrames(frameSize > 0 ? maxMemory / frameSize : 0),
      physicalMemory(maxMemory, 0),
      frameOwners(numFrames),
      nextAllocId(1),
      internalClock(0),
      numPagedIn(0),
      numPagedOut(0) {
    memoryAllocatorType  = PAGING;
    maximumSize          = maxMemory;
    currentAllocatedSize = 0;
    freeFrameList.reserve(numFrames);
    for (size_t i = 0; i < numFrames; ++i) {
        freeFrameList.push_back(i);
    }

    // pageOutFrame appends, so without this the file would accumulate entries
    // across runs and show pages from previous sessions. Start each run clean.
    std::ofstream reset(kBackingStorePath, std::ios::trunc);
}

void* PagingAllocator::allocate(size_t size) {
    std::lock_guard<std::mutex> lock(mutex);

    size_t numFramesNeeded = (size + frameSize - 1) / frameSize;  // ceil

    // A process whose page table cannot fit in physical memory can never be
    // made resident, so refuse it outright. The scheduler leaves it in the ready
    // queue and CPU utilisation stays at 0% -- the deadlock-under-memory-
    // pressure behaviour the spec describes, rather than endless thrashing.
    if (numFramesNeeded == 0 || numFramesNeeded > numFrames) {
        return nullptr;
    }

    // --- VIRTUAL ALLOCATION ONLY ---
    // Do not pop from the free frame list here. Just map the virtual space.
    int allocId = nextAllocId++;
    Allocation alloc;
    alloc.ownerId = allocId;
    alloc.size    = size;
    alloc.pageTable.resize(numFramesNeeded);
    
    for (size_t page = 0; page < numFramesNeeded; ++page) {
        alloc.pageTable[page].frameNumber = -1;
        alloc.pageTable[page].valid = false;
        alloc.pageTable[page].dirty = false;
    }

    // We use the allocId as a virtual handle instead of a raw physical pointer
    void* handle = reinterpret_cast<void*>(static_cast<uintptr_t>(allocId));
    allocations[handle] = std::move(alloc);
    
    return handle;
}

bool PagingAllocator::accessPage(void* handle, size_t pageIndex) {
    std::lock_guard<std::mutex> lock(mutex);
    internalClock++;

    auto it = allocations.find(handle);
    if (it == allocations.end() || pageIndex >= it->second.pageTable.size()) {
        return false; // Out of bounds or invalid handle
    }

    PageTableEntry& pte = it->second.pageTable[pageIndex];

    // 1. PAGE HIT: The page is already resident in a physical frame.
    if (pte.valid) {
        frameOwners[pte.frameNumber].lastAccess = internalClock;
        return true; 
    }

    // 2. PAGE FAULT: The page is virtual. Assign a physical frame.
    size_t frameToUse = 0;

    if (!freeFrameList.empty()) {
        frameToUse = freeFrameList.back();
        freeFrameList.pop_back();
    } else {
        // --- LRU PAGE REPLACEMENT ---
        size_t lruFrame = 0;
        uint64_t oldestAccess = UINT64_MAX;

        for (size_t i = 0; i < numFrames; ++i) {
            if (frameOwners[i].allocId != -1 && frameOwners[i].lastAccess < oldestAccess) {
                oldestAccess = frameOwners[i].lastAccess;
                lruFrame = i;
            }
        }

        frameToUse = lruFrame;
        int victimAllocId = frameOwners[frameToUse].allocId;
        size_t victimPage = frameOwners[frameToUse].pageIndex;

        // Page out the victim to the backing store
        pageOutFrame(frameToUse, victimAllocId, victimPage);

        // Invalidate the victim's PTE using their handle
        void* victimHandle = reinterpret_cast<void*>(static_cast<uintptr_t>(victimAllocId));
        auto victimIt = allocations.find(victimHandle);
        if (victimIt != allocations.end()) {
            victimIt->second.pageTable[victimPage].valid = false;
            victimIt->second.pageTable[victimPage].frameNumber = -1;
        }
        
        currentAllocatedSize -= frameSize;
    }

    // Assign the selected frame to our faulting page
    pte.frameNumber = static_cast<int>(frameToUse);
    pte.valid = true;
    pte.dirty = false;

    frameOwners[frameToUse] = {it->second.ownerId, pageIndex, internalClock};
    pageInFrame(frameToUse, it->second.ownerId, pageIndex);
    
    currentAllocatedSize += frameSize;

    // Return false to signify a fault occurred (so the instruction restarts)
    return false;
}

void PagingAllocator::deallocate(void* ptr) {
    std::lock_guard<std::mutex> lock(mutex);
    if (ptr == nullptr) return;

    auto it = allocations.find(ptr);
    if (it == allocations.end()) return;  // not one of ours / already freed

    Allocation& alloc = it->second;
    
    // Only free pages that were actively loaded into physical frames
    for (const PageTableEntry& pte : alloc.pageTable) {
        if (pte.valid && pte.frameNumber >= 0) {
            size_t frame = static_cast<size_t>(pte.frameNumber);
            frameOwners[frame] = FrameOwner{};   // mark free
            freeFrameList.push_back(frame);
            currentAllocatedSize -= frameSize;
        }
    }

    allocations.erase(it);
}

void PagingAllocator::pageOutFrame(size_t frameIndex, int allocId, size_t pageIndex) {
    std::ofstream file(kBackingStorePath, std::ios::app);
    if (file.is_open()) {
        file << "[ALLOC: " << allocId << "][PAGE: " << pageIndex
             << "][FRAME: " << frameIndex << "]\n";
    }
    ++numPagedOut;
}

void PagingAllocator::pageInFrame(size_t frameIndex, int allocId, size_t pageIndex) {
    std::ifstream file(kBackingStorePath);
    (void)frameIndex;
    (void)allocId;
    (void)pageIndex;
    ++numPagedIn;
}

size_t PagingAllocator::getNumPagedIn() const  { return numPagedIn; }
size_t PagingAllocator::getNumPagedOut() const { return numPagedOut; }

size_t PagingAllocator::getCurrentAllocatedSize() const {
    std::lock_guard<std::mutex> lock(mutex);
    // An evicted frame is handed straight to the faulting page rather than
    // returned to the free list, so this count never dips during a swap.
    return (numFrames - freeFrameList.size()) * frameSize;
}

String PagingAllocator::visualizeMemory() {
    std::lock_guard<std::mutex> lock(mutex);

    std::ostringstream out;
    out << "Timestamp: (" << currentTimestamp() << ")\n";
    out << "Number of allocations in memory: " << allocations.size() << "\n";
    out << "Frames used: " << (numFrames - freeFrameList.size())
        << " / " << numFrames << "\n";
    out << "num-paged-in: "  << numPagedIn  << "\n";
    out << "num-paged-out: " << numPagedOut << "\n\n";

    for (size_t frame = 0; frame < numFrames; ++frame) {
        const FrameOwner& owner = frameOwners[frame];
        out << "Frame " << frame << " -> ";
        if (owner.allocId < 0) {
            out << "Free\n";
        } else {
            out << "Alloc " << owner.allocId << " page " << owner.pageIndex << "\n";
        }
    }
    return out.str();
}

size_t PagingAllocator::getProcessResidentMemory(void* handle) const {
    std::lock_guard<std::mutex> lock(mutex);
    auto it = allocations.find(handle);
    if (it == allocations.end()) return 0;

    size_t residentPages = 0;
    for (const auto& pte : it->second.pageTable) {
        if (pte.valid) {
            residentPages++;
        }
    }
    return residentPages * frameSize;
}