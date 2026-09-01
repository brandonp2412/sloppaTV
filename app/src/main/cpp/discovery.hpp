#pragma once

#include <string>
#include <vector>

struct DiscoveredJellyfinServer {
    std::string address;
    std::string id;
    std::string name;
};

std::vector<DiscoveredJellyfinServer> discoverJellyfinServers(int timeoutMs = 1500);
