#pragma once

#include <string>

// Returns the current local time formatted as "MM/DD/YYYY HH:MM:SSAM"
// (12-hour clock, e.g. "06/19/2026 09:15:22AM"). Thread-safe.
std::string currentTimestamp();

// Returns the current local time of day as "HH:MM:SS" (24-hour clock). This is
// the format the access-violation message requires. Thread-safe.
std::string currentTimeOfDay();

// Left-pads an integer with zeros to the given width, e.g. zeroPad(5, 2) -> "05".
std::string zeroPad(int value, int width);

// Formats a memory address the way the spec writes them, e.g. 1280 -> "0x500".
std::string toHexAddress(size_t address);
