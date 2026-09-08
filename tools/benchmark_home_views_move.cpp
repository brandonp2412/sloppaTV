#include "jellyfin.hpp"

#include <chrono>
#include <cstdlib>
#include <iomanip>
#include <iostream>
#include <string>
#include <vector>

namespace {
JellyfinHomeData makeHomeTemplate() {
    JellyfinHomeData home;
    home.views.resize(12);
    for (size_t i = 0; i < home.views.size(); ++i) {
        auto& view = home.views[i];
        view.id = "view-" + std::to_string(i) + "-0123456789abcdef";
        view.name = "Library " + std::to_string(i);
        view.collectionType = i % 2 == 0 ? "movies" : "tvshows";
        view.imageTag = "tag-0123456789abcdef" + std::to_string(i);
    }
    home.rows.resize(8);
    return home;
}
}

int main(int argc, char** argv) {
    const int iterations = argc > 1 ? std::atoi(argv[1]) : 500000;
    const JellyfinHomeData source = makeHomeTemplate();
    size_t checksum = 0;

    const auto started = std::chrono::steady_clock::now();
    for (int iteration = 0; iteration < iterations; ++iteration) {
        JellyfinHomeData core = source;
#ifdef BENCH_BASELINE
        std::vector<JellyfinItem> views = core.views;
#else
        std::vector<JellyfinItem> views = std::move(core.views);
#endif
        JellyfinHomeData destination = std::move(core);
        checksum += views.size() + destination.rows.size();
    }
    const double elapsedMs = std::chrono::duration<double, std::milli>(
        std::chrono::steady_clock::now() - started
    ).count();
    std::cout << std::fixed << std::setprecision(3)
              << "iterations=" << iterations << " elapsed_ms=" << elapsedMs
              << " checksum=" << checksum << '\n';
    return checksum > 0 ? 0 : 1;
}
