#include "url_encoding.hpp"

#include <chrono>
#include <cstdlib>
#include <iomanip>
#include <iostream>
#include <sstream>
#include <string>
#include <string_view>

[[gnu::noinline]] static std::string oldUrlEncode(std::string_view value) {
    std::ostringstream escaped;
    escaped << std::uppercase << std::hex;
    for (const unsigned char c : value) {
        if (std::isalnum(c) || c == '-' || c == '_' || c == '.' || c == '~') {
            escaped << static_cast<char>(c);
        } else {
            escaped << '%' << std::setw(2) << std::setfill('0') << static_cast<int>(c);
        }
    }
    return escaped.str();
}

template <typename Function>
static double measure(Function function, std::string_view value, int iterations, size_t& checksum) {
    const auto started = std::chrono::steady_clock::now();
    size_t sum = 0;
    for (int iteration = 0; iteration < iterations; ++iteration) {
        const std::string encoded = function(value);
        sum += encoded.size() + static_cast<unsigned char>(encoded.front());
    }
    checksum = sum;
    return std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - started).count();
}

int main(int argc, char** argv) {
    const int iterations = argc > 1 ? std::atoi(argv[1]) : 1000000;
    const std::string value = "series/id 42?tag=Cafe\xCC\x81&token=a+b/c==";
    size_t oldChecksum = 0;
    size_t newChecksum = 0;
    const double oldMs = measure(oldUrlEncode, value, iterations, oldChecksum);
    const double newMs = measure(urlEncode, value, iterations, newChecksum);
    if (oldChecksum != newChecksum || oldUrlEncode(value) != urlEncode(value)) return 2;
    std::cout << std::fixed << std::setprecision(3)
              << "iterations=" << iterations
              << " old_ms=" << oldMs
              << " new_ms=" << newMs
              << " speedup_pct=" << ((oldMs - newMs) * 100.0 / oldMs)
              << " checksum=" << newChecksum << '\n';
    return 0;
}
