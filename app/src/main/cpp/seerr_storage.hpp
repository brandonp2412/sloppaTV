#pragma once

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <string>

struct SeerrStorageTarget {
    std::string mediaType;
    std::string serviceName;
    std::string path;
    int serverId = -1;
    int profileId = 0;
    int64_t freeSpace = 0;
    int64_t totalSpace = 0;
    bool isDefault = false;
    bool is4k = false;

    [[nodiscard]] int usedPercent() const {
        if (totalSpace <= 0) return 0;
        const int64_t available = std::clamp(freeSpace, int64_t{0}, totalSpace);
        const int64_t used = totalSpace - available;
        const long double percent = static_cast<long double>(used) * 100.0L / static_cast<long double>(totalSpace);
        return std::clamp(static_cast<int>(std::lround(percent)), 0, 100);
    }
};
