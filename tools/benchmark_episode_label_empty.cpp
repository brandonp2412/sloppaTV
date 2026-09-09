#include "jellyfin_types.hpp"

#include <chrono>
#include <cstdlib>
#include <iomanip>
#include <iostream>
#include <string>

static std::string baseline(const JellyfinItem& item) {
    std::string result;
    result.reserve(16);
    if (item.parentIndexNumber >= 0) {
        result.push_back('S');
        result += std::to_string(item.parentIndexNumber);
    }
    if (item.indexNumber >= 0) {
        result.push_back('E');
        result += std::to_string(item.indexNumber);
    }
    return result;
}

static std::string optimized(const JellyfinItem& item) {
    std::string result;
    if (item.parentIndexNumber < 0 && item.indexNumber < 0) return result;
    result.reserve(16);
    if (item.parentIndexNumber >= 0) {
        result.push_back('S');
        result += std::to_string(item.parentIndexNumber);
    }
    if (item.indexNumber >= 0) {
        result.push_back('E');
        result += std::to_string(item.indexNumber);
    }
    return result;
}

int main(int argc, char** argv) {
    const int iterations = argc > 1 ? std::atoi(argv[1]) : 10000000;
    JellyfinItem item;
    if (argc > 2) {
        item.parentIndexNumber = 12;
        item.indexNumber = 143;
    }
    size_t checksum = 0;
    const auto started = std::chrono::steady_clock::now();
    for (int i = 0; i < iterations; ++i) {
#ifdef BENCH_BASELINE
        checksum += baseline(item).capacity();
#else
        checksum += optimized(item).capacity();
#endif
    }
    const double elapsedMs = std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - started).count();
    std::cout << std::fixed << std::setprecision(3)
              << "iterations=" << iterations << " elapsed_ms=" << elapsedMs
              << " checksum=" << checksum << '\n';
    return 0;
}
