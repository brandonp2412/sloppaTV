#include <chrono>
#include <cstdlib>
#include <iomanip>
#include <iostream>
#include <string>
#include <unordered_map>

struct Entry {
    std::chrono::steady_clock::time_point expiresAt{};
    int value = 0;
};

int main(int argc, char** argv) {
    const int iterations = argc > 1 ? std::atoi(argv[1]) : 2000000;
    std::unordered_map<std::string, Entry> cache;
    const auto now = std::chrono::steady_clock::now();
    for (int i = 0; i < 32; ++i) {
        cache.emplace("request-key-" + std::to_string(i), Entry{now + std::chrono::seconds(5), i});
    }
    const std::string key = "request-key-17";
    size_t checksum = 0;
    const auto started = std::chrono::steady_clock::now();
    for (int i = 0; i < iterations; ++i) {
#ifdef BENCH_BASELINE
        std::erase_if(cache, [&](const auto& entry) { return entry.second.expiresAt <= now; });
        const auto cached = cache.find(key);
        if (cached != cache.end()) checksum += static_cast<size_t>(cached->second.value);
#else
        const auto cached = cache.find(key);
        if (cached != cache.end() && cached->second.expiresAt > now) {
            checksum += static_cast<size_t>(cached->second.value);
        }
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
