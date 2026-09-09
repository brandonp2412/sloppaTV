#include "text_fit.hpp"

#include <chrono>
#include <cctype>
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

static std::string baselineFit(std::string_view value, float maxWidth, int maxLines) {
    if (value.empty() || maxWidth <= 0.0f || maxLines <= 0) return {};
    auto ellipsize = [&](std::string line) {
        line.append("...");
        while (line.size() > 3 && measureText(line) > maxWidth) line.erase(line.end() - 4);
        return line;
    };

    std::string current;
    std::string fitted;
    current.reserve(std::min<size_t>(value.size(), 256));
    fitted.reserve(value.size() + 4);
    int line = 1;
    size_t position = 0;
    while (position < value.size()) {
        while (position < value.size() && std::isspace(static_cast<unsigned char>(value[position]))) ++position;
        if (position >= value.size()) break;
        const size_t start = position;
        while (position < value.size() && !std::isspace(static_cast<unsigned char>(value[position]))) ++position;
        const std::string_view word(value.data() + start, position - start);
        const size_t previousSize = current.size();
        if (previousSize > 0) current.push_back(' ');
        current.append(word);
        if (measureText(current) <= maxWidth) continue;

        current.resize(previousSize);
        if (current.empty()) current = ellipsize(std::string(word));
        if (line >= maxLines) {
            if (!fitted.empty()) fitted += '\n';
            fitted += ellipsize(current);
            return fitted;
        }
        if (!fitted.empty()) fitted += '\n';
        fitted += current;
        current.assign(word);
        ++line;
    }
    if (!current.empty()) {
        if (!fitted.empty()) fitted += '\n';
        fitted += measureText(current) <= maxWidth ? current : ellipsize(current);
    }
    return fitted;
}

template <typename Function>
double measureRun(const std::string& text, int iterations, Function&& function, size_t& checksum) {
    const auto started = std::chrono::steady_clock::now();
    for (int i = 0; i < iterations; ++i) checksum += function(text).size();
    return std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - started).count();
}

int main(int argc, char** argv) {
    const int iterations = argc > 1 ? std::atoi(argv[1]) : 1000000;
    const std::string text = "A considerably longer media title with several words that needs wrapping cleanly across the available television card width";
    const auto optimized = [](std::string_view value) {
        return fitTextLinesMeasured(value, 240.0f, 3, measureText);
    };
    if (baselineFit(text, 240.0f, 3) != optimized(text)) return 2;

    size_t baselineChecksum = 0;
    size_t optimizedChecksum = 0;
    const double baselineMs = measureRun(text, iterations, [](std::string_view value) {
        return baselineFit(value, 240.0f, 3);
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
