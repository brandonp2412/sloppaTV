#include <array>
#include <charconv>
#include <chrono>
#include <cstdlib>
#include <iomanip>
#include <iostream>
#include <string>
#include <string_view>

__attribute__((noinline)) static size_t baseline(int index, int current) {
    const bool isCurrent = index == current;
    const std::string marker = isCurrent ? "CURRENT" : (index == current + 1 ? "NEXT" : std::to_string(index - current + 1));
    return marker.size();
}

__attribute__((noinline)) static size_t optimized(int index, int current) {
    const bool isCurrent = index == current;
    std::array<char, 12> markerBuffer{};
    std::string_view marker;
    if (isCurrent) marker = "CURRENT";
    else if (index == current + 1) marker = "NEXT";
    else {
        const auto converted = std::to_chars(markerBuffer.data(), markerBuffer.data() + markerBuffer.size(), index - current + 1);
        marker = std::string_view(markerBuffer.data(), static_cast<size_t>(converted.ptr - markerBuffer.data()));
    }
    return marker.size();
}

template <typename Function>
double measure(int iterations, Function&& function, size_t& checksum) {
    const auto started = std::chrono::steady_clock::now();
    for (int iteration = 0; iteration < iterations; ++iteration) {
        const int current = iteration % 11;
        const int index = current + (iteration % 5);
        checksum += function(index, current);
    }
    return std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - started).count();
}

int main(int argc, char** argv) {
    const int iterations = argc > 1 ? std::atoi(argv[1]) : 30000000;
    size_t baselineChecksum = 0;
    size_t optimizedChecksum = 0;
    const double baselineMs = measure(iterations, baseline, baselineChecksum);
    const double optimizedMs = measure(iterations, optimized, optimizedChecksum);
    if (baselineChecksum != optimizedChecksum) return 2;
    std::cout << std::fixed << std::setprecision(3)
              << "iterations=" << iterations
              << " baseline_ms=" << baselineMs
              << " optimized_ms=" << optimizedMs
              << " speedup_pct=" << ((baselineMs - optimizedMs) * 100.0 / baselineMs)
              << " checksum=" << optimizedChecksum << '\n';
    return 0;
}
