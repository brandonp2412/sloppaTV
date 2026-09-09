#include "settings_screen.hpp"

#include <chrono>
#include <cstdlib>
#include <iomanip>
#include <iostream>

int main(int argc, char** argv) {
    const int iterations = argc > 1 ? std::atoi(argv[1]) : 1000000;
    AppSettings settings;
    settings.autoSubtitleSourceLanguage = "en-us";
    size_t checksum = 0;
    const auto started = std::chrono::steady_clock::now();
    for (int iteration = 0; iteration < iterations; ++iteration) {
        const int direction = (iteration & 1) == 0 ? 1 : -1;
        const auto effect = adjustSetting(settings, kAutoSubtitleSourceSetting, direction);
        checksum += settings.autoSubtitleSourceLanguage.size();
        checksum += static_cast<size_t>(effect == SettingChangeEffect::Save);
    }
    const double elapsedMs = std::chrono::duration<double, std::milli>(
        std::chrono::steady_clock::now() - started
    ).count();
    std::cout << std::fixed << std::setprecision(3)
              << "iterations=" << iterations << " elapsed_ms=" << elapsedMs
              << " checksum=" << checksum << '\n';
    return checksum > 0 ? 0 : 1;
}
