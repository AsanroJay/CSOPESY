#include "ProcessMemory.h"

ProcessMemory::ProcessMemory(size_t sizeBytes)
    : sizeBytes(sizeBytes),
      bytes(sizeBytes, 0) {}

uint16_t ProcessMemory::readWord(size_t address) const {
    return static_cast<uint16_t>(bytes[address]) |
           static_cast<uint16_t>(bytes[address + 1]) << 8;
}

void ProcessMemory::writeWord(size_t address, uint16_t value) {
    bytes[address]     = static_cast<uint8_t>(value & 0xFF);
    bytes[address + 1] = static_cast<uint8_t>((value >> 8) & 0xFF);
}
