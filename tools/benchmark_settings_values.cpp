#include "settings_screen.hpp"

#include <array>
#include <chrono>
#include <cstdlib>
#include <iomanip>
#include <iostream>

int main(int argc, char** argv) {
    const int iterations = argc > 1 ? std::atoi(argv[1]) : 500000;
    AppSettings settings;
    settings.maxBitrateMbps = 80;
    settings.maxAudioChannels = 8;
    settings.autoSubtitles = true;
    settings.autoSubtitleLanguage = "mri";
    settings.autoSubtitleSourceLanguage = "different";
    const std::array<int, 6> visible{18, 10, 11, 13, 12, kSubtitleLanguagesSetting};

    size_t checksum = 0;
    const auto started = std::chrono::steady_clock::now();
    for (int iteration = 0; iteration < iterations; ++iteration) {
#ifdef OPTIMIZED_SETTINGS_VALUES
        for (const int index : visible) {
            checksum += settingValue(settings, index, 6, "MPV", "viewer", false).size();
        }
#else
        const auto values = settingsValues(settings, 6, "MPV", "viewer", false);
        for (const int index : visible) checksum += values[static_cast<size_t>(index)].size();
#endif
    }
    const double elapsedMs = std::chrono::duration<double, std::milli>(
        std::chrono::steady_clock::now() - started
    ).count();
    std::cout << std::fixed << std::setprecision(3)
              << "iterations=" << iterations << " elapsed_ms=" << elapsedMs << " checksum=" << checksum << '\n';
    return checksum > 0 ? 0 : 1;
}
