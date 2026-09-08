#include "settings_screen.hpp"

#include <chrono>
#include <cstdlib>
#include <iomanip>
#include <iostream>

int main(int argc, char** argv) {
    const int iterations = argc > 1 ? std::atoi(argv[1]) : 2000000;
    SettingsScreenState state;
    state.reset();
    state.setSearchText("subtitle");

    auto measure = [&](bool copy) {
        size_t checksum = 0;
        const auto started = std::chrono::steady_clock::now();
        for (int iteration = 0; iteration < iterations; ++iteration) {
            if (copy) {
                const auto matches = state.matches();
                checksum += matches.size();
                checksum += static_cast<size_t>(matches.front());
            } else {
                const auto& matches = state.matches();
                checksum += matches.size();
                checksum += static_cast<size_t>(matches.front());
            }
        }
        return std::pair{
            std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - started).count(),
            checksum
        };
    };
    const auto [copyMs, copyChecksum] = measure(true);
    const auto [referenceMs, referenceChecksum] = measure(false);
    std::cout << std::fixed << std::setprecision(3)
              << "iterations=" << iterations << " copy_ms=" << copyMs
              << " reference_ms=" << referenceMs
              << " checksum=" << (copyChecksum + referenceChecksum) << '\n';
    return copyChecksum == referenceChecksum && copyChecksum > 0 ? 0 : 1;
}
