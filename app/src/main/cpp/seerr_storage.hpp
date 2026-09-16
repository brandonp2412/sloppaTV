#pragma once

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
        const int64_t used = totalSpace - freeSpace;
        return static_cast<int>((used * 100 + totalSpace / 2) / totalSpace);
    }
};
