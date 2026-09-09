#include "clock_text.hpp"

#include <chrono>
#include <cstdlib>
#include <ctime>
#include <iomanip>
#include <iostream>

int main(int argc, char** argv) {
    const int iterations = argc > 1 ? std::atoi(argv[1]) : 5000000;
    const std::time_t base = 1788883200;
    size_t baselineChecksum = 0;
    size_t cachedChecksum = 0;

    auto started = std::chrono::steady_clock::now();
    for (int i = 0; i < iterations; ++i) {
        baselineChecksum += formatLocalClock(base + i / 60, false).size();
    }
    const double baselineMs = std::chrono::duration<double, std::milli>(
        std::chrono::steady_clock::now() - started
    ).count();

    LocalClockTextCache cache;
    started = std::chrono::steady_clock::now();
    for (int i = 0; i < iterations; ++i) {
        cachedChecksum += cache.text(base + i / 60, false).size();
    }
    const double cachedMs = std::chrono::duration<double, std::milli>(
        std::chrono::steady_clock::now() - started
    ).count();

    if (baselineChecksum != cachedChecksum) return 2;
    std::cout << std::fixed << std::setprecision(3)
              << "iterations=" << iterations
              << " baseline_ms=" << baselineMs
              << " cached_ms=" << cachedMs
              << " speedup_pct=" << ((baselineMs - cachedMs) * 100.0 / baselineMs)
              << " checksum=" << cachedChecksum << '\n';
    return 0;
}
