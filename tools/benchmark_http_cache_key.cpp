#include <chrono>
#include <cstdlib>
#include <iomanip>
#include <iostream>
#include <map>
#include <sstream>
#include <string>

static std::string baseline(const std::string& url, const std::map<std::string, std::string>& headers) {
    std::ostringstream key;
    key << url;
    for (const auto& [name, value] : headers) key << '\n' << name << ':' << value;
    return key.str();
}

static std::string optimized(const std::string& url, const std::map<std::string, std::string>& headers) {
    size_t size = url.size();
    for (const auto& [name, value] : headers) size += name.size() + value.size() + 2;
    std::string key;
    key.reserve(size);
    key += url;
    for (const auto& [name, value] : headers) {
        key.push_back('\n');
        key += name;
        key.push_back(':');
        key += value;
    }
    return key;
}

int main(int argc, char** argv) {
    const int iterations = argc > 1 ? std::atoi(argv[1]) : 1000000;
    const std::string url = "https://jellyfin.presley.nz/Users/e3198d5ab1044be583218343af9050aa/Items?Recursive=true&Limit=30";
    const std::map<std::string, std::string> headers{
        {"Accept", "application/json"},
        {"Authorization", "MediaBrowser Client=\"SloppaTV\",Version=\"1.0\",DeviceId=\"streamer-device\",Device=\"Google TV Streamer\",Token=\"0123456789abcdef0123456789abcdef\""},
    };
    if (baseline(url, headers) != optimized(url, headers)) return 2;
    size_t checksum = 0;
    const auto started = std::chrono::steady_clock::now();
    for (int i = 0; i < iterations; ++i) {
#ifdef BENCH_BASELINE
        checksum += baseline(url, headers).size();
#else
        checksum += optimized(url, headers).size();
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
