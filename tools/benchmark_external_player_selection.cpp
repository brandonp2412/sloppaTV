#include "external_player.hpp"

#include <algorithm>
#include <chrono>
#include <cstdlib>
#include <iomanip>
#include <iostream>
#include <optional>
#include <string>
#include <vector>

static auto configured(const std::vector<ExternalPlayerApp>& players, const std::string& component) {
    if (component.empty()) return players.cend();
    return std::find_if(players.cbegin(), players.cend(), [&](const ExternalPlayerApp& player) {
        return player.componentName == component;
    });
}

static std::optional<ExternalPlayerApp> baseline(const std::vector<ExternalPlayerApp>& players, const std::string& component) {
    const auto selected = configured(players, component);
    if (selected == players.cend()) return std::nullopt;
    return *selected;
}

static const ExternalPlayerApp* optimized(const std::vector<ExternalPlayerApp>& players, const std::string& component) {
    const auto selected = configured(players, component);
    return selected == players.cend() ? nullptr : &*selected;
}

int main(int argc, char** argv) {
    const int iterations = argc > 1 ? std::atoi(argv[1]) : 10000000;
    std::vector<ExternalPlayerApp> players(8);
    for (int i = 0; i < 8; ++i) {
        auto& player = players[static_cast<size_t>(i)];
        player.componentName = "app.player." + std::to_string(i) + "/MainActivity";
        player.label = "A Long External Video Player Label " + std::to_string(i);
    }
    const std::string component = players[6].componentName;
    size_t checksum = 0;
    const auto started = std::chrono::steady_clock::now();
    for (int i = 0; i < iterations; ++i) {
#ifdef BENCH_BASELINE
        const auto selected = baseline(players, component);
        if (selected) checksum += selected->label.size();
#else
        const auto* selected = optimized(players, component);
        if (selected) checksum += selected->label.size();
#endif
    }
    const double elapsedMs = std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - started).count();
    std::cout << std::fixed << std::setprecision(3)
              << "iterations=" << iterations << " elapsed_ms=" << elapsedMs
              << " checksum=" << checksum << '\n';
    return checksum > 0 ? 0 : 1;
}
