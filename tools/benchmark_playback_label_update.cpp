#include "media_labels.hpp"

#include <chrono>
#include <cstdlib>
#include <iomanip>
#include <iostream>

static void baselineUpdate(PlaybackLabels& labels, const JellyfinItem& item) {
    labels.heading = item.seriesName.empty() ? item.name : item.seriesName;
    const std::string number = episodeNumberLabel(item);
    labels.secondary = item.seriesName.empty()
        ? episodeLabel(item)
        : number + (item.name.empty() ? "" : "  |  " + item.name);
}

static void optimizedUpdate(PlaybackLabels& labels, const JellyfinItem& item) {
    labels.update(item);
}

template <typename Function>
double measure(int iterations, const JellyfinItem& item, Function&& function, size_t& checksum) {
    PlaybackLabels labels;
    const auto started = std::chrono::steady_clock::now();
    for (int iteration = 0; iteration < iterations; ++iteration) {
        function(labels, item);
        checksum += labels.heading.size() + labels.secondary.size();
    }
    return std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - started).count();
}

int main(int argc, char** argv) {
    const int iterations = argc > 1 ? std::atoi(argv[1]) : 10000000;
    JellyfinItem item;
    item.name = "Standalone episode-like media title";
    item.parentIndexNumber = 2;
    item.indexNumber = 7;

    PlaybackLabels baselineLabels;
    PlaybackLabels optimizedLabels;
    baselineUpdate(baselineLabels, item);
    optimizedUpdate(optimizedLabels, item);
    if (baselineLabels.heading != optimizedLabels.heading || baselineLabels.secondary != optimizedLabels.secondary) return 2;

    size_t baselineChecksum = 0;
    size_t optimizedChecksum = 0;
    const double baselineMs = measure(iterations, item, baselineUpdate, baselineChecksum);
    const double optimizedMs = measure(iterations, item, optimizedUpdate, optimizedChecksum);
    if (baselineChecksum != optimizedChecksum) return 3;
    std::cout << std::fixed << std::setprecision(3)
              << "iterations=" << iterations
              << " baseline_ms=" << baselineMs
              << " optimized_ms=" << optimizedMs
              << " speedup_pct=" << ((baselineMs - optimizedMs) * 100.0 / baselineMs)
              << " checksum=" << optimizedChecksum << '\n';
    return 0;
}
