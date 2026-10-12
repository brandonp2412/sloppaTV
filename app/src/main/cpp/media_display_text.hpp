#pragma once

#include "jellyfin_types.hpp"

#include <algorithm>
#include <charconv>
#include <string_view>
#include <ctime>
#include <iomanip>
#include <sstream>
#include <string>

inline int episodeNumberFromText(std::string_view text, std::string_view marker) {
    const auto start = text.find(marker);
    if (start == std::string_view::npos) return -1;
    std::size_t digit = start + marker.size();
    while (digit < text.size() && text[digit] == ' ') ++digit;
    const std::size_t begin = digit;
    while (digit < text.size() && text[digit] >= '0' && text[digit] <= '9') ++digit;
    if (begin == digit) return -1;
    int result = -1;
    const auto [end, error] = std::from_chars(text.data() + begin, text.data() + digit, result);
    return error == std::errc{} && end == text.data() + digit ? result : -1;
}

inline std::string episodeNumberLabel(const JellyfinItem& item) {
    if (item.type != "Episode") return {};
    int season = item.parentIndexNumber;
    int episode = item.indexNumber;
    // Jellyfin-compatible providers occasionally omit numeric index fields but
    // preserve an explicit S01E02 label in the episode title.
    if (season < 0 || episode < 0) {
        const std::string_view name(item.name);
        for (std::size_t s = name.find_first_of("Ss"); s != std::string_view::npos;
             s = name.find_first_of("Ss", s + 1)) {
            const int parsedSeason = episodeNumberFromText(name.substr(s), name.substr(s, 1));
            const auto e = name.find_first_of("Ee", s + 1);
            if (parsedSeason < 0 || e == std::string_view::npos) continue;
            const int parsedEpisode = episodeNumberFromText(name.substr(e), name.substr(e, 1));
            if (parsedEpisode < 0) continue;
            if (season < 0) season = parsedSeason;
            if (episode < 0) episode = parsedEpisode;
            break;
        }
    }
    if (season < 0) season = episodeNumberFromText(item.seasonName, "Season");
    if (season < 0) season = episodeNumberFromText(item.name, "Season");
    if (episode < 0) episode = episodeNumberFromText(item.name, "Episode");
    std::string result;
    if (season >= 0) result += "S" + std::to_string(season);
    if (episode >= 0) result += "E" + std::to_string(episode);
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
