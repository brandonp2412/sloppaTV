#include <array>
#include <chrono>
#include <cstdlib>
#include <iomanip>
#include <iostream>
#include <string>
#include <string_view>

template <typename Actions>
static size_t checksumActions(const Actions& actions) {
    size_t checksum = 0;
    for (const auto& action : actions) {
        checksum += action.size();
        for (const unsigned char c : action) checksum = checksum * 33 + c;
    }
    return checksum;
}

__attribute__((noinline)) static size_t baseline(std::string_view repeatMode) {
    const std::array<std::string, 7> actions{
        "PLAY NOW", "PLAY NEXT", "MOVE UP", "MOVE DOWN", "REMOVE", "SHUFFLE",
        std::string("REPEAT ") + std::string(repeatMode),
    };
    return checksumActions(actions);
}

__attribute__((noinline)) static size_t optimized(std::string_view repeatMode) {
    const std::string_view repeatAction = repeatMode == "ONE" ? "REPEAT ONE"
        : (repeatMode == "ALL" ? "REPEAT ALL" : "REPEAT OFF");
    const std::array<std::string_view, 7> actions{
        "PLAY NOW", "PLAY NEXT", "MOVE UP", "MOVE DOWN", "REMOVE", "SHUFFLE", repeatAction,
    };
    return checksumActions(actions);
}

template <typename Function>
double measure(int iterations, Function&& function, size_t& checksum) {
    const auto started = std::chrono::steady_clock::now();
    const std::array<std::string_view, 3> modes{"OFF", "ONE", "ALL"};
    for (int iteration = 0; iteration < iterations; ++iteration) checksum += function(modes[static_cast<size_t>(iteration) % modes.size()]);
    return std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - started).count();
}

int main(int argc, char** argv) {
    const int iterations = argc > 1 ? std::atoi(argv[1]) : 10000000;
    size_t baselineChecksum = 0;
    size_t optimizedChecksum = 0;
    const double baselineMs = measure(iterations, baseline, baselineChecksum);
    const double optimizedMs = measure(iterations, optimized, optimizedChecksum);
    if (baselineChecksum != optimizedChecksum) return 2;
    std::cout << std::fixed << std::setprecision(3)
              << "iterations=" << iterations
              << " baseline_ms=" << baselineMs
              << " optimized_ms=" << optimizedMs
              << " speedup_pct=" << ((baselineMs - optimizedMs) * 100.0 / baselineMs)
              << " checksum=" << optimizedChecksum << '\n';
    return 0;
}
