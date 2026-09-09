#include "media_labels.hpp"

#include <chrono>
#include <cstdlib>
#include <iomanip>
#include <iostream>
#include <string>

static std::string baseline(const JellyfinItem& item) {
    std::string number;
    if (item.parentIndexNumber >= 0) {
        number.push_back('S');
        number += std::to_string(item.parentIndexNumber);
    }
    if (item.indexNumber >= 0) {
        number.push_back('E');
        number += std::to_string(item.indexNumber);
    }
    if (number.empty()) return item.seriesName;
    std::string result;
    result.reserve(item.seriesName.size() + (item.seriesName.empty() ? 0 : 3) + number.size());
    result += item.seriesName;
    if (!result.empty()) result += " - ";
    result += number;
    return result;
}

static std::string optimized(const JellyfinItem& item) {
    return episodeLabel(item);
}

int main(int argc, char** argv) {
    const int iterations = argc > 1 ? std::atoi(argv[1]) : 3000000;
    JellyfinItem item;
    item.seriesName = "A Particularly Long Television Series Name";
    item.parentIndexNumber = 12;
    item.indexNumber = 143;
    if (baseline(item) != optimized(item)) return 2;
    size_t checksum = 0;
    const auto started = std::chrono::steady_clock::now();
    for (int i = 0; i < iterations; ++i) {
#ifdef BENCH_BASELINE
        checksum += baseline(item).size();
#else
        checksum += optimized(item).size();
#endif
    }
    const double elapsedMs = std::chrono::duration<double, std::milli>(
        std::chrono::steady_clock::now() - started
    ).count();
    std::cout << std::fixed << std::setprecision(3)
              << "iterations=" << iterations << " elapsed_ms=" << elapsedMs
              << " checksum=" << checksum << '\n';
    return checksum > 0 ? 0 : 1;
}
