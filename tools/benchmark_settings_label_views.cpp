#include "app_settings.hpp"

#include <chrono>
#include <cstdlib>
#include <iomanip>
#include <iostream>
#include <string>
#include <string_view>

int main(int argc, char** argv) {
    const int iterations = argc > 1 ? std::atoi(argv[1]) : 10000000;
    const auto& labels = settingsLabels();
    size_t checksum = 0;
    const auto started = std::chrono::steady_clock::now();
    for (int iteration = 0; iteration < iterations; ++iteration) {
        const int index = iteration % static_cast<int>(labels.size());
#ifdef BENCH_BASELINE
        const std::string rowLabel = index == kAdvancedSettingsToggle
            ? "BASIC SETTINGS"
            : labels[static_cast<size_t>(index)];
        const std::string languageLabel = index % 3 == 0
            ? "ALL LANGUAGES"
            : kSubtitleLanguageOptions[static_cast<size_t>(index % kSubtitleLanguageOptions.size())].label;
#else
        const std::string_view rowLabel = index == kAdvancedSettingsToggle
            ? std::string_view{"BASIC SETTINGS"}
            : std::string_view{labels[static_cast<size_t>(index)]};
        const std::string_view languageLabel = index % 3 == 0
            ? std::string_view{"ALL LANGUAGES"}
            : std::string_view{kSubtitleLanguageOptions[static_cast<size_t>(index % kSubtitleLanguageOptions.size())].label};
#endif
        checksum += rowLabel.size() + languageLabel.size();
    }
    const double elapsedMs = std::chrono::duration<double, std::milli>(
        std::chrono::steady_clock::now() - started
    ).count();
    std::cout << std::fixed << std::setprecision(3)
              << "iterations=" << iterations << " elapsed_ms=" << elapsedMs
              << " checksum=" << checksum << '\n';
    return checksum > 0 ? 0 : 1;
}
