#pragma once

#include <string>
#include <unordered_map>
#include <mutex>
#include <cstdint>
#include <algorithm>
#include <limits>

class SymbolTable {
public:
    // Constructor now starts completely empty
    SymbolTable() = default;
    ~SymbolTable() = default;

    // DECLARE(var, value) -> Creates a new variable or overwrites it if it exists
    void declareVariable(const std::string& name, uint16_t initialValue);

    // Used by PRINT, ADD, SUBTRACT to read values safely. 
    // Automatically initializes to 0 if not declared yet.
    uint16_t getVariable(const std::string& name);

    // Used by ADD and SUBTRACT to save results with full clamping behavior.
    void setVariable(const std::string& name, uint32_t value);

private:
    std::unordered_map<std::string, uint16_t> table;
    mutable std::mutex tableMutex;
};