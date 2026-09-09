#include "media_labels.hpp"

#include <chrono>
#include <cstdlib>
#include <iomanip>
#include <iostream>
#include <string_view>

[[gnu::noinline]] static size_t consume(std::string_view heading, std::string_view secondary) {
    asm volatile("" : : "r"(heading.data()), "r"(secondary.data()) : "memory");
    return heading.size() + secondary.size();
}

static size_t rebuild(const JellyfinItem& item, int iterations) {
    size_t checksum = 0;
    for (int i = 0; i < iterations; ++i) {
        const std::string heading = item.seriesName.empty() ? item.name : item.seriesName;
        const std::string number = episodeNumberLabel(item);
        const std::string secondary = item.seriesName.empty()
            ? episodeLabel(item)
            : number + (item.name.empty() ? "" : "  |  " + item.name);
        checksum += consume(heading, secondary);
    }
    return checksum;
}

static size_t cached(const JellyfinItem& item, int iterations) {
    PlaybackLabels labels;
    labels.update(item);
    size_t checksum = 0;
    for (int i = 0; i < iterations; ++i) checksum += consume(labels.heading, labels.secondary);
    return checksum;
}

template <typename Function>
double measure(Function&& function, const JellyfinItem& item, int iterations, size_t& checksum) {
    const auto started = std::chrono::steady_clock::now();
    checksum = function(item, iterations);
    return std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - started).count();
}

int main(int argc, char** argv) {
    const int iterations = argc > 1 ? std::atoi(argv[1]) : 3000000;
    JellyfinItem item;
    item.name = "A Particularly Long Episode Title";
    item.seriesName = "A Particularly Long Television Series Name";
    item.parentIndexNumber = 4;
    item.indexNumber = 12;

    size_t rebuildChecksum = 0;
    size_t cachedChecksum = 0;
    const double rebuildMs = measure(rebuild, item, iterations, rebuildChecksum);
    const double cachedMs = measure(cached, item, iterations, cachedChecksum);
    if (rebuildChecksum != cachedChecksum) return 2;
    std::cout << std::fixed << std::setprecision(3)
              << "iterations=" << iterations
              << " rebuild_ms=" << rebuildMs
              << " cached_ms=" << cachedMs
              << " speedup_pct=" << ((rebuildMs - cachedMs) * 100.0 / rebuildMs)
              << " checksum=" << cachedChecksum << '\n';
    return 0;
}
