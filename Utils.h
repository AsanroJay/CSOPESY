#pragma once

#include <string>

// Returns the current local time formatted as "MM/DD/YYYY HH:MM:SSAM"
// (12-hour clock, e.g. "06/19/2026 09:15:22AM"). Thread-safe.
std::string currentTimestamp();

// Left-pads an integer with zeros to the given width, e.g. zeroPad(5, 2) -> "05".
std::string zeroPad(int value, int width);
