#pragma once

#include "deep_link.hpp"

#include <string>

struct android_app;

struct LaunchRequest {
    std::string itemId;
    std::string searchQuery;
};

inline LaunchRequest launchRequestFromIntentParts(
    const std::string& action,
    const std::string& data,
    const std::string& query
) {
    LaunchRequest request;
    if (action == "android.intent.action.VIEW") {
        request.itemId = normalizeJellyfinItemId(data);
    } else if (action == "android.intent.action.SEARCH") {
        request.searchQuery = normalizeExternalSearchQuery(query);
    }
    return request;
}

LaunchRequest readLaunchRequest(android_app* app);
