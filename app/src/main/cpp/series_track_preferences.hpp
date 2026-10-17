#pragma once

#include <map>
#include <optional>
#include <string>
#include <string_view>

struct SeriesTrackPreferences {
    std::optional<std::string> audioLanguage{};
    // A present, empty string explicitly means subtitles off.
    std::optional<std::string> subtitleLanguage{};
};

using SeriesTrackPreferenceMap = std::map<std::string, SeriesTrackPreferences>;

inline std::string seriesTrackPreferenceKey(std::string_view server, std::string_view userId,
                                            std::string_view seriesId) {
    if (server.empty() || userId.empty() || seriesId.empty()) return {};
    return std::string(server) + "\n" + std::string(userId) + "\n" + std::string(seriesId);
}

inline const SeriesTrackPreferences* findSeriesTrackPreferences(const SeriesTrackPreferenceMap& saved,
                                                                std::string_view server, std::string_view userId,
                                                                std::string_view seriesId) {
    const std::string key = seriesTrackPreferenceKey(server, userId, seriesId);
    if (key.empty()) return nullptr;
    const auto found = saved.find(key);
    return found == saved.end() ? nullptr : &found->second;
}
