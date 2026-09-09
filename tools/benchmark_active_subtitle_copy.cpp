#include <chrono>
#include <cstdlib>
#include <iomanip>
#include <iostream>
#include <string>

namespace {

void baseline(std::string& scratch, const std::string& playerSubtitle, const std::string& activeCue) {
    scratch = playerSubtitle;
    scratch = activeCue;
}

void optimized(std::string& scratch, const std::string&, const std::string& activeCue) {
    scratch = activeCue;
}

template <typename Function>
double measure(int iterations, Function&& function, size_t& checksum) {
    const std::string playerSubtitle = "Native subtitle text that is immediately discarded";
    const std::string activeCue = "Representative external subtitle cue used by SloppaTV";
    std::string scratch;
    const auto started = std::chrono::steady_clock::now();
    for (int iteration = 0; iteration < iterations; ++iteration) {
        function(scratch, playerSubtitle, activeCue);
        checksum += scratch.size();
    }
    return std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - started).count();
}

}

int main(int argc, char** argv) {
    const int iterations = argc > 1 ? std::atoi(argv[1]) : 10000000;
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
