#include "text_fit.hpp"

#include <chrono>
#include <cstdlib>
#include <iomanip>
#include <iostream>
#include <string>
#include <string_view>

static float measureText(std::string_view value) {
    return static_cast<float>(value.size()) * 8.0f;
}

static std::string baseline(const std::string& title) {
    std::string primary = title;
    return fitSingleLineMeasured(primary, 280.0f, measureText);
}

static std::string optimized(const std::string& title) {
    const std::string_view primary = title;
    return fitSingleLineMeasured(primary, 280.0f, measureText);
}

template <typename Function>
double measureRun(const std::string& title, int iterations, Function&& function, size_t& checksum) {
    const auto started = std::chrono::steady_clock::now();
    for (int i = 0; i < iterations; ++i) checksum += function(title).size();
    return std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - started).count();
}

int main(int argc, char** argv) {
    const int iterations = argc > 1 ? std::atoi(argv[1]) : 2000000;
    const std::string title = "An unusually descriptive episode title that is long enough to allocate outside SSO";
    if (baseline(title) != optimized(title)) return 2;
    size_t baselineChecksum = 0;
    size_t optimizedChecksum = 0;
    const double baselineMs = measureRun(title, iterations, baseline, baselineChecksum);
    const double optimizedMs = measureRun(title, iterations, optimized, optimizedChecksum);
    if (baselineChecksum != optimizedChecksum) return 3;
    std::cout << std::fixed << std::setprecision(3)
              << "iterations=" << iterations
              << " baseline_ms=" << baselineMs
              << " optimized_ms=" << optimizedMs
              << " speedup_pct=" << ((baselineMs - optimizedMs) * 100.0 / baselineMs)
              << " checksum=" << optimizedChecksum << '\n';
    return 0;
}
