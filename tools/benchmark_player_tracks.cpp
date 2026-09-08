#include "player_tracks.hpp"

#include <chrono>
#include <cstdlib>
#include <iomanip>
#include <iostream>
#include <vector>

int main(int argc, char** argv) {
    const int cueCount = argc > 1 ? std::atoi(argv[1]) : 2000;
    const int passes = argc > 2 ? std::atoi(argv[2]) : 100;
    std::vector<SubtitleCue> cues;
    cues.reserve(static_cast<size_t>(cueCount));
    for (int index = 0; index < cueCount; ++index) {
        const int start = index * 3600;
        cues.push_back({start, start + 2200, "subtitle"});
    }
    PlayerTrackState state;
    state.applySubtitle(1, "eng", std::move(cues));

    size_t hits = 0;
    const int durationMs = cueCount * 3600;
    const auto started = std::chrono::steady_clock::now();
    for (int pass = 0; pass < passes; ++pass) {
        for (int position = 0; position < durationMs; position += 16) {
            hits += state.activeSubtitleCue(position) != nullptr;
        }
    }
    const double elapsedMs = std::chrono::duration<double, std::milli>(
        std::chrono::steady_clock::now() - started
    ).count();
    std::cout << std::fixed << std::setprecision(3)
              << "cues=" << cueCount << " passes=" << passes
              << " elapsed_ms=" << elapsedMs << " hits=" << hits << '\n';
    return hits > 0 ? 0 : 1;
}
