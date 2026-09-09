#include <array>
#include <chrono>
#include <cstdlib>
#include <iomanip>
#include <iostream>
#include <string>
#include <string_view>

int main(int argc, char** argv) {
    const int iterations = argc > 1 ? std::atoi(argv[1]) : 10000000;
    size_t checksum = 0;
    const auto started = std::chrono::steady_clock::now();
    for (int i = 0; i < iterations; ++i) {
#ifdef BENCH_BASELINE
        const std::array<std::string, 3> navLabels{"HOME", "SEARCH", "SETTINGS"};
        const auto useLabel = [&](const std::string& label) { checksum += label.size(); };
#else
        static constexpr std::array<std::string_view, 3> navLabels{"HOME", "SEARCH", "SETTINGS"};
        const auto useLabel = [&](std::string_view label) { checksum += label.size(); };
#endif
        checksum += navLabels[static_cast<size_t>(i % 3)].size();
        useLabel((i & 1) == 0 ? "MOVIES & SHOWS" : "EPISODES");
    }
    const double elapsedMs = std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - started).count();
    std::cout << std::fixed << std::setprecision(3)
              << "iterations=" << iterations << " elapsed_ms=" << elapsedMs
              << " checksum=" << checksum << '\n';
    return checksum > 0 ? 0 : 1;
}
