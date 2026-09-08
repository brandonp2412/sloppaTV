#include <algorithm>
#include <chrono>
#include <iomanip>
#include <iostream>
#include <sstream>
#include <string>
#include <vector>

struct Layout {
    std::vector<std::string> lines;
    std::vector<float> widths;
    float widest = 0.0f;
};

static Layout buildLayout(const std::string& subtitle) {
    Layout layout;
    std::istringstream stream(subtitle);
    std::string line;
    while (std::getline(stream, line)) {
        if (line.empty()) continue;
        const float width = static_cast<float>(line.size()) * 17.5f;
        layout.widest = std::max(layout.widest, width);
        layout.lines.push_back(line);
        layout.widths.push_back(width);
    }
    if (layout.lines.empty()) {
        layout.lines.push_back(subtitle);
        layout.widths.push_back(static_cast<float>(subtitle.size()) * 17.5f);
        layout.widest = layout.widths.front();
    }
    return layout;
}

int main(int argc, char** argv) {
    const int iterations = argc > 1 ? std::stoi(argv[1]) : 1000000;
    const std::string subtitle = "I have crossed oceans of time\nto find you.";
    size_t checksum = 0;
    auto started = std::chrono::steady_clock::now();
    for (int i = 0; i < iterations; ++i) {
        const Layout layout = buildLayout(subtitle);
        checksum += layout.lines.size() + static_cast<size_t>(layout.widest);
    }
    const auto uncachedDone = std::chrono::steady_clock::now();
    const Layout cached = buildLayout(subtitle);
    for (int i = 0; i < iterations; ++i) {
        checksum += cached.lines.size() + static_cast<size_t>(cached.widest);
    }
    const auto cachedDone = std::chrono::steady_clock::now();
    const double uncachedMs = std::chrono::duration<double, std::milli>(uncachedDone - started).count();
    const double cachedMs = std::chrono::duration<double, std::milli>(cachedDone - uncachedDone).count();
    std::cout << std::fixed << std::setprecision(3)
              << "iterations=" << iterations << " uncached_ms=" << uncachedMs
              << " cached_hit_ms=" << cachedMs << " checksum=" << checksum << "\n";
}
