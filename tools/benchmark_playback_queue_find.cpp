#include "playback_queue.hpp"

#include <chrono>
#include <cstdlib>
#include <iomanip>
#include <iostream>
#include <string>
#include <vector>

int main(int argc, char** argv) {
    const int iterations = argc > 1 ? std::atoi(argv[1]) : 5000000;
    std::vector<JellyfinItem> items(100);
    for (int index = 0; index < 100; ++index) items[static_cast<size_t>(index)].id = "item-" + std::to_string(index);

    PlaybackQueueState state;
    state.replace(std::move(items), 50);

    size_t checksum = 0;
    const auto started = std::chrono::steady_clock::now();
    for (int iteration = 0; iteration < iterations; ++iteration) {
        checksum += static_cast<size_t>(state.findItemIndex("item-50") + 1);
        checksum += static_cast<size_t>(state.findItemIndex("item-99") + 1);
    }
    const double elapsedMs = std::chrono::duration<double, std::milli>(
        std::chrono::steady_clock::now() - started
    ).count();
    std::cout << std::fixed << std::setprecision(3)
              << "iterations=" << iterations << " elapsed_ms=" << elapsedMs
              << " checksum=" << checksum << '\n';
    return checksum > 0 ? 0 : 1;
}
