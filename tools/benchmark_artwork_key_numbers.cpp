#include "home_screen.hpp"

#include <chrono>
#include <cstdlib>
#include <iomanip>
#include <iostream>
#include <string>
#include <string_view>

static std::string makeKey(std::string_view server, std::string_view user, std::string_view item,
                           std::string_view tag, int value) {
#ifdef BENCH_BASELINE
    const std::string number = std::to_string(value);
    std::string key;
    key.reserve(server.size() + user.size() + item.size() + tag.size() + number.size() + 31);
    key.append(server).append(":user:").append(user).push_back(':');
    key.append(item).append(":home:v5-480x270:").append(number).push_back(':');
#else
    std::string key;
    key.reserve(server.size() + user.size() + item.size() + tag.size() + 32);
    key.append(server).append(":user:").append(user).push_back(':');
    key.append(item).append(":home:v5-480x270:").push_back(static_cast<char>('0' + value));
    key.push_back(':');
#endif
    key.append(tag);
    return key;
}

int main(int argc, char** argv) {
    const int iterations = argc > 1 ? std::atoi(argv[1]) : 5000000;
    constexpr std::string_view server = "https://jellyfin.presley.nz";
    constexpr std::string_view user = "e3198d5ab1044be583218343af9050aa";
    constexpr std::string_view item = "9efbb45d91224912933f167750cdb65d";
    constexpr std::string_view tag = "0e733197fae44c099591756f6988c70f";
    size_t checksum = 0;
    const auto started = std::chrono::steady_clock::now();
    for (int i = 0; i < iterations; ++i) checksum += makeKey(server, user, item, tag, i & 3).size();
    const double elapsedMs = std::chrono::duration<double, std::milli>(
        std::chrono::steady_clock::now() - started
    ).count();
    std::cout << std::fixed << std::setprecision(3)
              << "iterations=" << iterations << " elapsed_ms=" << elapsedMs
              << " checksum=" << checksum << '\n';
    return checksum > 0 ? 0 : 1;
}
