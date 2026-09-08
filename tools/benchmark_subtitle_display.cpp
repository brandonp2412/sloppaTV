#include "subtitle_display.hpp"

#include <algorithm>
#include <chrono>
#include <cstdlib>
#include <iomanip>
#include <iostream>
#include <sstream>
#include <string>
#include <string_view>
#include <vector>

std::string baselineNormalizeSubtitleDisplayText(const std::string& input) {
    const std::string displaySafe = displayText(input, '\0');
    auto attachesToPrevious = [](std::string_view token) {
        if (token.empty()) return false;
        return std::all_of(token.begin(), token.end(), [](unsigned char c) {
            switch (c) {
                case '!': case '?': case '.': case ',': case ';': case ':':
                case '%': case ')': case ']': case '}':
                    return true;
                default:
                    return false;
            }
        });
    };

    std::istringstream words(displaySafe);
    std::vector<std::string> tokens;
    std::string word;
    while (words >> word) {
        if (!tokens.empty() && attachesToPrevious(word)) tokens.back() += word;
        else tokens.push_back(std::move(word));
    }

    std::string output;
    for (const auto& token : tokens) {
        if (!output.empty()) output += ' ';
        output += token;
    }
    return output;
}

template <typename Function>
double measure(const std::vector<std::string>& samples, int iterations, Function&& function, size_t& checksum) {
    const auto started = std::chrono::steady_clock::now();
    for (int iteration = 0; iteration < iterations; ++iteration) {
        for (const auto& sample : samples) checksum += function(sample).size();
    }
    return std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - started).count();
}

int main(int argc, char** argv) {
    const int iterations = argc > 1 ? std::atoi(argv[1]) : 250000;
    const std::vector<std::string> samples{
        "This is a normal subtitle line.",
        "Wait ... what ? Really !",
        "  Multiple   spaces\tand\nnewlines  ",
        "Hello , world ! ( test )",
        "caf\xC3\xA9 \xE2\x80\x94 deja vu\xE2\x80\xA6",
        "[MUSIC] We should go now, quickly.",
    };

    for (const auto& sample : samples) {
        const auto baseline = baselineNormalizeSubtitleDisplayText(sample);
        const auto optimized = normalizeSubtitleDisplayText(sample);
        if (baseline != optimized) {
            std::cerr << "mismatch baseline='" << baseline << "' optimized='" << optimized << "'\n";
            return 2;
        }
    }

    size_t baselineChecksum = 0;
    size_t optimizedChecksum = 0;
    const double baselineMs = measure(samples, iterations, baselineNormalizeSubtitleDisplayText, baselineChecksum);
    const double optimizedMs = measure(samples, iterations, normalizeSubtitleDisplayText, optimizedChecksum);
    if (baselineChecksum != optimizedChecksum) return 3;

    std::cout << std::fixed << std::setprecision(3)
              << "iterations=" << iterations
              << " baseline_ms=" << baselineMs
              << " optimized_ms=" << optimizedMs
              << " speedup_pct=" << ((baselineMs - optimizedMs) * 100.0 / baselineMs)
              << " checksum=" << optimizedChecksum << '\n';
    return 0;
}
