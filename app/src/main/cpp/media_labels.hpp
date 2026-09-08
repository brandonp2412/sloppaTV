#pragma once

#include "jellyfin_types.hpp"

#include <string>

inline void appendEpisodeNumber(std::string& result, const JellyfinItem& item) {
    const auto appendNumber = [&](char prefix, int value) {
        if (value < 0) return;
        result.push_back(prefix);
        result += std::to_string(value);
    };
    appendNumber('S', item.parentIndexNumber);
    appendNumber('E', item.indexNumber);
}

inline std::string episodeNumberLabel(const JellyfinItem& item) {
    if (item.parentIndexNumber < 0 && item.indexNumber < 0) return {};
    std::string result;
    result.reserve(16);
    appendEpisodeNumber(result, item);
    return result;
}

inline void episodeLabelInto(std::string& result, const JellyfinItem& item) {
    result.clear();
    result.reserve(item.seriesName.size() + (item.seriesName.empty() ? 0 : 3) + 16);
    result += item.seriesName;
    if (item.parentIndexNumber < 0 && item.indexNumber < 0) return;
    if (!result.empty()) result += " - ";
    appendEpisodeNumber(result, item);
}

inline std::string episodeLabel(const JellyfinItem& item) {
    std::string result;
    episodeLabelInto(result, item);
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
