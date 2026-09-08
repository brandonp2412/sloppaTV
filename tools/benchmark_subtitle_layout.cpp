#include "subtitle_display.hpp"

#include <chrono>
#include <cstdlib>
#include <iomanip>
#include <iostream>
#include <sstream>
#include <string>
#include <string_view>
#include <vector>

static std::vector<std::string> baselineSplit(const std::string& subtitle) {
    std::vector<std::string> lines;
    std::istringstream stream(subtitle);
    std::string line;
    while (std::getline(stream, line)) {
        if (line.empty()) continue;
        lines.push_back(line);
    }
    if (lines.empty()) lines.push_back(subtitle);
    return lines;
}

static std::vector<std::string> optimizedSplit(std::string_view subtitle) {
    std::vector<std::string> lines;
    splitSubtitleDisplayLines(subtitle, lines);
    return lines;
}

template <typename Function>
double measure(const std::string& subtitle, int iterations, Function&& function, size_t& checksum) {
    const auto started = std::chrono::steady_clock::now();
    for (int i = 0; i < iterations; ++i) {
        const auto lines = function(subtitle);
        for (const auto& line : lines) checksum += line.size();
    }
    return std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - started).count();
}

int main(int argc, char** argv) {
    const int iterations = argc > 1 ? std::atoi(argv[1]) : 1000000;
    const std::string subtitle = "I have crossed oceans of time\nto find you.\n\nReally !";
    if (baselineSplit(subtitle) != optimizedSplit(subtitle)) return 2;

    size_t baselineChecksum = 0;
    size_t optimizedChecksum = 0;
    const double baselineMs = measure(subtitle, iterations, baselineSplit, baselineChecksum);
    const double optimizedMs = measure(subtitle, iterations, optimizedSplit, optimizedChecksum);
    if (baselineChecksum != optimizedChecksum) return 3;

    std::cout << std::fixed << std::setprecision(3)
              << "iterations=" << iterations
              << " baseline_ms=" << baselineMs
              << " optimized_ms=" << optimizedMs
              << " speedup_pct=" << ((baselineMs - optimizedMs) * 100.0 / baselineMs)
              << " checksum=" << optimizedChecksum << '\n';
    return 0;
}
