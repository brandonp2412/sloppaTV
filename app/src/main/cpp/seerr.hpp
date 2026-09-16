#pragma once

#include "jellyfin_types.hpp"
#include "jni_http.hpp"
#include "seerr_media.hpp"

#include <jni.h>

#include <cstdint>
#include <map>
#include <string>
#include <vector>

struct SeerrAuth {
    std::string sessionCookie;
    std::string apiKey;

    [[nodiscard]] bool valid() const { return !sessionCookie.empty() || !apiKey.empty(); }
};

struct SeerrQuickConnectRequest {
    std::string code;
    std::string secret;
    std::string csrfCookie;
    std::string csrfToken;
};

struct SeerrStorageTarget {
    std::string mediaType;
    std::string serviceName;
    std::string path;
    int serverId = -1;
    int profileId = 0;
    int64_t freeSpace = 0;
    int64_t totalSpace = 0;
    bool isDefault = false;
    bool is4k = false;

    [[nodiscard]] int usedPercent() const {
        if (totalSpace <= 0) return 0;
        const int64_t used = totalSpace - freeSpace;
        return static_cast<int>((used * 100 + totalSpace / 2) / totalSpace);
    }
};

class SeerrClient {
public:
    SeerrClient(JavaVM* vm, jobject activity) : http_(vm, activity) {}

    void cancelPendingRequests() const { http_.cancelPending(); }

    [[nodiscard]] static bool configured(const std::string& server, const SeerrAuth& auth) {
        return !server.empty() && auth.valid();
    }

    ApiValueResult<SeerrQuickConnectRequest> initiateQuickConnect(const std::string& server) const;
    ApiValueResult<std::string> authenticateQuickConnect(
        const std::string& server,
        const SeerrQuickConnectRequest& request
    ) const;
    ApiValueResult<std::vector<SeerrStorageTarget>> storageTargets(
        const std::string& server,
        const SeerrAuth& auth
    ) const;

    ApiValueResult<std::vector<SeerrMediaItem>> search(
        const std::string& server,
        const SeerrAuth& auth,
        const std::string& query
    ) const;
    ApiValueResult<std::vector<SeerrMediaItem>> pendingRequests(
        const std::string& server,
        const SeerrAuth& auth,
        int limit = 20
    ) const;
    ApiValueResult<int> requestMedia(
        const std::string& server,
        const SeerrAuth& auth,
        const SeerrMediaItem& item,
        const SeerrStorageTarget* target = nullptr
    ) const;
    ApiResult deleteRequest(
        const std::string& server,
        const SeerrAuth& auth,
        int requestId
    ) const;
    ApiValueResult<std::string> downloadImage(const std::string& url) const;

private:
    [[nodiscard]] std::string apiBase(std::string server) const;
    [[nodiscard]] std::string serverBase(std::string server) const;
    [[nodiscard]] std::string urlEncode(const std::string& value) const;
    [[nodiscard]] std::map<std::string, std::string> headers(const SeerrAuth& auth) const;
    [[nodiscard]] std::map<std::string, std::string> quickConnectHeaders(
        const std::string& server,
        const SeerrQuickConnectRequest& request
    ) const;
    ApiValueResult<SeerrMediaItem> loadMediaDetails(
        const std::string& server,
        const SeerrAuth& auth,
        const std::string& mediaType,
        int tmdbId
    ) const;

    JniHttpClient http_;
};
