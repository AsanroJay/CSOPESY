#include "SymbolTable.h"

bool SymbolTable::addressOf(const std::string& name, size_t& outAddress) {
    std::lock_guard<std::mutex> lock(mutex);

    auto it = slots.find(name);
    if (it != slots.end()) {
        outAddress = it->second * BYTES_PER_VARIABLE;
        return true;
    }

    // Segment is full: the spec says succeeding declarations are ignored.
    if (slots.size() >= MAX_VARIABLES) {
        return false;
    }

    size_t slot = slots.size();
    slots[name] = slot;
    outAddress  = slot * BYTES_PER_VARIABLE;
    return true;
}

bool SymbolTable::find(const std::string& name, size_t& outAddress) const {
    std::lock_guard<std::mutex> lock(mutex);

    auto it = slots.find(name);
    if (it == slots.end()) {
        return false;
    }
    outAddress = it->second * BYTES_PER_VARIABLE;
    return true;
}

bool SymbolTable::isFull() const {
    std::lock_guard<std::mutex> lock(mutex);
    return slots.size() >= MAX_VARIABLES;
}

size_t SymbolTable::count() const {
    std::lock_guard<std::mutex> lock(mutex);
    return slots.size();
}
