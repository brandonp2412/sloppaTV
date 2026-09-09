#include "unicode_text.hpp"

#include <algorithm>
#include <array>
#include <chrono>
#include <cstdlib>
#include <iomanip>
#include <iostream>
#include <string>
#include <string_view>

[[gnu::noinline]] static bool oldContainsNonAscii(std::string_view value) {
    return std::any_of(value.begin(), value.end(), [](unsigned char byte) { return byte >= 0x80; });
}

[[gnu::noinline]] static bool newContainsNonAscii(std::string_view value) {
    return containsNonAscii(value);
}

template <typename Function>
static double measure(Function function, const std::array<std::string, 4>& values, int iterations, size_t& checksum) {
    const auto started = std::chrono::steady_clock::now();
    size_t sum = 0;
    for (int iteration = 0; iteration < iterations; ++iteration) {
        const auto& value = values[static_cast<size_t>(iteration) & 3u];
        sum += function(value) ? 1u : value.size();
    }
    checksum = sum;
    return std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - started).count();
}

int main(int argc, char** argv) {
    const int iterations = argc > 1 ? std::atoi(argv[1]) : 20000000;
    const std::array<std::string, 4> values{
        "PLAYBACK QUEUE",
        "LEFT/RIGHT SEEK   |   OK PLAY/PAUSE   |   UP OPTIONS   |   BACK EXIT",
        std::string(128, 'A'),
        std::string(256, 'Z'),
    };
    size_t oldChecksum = 0;
    size_t newChecksum = 0;
    const double oldMs = measure(oldContainsNonAscii, values, iterations, oldChecksum);
    const double newMs = measure(newContainsNonAscii, values, iterations, newChecksum);
    if (oldChecksum != newChecksum) return 2;
    std::cout << std::fixed << std::setprecision(3)
              << "iterations=" << iterations
              << " old_ms=" << oldMs
              << " new_ms=" << newMs
              << " speedup_pct=" << ((oldMs - newMs) * 100.0 / oldMs)
              << " checksum=" << newChecksum << '\n';
    return 0;
}
