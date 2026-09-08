#include "home_screen.hpp"

#include <chrono>
#include <cstdlib>
#include <iomanip>
#include <iostream>
#include <string>
#include <vector>

int main(int argc, char** argv) {
    const int iterations = argc > 1 ? std::atoi(argv[1]) : 200000;
    std::vector<JellyfinHomeRow> rows(24);
    for (size_t row = 0; row < rows.size(); ++row) {
        rows[row].title = "Home row " + std::to_string(row);
        rows[row].items.resize(12);
        for (size_t item = 0; item < rows[row].items.size(); ++item) {
            rows[row].items[item].id = "item-" + std::to_string(row) + "-" + std::to_string(item);
        }
    }
    HomeScreenState state;
    state.setSelections(std::vector<int>(rows.size(), 5));
    state.setRow(7);
    const auto snapshot = state.snapshot(rows);
    std::rotate(rows.begin(), rows.begin() + 7, rows.end());

    size_t checksum = 0;
    const auto started = std::chrono::steady_clock::now();
    for (int iteration = 0; iteration < iterations; ++iteration) {
        const auto plan = HomeScreenState::restorePlan(snapshot, rows);
        checksum += plan.selections.size() + static_cast<size_t>(plan.focusedRow + 1);
    }
    const double elapsedMs = std::chrono::duration<double, std::milli>(
        std::chrono::steady_clock::now() - started
    ).count();
    std::cout << std::fixed << std::setprecision(3)
              << "iterations=" << iterations << " elapsed_ms=" << elapsedMs
              << " checksum=" << checksum << '\n';
    return checksum > 0 ? 0 : 1;
}
