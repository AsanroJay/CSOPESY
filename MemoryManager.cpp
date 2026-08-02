#include "MemoryManager.h"

#include <algorithm>
#include <fstream>

#include "Utils.h"

MemoryManager::MemoryManager(int maxOverallMem, int memPerFrame, int memPerProc)
    : maxOverallMem(maxOverallMem),
      memPerFrame(memPerFrame),
      memPerProc(memPerProc) {}

// First-fit scan: walk the allocated blocks (sorted by base) and return the
// first gap large enough to hold memPerProc, checking the leading gap, the gaps
// between blocks, and the trailing gap. Returns -1 if nothing fits.
int MemoryManager::firstFitBaseLocked() const {
    int prevEnd = 0;
    for (const auto& block : blocks) {
        if (block.base - prevEnd >= memPerProc) {
            return prevEnd;
        }
        prevEnd = block.limit;
    }
    if (maxOverallMem - prevEnd >= memPerProc) {
        return prevEnd;
    }
    return -1;
}

int MemoryManager::allocate(int pid, const std::string& name) {
    std::lock_guard<std::mutex> lock(mutex);

    // Idempotent: a process that already holds memory keeps it across quanta.
    for (const auto& block : blocks) {
        if (block.pid == pid) {
            return block.base;
        }
    }

    int base = firstFitBaseLocked();
    if (base < 0) {
        return -1;  // memory full
    }

    MemoryBlock block{base, base + memPerProc, pid, name};

    // Insert while keeping `blocks` sorted by base address.
    auto pos = std::lower_bound(blocks.begin(), blocks.end(), base,
                                [](const MemoryBlock& b, int value) {
                                    return b.base < value;
                                });
    blocks.insert(pos, block);
    return base;
}

void MemoryManager::deallocate(int pid) {
    std::lock_guard<std::mutex> lock(mutex);
    blocks.erase(std::remove_if(blocks.begin(), blocks.end(),
                                [pid](const MemoryBlock& b) { return b.pid == pid; }),
                 blocks.end());
}

bool MemoryManager::isAllocated(int pid) {
    std::lock_guard<std::mutex> lock(mutex);
    for (const auto& block : blocks) {
        if (block.pid == pid) return true;
    }
    return false;
}

int MemoryManager::processCount() {
    std::lock_guard<std::mutex> lock(mutex);
    return static_cast<int>(blocks.size());
}

int MemoryManager::externalFragmentationBytesLocked() const {
    return maxOverallMem - static_cast<int>(blocks.size()) * memPerProc;
}

int MemoryManager::externalFragmentationBytes() {
    std::lock_guard<std::mutex> lock(mutex);
    return externalFragmentationBytesLocked();
}

void MemoryManager::writeSnapshot(const std::string& path) {
    std::lock_guard<std::mutex> lock(mutex);

    std::ofstream file(path);
    if (!file.is_open()) return;

    file << "Timestamp: (" << currentTimestamp() << ")\n";
    file << "Number of processes in memory: " << blocks.size() << "\n";
    file << "Total external fragmentation in KB: "
         << (externalFragmentationBytesLocked() / 1024) << "\n";
    file << "\n";

    // ASCII memory map, printed from the highest address down to 0. For each
    // resident process we print its upper limit, name, and lower limit.
    file << "----end---- = " << maxOverallMem << "\n";
    for (auto it = blocks.rbegin(); it != blocks.rend(); ++it) {
        file << "\n";
        file << it->limit << "\n";
        file << it->name << "\n";
        file << it->base << "\n";
    }
    file << "\n";
    file << "----start---- = 0\n";
}
