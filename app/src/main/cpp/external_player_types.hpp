#pragma once

#include <cstdint>
#include <string>

struct ExternalPlayerApp {
    std::string componentName;
    std::string packageName;
    std::string label;
};

struct ExternalPlayerResult {
    bool success = false;
    bool completionKnown = false;
    bool completed = false;
    int64_t positionMs = -1;
};
