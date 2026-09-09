#include "jellyfin_types.hpp"

#include <chrono>
#include <cstdlib>
#include <iomanip>
#include <iostream>
#include <string>
#include <string_view>

static std::string artworkKey(
    std::string_view server,
    std::string_view user,
    std::string_view itemId,
    std::string_view imageTag
) {
    std::string key;
    key.reserve(server.size() + user.size() + itemId.size() + imageTag.size() + 16);
    key.append(server).append(":user:").append(user).push_back(':');
    key.append(itemId).append(":primary:").append(imageTag);
    return key;
}

[[gnu::noinline]] static size_t oldRenderIdentity(const JellyfinItem& item, std::string_view server, std::string_view user) {
    JellyfinItem cover;
    cover.id = item.seriesId;
    cover.imageTag = item.seriesPrimaryImageTag;
    cover.type = "Series";
    const std::string key = artworkKey(server, user, cover.id, cover.imageTag);
    asm volatile("" : : "r"(key.data()) : "memory");
    return key.size();
}

[[gnu::noinline]] static size_t newRenderIdentity(const JellyfinItem& item, std::string_view server, std::string_view user) {
    const std::string key = artworkKey(server, user, item.seriesId, item.seriesPrimaryImageTag);
    asm volatile("" : : "r"(key.data()) : "memory");
    return key.size();
}

template <typename Function>
static double measure(Function function, const JellyfinItem& item, int iterations, size_t& checksum) {
    constexpr std::string_view server = "https://jellyfin.presley.nz";
    constexpr std::string_view user = "e3198d5ab1044be583218343af9050aa";
    const auto started = std::chrono::steady_clock::now();
    size_t sum = 0;
    for (int i = 0; i < iterations; ++i) sum += function(item, server, user);
    checksum = sum;
    return std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - started).count();
}

int main(int argc, char** argv) {
    const int iterations = argc > 1 ? std::atoi(argv[1]) : 3000000;
    JellyfinItem item;
    item.seriesId = "9efbb45d91224912933f167750cdb65d";
    item.seriesPrimaryImageTag = "0e733197fae44c099591756f6988c70f";
    size_t oldChecksum = 0;
    size_t newChecksum = 0;
    const double oldMs = measure(oldRenderIdentity, item, iterations, oldChecksum);
    const double newMs = measure(newRenderIdentity, item, iterations, newChecksum);
    if (oldChecksum != newChecksum) return 2;
    std::cout << std::fixed << std::setprecision(3)
              << "iterations=" << iterations
              << " old_ms=" << oldMs
              << " new_ms=" << newMs
              << " speedup_pct=" << ((oldMs - newMs) * 100.0 / oldMs)
              << " checksum=" << newChecksum << '\n';
    return 0;
}
