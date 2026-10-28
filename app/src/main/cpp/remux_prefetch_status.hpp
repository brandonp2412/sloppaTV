#pragma once
#include <cstdint>
#include <string>

struct RemuxPrefetchStatus {
    std::uint64_t currentCachedBytes = 0;
    std::uint64_t currentTotalBytes = 0;
    std::uint64_t nextCachedBytes = 0;
    std::uint64_t nextTotalBytes = 0;
    double downloadMiBps = 0.0;
    std::uint64_t livePeers = 0;
    std::uint64_t startupRequiredBytes = 0;
    std::uint64_t startupReadableBytes = 0;
    bool startupReady = false;
};

struct RemuxStartupProbeStatus {
    bool active = false;
    std::string sourceId;
    int candidateIndex = 0;
    int candidateCount = 0;
    int elapsedSeconds = 0;
    int probeElapsedSeconds = 0;
    int probeBudgetSeconds = 0;
    bool hasPrefetch = false;
    RemuxPrefetchStatus prefetch;
};
