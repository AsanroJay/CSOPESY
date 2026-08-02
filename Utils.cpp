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

std::string zeroPad(int value, int width) {
    std::ostringstream oss;
    oss << std::setw(width) << std::setfill('0') << value;
    return oss.str();
}
