#pragma once

#include "jellyfin_types.hpp"
#include "jni_http.hpp"
#include "seerr_auth.hpp"
#include "seerr_media.hpp"
#include "seerr_quick_connect.hpp"
#include "seerr_storage.hpp"
#include "seerr_seasons.hpp"

#include <jni.h>

#include <cstdint>
#include <map>
#include <string>
#include <vector>

class SeerrClient {
public:
    SeerrClient(JavaVM* vm, jobject activity) : http_(vm, activity) {}

    void cancelPendingRequests() const { http_.cancelPending(); }

    [[nodiscard]] static bool configured(const std::string& server, const SeerrAuth& auth) {
        return SeerrEndpoint{server, auth}.configured();
    }

    ApiValueResult<SeerrQuickConnectRequest> initiateQuickConnect(const std::string& server) const;
    ApiValueResult<std::string> authenticateQuickConnect(const std::string& server,
                                                         const SeerrQuickConnectRequest& request) const;
    ApiValueResult<std::vector<SeerrStorageTarget>> storageTargets(const std::string& server,
                                                                   const SeerrAuth& auth) const;

    ApiValueResult<std::vector<SeerrMediaItem>> search(const std::string& server, const SeerrAuth& auth,
                                                       const std::string& query) const;
    ApiValueResult<std::vector<SeerrMediaItem>> pendingRequests(const std::string& server, const SeerrAuth& auth,
                                                                int limit = 20) const;
    ApiValueResult<int> requestMedia(const std::string& server, const SeerrAuth& auth, const SeerrMediaItem& item,
                                     const SeerrStorageTarget* target = nullptr) const;
    ApiResult deleteRequest(const std::string& server, const SeerrAuth& auth, int requestId) const;
    ApiValueResult<std::vector<SeerrSeason>> seasons(const std::string& server, const SeerrAuth& auth, int tmdbId,
                                                     bool is4k) const;
    ApiValueResult<std::string> downloadImage(const std::string& url) const;

private:
    [[nodiscard]] std::string apiBase(std::string server) const;
    [[nodiscard]] std::string serverBase(std::string server) const;
    [[nodiscard]] std::string urlEncode(const std::string& value) const;
    [[nodiscard]] std::map<std::string, std::string> headers(const SeerrAuth& auth) const;
    [[nodiscard]] std::map<std::string, std::string> quickConnectHeaders(const std::string& server,
                                                                         const SeerrQuickConnectRequest& request) const;
    ApiValueResult<SeerrMediaItem> loadMediaDetails(const std::string& server, const SeerrAuth& auth,
                                                    const std::string& mediaType, int tmdbId) const;

    JniHttpClient http_;
};
