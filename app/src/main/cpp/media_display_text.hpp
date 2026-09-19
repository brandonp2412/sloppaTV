#pragma once

#include "jellyfin_types.hpp"

#include <algorithm>
#include <ctime>
#include <iomanip>
#include <sstream>
#include <string>

inline std::string episodeNumberLabel(const JellyfinItem& item) {
    if (item.type != "Episode") return {};
    std::string result;
    if (item.parentIndexNumber >= 0) result += "S" + std::to_string(item.parentIndexNumber);
    if (item.indexNumber >= 0) result += "E" + std::to_string(item.indexNumber);
    return result;
}

inline std::string episodeLabel(const JellyfinItem& item) {
    std::string result = item.seriesName;
    const std::string number = episodeNumberLabel(item);
    if (!number.empty()) {
        if (!result.empty()) result += " - ";
        result += number;
    }
    return result;
}

inline std::string formatLocalClock(std::time_t instant, bool clock24Hour) {
    std::tm local{};
    localtime_r(&instant, &local);
    std::ostringstream out;
    out << std::put_time(&local, clock24Hour ? "%H:%M" : "%I:%M %p");
    std::string value = out.str();
    if (!clock24Hour && !value.empty() && value.front() == '0') value.erase(value.begin());
    return value;
}

inline std::string formatPlaybackTime(int milliseconds) {
    const int totalSeconds = std::max(0, milliseconds / 1000);
    const int hours = totalSeconds / 3600;
    const int minutes = (totalSeconds / 60) % 60;
    const int seconds = totalSeconds % 60;
    std::ostringstream out;
    if (hours > 0)
        out << hours << ':' << std::setw(2) << std::setfill('0') << minutes;
    else
        out << minutes;
    out << ':' << std::setw(2) << std::setfill('0') << seconds;
    return out.str();
}
