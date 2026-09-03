#pragma once

#include <cstddef>
#include <cstdint>
#include <string_view>

constexpr std::size_t kMaxApiGetCacheEntries = 32;

constexpr bool shouldJoinInFlightApiGet(uint64_t currentGeneration, uint64_t pendingGeneration) {
    return currentGeneration == pendingGeneration;
}

inline bool shouldCacheApiGet(std::string_view url) {
    return url.find("/Images/") == std::string_view::npos
        && url.find("/Subtitles/") == std::string_view::npos
        && url.find("/Videos/") == std::string_view::npos
        && url.find("/Items/Resume") == std::string_view::npos
        && url.find("/Shows/NextUp") == std::string_view::npos
        && url.find("SortBy=Random") == std::string_view::npos;
}
