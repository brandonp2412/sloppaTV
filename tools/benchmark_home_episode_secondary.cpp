#include "media_labels.hpp"

#include <array>
#include <chrono>
#include <cstdlib>
#include <iomanip>
#include <iostream>
#include <string>

static std::array<JellyfinItem, 5> makeItems() {
    std::array<JellyfinItem, 5> items;
    for (size_t i = 0; i < items.size(); ++i) {
        items[i].name = "Episode With A Moderately Long Display Title " + std::to_string(i);
        items[i].seriesName = "Series";
        items[i].parentIndexNumber = 3;
        items[i].indexNumber = 20 + static_cast<int>(i);
    }
    return items;
}

__attribute__((noinline)) static size_t baseline(const std::array<JellyfinItem, 5>& items) {
    size_t checksum = 0;
    for (const auto& item : items) {
        std::string episode = episodeNumberLabel(item);
        if (!item.name.empty() && item.name != item.seriesName) {
            if (!episode.empty()) episode += "  |  ";
            episode += item.name;
        }
        checksum += episode.size();
    }
    return checksum;
}

__attribute__((noinline)) static size_t optimized(const std::array<JellyfinItem, 5>& items, std::string& scratch) {
    size_t checksum = 0;
    for (const auto& item : items) {
        episodeNumberLabelInto(scratch, item);
        if (!item.name.empty() && item.name != item.seriesName) {
            if (!scratch.empty()) scratch += "  |  ";
            scratch += item.name;
        }
        checksum += scratch.size();
    }
    return checksum;
}

template <typename Function>
double measure(int iterations, Function&& function, size_t& checksum) {
    const auto started = std::chrono::steady_clock::now();
    for (int iteration = 0; iteration < iterations; ++iteration) checksum += function();
    return std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - started).count();
}

int main(int argc, char** argv) {
    const int iterations = argc > 1 ? std::atoi(argv[1]) : 3000000;
    const auto items = makeItems();
    std::string scratch;
    if (baseline(items) != optimized(items, scratch)) return 2;
    size_t baselineChecksum = 0;
    size_t optimizedChecksum = 0;
    const double baselineMs = measure(iterations, [&] { return baseline(items); }, baselineChecksum);
    const double optimizedMs = measure(iterations, [&] { return optimized(items, scratch); }, optimizedChecksum);
    if (baselineChecksum != optimizedChecksum) return 3;
    std::cout << std::fixed << std::setprecision(3)
              << "iterations=" << iterations
              << " baseline_ms=" << baselineMs
              << " optimized_ms=" << optimizedMs
              << " speedup_pct=" << ((baselineMs - optimizedMs) * 100.0 / baselineMs)
              << " checksum=" << optimizedChecksum << '\n';
    return 0;
}
