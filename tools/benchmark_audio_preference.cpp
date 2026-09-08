#include "audio_policy.hpp"

#include <chrono>
#include <cstdlib>
#include <iomanip>
#include <iostream>
#include <string>
#include <vector>

int main(int argc, char** argv) {
    const int candidates = argc > 1 ? std::atoi(argv[1]) : 100;
    const int iterations = argc > 2 ? std::atoi(argv[2]) : 200000;
    std::vector<AudioPreferenceCandidate> audios;
    audios.reserve(static_cast<size_t>(candidates));
    for (int index = 0; index < candidates; ++index) {
        audios.push_back({index, index + 1 == candidates ? "EN-US" : "language-" + std::to_string(index)});
    }

    int result = -1;
    const auto started = std::chrono::steady_clock::now();
    for (int iteration = 0; iteration < iterations; ++iteration) {
        result = audioIndexForQueuePreference(audios, std::string{"en-us"});
    }
    const double elapsedMs = std::chrono::duration<double, std::milli>(
        std::chrono::steady_clock::now() - started
    ).count();
    std::cout << std::fixed << std::setprecision(3)
              << "candidates=" << candidates << " iterations=" << iterations
              << " elapsed_ms=" << elapsedMs << " result=" << result << '\n';
    return result == candidates - 1 ? 0 : 1;
}
