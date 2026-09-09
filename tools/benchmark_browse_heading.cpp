#include "browse_screen.hpp"

#include <chrono>
#include <cstdlib>
#include <iomanip>
#include <iostream>

int main(int argc, char** argv) {
    const int iterations = argc > 1 ? std::atoi(argv[1]) : 2000000;
    BrowseScreenState state;
    JellyfinItem movies;
    movies.id = "movies";
    movies.name = "Movies";
    movies.collectionType = "movies";
    state.resetForLibrary(movies);
    state.selectGenre("Documentary");

    size_t checksum = 0;
    const auto started = std::chrono::steady_clock::now();
    for (int iteration = 0; iteration < iterations; ++iteration) {
        const auto heading = state.heading();
        checksum += heading.size();
        checksum += static_cast<unsigned char>(heading.front());
    }
    const double elapsedMs = std::chrono::duration<double, std::milli>(
        std::chrono::steady_clock::now() - started
    ).count();
    std::cout << std::fixed << std::setprecision(3)
              << "iterations=" << iterations << " elapsed_ms=" << elapsedMs
              << " checksum=" << checksum << '\n';
    return checksum > 0 ? 0 : 1;
}
