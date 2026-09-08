#pragma once

#include <ctime>
#include <string>

inline std::string formatLocalClock(std::time_t instant, bool clock24Hour) {
    std::tm local{};
    localtime_r(&instant, &local);
    char value[16];
    const size_t length = std::strftime(value, sizeof(value), clock24Hour ? "%H:%M" : "%I:%M %p", &local);
    const size_t start = !clock24Hour && length > 0 && value[0] == '0' ? 1 : 0;
    return std::string(value + start, length - start);
}

class LocalClockTextCache {
public:
    const std::string& text(std::time_t instant, bool clock24Hour) {
        const std::time_t minute = instant / 60;
        if (minute != minute_ || clock24Hour != clock24Hour_) {
            value_ = formatLocalClock(instant, clock24Hour);
            minute_ = minute;
            clock24Hour_ = clock24Hour;
        }
        return value_;
    }

private:
    std::time_t minute_ = static_cast<std::time_t>(-1);
    bool clock24Hour_ = false;
    std::string value_;
};
