#pragma once

#include "jellyfin_types.hpp"
#include "jni_http.hpp"

#include <jni.h>

#include <map>
#include <string>
#include <vector>

inline bool isSeerrItem(const JellyfinItem& item) {
    return item.externalSource == "seerr";
}

class SeerrClient {
public:
    SeerrClient(JavaVM* vm, jobject activity) : http_(vm, activity) {}

    void cancelPendingRequests() const { http_.cancelPending(); }

    [[nodiscard]] static bool configured(const std::string& server, const std::string& apiKey) {
        return !server.empty() && !apiKey.empty();
    }

    ApiValueResult<std::vector<JellyfinItem>> search(
        const std::string& server,
        const std::string& apiKey,
        const std::string& query
    ) const;
    ApiValueResult<std::vector<JellyfinItem>> pendingRequests(
        const std::string& server,
        const std::string& apiKey,
        int limit = 20
    ) const;
    ApiResult requestMedia(
        const std::string& server,
        const std::string& apiKey,
        const JellyfinItem& item
    ) const;
    ApiValueResult<std::string> downloadImage(const std::string& url) const;

private:
    [[nodiscard]] std::string apiBase(std::string server) const;
    [[nodiscard]] std::string urlEncode(const std::string& value) const;
    [[nodiscard]] std::map<std::string, std::string> headers(const std::string& apiKey) const;
    ApiValueResult<JellyfinItem> loadMediaDetails(
        const std::string& server,
        const std::string& apiKey,
        const std::string& mediaType,
        int tmdbId
    ) const;

    JniHttpClient http_;
};
