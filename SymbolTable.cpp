#include "SymbolTable.h"

void SymbolTable::declareVariable(const std::string& name, uint16_t initialValue) {
    std::lock_guard<std::mutex> lock(tableMutex);
    // Overwrites if it exists, inserts a new pair if it doesn't
    table[name] = initialValue;
}

uint16_t SymbolTable::getVariable(const std::string& name) {
    std::lock_guard<std::mutex> lock(tableMutex);
    
    auto it = table.find(name);
    if (it == table.end()) {
        // Fallback: Implicitly declare with a value of 0 if missing [cite: 50]
        table[name] = 0;
        return 0;
    }
    return it->second;
}

void SymbolTable::setVariable(const std::string& name, uint32_t value) {
    std::lock_guard<std::mutex> lock(tableMutex);
    
    // Clamp values strictly between (0, max(uint16)) [cite: 52]
    uint32_t clampedValue = std::clamp<uint32_t>(
        value, 
        0u, 
        static_cast<uint32_t>(std::numeric_limits<uint16_t>::max())
    );
    
    table[name] = static_cast<uint16_t>(clampedValue);
}