#include <chrono>
#include <cstdlib>
#include <iomanip>
#include <iostream>
#include <string>
#include <string_view>

namespace {
[[gnu::noinline]] size_t consumeString(const std::string& value) {
    asm volatile("" : : "r"(value.data()) : "memory");
    return value.size();
}

[[gnu::noinline]] size_t consumeView(std::string_view value) {
    asm volatile("" : : "r"(value.data()) : "memory");
    return value.size();
}

template <typename Consume>
double benchmark(int iterations, Consume consume, size_t& checksum) {
    constexpr const char* text = "LEFT/RIGHT SEEK | OK PLAY/PAUSE | UP OPTIONS";
    const auto started = std::chrono::steady_clock::now();
    for (int iteration = 0; iteration < iterations; ++iteration) checksum += consume(text);
    return std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - started).count();
}
}

int main(int argc, char** argv) {
    const int iterations = argc > 1 ? std::atoi(argv[1]) : 5000000;
    size_t stringChecksum = 0;
    size_t viewChecksum = 0;
    const double stringMs = benchmark(iterations, consumeString, stringChecksum);
    const double viewMs = benchmark(iterations, consumeView, viewChecksum);
    std::cout << std::fixed << std::setprecision(3)
              << "iterations=" << iterations << " string_ms=" << stringMs
              << " string_view_ms=" << viewMs << " checksum=" << (stringChecksum + viewChecksum) << '\n';
    return stringChecksum == viewChecksum && stringChecksum > 0 ? 0 : 1;
}
