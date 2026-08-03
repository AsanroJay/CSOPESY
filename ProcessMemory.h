#pragma once

#include <cstddef>
#include <cstdint>
#include <vector>

// The emulated address space of a single process.
//
// Layout: bytes [0, 64) are the symbol table segment (32 uint16 slots); every
// byte from 64 up is free data space. Addresses are virtual, byte-granular, and
// emulated -- they are not a 1:1 mapping of the host's RAM.
//
// Bytes are stored little-endian and start zero-filled, which is what makes a
// READ of an address that was never written come back as 0.
//
// NOTE: this class deliberately knows nothing about frames, page tables or the
// backing store. Physical memory is the memory manager's concern. When demand
// paging is wired up, Process::readMemory / writeMemory are the single place
// that needs to consult it -- see the seam documented there.
class ProcessMemory {
public:
    static constexpr size_t SYMBOL_TABLE_BYTES = 64;
    static constexpr size_t BYTES_PER_VARIABLE = 2;
    static constexpr size_t MAX_VARIABLES      = SYMBOL_TABLE_BYTES / BYTES_PER_VARIABLE;

    explicit ProcessMemory(size_t sizeBytes);

    size_t getSize() const { return sizeBytes; }

    // A uint16 occupies 2 bytes, so both must fall inside the address space.
    // Anything else is an access violation.
    bool isValidWordAddress(size_t address) const {
        return address + BYTES_PER_VARIABLE <= sizeBytes;
    }

    uint16_t readWord(size_t address) const;
    void     writeWord(size_t address, uint16_t value);

private:
    size_t               sizeBytes;
    std::vector<uint8_t> bytes;  // zero-filled: uninitialised memory reads as 0
};
