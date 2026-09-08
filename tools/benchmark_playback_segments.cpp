#include "playback_session.hpp"

#include <chrono>
#include <cstdlib>
#include <iomanip>
#include <iostream>
#include <vector>

int main(int argc, char** argv) {
    const int iterations = argc > 1 ? std::atoi(argv[1]) : 5000000;
    PlaybackSessionState state;
    std::vector<JellyfinMediaSegment> segments;
    segments.reserve(24);
    for (int index = 0; index < 24; ++index) {
        const int64_t start = static_cast<int64_t>(index) * 120'000'000;
        segments.push_back({index == 10 ? "Intro" : "Chapter", start, start + 60'000'000});
    }
    state.setMediaSegments(std::move(segments));

    size_t checksum = 0;
    const auto started = std::chrono::steady_clock::now();
    for (int iteration = 0; iteration < iterations; ++iteration) {
        const int64_t position = static_cast<int64_t>(iteration % 24) * 120'000'000 + 10'000'000;
        const auto segment = state.activeSkippableSegment(position);
        if (segment) checksum += segment->type.size();
    }
    const double elapsedMs = std::chrono::duration<double, std::milli>(
        std::chrono::steady_clock::now() - started
    ).count();
    std::cout << std::fixed << std::setprecision(3)
              << "iterations=" << iterations << " elapsed_ms=" << elapsedMs
              << " checksum=" << checksum << '\n';
    return checksum > 0 ? 0 : 1;
}
