#include "home_screen.hpp"

#include <chrono>
#include <cstdlib>
#include <iomanip>
#include <iostream>

int main(int argc, char** argv) {
    const int iterations = argc > 1 ? std::atoi(argv[1]) : 5000000;
    const std::string itemId = "episode-1234567890";
    const std::string primaryTag = "primary-tag-1234567890";
    const std::string seriesId = "series-1234567890";
    const std::string seriesTag = "series-tag-1234567890";
    const std::string thumbTag = "thumb-tag-1234567890";
    const std::string backdropTag = "backdrop-tag-1234567890";
    const std::string backdropItemId = "series-1234567890";

    size_t checksum = 0;
    const auto started = std::chrono::steady_clock::now();
    for (int i = 0; i < iterations; ++i) {
        const ArtworkReference artwork = homeArtworkReference(
            itemId,
            primaryTag,
            seriesId,
            seriesTag,
            true,
            thumbTag,
            backdropTag,
            backdropItemId
        );
        asm volatile("" : : "r"(artwork.itemId.data()), "r"(artwork.tag.data()) : "memory");
        checksum += artwork.itemId.size() + artwork.tag.size() + static_cast<size_t>(artwork.kind);
    }
    const double elapsed = std::chrono::duration<double, std::milli>(
        std::chrono::steady_clock::now() - started
    ).count();
    std::cout << std::fixed << std::setprecision(3)
              << "iterations=" << iterations
              << " elapsed_ms=" << elapsed
              << " checksum=" << checksum << '\n';
}
