#include <algorithm>
#include <array>
#include <charconv>
#include <chrono>
#include <cstdlib>
#include <iomanip>
#include <iostream>
#include <string>
#include <string_view>

__attribute__((noinline)) static size_t baseline(int remaining) {
    const std::string label = std::to_string(remaining) + " REMAINING";
    return label.size();
}

__attribute__((noinline)) static size_t optimized(int remaining) {
    std::array<char, 24> buffer{};
    const auto converted = std::to_chars(buffer.data(), buffer.data() + buffer.size(), remaining);
    const std::string_view suffix = " REMAINING";
    const size_t numberLength = static_cast<size_t>(converted.ptr - buffer.data());
    std::copy(suffix.begin(), suffix.end(), converted.ptr);
    return numberLength + suffix.size();
}

template <typename Function>
double measure(int iterations, Function&& function, size_t& checksum) {
    const auto started = std::chrono::steady_clock::now();
    for (int iteration = 0; iteration < iterations; ++iteration) {
        checksum += function(1 + (iteration % 999));
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
