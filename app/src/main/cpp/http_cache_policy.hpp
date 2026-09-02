#pragma once

#include <cstddef>
#include <string_view>

constexpr std::size_t kMaxApiGetCacheEntries = 32;

inline bool shouldCacheApiGet(std::string_view url) {
    return url.find("/Images/") == std::string_view::npos
        && url.find("/Subtitles/") == std::string_view::npos
        && url.find("/Videos/") == std::string_view::npos
        && url.find("/Items/Resume") == std::string_view::npos
        && url.find("/Shows/NextUp") == std::string_view::npos
        && url.find("SortBy=Random") == std::string_view::npos;
}
