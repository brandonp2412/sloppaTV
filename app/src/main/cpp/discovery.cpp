#include "discovery.hpp"

#include <nlohmann/json.hpp>

#include <arpa/inet.h>
#include <fcntl.h>
#include <ifaddrs.h>
#include <net/if.h>
#include <poll.h>
#include <sys/socket.h>
#include <unistd.h>

#include <algorithm>
#include <array>
#include <chrono>
#include <cstdint>
#include <cstring>
#include <unordered_set>

using nlohmann::json;

std::vector<DiscoveredJellyfinServer> discoverJellyfinServers(int timeoutMs) {
    std::vector<DiscoveredJellyfinServer> result;
    const int fd = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
    if (fd < 0) return result;

    const int enabled = 1;
    setsockopt(fd, SOL_SOCKET, SO_BROADCAST, &enabled, sizeof(enabled));
    fcntl(fd, F_SETFL, fcntl(fd, F_GETFL, 0) | O_NONBLOCK);

    static constexpr char kProbe[] = "Who is JellyfinServer?";
    std::vector<in_addr> destinations;
    destinations.push_back(in_addr{htonl(INADDR_BROADCAST)});

    ifaddrs* interfaces = nullptr;
    if (getifaddrs(&interfaces) == 0) {
        for (const ifaddrs* current = interfaces; current; current = current->ifa_next) {
            if (!current->ifa_addr || current->ifa_addr->sa_family != AF_INET) continue;
            if ((current->ifa_flags & IFF_UP) == 0 || (current->ifa_flags & IFF_LOOPBACK) != 0) continue;
            if ((current->ifa_flags & IFF_BROADCAST) == 0 || !current->ifa_broadaddr) continue;
            const auto* broadcast = reinterpret_cast<const sockaddr_in*>(current->ifa_broadaddr);
            destinations.push_back(broadcast->sin_addr);
        }
        freeifaddrs(interfaces);
    }

    std::unordered_set<uint32_t> sentAddresses;
    for (const auto& destination : destinations) {
        if (!sentAddresses.insert(destination.s_addr).second) continue;
        sockaddr_in target{};
        target.sin_family = AF_INET;
        target.sin_port = htons(7359);
        target.sin_addr = destination;
        sendto(fd, kProbe, sizeof(kProbe) - 1, 0, reinterpret_cast<sockaddr*>(&target), sizeof(target));
    }

    std::unordered_set<std::string> ids;
    const auto deadline = std::chrono::steady_clock::now() + std::chrono::milliseconds(std::max(100, timeoutMs));
    std::array<char, 4096> buffer{};

    while (std::chrono::steady_clock::now() < deadline) {
        const auto remaining = std::chrono::duration_cast<std::chrono::milliseconds>(deadline - std::chrono::steady_clock::now()).count();
        pollfd pfd{fd, POLLIN, 0};
        const int ready = poll(&pfd, 1, static_cast<int>(std::max<int64_t>(1, remaining)));
        if (ready <= 0) break;

        sockaddr_in from{};
        socklen_t fromLength = sizeof(from);
        const ssize_t length = recvfrom(fd, buffer.data(), buffer.size() - 1, 0, reinterpret_cast<sockaddr*>(&from), &fromLength);
        if (length <= 0) continue;
        buffer[static_cast<size_t>(length)] = '\0';

        try {
            const auto payload = json::parse(buffer.data(), buffer.data() + length);
            DiscoveredJellyfinServer server;
            server.address = payload.value("Address", std::string{});
            server.id = payload.value("Id", std::string{});
            server.name = payload.value("Name", std::string{});
            if (server.address.empty()) continue;
            const std::string dedupeKey = !server.id.empty() ? server.id : server.address;
            if (!ids.insert(dedupeKey).second) continue;
            result.push_back(std::move(server));
        } catch (...) {
            // Ignore non-Jellyfin UDP traffic on the ephemeral receive socket.
        }
    }

    close(fd);
    return result;
}
