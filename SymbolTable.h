#pragma once

#include <cstddef>
#include <cstdint>
#include <mutex>
#include <string>
#include <unordered_map>

// Name -> slot mapping for the 64-byte symbol table segment that sits at the
// base of every process's address space. Slot i occupies bytes [i*2, i*2 + 2),
// so the segment holds at most 32 uint16 variables.
//
// The values are deliberately NOT stored here. They live in the process's
// emulated memory, so reading or writing a variable goes through exactly the
// same paging path as any other memory access -- which is what makes a DECLARE
// page-fault when the symbol table segment is not resident.
class SymbolTable {
public:
    static constexpr size_t MAX_VARIABLES      = 32;
    static constexpr size_t BYTES_PER_VARIABLE = 2;

    // Resolves `name` to its byte address, claiming a free slot on first use.
    // Returns false when the segment is already full and `name` is new; the
    // caller must then ignore the instruction, per the spec.
    bool addressOf(const std::string& name, size_t& outAddress);

    // Resolves `name` without claiming a slot. False when never declared.
    bool find(const std::string& name, size_t& outAddress) const;

    bool   isFull() const;
    size_t count() const;

private:
    std::unordered_map<std::string, size_t> slots;  // name -> slot index
    mutable std::mutex mutex;
};
