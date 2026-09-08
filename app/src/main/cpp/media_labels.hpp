#pragma once

#include "jellyfin_types.hpp"

#include <string>

inline std::string episodeNumberLabel(const JellyfinItem& item) {
    std::string result;
    if (item.parentIndexNumber < 0 && item.indexNumber < 0) return result;
    result.reserve(16);
    if (item.parentIndexNumber >= 0) {
        result.push_back('S');
        result += std::to_string(item.parentIndexNumber);
    }
    if (item.indexNumber >= 0) {
        result.push_back('E');
        result += std::to_string(item.indexNumber);
    }
    return result;
}

inline std::string episodeLabel(const JellyfinItem& item) {
    const std::string number = episodeNumberLabel(item);
    if (number.empty()) return item.seriesName;
    std::string result;
    result.reserve(item.seriesName.size() + (item.seriesName.empty() ? 0 : 3) + number.size());
    result += item.seriesName;
    if (!result.empty()) result += " - ";
    result += number;
    return result;
}

struct PlaybackLabels {
    std::string heading;
    std::string secondary;

    void update(const JellyfinItem& item) {
        heading = item.seriesName.empty() ? item.name : item.seriesName;
        const std::string number = episodeNumberLabel(item);
        secondary = item.seriesName.empty()
            ? number
            : number + (item.name.empty() ? "" : "  |  " + item.name);
    }

    void clear() {
        heading.clear();
        secondary.clear();
    }
};
