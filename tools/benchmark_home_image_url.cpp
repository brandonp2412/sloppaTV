#include "url_encoding.hpp"

#include <chrono>
#include <cstdlib>
#include <iomanip>
#include <iostream>
#include <string>
#include <string_view>

static std::string oldUrl(
    const std::string& server,
    const std::string& itemId,
    const std::string& tag,
    const std::string& token,
    int width,
    int height
) {
    std::string url = server + "/Items/" + itemId + "/Images/Primary?maxWidth=" + std::to_string(width)
        + "&maxHeight=" + std::to_string(height) + "&quality=92&tag=" + urlEncode(tag);
    url += "&api_key=" + urlEncode(token);
    return url;
}

static std::string newUrl(
    std::string_view server,
    std::string_view itemId,
    std::string_view tag,
    std::string_view token,
    int width,
    int height
) {
    const std::string encodedTag = urlEncode(tag);
    const std::string encodedToken = urlEncode(token);
    const std::string widthText = std::to_string(width);
    const std::string heightText = std::to_string(height);
    constexpr std::string_view imagePath = "/Images/Primary?maxWidth=";
    std::string url;
    url.reserve(server.size() + itemId.size() + imagePath.size() + widthText.size() + heightText.size()
        + encodedTag.size() + encodedToken.size() + 43);
    url.append(server).append("/Items/").append(itemId).append(imagePath).append(widthText)
        .append("&maxHeight=").append(heightText).append("&quality=92&tag=").append(encodedTag)
        .append("&api_key=").append(encodedToken);
    return url;
}

template <typename Function>
static double measure(Function function, int iterations, size_t& checksum) {
    const auto started = std::chrono::steady_clock::now();
    size_t sum = 0;
    for (int i = 0; i < iterations; ++i) {
        const std::string url = function();
        asm volatile("" : : "r"(url.data()) : "memory");
        sum += url.size();
    }
    checksum = sum;
    return std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - started).count();
}

int main(int argc, char** argv) {
    const int iterations = argc > 1 ? std::atoi(argv[1]) : 2000000;
    const std::string server = "https://jellyfin.presley.nz";
    const std::string itemId = "9efbb45d91224912933f167750cdb65d";
    const std::string tag = "0e733197fae44c099591756f6988c70f";
    const std::string token = "c8a3940f9fca43e9b355dfc5f971b214";
    size_t oldChecksum = 0;
    size_t newChecksum = 0;
    const double oldMs = measure([&] { return oldUrl(server, itemId, tag, token, 480, 270); }, iterations, oldChecksum);
    const double newMs = measure([&] { return newUrl(server, itemId, tag, token, 480, 270); }, iterations, newChecksum);
    if (oldChecksum != newChecksum) return 2;
    std::cout << std::fixed << std::setprecision(3)
              << "iterations=" << iterations
              << " old_ms=" << oldMs
              << " new_ms=" << newMs
              << " speedup_pct=" << ((oldMs - newMs) * 100.0 / oldMs)
              << " checksum=" << newChecksum << '\n';
}
