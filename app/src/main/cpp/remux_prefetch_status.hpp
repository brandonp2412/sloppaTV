#pragma once
#include <cstdint>

struct RemuxPrefetchStatus {
    std::uint64_t currentCachedBytes = 0;
    std::uint64_t currentTotalBytes = 0;
    std::uint64_t nextCachedBytes = 0;
    std::uint64_t nextTotalBytes = 0;
    double downloadMiBps = 0.0;
    std::uint64_t livePeers = 0;
};
