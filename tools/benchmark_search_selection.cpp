#include "search_screen.hpp"

#include <chrono>
#include <cstdlib>
#include <iomanip>
#include <iostream>
#include <string>
#include <vector>

int main(int argc, char** argv) {
    const int iterations = argc > 1 ? std::atoi(argv[1]) : 10000000;
    SearchScreenState state;
    state.setQuery("benchmark");
    std::vector<JellyfinItem> results(100);
    for (int index = 0; index < 100; ++index) {
        results[static_cast<size_t>(index)].id = std::to_string(index);
        results[static_cast<size_t>(index)].type = index < 50 ? "Movie" : "Episode";
    }
    if (!state.finishSearch("benchmark", std::move(results))) return 1;

    size_t checksum = 0;
    const auto started = std::chrono::steady_clock::now();
    for (int iteration = 0; iteration < iterations; ++iteration) {
        state.setSelection(iteration % 100);
        checksum += static_cast<size_t>(state.selectedRow());
        checksum += static_cast<size_t>(state.selectionOnFirstResultRow());
    }
    const double elapsedMs = std::chrono::duration<double, std::milli>(
        std::chrono::steady_clock::now() - started
    ).count();
    std::cout << std::fixed << std::setprecision(3)
              << "iterations=" << iterations << " elapsed_ms=" << elapsedMs
              << " checksum=" << checksum << '\n';
    return checksum > 0 ? 0 : 1;
}
