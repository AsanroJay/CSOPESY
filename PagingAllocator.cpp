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
      numPagedIn(0),
      numPagedOut(0) {
    memoryAllocatorType  = PAGING;
    maximumSize          = maxMemory;
    currentAllocatedSize = 0;
    freeFrameList.reserve(numFrames);
    for (size_t i = 0; i < numFrames; ++i) {
        freeFrameList.push_back(i);
    }
}

void* PagingAllocator::frameToPointer(size_t frameIndex) {
    return static_cast<void*>(physicalMemory.data() + frameIndex * frameSize);
}

size_t PagingAllocator::pointerToFrame(void* ptr) const {
    const char* base = physicalMemory.data();
    return (static_cast<char*>(ptr) - base) / frameSize;
}


void* PagingAllocator::allocate(size_t size) {
    std::lock_guard<std::mutex> lock(mutex);

    size_t numFramesNeeded = (size + frameSize - 1) / frameSize;  // ceil
    if (numFramesNeeded == 0 || numFramesNeeded > freeFrameList.size()) {
        return nullptr;  // not enough free frames -> allocation fails
    }

    int allocId = nextAllocId++;
    Allocation alloc;
    alloc.ownerId = allocId;
    alloc.size    = size;
    alloc.pageTable.resize(numFramesNeeded);

    size_t firstFrame = 0;
    for (size_t page = 0; page < numFramesNeeded; ++page) {
        size_t frame = freeFrameList.back();
        freeFrameList.pop_back();

        PageTableEntry& pte = alloc.pageTable[page];
        pte.frameNumber = static_cast<int>(frame);
        pte.valid       = true;   // demand paging would leave this false until first touch
        pte.dirty       = false;

        frameOwners[frame] = FrameOwner{allocId, page};
        if (page == 0) firstFrame = frame;
    }

    currentAllocatedSize += numFramesNeeded * frameSize;

    void* handle = frameToPointer(firstFrame);
    allocations[handle] = std::move(alloc);
    return handle;
}

void PagingAllocator::deallocate(void* ptr) {
    std::lock_guard<std::mutex> lock(mutex);
    if (ptr == nullptr) return;

    auto it = allocations.find(ptr);
    if (it == allocations.end()) return;  // not one of ours / already freed

    Allocation& alloc = it->second;
    for (const PageTableEntry& pte : alloc.pageTable) {
        if (pte.frameNumber < 0) continue;  // page currently swapped out
        size_t frame = static_cast<size_t>(pte.frameNumber);
        frameOwners[frame] = FrameOwner{};   // mark free
        freeFrameList.push_back(frame);
    }

    currentAllocatedSize -= alloc.pageTable.size() * frameSize;
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
