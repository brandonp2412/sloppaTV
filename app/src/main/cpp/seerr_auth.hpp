#pragma once

#include <string>
#include <string_view>

struct SeerrAuth {
    std::string sessionCookie;
    std::string apiKey;

    [[nodiscard]] bool valid() const { return !sessionCookie.empty() || !apiKey.empty(); }

    bool operator==(const SeerrAuth&) const = default;
};

struct SeerrEndpoint {
    std::string server;
    SeerrAuth auth;

    [[nodiscard]] bool configured() const { return !server.empty() && auth.valid(); }

    [[nodiscard]] bool matches(const std::string& currentServer, const SeerrAuth& currentAuth) const {
        return server == currentServer && auth == currentAuth;
    }
};

inline bool isSeerrAuthError(std::string_view error) {
    return error.find("HTTP 401") != std::string_view::npos || error.find("HTTP 403") != std::string_view::npos;
}
