#include "text_fit.hpp"

#include <chrono>
#include <cstdlib>
#include <iomanip>
#include <iostream>
#include <string>
#include <string_view>

static float measureText(std::string_view value) {
    float width = 0.0f;
    for (unsigned char c : value) width += c == ' ' ? 4.0f : 8.0f;
    return width;
}

static std::string baselineSingleLine(std::string value, float maxWidth) {
    if (measureText(value) <= maxWidth) return value;
    while (value.size() > 4 && measureText(value + "...") > maxWidth) value.pop_back();
    return value + "...";
}

template <typename Function>
double measureRun(const std::string& text, int iterations, Function&& function, size_t& checksum) {
    const auto started = std::chrono::steady_clock::now();
    for (int i = 0; i < iterations; ++i) checksum += function(text).size();
    return std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - started).count();
}

int main(int argc, char** argv) {
    const int iterations = argc > 1 ? std::atoi(argv[1]) : 1000000;
    const std::string text = "A deliberately long television episode title that must be ellipsized for a compact home card";
    constexpr float maxWidth = 280.0f;
    const auto optimized = [](std::string_view value) {
        return fitSingleLineMeasured(value, maxWidth, measureText);
    };
    if (baselineSingleLine(text, maxWidth) != optimized(text)) return 2;

    size_t baselineChecksum = 0;
    size_t optimizedChecksum = 0;
    const double baselineMs = measureRun(text, iterations, [](const std::string& value) {
        return baselineSingleLine(value, maxWidth);
    }, baselineChecksum);
    const double optimizedMs = measureRun(text, iterations, optimized, optimizedChecksum);
    if (baselineChecksum != optimizedChecksum) return 3;

    std::cout << std::fixed << std::setprecision(3)
              << "iterations=" << iterations
              << " baseline_ms=" << baselineMs
              << " optimized_ms=" << optimizedMs
              << " speedup_pct=" << ((baselineMs - optimizedMs) * 100.0 / baselineMs)
              << " checksum=" << optimizedChecksum << '\n';
    return 0;
}
