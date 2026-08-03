#include "Utils.h"

#include <ctime>
#include <iomanip>
#include <sstream>

std::string currentTimestamp() {
    std::time_t now = std::time(nullptr);
    std::tm localTime{};
#ifdef _WIN32
    localtime_s(&localTime, &now);   // thread-safe variant (CPU workers format concurrently)
#else
    localtime_r(&now, &localTime);
#endif
    char buffer[32];
    std::strftime(buffer, sizeof(buffer), "%m/%d/%Y %I:%M:%S%p", &localTime);
    return std::string(buffer);
}

std::string currentTimeOfDay() {
    std::time_t now = std::time(nullptr);
    std::tm localTime{};
#ifdef _WIN32
    localtime_s(&localTime, &now);
#else
    localtime_r(&now, &localTime);
#endif
    char buffer[16];
    std::strftime(buffer, sizeof(buffer), "%H:%M:%S", &localTime);
    return std::string(buffer);
}

std::string zeroPad(int value, int width) {
    std::ostringstream oss;
    oss << std::setw(width) << std::setfill('0') << value;
    return oss.str();
}

std::string toHexAddress(size_t address) {
    std::ostringstream oss;
    oss << "0x" << std::uppercase << std::hex << address;
    return oss.str();
}
