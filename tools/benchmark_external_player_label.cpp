#include "external_player.hpp"

#include <chrono>
#include <cstdlib>
#include <iomanip>
#include <iostream>
#include <string>
#include <string_view>
#include <vector>

static std::string baseline(const std::vector<ExternalPlayerApp>& players, const std::string& component) {
    if (component.empty()) return "INTERNAL";
    const auto selected = std::find_if(players.begin(), players.end(), [&](const ExternalPlayerApp& player) {
        return player.componentName == component;
    });
    return selected == players.end() ? "INTERNAL" : selected->label;
}

static std::string_view optimized(const std::vector<ExternalPlayerApp>& players, const std::string& component) {
    if (component.empty()) return "INTERNAL";
    const auto selected = std::find_if(players.begin(), players.end(), [&](const ExternalPlayerApp& player) {
        return player.componentName == component;
    });
    return selected == players.end() ? std::string_view{"INTERNAL"} : std::string_view{selected->label};
}

int main(int argc, char** argv) {
    const int iterations = argc > 1 ? std::atoi(argv[1]) : 10000000;
    std::vector<ExternalPlayerApp> players(8);
    for (int i = 0; i < 8; ++i) {
        players[static_cast<size_t>(i)].componentName = "app.player." + std::to_string(i) + "/MainActivity";
        players[static_cast<size_t>(i)].label = "A Long External Video Player Label " + std::to_string(i);
    }
    const std::string component = players[6].componentName;
    size_t checksum = 0;
    const auto started = std::chrono::steady_clock::now();
    for (int i = 0; i < iterations; ++i) {
#ifdef BENCH_BASELINE
        checksum += baseline(players, component).size();
#else
        checksum += optimized(players, component).size();
#endif
    }
    const double elapsedMs = std::chrono::duration<double, std::milli>(
        std::chrono::steady_clock::now() - started
    ).count();
    std::cout << std::fixed << std::setprecision(3)
              << "iterations=" << iterations << " elapsed_ms=" << elapsedMs
              << " checksum=" << checksum << '\n';
    return checksum > 0 ? 0 : 1;
}
