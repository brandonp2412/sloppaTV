#pragma once

#include "jellyfin_types.hpp"

#include <string>

inline std::string episodeNumberLabel(const JellyfinItem& item) {
    std::string result;
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
    std::string result = item.seriesName;
    const std::string number = episodeNumberLabel(item);
    if (!number.empty()) {
        if (!result.empty()) result += " - ";
        result += number;
    }
    return result;
}

struct PlaybackLabels {
    std::string heading;
    std::string secondary;

    void update(const JellyfinItem& item) {
        heading = item.seriesName.empty() ? item.name : item.seriesName;
        const std::string number = episodeNumberLabel(item);
        secondary = item.seriesName.empty()
            ? episodeLabel(item)
            : number + (item.name.empty() ? "" : "  |  " + item.name);
    }

    void clear() {
        heading.clear();
        secondary.clear();
    }
};
