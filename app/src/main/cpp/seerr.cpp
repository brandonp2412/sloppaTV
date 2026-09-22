#include "seerr.hpp"
#include "seerr_season_parser.hpp"
#include "json_boolean.hpp"
#include "seerr_download_progress.hpp"

#include <nlohmann/json.hpp>

#include <algorithm>
#include <charconv>
#include <cctype>
#include <iomanip>
#include <limits>
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
        if (found->is_number_unsigned()) {
            const uint64_t number = found->get<uint64_t>();
            return number <= static_cast<uint64_t>(std::numeric_limits<int>::max()) ? static_cast<int>(number)
                                                                                    : fallback;
        }
        if (found->is_number_integer()) {
            const int64_t number = found->get<int64_t>();
            return number >= std::numeric_limits<int>::min() && number <= std::numeric_limits<int>::max()
                       ? static_cast<int>(number)
                       : fallback;
        }
        if (found->is_string()) {
            const std::string text = found->get<std::string>();
            size_t consumed = 0;
            const long long number = std::stoll(text, &consumed);
            return consumed == text.size() && number >= std::numeric_limits<int>::min() &&
                           number <= std::numeric_limits<int>::max()
                       ? static_cast<int>(number)
                       : fallback;
        }
    } catch (...) {}
    return fallback;
}

double doubleValue(const json& value, const char* key, double fallback = 0.0) {
    const auto found = value.find(key);
    if (found == value.end() || found->is_null()) return fallback;
    try {
        if (found->is_number()) return found->get<double>();
        if (found->is_string()) {
            const std::string text = found->get<std::string>();
            size_t consumed = 0;
            const double number = std::stod(text, &consumed);
            return consumed == text.size() ? number : fallback;
        }
    } catch (...) {}
    return fallback;
}

int productionYearFromDate(std::string_view date) {
    if (date.size() < 4) return 0;
    int year = 0;
    const char* begin = date.data();
    const char* end = begin + 4;
    const auto [parsedEnd, error] = std::from_chars(begin, end, year);
    return error == std::errc{} && parsedEnd == end && year > 0 ? year : 0;
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
    (void)television;
    switch (status) {
    case 1:
        return "Requested";
    case 2:
        return "Queued";
    case 3:
        return "Waiting for download";
    case 4:
        return "Partially available";
    case 5:
        return "Available";
    case 6:
        return "Blocklisted";
    case 7:
        return "Deleted";
    default:
        return "Requested";
    }
}

SeerrMediaItem itemFromSearchResult(const json& value) {
    SeerrMediaItem item;
    const std::string mediaType = stringValue(value, "mediaType");
    if (mediaType != "movie" && mediaType != "tv") return item;
    const int tmdbId = integerValue(value, "id");
    if (tmdbId <= 0) return item;

    item.mediaType = mediaType;
    item.tmdbId = tmdbId;
    item.id = seerrMediaId(mediaType, tmdbId);
    item.name = mediaType == "tv" ? stringValue(value, "name") : stringValue(value, "title");
    if (item.name.empty()) item.name = stringValue(value, "originalName");
    if (item.name.empty()) item.name = stringValue(value, "originalTitle");
    item.overview = stringValue(value, "overview");
    item.posterUrl = tmdbImageUrl(kTmdbPosterBase, stringValue(value, "posterPath"));
    item.backdropUrl = tmdbImageUrl(kTmdbBackdropBase, stringValue(value, "backdropPath"));

    const std::string date = mediaType == "tv" ? stringValue(value, "firstAirDate") : stringValue(value, "releaseDate");
    item.productionYear = productionYearFromDate(date);

    const auto mediaInfo = value.find("mediaInfo");
    if (mediaInfo != value.end() && mediaInfo->is_object()) {
        item.mediaStatus = integerValue(*mediaInfo, "status");
        item.available = item.mediaStatus == 5;
        item.requested = item.mediaStatus >= 2 && item.mediaStatus <= 4;
        item.status = mediaStatusLabel(item.mediaStatus, mediaType == "tv");
        const auto requests = mediaInfo->find("requests");
        if (requests != mediaInfo->end() && requests->is_array() && !requests->empty()) {
            item.requested = true;
            item.requestId = integerValue(requests->front(), "id");
        }
    }
    return item;
}

void applyMediaDetails(SeerrMediaItem& item, const SeerrMediaItem& details) {
    if (!details.name.empty()) item.name = details.name;
    if (!details.overview.empty()) item.overview = details.overview;
    if (!details.posterUrl.empty()) item.posterUrl = details.posterUrl;
    if (!details.backdropUrl.empty()) item.backdropUrl = details.backdropUrl;
    if (details.productionYear > 0) item.productionYear = details.productionYear;
}

void applyDownloadProgress(SeerrMediaItem& item, const json& downloads) {
    if (!downloads.is_array() || downloads.empty()) return;

    std::vector<SeerrDownloadProgressEntry> entries;
    entries.reserve(downloads.size());
    for (const auto& download : downloads) {
        if (!download.is_object()) continue;
        SeerrDownloadProgressEntry entry;
        entry.size = doubleValue(download, "size");
        entry.sizeLeft = doubleValue(download, "sizeLeft");
        entry.timeLeft = stringValue(download, "timeLeft");
        const auto episode = download.find("episode");
        if (episode != download.end() && episode->is_object()) {
            entry.seasonNumber = integerValue(*episode, "seasonNumber", -1);
            entry.episodeNumber = integerValue(*episode, "episodeNumber", -1);
        }
        entries.push_back(std::move(entry));
    }
    if (entries.empty()) return;

    const auto progress = seerrDownloadProgressSummary(item.mediaType, item.mediaStatus, entries);
    item.progressPercent = progress.percent;
    item.progressLabel = progress.label;
    item.progressEta = progress.eta;
    if (!progress.status.empty()) item.status = progress.status;
}

std::string cookieValue(const std::string& headers, const std::string& wanted) {
    size_t start = 0;
    while (start < headers.size()) {
        const size_t end = headers.find('\n', start);
        const std::string_view line(headers.data() + start, (end == std::string::npos ? headers.size() : end) - start);
        const size_t equals = line.find('=');
        if (equals != std::string_view::npos && line.substr(0, equals) == wanted) {
            const size_t semi = line.find(';', equals + 1);
            return std::string(
                line.substr(equals + 1, (semi == std::string_view::npos ? line.size() : semi) - equals - 1));
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
        std::string_view line(setCookies.data() + start, (end == std::string::npos ? setCookies.size() : end) - start);
        const size_t equals = line.find('=');
        if (equals != std::string_view::npos) {
            const std::string_view name = line.substr(0, equals);
            const size_t semi = line.find(';', equals + 1);
            const std::string_view cookie =
                line.substr(equals + 1, (semi == std::string_view::npos ? line.size() : semi) - equals - 1);
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
        if (found->is_number_unsigned()) {
            const uint64_t number = found->get<uint64_t>();
            return number <= static_cast<uint64_t>(std::numeric_limits<int64_t>::max()) ? static_cast<int64_t>(number)
                                                                                        : 0;
        }
        if (found->is_number_integer()) return found->get<int64_t>();
        if (found->is_string()) {
            const std::string text = found->get<std::string>();
            size_t consumed = 0;
            const int64_t number = std::stoll(text, &consumed);
            return consumed == text.size() ? number : 0;
        }
    } catch (...) {}
    return 0;
}
} // namespace

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
        if (std::isalnum(c) || c == '-' || c == '_' || c == '.' || c == '~')
            encoded << c;
        else
            encoded << '%' << std::setw(2) << std::setfill('0') << static_cast<int>(c);
    }
    return encoded.str();
}

std::map<std::string, std::string> SeerrClient::headers(const SeerrAuth& auth) const {
    std::map<std::string, std::string> result{
        {"Accept", "application/json"},
        {"Content-Type", "application/json"},
        {"User-Agent", "sloppaTV/0.1.0"},
    };
    if (!auth.sessionCookie.empty())
        result["Cookie"] = auth.sessionCookie;
    else if (!auth.apiKey.empty())
        result["X-Api-Key"] = auth.apiKey;
    return result;
}

std::map<std::string, std::string> SeerrClient::quickConnectHeaders(const std::string& server,
                                                                    const SeerrQuickConnectRequest& request) const {
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
    const auto seed = http_.request("GET", apiBase(server) + "/auth/me",
                                    {
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

    const auto response = http_.request("POST", apiBase(server) + "/auth/jellyfin/quickconnect/initiate",
                                        quickConnectHeaders(server, request), "{}");
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

ApiValueResult<std::string> SeerrClient::authenticateQuickConnect(const std::string& server,
                                                                  const SeerrQuickConnectRequest& request) const {
    ApiValueResult<std::string> result;
    if (request.secret.empty()) {
        result.error = "Seerr Quick Connect request is incomplete";
        return result;
    }
    const auto response = http_.request("POST", apiBase(server) + "/auth/jellyfin/quickconnect/authenticate",
                                        quickConnectHeaders(server, request), json{{"secret", request.secret}}.dump());
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

ApiValueResult<std::vector<SeerrStorageTarget>> SeerrClient::storageTargets(const std::string& server,
                                                                            const SeerrAuth& auth) const {
    ApiValueResult<std::vector<SeerrStorageTarget>> result;
    if (!configured(server, auth)) {
        result.error = "Seerr is not connected";
        return result;
    }

    std::unordered_set<std::string> seenTargets;
    std::vector<std::string> failures;
    auto addTarget = [&](const std::string& mediaType, const std::string& serviceName, const std::string& path,
                         int serverId, int profileId, int64_t freeSpace, int64_t totalSpace, bool isDefault,
                         bool is4k) {
        if (path.empty() || serverId < 0) return;
        const std::string key = mediaType + ":" + std::to_string(serverId) + ":" + path;
        if (!seenTargets.insert(key).second) return;
        SeerrStorageTarget target;
        target.mediaType = mediaType;
        target.serviceName = serviceName;
        target.path = path;
        target.serverId = serverId;
        target.profileId = profileId;
        target.freeSpace = freeSpace;
        target.totalSpace = totalSpace;
        target.isDefault = isDefault;
        target.is4k = is4k;
        result.value.push_back(std::move(target));
    };

    for (const std::string& mediaType : {std::string("movie"), std::string("tv")}) {
        const std::string service = mediaType == "movie" ? "radarr" : "sonarr";
        const auto servers = http_.request("GET", apiBase(server) + "/service/" + service, headers(auth));
        if (!servers.ok()) {
            failures.push_back(service + ": " + apiError(servers));
            continue;
        }
        try {
            const auto list = json::parse(servers.body);
            if (!list.is_array()) {
                failures.push_back(service + ": invalid service response");
                continue;
            }
            for (const auto& entry : list) {
                if (!entry.is_object()) continue;
                const int id = integerValue(entry, "id", -1);
                if (id < 0) continue;

                bool is4k = jsonBooleanValue(entry, "is4k", false);
                bool isDefault = jsonBooleanValue(entry, "isDefault", false);
                std::string name = stringValue(entry, "name");
                int profileId = integerValue(entry, "activeProfileId");
                int animeProfileId = integerValue(entry, "activeAnimeProfileId");
                std::string activeDirectory = stringValue(entry, "activeDirectory");
                std::string animeDirectory = stringValue(entry, "activeAnimeDirectory");

                const auto detailsResponse = http_.request(
                    "GET", apiBase(server) + "/service/" + service + "/" + std::to_string(id), headers(auth));
                if (detailsResponse.ok()) {
                    try {
                        const auto details = json::parse(detailsResponse.body);
                        const auto serverInfo = details.find("server");
                        if (serverInfo != details.end() && serverInfo->is_object()) {
                            if (!stringValue(*serverInfo, "name").empty()) name = stringValue(*serverInfo, "name");
                            is4k = jsonBooleanValue(*serverInfo, "is4k", is4k);
                            isDefault = jsonBooleanValue(*serverInfo, "isDefault", isDefault);
                            profileId = integerValue(*serverInfo, "activeProfileId", profileId);
                            animeProfileId = integerValue(*serverInfo, "activeAnimeProfileId", animeProfileId);
                            if (!stringValue(*serverInfo, "activeDirectory").empty()) {
                                activeDirectory = stringValue(*serverInfo, "activeDirectory");
                            }
                            if (!stringValue(*serverInfo, "activeAnimeDirectory").empty()) {
                                animeDirectory = stringValue(*serverInfo, "activeAnimeDirectory");
                            }
                        }
                        const auto folders = details.find("rootFolders");
                        if (folders != details.end() && folders->is_array()) {
                            for (const auto& folder : *folders) {
                                if (!folder.is_object()) continue;
                                const std::string path = stringValue(folder, "path");
                                const bool anime =
                                    mediaType == "tv" && !animeDirectory.empty() && path == animeDirectory;
                                addTarget(mediaType, anime ? name + " - Anime" : name, path, id,
                                          anime && animeProfileId > 0 ? animeProfileId : profileId,
                                          int64Value(folder, "freeSpace"), int64Value(folder, "totalSpace"), isDefault,
                                          is4k);
                            }
                        }
                    } catch (const std::exception& e) {
                        failures.push_back(service + " details: " + e.what());
                    }
                } else {
                    failures.push_back(service + " details: " + apiError(detailsResponse));
                }

                // Seerr's service list already contains the configured active directories.
                // Keep those as routing fallbacks when Sonarr/Radarr cannot report root-folder
                // capacity/details (common with some remote mounts and permission setups).
                addTarget(mediaType, name, activeDirectory, id, profileId, 0, 0, isDefault, is4k);
                if (mediaType == "tv" && !animeDirectory.empty() && animeDirectory != activeDirectory) {
                    addTarget(mediaType, name + " - Anime", animeDirectory, id,
                              animeProfileId > 0 ? animeProfileId : profileId, 0, 0, isDefault, is4k);
                }
            }
        } catch (const std::exception& e) {
            failures.push_back(service + ": " + e.what());
        }
    }

    if (result.value.empty()) {
        result.error = failures.empty() ? "No Sonarr/Radarr storage directories are configured" : failures.front();
        return result;
    }
    result.ok = true;
    return result;
}

ApiValueResult<std::vector<SeerrMediaItem>> SeerrClient::search(const std::string& server, const SeerrAuth& auth,
                                                                const std::string& query) const {
    ApiValueResult<std::vector<SeerrMediaItem>> result;
    if (!configured(server, auth)) {
        result.error = "Seerr is not connected";
        return result;
    }
    const auto response = http_.request(
        "GET", apiBase(server) + "/search?query=" + urlEncode(query) + "&page=1&language=en", headers(auth));
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
                SeerrMediaItem item = itemFromSearchResult(value);
                if (item.id.empty() || (item.available && !item.television())) continue;
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

ApiValueResult<SeerrMediaItem> SeerrClient::loadMediaDetails(const std::string& server, const SeerrAuth& auth,
                                                             const std::string& mediaType, int tmdbId) const {
    ApiValueResult<SeerrMediaItem> result;
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
        result.value.mediaType = mediaType;
        result.value.tmdbId = tmdbId;
        result.value.id = seerrMediaId(mediaType, tmdbId);
        result.value.name = mediaType == "tv" ? stringValue(data, "name") : stringValue(data, "title");
        result.value.overview = stringValue(data, "overview");
        result.value.posterUrl = tmdbImageUrl(kTmdbPosterBase, stringValue(data, "posterPath"));
        result.value.backdropUrl = tmdbImageUrl(kTmdbBackdropBase, stringValue(data, "backdropPath"));
        const std::string date =
            mediaType == "tv" ? stringValue(data, "firstAirDate") : stringValue(data, "releaseDate");
        result.value.productionYear = productionYearFromDate(date);
        result.ok = true;
    } catch (const std::exception& e) {
        result.error = std::string("Invalid Seerr media response: ") + e.what();
    }
    return result;
}

ApiValueResult<std::vector<SeerrMediaItem>> SeerrClient::pendingRequests(const std::string& server,
                                                                         const SeerrAuth& auth, int limit) const {
    ApiValueResult<std::vector<SeerrMediaItem>> result;
    if (!configured(server, auth)) {
        result.ok = true;
        return result;
    }
    limit = std::clamp(limit, 1, 30);
    const auto response = http_.request("GET",
                                        apiBase(server) + "/request?take=" + std::to_string(limit) +
                                            "&skip=0&filter=unavailable&sort=added&sortDirection=desc",
                                        headers(auth));
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

            SeerrMediaItem item;
            item.mediaType = mediaType;
            item.tmdbId = tmdbId;
            item.id = seerrMediaId(mediaType, tmdbId);
            item.requestId = integerValue(request, "id");
            item.requested = true;
            const bool is4k = jsonBooleanValue(request, "is4k", false);
            item.mediaStatus = integerValue(*media, is4k ? "status4k" : "status");
            item.jellyfinId = stringValue(*media, is4k ? "jellyfinMediaId4k" : "jellyfinMediaId");
            item.status = mediaStatusLabel(item.mediaStatus, mediaType == "tv");
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

ApiValueResult<std::vector<SeerrSeason>> SeerrClient::seasons(const std::string& server, const SeerrAuth& auth,
                                                              int tmdbId, bool is4k) const {
    ApiValueResult<std::vector<SeerrSeason>> result;
    const auto response = http_.request("GET", apiBase(server) + "/tv/" + std::to_string(tmdbId), headers(auth));
    if (!response.ok()) {
        result.error = apiError(response);
        return result;
    }
    try {
        result.value = parseSeerrSeasons(json::parse(response.body), is4k);
        result.ok = true;
    } catch (const std::exception& e) {
        result.error = std::string("Invalid Seerr seasons response: ") + e.what();
    }
    return result;
}

ApiValueResult<int> SeerrClient::requestMedia(const std::string& server, const SeerrAuth& auth,
                                              const SeerrMediaItem& item, const SeerrStorageTarget* target) const {
    ApiValueResult<int> result;
    if (!configured(server, auth)) {
        result.error = "Seerr is not connected";
        return result;
    }
    if (!item.valid()) {
        result.error = "Invalid Seerr item";
        return result;
    }
    const int mediaId = item.tmdbId;
    if (mediaId <= 0) {
        result.error = "Invalid Seerr media ID";
        return result;
    }

    json body{
        {"mediaType", item.mediaType},
        {"mediaId", mediaId},
        {"is4k", target ? target->is4k : false},
    };
    if (item.television()) {
        if (item.selectedSeasons.empty() || std::any_of(item.selectedSeasons.begin(), item.selectedSeasons.end(),
                                                        [](int number) { return number < 0; })) {
            result.error = "Select at least one season";
            return result;
        }
        body["seasons"] = item.selectedSeasons;
    }
    if (target && target->serverId >= 0 && !target->path.empty()) {
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

ApiResult SeerrClient::deleteRequest(const std::string& server, const SeerrAuth& auth, int requestId) const {
    ApiResult result;
    if (!configured(server, auth)) {
        result.error = "Seerr is not connected";
        return result;
    }
    if (requestId <= 0) {
        result.error = "Seerr request ID is unavailable";
        return result;
    }
    const auto response =
        http_.request("DELETE", apiBase(server) + "/request/" + std::to_string(requestId), headers(auth));
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
