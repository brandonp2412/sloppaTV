#include "search_screen.hpp"

#include <chrono>
#include <cstdlib>
#include <iomanip>
#include <iostream>
#include <string>
#include <vector>

int main(int argc, char** argv) {
    const int resultsCount = argc > 1 ? std::atoi(argv[1]) : 1000;
    const int iterations = argc > 2 ? std::atoi(argv[2]) : 200000;
    std::vector<SeerrMediaItem> seerrResults;
    SearchScreenState state(seerrResults);
    state.setQuery("benchmark");
    std::vector<JellyfinItem> results(static_cast<size_t>(resultsCount));
    for (int index = 0; index < resultsCount; ++index) {
        results[static_cast<size_t>(index)].id = std::to_string(index);
        results[static_cast<size_t>(index)].type = index % 2 == 0 ? "Movie" : "Episode";
    }
    if (!state.finishLibrarySearch("benchmark", std::move(results))) return 1;

    int checksum = 0;
    const auto started = std::chrono::steady_clock::now();
    for (int iteration = 0; iteration < iterations; ++iteration) {
        const int row = iteration % SearchScreenState::kRowCount;
        checksum += state.rowItemCount(SearchScreenState::kLibraryRow);
        checksum += state.rowItemCount(row);
        checksum += state.firstVisibleInRow(row, 6);
    }
    const double elapsedMs =
        std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - started).count();
    std::cout << std::fixed << std::setprecision(3) << "results=" << resultsCount << " iterations=" << iterations
              << " elapsed_ms=" << elapsedMs << " checksum=" << checksum << '\n';
    return checksum > 0 ? 0 : 1;
}
