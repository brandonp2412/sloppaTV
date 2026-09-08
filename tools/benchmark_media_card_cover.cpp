#include "jellyfin_types.hpp"

#include <chrono>
#include <cstdlib>
#include <iomanip>
#include <iostream>
#include <string>

static size_t baseline(const JellyfinItem& item, int iterations) {
    size_t checksum = 0;
    for (int i = 0; i < iterations; ++i) {
        JellyfinItem cover = item;
        checksum += cover.id.size() + cover.imageTag.size() + cover.genres.size() + cover.people.size();
    }
    return checksum;
}

static size_t optimized(const JellyfinItem& item, int iterations) {
    size_t checksum = 0;
    for (int i = 0; i < iterations; ++i) {
        const JellyfinItem* cover = &item;
        JellyfinItem seriesCover;
        const bool seriesCoverForEpisode = false;
        if (seriesCoverForEpisode) {
            seriesCover.id = item.seriesId;
            seriesCover.imageTag = item.seriesPrimaryImageTag;
            seriesCover.type = "Series";
            cover = &seriesCover;
        }
        checksum += cover->id.size() + cover->imageTag.size() + cover->genres.size() + cover->people.size();
    }
    return checksum;
}

template <typename Function>
double measure(Function&& function, const JellyfinItem& item, int iterations, size_t& checksum) {
    const auto started = std::chrono::steady_clock::now();
    checksum = function(item, iterations);
    return std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - started).count();
}

int main(int argc, char** argv) {
    const int iterations = argc > 1 ? std::atoi(argv[1]) : 1000000;
    JellyfinItem item;
    item.id = "episode-id";
    item.imageTag = "primary-image-tag";
    item.overview = std::string(600, 'o');
    item.genres = {"Drama", "Science Fiction", "Mystery", "Adventure", "Comedy"};
    item.cast.resize(12, "Performer Name");
    item.people.resize(12);
    item.audios.resize(4);
    item.subtitles.resize(6);

    size_t baselineChecksum = 0;
    size_t optimizedChecksum = 0;
    const double baselineMs = measure(baseline, item, iterations, baselineChecksum);
    const double optimizedMs = measure(optimized, item, iterations, optimizedChecksum);
    if (baselineChecksum != optimizedChecksum) return 2;
    std::cout << std::fixed << std::setprecision(3)
              << "iterations=" << iterations
              << " baseline_ms=" << baselineMs
              << " optimized_ms=" << optimizedMs
              << " speedup_pct=" << ((baselineMs - optimizedMs) * 100.0 / baselineMs)
              << " checksum=" << optimizedChecksum << '\n';
    return 0;
}
