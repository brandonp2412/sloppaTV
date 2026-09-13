#include "seerr.hpp"
#include "seerr_progress.hpp"

#include <nlohmann/json.hpp>

#include <algorithm>
#include <cctype>
#include <iomanip>
#include <sstream>
#include <unordered_set>

using nlohmann::json;

namespace {
constexpr const char* kTmdbPosterBase = "https://image.tmdb.org/t/p/w500";
constexpr const char* kTmdbBackdropBase = "https://image.tmdb.org/t/p/w780";

std::string apiError(const HttpResponse& response) {
    if (!response.error.empty()) return response.error;
    std::string detail;
    try {
        if (!response.body.empty()) {
            const auto body = json::parse(response.body);
            detail = body.value("message", body.value("Message", std::string{}));
        }
    } catch (...) {
        detail.clear();
    }
    std::string result = "HTTP " + std::to_string(response.status);
    if (!detail.empty()) result += ": " + detail;
    return result;
}

int integerValue(const json& value, const char* key, int fallback = 0) {
    const auto found = value.find(key);
    if (found == value.end() || found->is_null()) return fallback;
    try {
        if (found->is_number_integer()) return found->get<int>();
        if (found->is_string()) return std::stoi(found->get<std::string>());
    } catch (...) {
    }
    return fallback;
}

double doubleValue(const json& value, const char* key, double fallback = 0.0) {
    const auto found = value.find(key);
    if (found == value.end() || found->is_null()) return fallback;
    try {
        if (found->is_number()) return found->get<double>();
        if (found->is_string()) return std::stod(found->get<std::string>());
    } catch (...) {
    }
    return fallback;
}

std::string stringValue(const json& value, const char* key) {
    const auto found = value.find(key);
    return found != value.end() && found->is_string() ? found->get<std::string>() : std::string{};
}

std::string tmdbImageUrl(const std::string& base, const std::string& path) {
    if (path.empty()) return {};
    if (path.rfind("http://", 0) == 0 || path.rfind("https://", 0) == 0) return path;
    return base + path;
}

std::string mediaStatusLabel(int status, bool television) {
    if (television) {
        switch (status) {
            case 1: return "Episode 1 requested";
            case 2: return "Episode 1 queued";
            case 3: return "Episode 1 waiting for download";
            case 4: return "Episode 1 partially available";
            case 5: return "Available";
            case 6: return "Blocklisted";
            case 7: return "Deleted";
            default: return "Episode 1 requested";
        }
    }
    switch (status) {
        case 1: return "Requested";
        case 2: return "Queued";
        case 3: return "Waiting for download";
        case 4: return "Partially available";
        case 5: return "Available";
        case 6: return "Blocklisted";
        case 7: return "Deleted";
        default: return "Requested";
    }
}

JellyfinItem itemFromSearchResult(const json& value) {
    JellyfinItem item;
    const std::string mediaType = stringValue(value, "mediaType");
    if (mediaType != "movie" && mediaType != "tv") return item;
    const int tmdbId = integerValue(value, "id");
    if (tmdbId <= 0) return item;

    item.externalSource = "seerr";
    item.externalMediaType = mediaType;
    item.tmdbId = std::to_string(tmdbId);
    item.id = "seerr:" + mediaType + ":" + item.tmdbId;
    item.type = mediaType == "tv" ? "Series" : "Movie";
    item.name = mediaType == "tv" ? stringValue(value, "name") : stringValue(value, "title");
    if (item.name.empty()) item.name = stringValue(value, "originalName");
    if (item.name.empty()) item.name = stringValue(value, "originalTitle");
    item.overview = stringValue(value, "overview");
    item.externalPosterUrl = tmdbImageUrl(kTmdbPosterBase, stringValue(value, "posterPath"));
    item.externalBackdropUrl = tmdbImageUrl(kTmdbBackdropBase, stringValue(value, "backdropPath"));

    const std::string date = mediaType == "tv"
        ? stringValue(value, "firstAirDate")
        : stringValue(value, "releaseDate");
    if (date.size() >= 4) {
        try { item.productionYear = std::stoi(date.substr(0, 4)); } catch (...) {}
    }

    const auto mediaInfo = value.find("mediaInfo");
    if (mediaInfo != value.end() && mediaInfo->is_object()) {
        item.externalMediaStatus = integerValue(*mediaInfo, "status");
        item.externalAvailable = item.externalMediaStatus == 5;
        item.externalRequested = item.externalMediaStatus >= 2 && item.externalMediaStatus <= 4;
        item.externalStatus = mediaStatusLabel(item.externalMediaStatus, mediaType == "tv");
        const auto requests = mediaInfo->find("requests");
        if (requests != mediaInfo->end() && requests->is_array() && !requests->empty()) {
            item.externalRequested = true;
            item.externalRequestId = integerValue(requests->front(), "id");
        }
    }
    return item;
}

void applyMediaDetails(JellyfinItem& item, const JellyfinItem& details) {
    if (!details.name.empty()) item.name = details.name;
    if (!details.overview.empty()) item.overview = details.overview;
    if (!details.externalPosterUrl.empty()) item.externalPosterUrl = details.externalPosterUrl;
    if (!details.externalBackdropUrl.empty()) item.externalBackdropUrl = details.externalBackdropUrl;
    if (details.productionYear > 0) item.productionYear = details.productionYear;
}

void applyDownloadProgress(JellyfinItem& item, const json& downloads) {
    if (!downloads.is_array() || downloads.empty()) return;

    const json* selected = nullptr;
    int selectedSeason = -1;
    int selectedEpisode = -1;
    int selectedScore = 0;
    for (const auto& download : downloads) {
        if (!download.is_object()) continue;
        if (!selected) selected = &download;
        if (item.externalMediaType != "tv") continue;
        const auto episode = download.find("episode");
        if (episode == download.end() || !episode->is_object()) continue;
        const int season = integerValue(*episode, "seasonNumber", -1);
        const int number = integerValue(*episode, "episodeNumber", -1);
        if (season < 0 || number < 0) continue;
        const int score = (season > 0 ? season : 10000 + season) * 10000 + number;
        if (selectedSeason < 0 || score < selectedScore) {
            selected = &download;
            selectedSeason = season;
            selectedEpisode = number;
            selectedScore = score;
        }
    }
    if (!selected) return;

    const int percent = seerrProgressPercent(
        doubleValue(*selected, "size"),
        doubleValue(*selected, "sizeLeft")
    );
    if (percent < 0) return;
    const std::string timeLeft = stringValue(*selected, "timeLeft");
    item.externalProgressPercent = percent;
    item.externalProgressLabel = seerrProgressLabel(
        item.externalMediaType,
        selectedSeason,
        selectedEpisode,
        percent
    );
    item.externalProgressEta = seerrProgressEta(percent, timeLeft);
    item.externalStatus = seerrProgressStatus(
        item.externalMediaType,
        selectedSeason,
        selectedEpisode,
        percent,
        timeLeft
    );
}

std::string cookieValue(const std::string& headers, const std::string& wanted) {
    size_t start = 0;
    while (start < headers.size()) {
        const size_t end = headers.find('\n', start);
        const std::string_view line(headers.data() + start,
            (end == std::string::npos ? headers.size() : end) - start);
        const size_t equals = line.find('=');
        if (equals != std::string_view::npos && line.substr(0, equals) == wanted) {
            const size_t semi = line.find(';', equals + 1);
            return std::string(line.substr(equals + 1,
                (semi == std::string_view::npos ? line.size() : semi) - equals - 1));
        }
        if (end == std::string::npos) break;
        start = end + 1;
    }
    return {};
}

std::string sessionCookie(const std::string& setCookies) {
    std::string value = cookieValue(setCookies, "connect.sid");
    if (!value.empty()) return "connect.sid=" + value;
    size_t start = 0;
    while (start < setCookies.size()) {
        const size_t end = setCookies.find('\n', start);
        std::string_view line(setCookies.data() + start,
            (end == std::string::npos ? setCookies.size() : end) - start);
        const size_t equals = line.find('=');
        if (equals != std::string_view::npos) {
            const std::string_view name = line.substr(0, equals);
            const size_t semi = line.find(';', equals + 1);
            const std::string_view cookie = line.substr(equals + 1,
                (semi == std::string_view::npos ? line.size() : semi) - equals - 1);
            if (cookie.starts_with("s%3A") || cookie.starts_with("s:")) {
                return std::string(name) + "=" + std::string(cookie);
            }
        }
        if (end == std::string::npos) break;
        start = end + 1;
    }
    return {};
}

int64_t int64Value(const json& value, const char* key) {
    const auto found = value.find(key);
    if (found == value.end() || found->is_null()) return 0;
    try {
        if (found->is_number_integer()) return found->get<int64_t>();
        if (found->is_number_unsigned()) return static_cast<int64_t>(found->get<uint64_t>());
        if (found->is_string()) return std::stoll(found->get<std::string>());
    } catch (...) {}
    return 0;
}
}

std::string SeerrClient::serverBase(std::string server) const {
    while (!server.empty() && std::isspace(static_cast<unsigned char>(server.front()))) server.erase(server.begin());
    while (!server.empty() && std::isspace(static_cast<unsigned char>(server.back()))) server.pop_back();
    while (!server.empty() && server.back() == '/') server.pop_back();
    if (!server.empty() && server.find("://") == std::string::npos) server = "https://" + server;
    constexpr std::string_view suffix = "/api/v1";
    if (server.size() >= suffix.size() && server.compare(server.size() - suffix.size(), suffix.size(), suffix) == 0) {
        server.resize(server.size() - suffix.size());
    }
    return server;
}

std::string SeerrClient::apiBase(std::string server) const {
    server = serverBase(std::move(server));
    constexpr std::string_view suffix = "/api/v1";
    if (server.size() >= suffix.size() && server.compare(server.size() - suffix.size(), suffix.size(), suffix) == 0) {
        return server;
    }
    return server + "/api/v1";
}

std::string SeerrClient::urlEncode(const std::string& value) const {
    std::ostringstream encoded;
    encoded << std::uppercase << std::hex;
    for (unsigned char c : value) {
        if (std::isalnum(c) || c == '-' || c == '_' || c == '.' || c == '~') encoded << c;
        else encoded << '%' << std::setw(2) << std::setfill('0') << static_cast<int>(c);
    }
    return encoded.str();
}

std::map<std::string, std::string> SeerrClient::headers(const SeerrAuth& auth) const {
    std::map<std::string, std::string> result{
        {"Accept", "application/json"},
        {"Content-Type", "application/json"},
        {"User-Agent", "sloppaTV/0.1.0"},
    };
    if (!auth.sessionCookie.empty()) result["Cookie"] = auth.sessionCookie;
    else if (!auth.apiKey.empty()) result["X-Api-Key"] = auth.apiKey;
    return result;
}

std::map<std::string, std::string> SeerrClient::quickConnectHeaders(
    const std::string& server,
    const SeerrQuickConnectRequest& request
) const {
    std::map<std::string, std::string> result{
        {"Accept", "application/json"},
        {"Content-Type", "application/json"},
        {"User-Agent", "sloppaTV/0.1.0"},
    };
    if (!request.csrfCookie.empty()) result["Cookie"] = request.csrfCookie;
    if (!request.csrfToken.empty()) {
        result["X-XSRF-TOKEN"] = request.csrfToken;
        result["X-CSRF-Token"] = request.csrfToken;
    }
    const std::string base = serverBase(server);
    std::string origin = base;
    const size_t scheme = origin.find("://");
    if (scheme != std::string::npos) {
        const size_t path = origin.find('/', scheme + 3);
        if (path != std::string::npos) origin.resize(path);
    }
    result["Origin"] = origin;
    result["Referer"] = base + "/";
    return result;
}

ApiValueResult<SeerrQuickConnectRequest> SeerrClient::initiateQuickConnect(const std::string& server) const {
    ApiValueResult<SeerrQuickConnectRequest> result;
    if (serverBase(server).empty()) {
        result.error = "Seerr server is required";
        return result;
    }

    SeerrQuickConnectRequest request;
    const auto seed = http_.request("GET", apiBase(server) + "/auth/me", {
        {"Accept", "application/json"},
        {"User-Agent", "sloppaTV/0.1.0"},
    });
    const std::string xsrf = cookieValue(seed.setCookie, "XSRF-TOKEN");
    const std::string csrf = cookieValue(seed.setCookie, "_csrf");
    if (!xsrf.empty()) {
        request.csrfToken = xsrf;
        request.csrfCookie = "XSRF-TOKEN=" + xsrf;
        if (!csrf.empty()) request.csrfCookie += "; _csrf=" + csrf;
    }

    const auto response = http_.request(
        "POST",
        apiBase(server) + "/auth/jellyfin/quickconnect/initiate",
        quickConnectHeaders(server, request),
        "{}"
    );
    if (!response.ok()) {
        result.error = apiError(response);
        return result;
    }
    try {
        const auto data = json::parse(response.body);
        request.code = data.value("code", std::string{});
        request.secret = data.value("secret", std::string{});
        if (request.code.empty() || request.secret.empty()) {
            result.error = "Seerr Quick Connect returned an incomplete request";
            return result;
        }
        result.value = std::move(request);
        result.ok = true;
    } catch (const std::exception& e) {
        result.error = std::string("Invalid Seerr Quick Connect response: ") + e.what();
    }
    return result;
}

ApiValueResult<std::string> SeerrClient::authenticateQuickConnect(
    const std::string& server,
    const SeerrQuickConnectRequest& request
) const {
    ApiValueResult<std::string> result;
    if (request.secret.empty()) {
        result.error = "Seerr Quick Connect request is incomplete";
        return result;
    }
    const auto response = http_.request(
        "POST",
        apiBase(server) + "/auth/jellyfin/quickconnect/authenticate",
        quickConnectHeaders(server, request),
        json{{"secret", request.secret}}.dump()
    );
    if (!response.ok()) {
        result.error = apiError(response);
        return result;
    }
    result.value = sessionCookie(response.setCookie);
    if (result.value.empty()) {
        result.error = "Seerr authenticated but did not return a session cookie";
        return result;
    }
    result.ok = true;
    return result;
}

ApiValueResult<std::vector<SeerrStorageTarget>> SeerrClient::storageTargets(
    const std::string& server,
    const SeerrAuth& auth
) const {
    ApiValueResult<std::vector<SeerrStorageTarget>> result;
    if (!configured(server, auth)) {
        result.error = "Seerr is not connected";
        return result;
    }
    for (const std::string& mediaType : {std::string("movie"), std::string("tv")}) {
        const std::string service = mediaType == "movie" ? "radarr" : "sonarr";
        const auto servers = http_.request("GET", apiBase(server) + "/service/" + service, headers(auth));
        if (!servers.ok()) continue;
        try {
            const auto list = json::parse(servers.body);
            if (!list.is_array()) continue;
            for (const auto& entry : list) {
                if (!entry.is_object() || entry.value("is4k", false)) continue;
                const int id = integerValue(entry, "id");
                if (id <= 0) continue;
                const auto detailsResponse = http_.request(
                    "GET", apiBase(server) + "/service/" + service + "/" + std::to_string(id), headers(auth));
                if (!detailsResponse.ok()) continue;
                const auto details = json::parse(detailsResponse.body);
                const auto serverInfo = details.find("server");
                const int profileId = serverInfo != details.end() && serverInfo->is_object()
                    ? integerValue(*serverInfo, "activeProfileId")
                    : integerValue(entry, "activeProfileId");
                const std::string name = serverInfo != details.end() && serverInfo->is_object()
                    ? stringValue(*serverInfo, "name")
                    : stringValue(entry, "name");
                const bool isDefault = serverInfo != details.end() && serverInfo->is_object()
                    ? serverInfo->value("isDefault", false)
                    : entry.value("isDefault", false);
                const auto folders = details.find("rootFolders");
                if (folders == details.end() || !folders->is_array()) continue;
                for (const auto& folder : *folders) {
                    if (!folder.is_object()) continue;
                    SeerrStorageTarget target;
                    target.mediaType = mediaType;
                    target.serviceName = name;
                    target.path = stringValue(folder, "path");
                    target.serverId = id;
                    target.profileId = profileId;
                    target.freeSpace = int64Value(folder, "freeSpace");
                    target.totalSpace = int64Value(folder, "totalSpace");
                    target.isDefault = isDefault;
                    if (!target.path.empty() && target.totalSpace > 0) result.value.push_back(std::move(target));
                }
            }
        } catch (...) {
            continue;
        }
    }
    result.ok = true;
    return result;
}

ApiValueResult<std::vector<JellyfinItem>> SeerrClient::search(
    const std::string& server,
    const SeerrAuth& auth,
    const std::string& query
) const {
    ApiValueResult<std::vector<JellyfinItem>> result;
    if (!configured(server, auth)) {
        result.error = "Seerr is not connected";
        return result;
    }
    const auto response = http_.request(
        "GET",
        apiBase(server) + "/search?query=" + urlEncode(query) + "&page=1&language=en",
        headers(auth)
    );
    if (!response.ok()) {
        result.error = apiError(response);
        return result;
    }
    try {
        const auto data = json::parse(response.body);
        const auto values = data.find("results");
        if (values != data.end() && values->is_array()) {
            result.value.reserve(values->size());
            for (const auto& value : *values) {
                if (!value.is_object()) continue;
                JellyfinItem item = itemFromSearchResult(value);
                if (item.id.empty() || item.externalAvailable) continue;
                result.value.push_back(std::move(item));
                if (result.value.size() >= 24) break;
            }
        }
        result.ok = true;
    } catch (const std::exception& e) {
        result.error = std::string("Invalid Seerr search response: ") + e.what();
    }
    return result;
}

ApiValueResult<JellyfinItem> SeerrClient::loadMediaDetails(
    const std::string& server,
    const SeerrAuth& auth,
    const std::string& mediaType,
    int tmdbId
) const {
    ApiValueResult<JellyfinItem> result;
    if (mediaType != "movie" && mediaType != "tv") {
        result.error = "Unsupported Seerr media type";
        return result;
    }
    const std::string route = mediaType == "tv" ? "/tv/" : "/movie/";
    const auto response = http_.request("GET", apiBase(server) + route + std::to_string(tmdbId), headers(auth));
    if (!response.ok()) {
        result.error = apiError(response);
        return result;
    }
    try {
        const auto data = json::parse(response.body);
        result.value.externalSource = "seerr";
        result.value.externalMediaType = mediaType;
        result.value.tmdbId = std::to_string(tmdbId);
        result.value.id = "seerr:" + mediaType + ":" + result.value.tmdbId;
        result.value.type = mediaType == "tv" ? "Series" : "Movie";
        result.value.name = mediaType == "tv" ? stringValue(data, "name") : stringValue(data, "title");
        result.value.overview = stringValue(data, "overview");
        result.value.externalPosterUrl = tmdbImageUrl(kTmdbPosterBase, stringValue(data, "posterPath"));
        result.value.externalBackdropUrl = tmdbImageUrl(kTmdbBackdropBase, stringValue(data, "backdropPath"));
        const std::string date = mediaType == "tv" ? stringValue(data, "firstAirDate") : stringValue(data, "releaseDate");
        if (date.size() >= 4) {
            try { result.value.productionYear = std::stoi(date.substr(0, 4)); } catch (...) {}
        }
        result.ok = true;
    } catch (const std::exception& e) {
        result.error = std::string("Invalid Seerr media response: ") + e.what();
    }
    return result;
}

ApiValueResult<std::vector<JellyfinItem>> SeerrClient::pendingRequests(
    const std::string& server,
    const SeerrAuth& auth,
    int limit
) const {
    ApiValueResult<std::vector<JellyfinItem>> result;
    if (!configured(server, auth)) {
        result.ok = true;
        return result;
    }
    limit = std::clamp(limit, 1, 30);
    const auto response = http_.request(
        "GET",
        apiBase(server) + "/request?take=" + std::to_string(limit)
            + "&skip=0&filter=unavailable&sort=added&sortDirection=desc",
        headers(auth)
    );
    if (!response.ok()) {
        result.error = apiError(response);
        return result;
    }
    try {
        const auto data = json::parse(response.body);
        const auto values = data.find("results");
        if (values == data.end() || !values->is_array()) {
            result.ok = true;
            return result;
        }

        std::unordered_set<std::string> seen;
        for (const auto& request : *values) {
            if (!request.is_object()) continue;
            const auto media = request.find("media");
            if (media == request.end() || !media->is_object()) continue;
            const std::string mediaType = stringValue(*media, "mediaType");
            const int tmdbId = integerValue(*media, "tmdbId");
            if ((mediaType != "movie" && mediaType != "tv") || tmdbId <= 0) continue;
            const std::string unique = mediaType + ":" + std::to_string(tmdbId);
            if (!seen.insert(unique).second) continue;

            JellyfinItem item;
            item.externalSource = "seerr";
            item.externalMediaType = mediaType;
            item.tmdbId = std::to_string(tmdbId);
            item.id = "seerr:" + unique;
            item.type = mediaType == "tv" ? "Series" : "Movie";
            item.externalRequestId = integerValue(request, "id");
            item.externalRequested = true;
            const bool is4k = request.value("is4k", false);
            item.externalMediaStatus = integerValue(*media, is4k ? "status4k" : "status");
            item.externalStatus = mediaStatusLabel(item.externalMediaStatus, mediaType == "tv");
            const auto downloads = media->find(is4k ? "downloadStatus4k" : "downloadStatus");
            if (downloads != media->end()) applyDownloadProgress(item, *downloads);

            auto details = loadMediaDetails(server, auth, mediaType, tmdbId);
            if (details.ok) applyMediaDetails(item, details.value);
            if (item.name.empty()) item.name = mediaType == "tv" ? "Requested series" : "Requested movie";
            result.value.push_back(std::move(item));
        }
        result.ok = true;
    } catch (const std::exception& e) {
        result.error = std::string("Invalid Seerr request response: ") + e.what();
    }
    return result;
}

ApiValueResult<int> SeerrClient::requestMedia(
    const std::string& server,
    const SeerrAuth& auth,
    const JellyfinItem& item,
    const SeerrStorageTarget* target
) const {
    ApiValueResult<int> result;
    if (!configured(server, auth)) {
        result.error = "Seerr is not connected";
        return result;
    }
    if (!isSeerrItem(item) || item.tmdbId.empty()) {
        result.error = "Invalid Seerr item";
        return result;
    }
    int mediaId = 0;
    try { mediaId = std::stoi(item.tmdbId); } catch (...) {}
    if (mediaId <= 0) {
        result.error = "Invalid Seerr media ID";
        return result;
    }

    json body{
        {"mediaType", item.externalMediaType},
        {"mediaId", mediaId},
        {"is4k", false},
    };
    if (item.externalMediaType == "tv") body["seasons"] = "all";
    if (target && target->serverId > 0 && !target->path.empty()) {
        body["serverId"] = target->serverId;
        body["rootFolder"] = target->path;
        if (target->profileId > 0) body["profileId"] = target->profileId;
    }
    const auto response = http_.request("POST", apiBase(server) + "/request", headers(auth), body.dump());
    result.ok = response.status == 201;
    if (!result.ok) {
        result.error = apiError(response);
        return result;
    }
    try {
        if (!response.body.empty()) result.value = integerValue(json::parse(response.body), "id");
    } catch (...) {
        result.value = 0;
    }
    return result;
}

ApiResult SeerrClient::deleteRequest(
    const std::string& server,
    const SeerrAuth& auth,
    int requestId
) const {
    ApiResult result;
    if (!configured(server, auth)) {
        result.error = "Seerr is not connected";
        return result;
    }
    if (requestId <= 0) {
        result.error = "Seerr request ID is unavailable";
        return result;
    }
    const auto response = http_.request(
        "DELETE",
        apiBase(server) + "/request/" + std::to_string(requestId),
        headers(auth)
    );
    result.ok = response.ok();
    if (!result.ok) result.error = apiError(response);
    return result;
}

ApiValueResult<std::string> SeerrClient::downloadImage(const std::string& url) const {
    ApiValueResult<std::string> result;
    if (url.empty()) {
        result.error = "Image URL unavailable";
        return result;
    }
    const auto response = http_.request("GET", url, {{"Accept", "image/*"}});
    if (!response.ok()) {
        result.error = apiError(response);
        return result;
    }
    result.ok = true;
    result.value = response.body;
    return result;
}
