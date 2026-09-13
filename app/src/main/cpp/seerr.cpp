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
}

std::string SeerrClient::apiBase(std::string server) const {
    while (!server.empty() && std::isspace(static_cast<unsigned char>(server.front()))) server.erase(server.begin());
    while (!server.empty() && std::isspace(static_cast<unsigned char>(server.back()))) server.pop_back();
    while (!server.empty() && server.back() == '/') server.pop_back();
    if (!server.empty() && server.find("://") == std::string::npos) server = "https://" + server;
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

std::map<std::string, std::string> SeerrClient::headers(const std::string& apiKey) const {
    return {
        {"Accept", "application/json"},
        {"Content-Type", "application/json"},
        {"X-Api-Key", apiKey},
        {"User-Agent", "sloppaTV/0.1.0"},
    };
}

ApiValueResult<std::vector<JellyfinItem>> SeerrClient::search(
    const std::string& server,
    const std::string& apiKey,
    const std::string& query
) const {
    ApiValueResult<std::vector<JellyfinItem>> result;
    if (!configured(server, apiKey)) {
        result.error = "Seerr is not connected";
        return result;
    }
    const auto response = http_.request(
        "GET",
        apiBase(server) + "/search?query=" + urlEncode(query) + "&page=1&language=en",
        headers(apiKey)
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
    const std::string& apiKey,
    const std::string& mediaType,
    int tmdbId
) const {
    ApiValueResult<JellyfinItem> result;
    if (mediaType != "movie" && mediaType != "tv") {
        result.error = "Unsupported Seerr media type";
        return result;
    }
    const std::string route = mediaType == "tv" ? "/tv/" : "/movie/";
    const auto response = http_.request("GET", apiBase(server) + route + std::to_string(tmdbId), headers(apiKey));
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
    const std::string& apiKey,
    int limit
) const {
    ApiValueResult<std::vector<JellyfinItem>> result;
    if (!configured(server, apiKey)) {
        result.ok = true;
        return result;
    }
    limit = std::clamp(limit, 1, 30);
    const auto response = http_.request(
        "GET",
        apiBase(server) + "/request?take=" + std::to_string(limit)
            + "&skip=0&filter=unavailable&sort=added&sortDirection=desc",
        headers(apiKey)
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

            auto details = loadMediaDetails(server, apiKey, mediaType, tmdbId);
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
    const std::string& apiKey,
    const JellyfinItem& item
) const {
    ApiValueResult<int> result;
    if (!configured(server, apiKey)) {
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
    const auto response = http_.request("POST", apiBase(server) + "/request", headers(apiKey), body.dump());
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
    const std::string& apiKey,
    int requestId
) const {
    ApiResult result;
    if (!configured(server, apiKey)) {
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
        headers(apiKey)
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
