#include "url_encoding.hpp"

#include <chrono>
#include <cstdlib>
#include <iomanip>
#include <iostream>
#include <string>
#include <string_view>

static std::string oldUrlEncode(std::string_view value) {
    constexpr char hex[] = "0123456789ABCDEF";
    std::string encoded;
    encoded.reserve(value.size() * 3);
    for (const unsigned char byte : value) {
        if (urlUnreserved(byte)) {
            encoded.push_back(static_cast<char>(byte));
            continue;
        }
        encoded.push_back('%');
        encoded.push_back(hex[byte >> 4]);
        encoded.push_back(hex[byte & 0x0F]);
    }
    return encoded;
}

static std::string candidateUrlEncode(std::string_view value) {
    size_t firstEscaped = 0;
    while (firstEscaped < value.size() && urlUnreserved(static_cast<unsigned char>(value[firstEscaped]))) {
        ++firstEscaped;
    }
    if (firstEscaped == value.size()) return std::string(value);

    constexpr char hex[] = "0123456789ABCDEF";
    std::string encoded;
    encoded.reserve(value.size() * 3);
    encoded.append(value.substr(0, firstEscaped));
    for (size_t index = firstEscaped; index < value.size(); ++index) {
        const unsigned char byte = static_cast<unsigned char>(value[index]);
        if (urlUnreserved(byte)) {
            encoded.push_back(static_cast<char>(byte));
            continue;
        }
        encoded.push_back('%');
        encoded.push_back(hex[byte >> 4]);
        encoded.push_back(hex[byte & 0x0F]);
    }
    return encoded;
}

template <typename Function>
static double measure(Function function, std::string_view value, int iterations, size_t& checksum) {
    const auto started = std::chrono::steady_clock::now();
    size_t sum = 0;
    for (int i = 0; i < iterations; ++i) {
        const std::string encoded = function(value);
        asm volatile("" : : "r"(encoded.data()) : "memory");
        sum += encoded.size();
    }
    checksum = sum;
    return std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - started).count();
}

static void run(std::string_view label, std::string_view value, int iterations) {
    size_t oldChecksum = 0;
    size_t newChecksum = 0;
    const double oldMs = measure(oldUrlEncode, value, iterations, oldChecksum);
    const double newMs = measure(candidateUrlEncode, value, iterations, newChecksum);
    if (oldChecksum != newChecksum || oldUrlEncode(value) != candidateUrlEncode(value)) std::exit(2);
    std::cout << label << " old_ms=" << oldMs << " new_ms=" << newMs
              << " speedup_pct=" << ((oldMs - newMs) * 100.0 / oldMs) << '\n';
}

int main(int argc, char** argv) {
    const int iterations = argc > 1 ? std::atoi(argv[1]) : 3000000;
    std::cout << std::fixed << std::setprecision(3);
    run("token", "c8a3940f9fca43e9b355dfc5f971b214", iterations);
    run("tag", "0e733197fae44c099591756f6988c70f", iterations);
    run("mixed", "series/id 42?tag=Cafe%20&token=a+b/c==", iterations);
}
