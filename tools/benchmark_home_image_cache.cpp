#include "home_image_disk_cache.hpp"

#include <chrono>
#include <cstdlib>
#include <filesystem>
#include <iomanip>
#include <iostream>
#include <string>

int main(int argc, char** argv) {
    namespace fs = std::filesystem;
    const int iterations = argc > 1 ? std::atoi(argv[1]) : 300;
    const int kib = argc > 2 ? std::atoi(argv[2]) : 256;
    const fs::path root = fs::temp_directory_path() / "sloppatv-home-image-cache-benchmark";
    std::error_code ec;
    fs::remove_all(root, ec);

    HomeImageDiskCache cache;
    cache.setDataPath(root.string());
    const std::string payload(static_cast<size_t>(kib) * 1024, 'x');
    cache.write("benchmark-artwork-key", payload);

    size_t totalBytes = 0;
    const auto started = std::chrono::steady_clock::now();
    for (int iteration = 0; iteration < iterations; ++iteration) {
        const auto value = cache.read("benchmark-artwork-key");
        if (!value || value->size() != payload.size()) return 1;
        totalBytes += value->size();
    }
    const double elapsedMs = std::chrono::duration<double, std::milli>(
        std::chrono::steady_clock::now() - started
    ).count();
    const auto writeStarted = std::chrono::steady_clock::now();
    for (int iteration = 0; iteration < iterations; ++iteration) {
        cache.write("benchmark-write-" + std::to_string(iteration), payload);
    }
    const double writeElapsedMs = std::chrono::duration<double, std::milli>(
        std::chrono::steady_clock::now() - writeStarted
    ).count();
    fs::remove_all(root, ec);

    std::cout << std::fixed << std::setprecision(3)
              << "iterations=" << iterations << " kib=" << kib
              << " read_ms=" << elapsedMs
              << " read_mib_per_s=" << (static_cast<double>(totalBytes) / (1024.0 * 1024.0)) / (elapsedMs / 1000.0)
              << " write_ms=" << writeElapsedMs
              << '\n';
    return 0;
}
