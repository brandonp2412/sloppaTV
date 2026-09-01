#include "jellyfin.hpp"
#include "media_player_policy.hpp"
#include "ui_policy.hpp"

#include <android/log.h>

#include <algorithm>
#include <cctype>
#include <iomanip>
#include <sstream>

using nlohmann::json;

namespace {
constexpr const char* kTag = "sloppaTV/api";
constexpr const char* kClientName = "sloppaTV";
constexpr const char* kClientVersion = "0.1.0";
constexpr const char* kDeviceName = "Android TV";

std::string apiError(const HttpResponse& response) {
    if (!response.error.empty()) return response.error;
    std::string detail;
    try {
        if (!response.body.empty()) {
            const auto body = json::parse(response.body);
            detail = body.value("Message", body.value("message", std::string{}));
        }
    } catch (...) {
        detail.clear();
    }
    std::string result = "HTTP " + std::to_string(response.status);
    if (!detail.empty()) result += ": " + detail;
    return result;
}

std::string addApiKey(std::string url, const std::string& token) {
    if (token.empty() || url.find("api_key=") != std::string::npos) return url;
    url += url.find('?') == std::string::npos ? "?" : "&";
    url += "api_key=" + token;
    return url;
}

std::string firstContainer(std::string container) {
    const auto comma = container.find(',');
    if (comma != std::string::npos) container.resize(comma);
    container.erase(std::remove_if(container.begin(), container.end(), [](unsigned char c) {
        return !std::isalnum(c);
    }), container.end());
    return container.empty() ? "mp4" : container;
}

std::string joinCodecs(const std::vector<std::string>& codecs) {
    std::ostringstream out;
    for (size_t index = 0; index < codecs.size(); ++index) {
        if (index) out << ',';
        out << codecs[index];
    }
    return out.str();
}

bool isScopedVideoItem(const JellyfinItem& item) {
    return item.type == "Movie" || item.type == "Series" || item.type == "Episode";
}

bool isScopedVideoCollection(const JellyfinItem& item) {
    if (item.collectionType.empty()) return true;
    return item.collectionType == "movies" || item.collectionType == "tvshows"
        || item.collectionType == "mixed" || item.collectionType == "boxsets";
}

void retainScopedVideoItems(std::vector<JellyfinItem>& items) {
    items.erase(std::remove_if(items.begin(), items.end(), [](const JellyfinItem& item) {
        return !isScopedVideoItem(item);
    }), items.end());
}
}  // namespace

std::string JellyfinClient::normalizeServer(std::string value) const {
    while (!value.empty() && std::isspace(static_cast<unsigned char>(value.front()))) value.erase(value.begin());
    while (!value.empty() && std::isspace(static_cast<unsigned char>(value.back()))) value.pop_back();
    while (!value.empty() && value.back() == '/') value.pop_back();
    if (!value.empty() && value.find("://") == std::string::npos) value = "https://" + value;
    return value;
}

std::string JellyfinClient::discoverServerBase(const std::string& value, const std::string& deviceId) const {
    const std::string normalized = normalizeServer(value);
    if (normalized.empty()) return {};

    std::vector<std::string> candidates{normalized};
    if (normalized.size() < 9 || normalized.substr(normalized.size() - 9) != "/jellyfin") {
        candidates.push_back(normalized + "/jellyfin");
    }

    for (const auto& candidate : candidates) {
        const auto response = http_.request(
            "GET",
            candidate + "/System/Info/Public",
            headers(nullptr, deviceId)
        );
        if (!response.ok() || response.body.empty()) continue;
        try {
            const auto data = json::parse(response.body);
            if (data.is_object() && data.contains("Version") && data.contains("Id")) {
                return candidate;
            }
        } catch (...) {
            // A root URL with a configured Jellyfin BaseUrl may redirect to the web UI.
            // Ignore non-JSON responses and try the conventional /jellyfin base path.
        }
    }
    return {};
}

std::string JellyfinClient::authorization(const JellyfinSession* session, const std::string& deviceId) const {
    std::ostringstream out;
    out << "MediaBrowser "
        << "Client=\"" << kClientName << "\","
        << "Version=\"" << kClientVersion << "\","
        << "DeviceId=\"" << deviceId << "\","
        << "Device=\"" << kDeviceName << "\"";
    if (session && !session->token.empty()) out << ",Token=\"" << session->token << "\"";
    return out.str();
}

std::map<std::string, std::string> JellyfinClient::headers(const JellyfinSession* session, const std::string& deviceId) const {
    std::map<std::string, std::string> result{
        {"Accept", "application/json"},
        {"Content-Type", "application/json"},
        {"Authorization", authorization(session, deviceId)},
        {"User-Agent", "sloppaTV/0.1.0"},
    };
    if (session && !session->token.empty()) result["X-Emby-Token"] = session->token;
    return result;
}

std::string JellyfinClient::urlEncode(const std::string& value) const {
    std::ostringstream escaped;
    escaped << std::uppercase << std::hex;
    for (const unsigned char c : value) {
        if (std::isalnum(c) || c == '-' || c == '_' || c == '.' || c == '~') {
            escaped << static_cast<char>(c);
        } else {
            escaped << '%' << std::setw(2) << std::setfill('0') << static_cast<int>(c);
        }
    }
    return escaped.str();
}

ApiValueResult<JellyfinSession> JellyfinClient::parseAuthenticationResult(
    const HttpResponse& response,
    const std::string& server,
    const std::string& deviceId,
    const std::string& fallbackUsername
) const {
    ApiValueResult<JellyfinSession> result;
    if (!response.ok()) {
        result.error = apiError(response);
        return result;
    }

    try {
        const auto data = json::parse(response.body);
        JellyfinSession session;
        session.server = server;
        session.username = fallbackUsername;
        session.deviceId = deviceId;
        session.token = data.value("AccessToken", std::string{});
        if (data.contains("User") && data["User"].is_object()) {
            session.userId = data["User"].value("Id", std::string{});
            session.username = data["User"].value("Name", session.username);
        }
        if (!session.valid()) {
            result.error = "Jellyfin login returned an incomplete session";
            return result;
        }
        result.value = std::move(session);
        result.ok = true;
    } catch (const std::exception& e) {
        result.error = std::string("Invalid login response: ") + e.what();
    }
    return result;
}

ApiValueResult<JellyfinSession> JellyfinClient::login(
    std::string server,
    const std::string& username,
    const std::string& password,
    const std::string& deviceId
) const {
    ApiValueResult<JellyfinSession> result;
    server = normalizeServer(std::move(server));
    if (server.empty() || username.empty()) {
        result.error = "Server and username are required";
        return result;
    }

    const std::string discoveredServer = discoverServerBase(server, deviceId);
    if (discoveredServer.empty()) {
        result.error = "Could not find a Jellyfin server at that address";
        return result;
    }
    server = discoveredServer;

    json body = {
        {"Username", username},
        {"Pw", password},
    };
    const HttpResponse response = http_.request(
        "POST",
        server + "/Users/AuthenticateByName",
        headers(nullptr, deviceId),
        body.dump()
    );
    return parseAuthenticationResult(response, server, deviceId, username);
}

ApiValueResult<QuickConnectRequest> JellyfinClient::initiateQuickConnect(
    std::string server,
    const std::string& deviceId
) const {
    ApiValueResult<QuickConnectRequest> result;
    server = normalizeServer(std::move(server));
    if (server.empty()) {
        result.error = "Server is required";
        return result;
    }

    const std::string discoveredServer = discoverServerBase(server, deviceId);
    if (discoveredServer.empty()) {
        result.error = "Could not find a Jellyfin server at that address";
        return result;
    }
    server = discoveredServer;

    const auto enabled = http_.request(
        "GET",
        server + "/QuickConnect/Enabled",
        headers(nullptr, deviceId)
    );
    if (!enabled.ok()) {
        result.error = apiError(enabled);
        return result;
    }
    if (enabled.body.find("true") == std::string::npos) {
        result.error = "Quick Connect is disabled on this Jellyfin server";
        return result;
    }

    const auto response = http_.request(
        "POST",
        server + "/QuickConnect/Initiate",
        headers(nullptr, deviceId)
    );
    if (!response.ok()) {
        result.error = apiError(response);
        return result;
    }

    try {
        const auto data = json::parse(response.body);
        QuickConnectRequest request;
        request.server = server;
        request.secret = data.value("Secret", std::string{});
        request.code = data.value("Code", std::string{});
        if (request.secret.empty() || request.code.empty()) {
            result.error = "Quick Connect returned an incomplete request";
            return result;
        }
        result.value = std::move(request);
        result.ok = true;
    } catch (const std::exception& e) {
        result.error = std::string("Invalid Quick Connect response: ") + e.what();
    }
    return result;
}

ApiValueResult<bool> JellyfinClient::pollQuickConnect(
    const QuickConnectRequest& request,
    const std::string& deviceId
) const {
    ApiValueResult<bool> result;
    if (request.server.empty() || request.secret.empty()) {
        result.error = "Quick Connect request is incomplete";
        return result;
    }
    const auto response = http_.request(
        "GET",
        request.server + "/QuickConnect/Connect?secret=" + urlEncode(request.secret),
        headers(nullptr, deviceId)
    );
    if (!response.ok()) {
        result.error = apiError(response);
        return result;
    }
    try {
        const auto data = json::parse(response.body);
        result.value = data.value("Authenticated", false);
        result.ok = true;
    } catch (const std::exception& e) {
        result.error = std::string("Invalid Quick Connect state response: ") + e.what();
    }
    return result;
}

ApiValueResult<JellyfinSession> JellyfinClient::completeQuickConnect(
    const QuickConnectRequest& request,
    const std::string& deviceId
) const {
    if (request.server.empty() || request.secret.empty()) {
        ApiValueResult<JellyfinSession> result;
        result.error = "Quick Connect request is incomplete";
        return result;
    }
    const json body = {{"Secret", request.secret}};
    const auto response = http_.request(
        "POST",
        request.server + "/Users/AuthenticateWithQuickConnect",
        headers(nullptr, deviceId),
        body.dump()
    );
    return parseAuthenticationResult(response, request.server, deviceId, "");
}

JellyfinItem JellyfinClient::parseItem(const json& value) const {
    JellyfinItem item;
    item.id = value.value("Id", std::string{});
    item.name = value.value("Name", std::string{});
    item.type = value.value("Type", std::string{});
    item.collectionType = value.value("CollectionType", std::string{});
    item.seriesId = value.value("SeriesId", std::string{});
    item.seriesName = value.value("SeriesName", std::string{});
    item.seriesPrimaryImageTag = value.value("SeriesPrimaryImageTag", std::string{});
    item.seasonName = value.value("SeasonName", std::string{});
    item.overview = value.value("Overview", std::string{});
    item.container = value.value("Container", std::string{});
    item.officialRating = value.value("OfficialRating", std::string{});
    item.productionYear = value.value("ProductionYear", 0);
    if (value.contains("CommunityRating") && value["CommunityRating"].is_number()) {
        item.communityRating = value["CommunityRating"].get<float>();
    }
    item.indexNumber = value.value("IndexNumber", -1);
    item.parentIndexNumber = value.value("ParentIndexNumber", -1);
    item.runtimeTicks = value.value("RunTimeTicks", static_cast<int64_t>(0));
    item.canDelete = value.value("CanDelete", false);

    if (value.contains("ProviderIds") && value["ProviderIds"].is_object()) {
        item.tmdbId = value["ProviderIds"].value("Tmdb", std::string{});
        item.tmdbCollectionId = value["ProviderIds"].value("TmdbCollection", std::string{});
    }
    if (value.contains("Genres") && value["Genres"].is_array()) {
        for (const auto& genre : value["Genres"]) {
            if (genre.is_string()) item.genres.push_back(genre.get<std::string>());
        }
    }
    if (value.contains("People") && value["People"].is_array()) {
        for (const auto& person : value["People"]) {
            if (!person.is_object() || person.value("Type", std::string{}) != "Actor") continue;
            JellyfinPerson parsed;
            parsed.id = person.value("Id", std::string{});
            parsed.name = person.value("Name", std::string{});
            parsed.imageTag = person.value("PrimaryImageTag", std::string{});
            parsed.role = person.value("Role", std::string{});
            if (parsed.name.empty()) continue;
            item.cast.push_back(parsed.name);
            item.people.push_back(std::move(parsed));
            if (item.people.size() >= 12) break;
        }
    }
    if (value.contains("Chapters") && value["Chapters"].is_array()) {
        for (const auto& chapter : value["Chapters"]) {
            if (!chapter.is_object()) continue;
            JellyfinChapter parsed;
            parsed.name = chapter.value("Name", std::string{});
            parsed.startTicks = chapter.value("StartPositionTicks", static_cast<int64_t>(0));
            item.chapters.push_back(std::move(parsed));
        }
    }

    if (value.contains("UserData") && value["UserData"].is_object()) {
        item.positionTicks = value["UserData"].value("PlaybackPositionTicks", static_cast<int64_t>(0));
        item.favorite = value["UserData"].value("IsFavorite", false);
        item.played = value["UserData"].value("Played", false);
    }
    if (value.contains("ImageTags") && value["ImageTags"].is_object()) {
        item.imageTag = value["ImageTags"].value("Primary", std::string{});
        item.thumbTag = value["ImageTags"].value("Thumb", std::string{});
    }
    if (value.contains("BackdropImageTags") && value["BackdropImageTags"].is_array() && !value["BackdropImageTags"].empty()) {
        item.backdropTag = value["BackdropImageTags"][0].get<std::string>();
    }
    if (value.contains("MediaSources") && value["MediaSources"].is_array() && !value["MediaSources"].empty()) {
        const auto& source = value["MediaSources"][0];
        item.mediaSourceId = source.value("Id", std::string{});
        if (item.container.empty()) item.container = source.value("Container", std::string{});
        if (source.contains("MediaStreams") && source["MediaStreams"].is_array()) {
            for (const auto& stream : source["MediaStreams"]) {
                if (!stream.is_object()) continue;
                const std::string streamType = stream.value("Type", std::string{});
                if (streamType == "Video" && item.videoCodec.empty()) {
                    item.videoCodec = stream.value("Codec", std::string{});
                    item.videoProfile = stream.value("Profile", std::string{});
                    item.videoRangeType = stream.value("VideoRangeType", std::string{});
                    item.videoWidth = stream.value("Width", 0);
                    item.videoHeight = stream.value("Height", 0);
                    item.videoBitDepth = stream.value("BitDepth", 0);
                    if (stream.contains("Level") && stream["Level"].is_number_integer()) {
                        item.videoLevel = stream["Level"].get<int>();
                    }
                    if (stream.contains("RealFrameRate") && stream["RealFrameRate"].is_number()) {
                        item.videoFrameRate = stream["RealFrameRate"].get<float>();
                    } else if (stream.contains("AverageFrameRate") && stream["AverageFrameRate"].is_number()) {
                        item.videoFrameRate = stream["AverageFrameRate"].get<float>();
                    }
                } else if (streamType == "Audio") {
                    JellyfinAudioStream audio;
                    audio.index = stream.value("Index", -1);
                    audio.codec = stream.value("Codec", std::string{});
                    audio.language = stream.value("Language", std::string{});
                    audio.title = stream.value("DisplayTitle", stream.value("Title", std::string{}));
                    audio.isDefault = stream.value("IsDefault", false);
                    if (audio.index >= 0) item.audios.push_back(std::move(audio));
                } else if (streamType == "Subtitle") {
                    JellyfinSubtitleStream subtitle;
                    subtitle.index = stream.value("Index", -1);
                    subtitle.codec = stream.value("Codec", std::string{});
                    subtitle.language = stream.value("Language", std::string{});
                    subtitle.title = stream.value("DisplayTitle", stream.value("Title", std::string{}));
                    subtitle.forced = stream.value("IsForced", false);
                    subtitle.isDefault = stream.value("IsDefault", false);
                    if (subtitle.index >= 0) item.subtitles.push_back(std::move(subtitle));
                }
            }
        }
    }
    return item;
}

ApiValueResult<std::vector<JellyfinItem>> JellyfinClient::parseItemList(const HttpResponse& response) const {
    ApiValueResult<std::vector<JellyfinItem>> result;
    if (!response.ok()) {
        result.error = apiError(response);
        return result;
    }
    try {
        const auto data = json::parse(response.body);
        const auto& items = data.contains("Items") ? data["Items"] : data;
        if (!items.is_array()) {
            result.error = "Jellyfin item response did not contain an Items array";
            return result;
        }
        result.value.reserve(items.size());
        for (const auto& value : items) {
            JellyfinItem item = parseItem(value);
            if (!item.id.empty()) result.value.push_back(std::move(item));
        }
        result.ok = true;
    } catch (const std::exception& e) {
        result.error = std::string("Unable to parse Jellyfin items: ") + e.what();
    }
    return result;
}

ApiValueResult<JellyfinServerInfo> JellyfinClient::getServerInfo(const JellyfinSession& session) const {
    ApiValueResult<JellyfinServerInfo> result;
    if (session.server.empty()) {
        result.error = "Server info request is incomplete";
        return result;
    }
    const auto response = http_.request(
        "GET",
        session.server + "/System/Info/Public",
        headers(nullptr, session.deviceId)
    );
    if (!response.ok()) {
        result.error = apiError(response);
        return result;
    }
    try {
        const auto data = json::parse(response.body);
        result.value.name = data.value("ServerName", std::string{});
        result.value.version = data.value("Version", std::string{});
        result.value.productName = data.value("ProductName", std::string{});
        result.value.operatingSystem = data.value("OperatingSystem", std::string{});
        result.ok = true;
    } catch (const std::exception& e) {
        result.error = std::string("Unable to parse server info: ") + e.what();
    }
    return result;
}

ApiValueResult<JellyfinHomeData> JellyfinClient::loadHomeCore(const JellyfinSession& session) const {
    ApiValueResult<JellyfinHomeData> result;
    if (!session.valid()) {
        result.error = "Not logged in";
        return result;
    }

    auto views = loadViews(session);
    if (!views.ok) {
        result.error = "Libraries: " + views.error;
        return result;
    }
    result.value.views = std::move(views.value);

    const std::string common =
        "&Fields=Overview,PrimaryImageAspectRatio,MediaSources"
        "&ImageTypeLimit=1&EnableImageTypes=Primary,Backdrop,Thumb"
        "&EnableTotalRecordCount=false";
    auto addWarning = [&](const std::string& warning) {
        if (!result.value.warning.empty()) result.value.warning += " | ";
        result.value.warning += warning;
    };

    auto resume = parseItemList(http_.request(
        "GET",
        session.server + "/Users/" + session.userId + "/Items/Resume?Limit=30&MediaTypes=Video&ExcludeItemTypes=AudioBook" + common,
        headers(&session, session.deviceId)
    ));
    if (resume.ok) {
        if (!resume.value.empty()) result.value.rows.push_back({"CONTINUE WATCHING", std::move(resume.value)});
    } else {
        addWarning("Continue Watching unavailable");
    }

    auto nextUp = parseItemList(http_.request(
        "GET",
        session.server + "/Shows/NextUp?UserId=" + session.userId + "&Limit=30&EnableResumable=false" + common,
        headers(&session, session.deviceId)
    ));
    if (nextUp.ok) {
        if (!nextUp.value.empty()) result.value.rows.push_back({"NEXT UP", std::move(nextUp.value)});
    } else {
        addWarning("Next Up unavailable");
    }

    result.ok = true;
    return result;
}

ApiValueResult<JellyfinHomeData> JellyfinClient::loadHomeSecondary(
    const JellyfinSession& session,
    const std::vector<JellyfinItem>& views
) const {
    ApiValueResult<JellyfinHomeData> result;
    if (!session.valid()) {
        result.error = "Not logged in";
        return result;
    }
    result.value.views = views;

    const std::string common =
        "&Fields=Overview,PrimaryImageAspectRatio,MediaSources"
        "&ImageTypeLimit=1&EnableImageTypes=Primary,Backdrop,Thumb"
        "&EnableTotalRecordCount=false";
    auto addWarning = [&](const std::string& warning) {
        if (!result.value.warning.empty()) result.value.warning += " | ";
        result.value.warning += warning;
    };

    for (const auto& view : views) {
        if (view.id.empty()) continue;
        const std::string url = session.server + "/Users/" + session.userId + "/Items/Latest"
            + "?ParentId=" + urlEncode(view.id)
            + "&Limit=24&GroupItems=true" + common;
        auto latest = parseItemList(http_.request("GET", url, headers(&session, session.deviceId)));
        if (latest.ok) retainScopedVideoItems(latest.value);
        if (!latest.ok) {
            addWarning("Recently added " + view.name + " unavailable");
            continue;
        }
        if (!latest.value.empty()) {
            result.value.rows.push_back({"RECENTLY ADDED IN " + view.name, std::move(latest.value)});
        }
    }

    auto recommended = parseItemList(http_.request(
        "GET",
        session.server + "/Users/" + session.userId + "/Items?Recursive=true&IncludeItemTypes=Movie,Series"
            "&Limit=30&SortBy=Random&EnableTotalRecordCount=false" + common,
        headers(&session, session.deviceId)
    ));
    if (recommended.ok) {
        if (!recommended.value.empty()) result.value.rows.push_back({"RECOMMENDED", std::move(recommended.value)});
    } else {
        addWarning("Recommendations unavailable");
    }

    auto favorites = parseItemList(http_.request(
        "GET",
        session.server + "/Users/" + session.userId + "/Items?Recursive=true&Filters=IsFavorite"
            "&IncludeItemTypes=Movie,Series,Episode&Limit=30&SortBy=SortName&SortOrder=Ascending" + common,
        headers(&session, session.deviceId)
    ));
    if (favorites.ok) {
        if (!favorites.value.empty()) result.value.rows.push_back({"FAVORITES", std::move(favorites.value)});
    } else {
        addWarning("Favorites unavailable");
    }

    result.ok = true;
    return result;
}

ApiValueResult<JellyfinHomeData> JellyfinClient::loadHome(const JellyfinSession& session) const {
    auto core = loadHomeCore(session);
    if (!core.ok) return core;

    auto secondary = loadHomeSecondary(session, core.value.views);
    if (!secondary.ok) {
        if (!core.value.warning.empty()) core.value.warning += " | ";
        core.value.warning += "Secondary Home rows unavailable";
        return core;
    }

    core.value.rows.insert(
        core.value.rows.end(),
        std::make_move_iterator(secondary.value.rows.begin()),
        std::make_move_iterator(secondary.value.rows.end())
    );
    if (!secondary.value.warning.empty()) {
        if (!core.value.warning.empty()) core.value.warning += " | ";
        core.value.warning += secondary.value.warning;
    }
    return core;
}

ApiValueResult<std::vector<JellyfinItem>> JellyfinClient::loadViews(const JellyfinSession& session) const {
    if (!session.valid()) {
        ApiValueResult<std::vector<JellyfinItem>> result;
        result.error = "Not logged in";
        return result;
    }
    auto result = parseItemList(http_.request(
        "GET",
        session.server + "/Users/" + session.userId + "/Views?IncludeExternalContent=false",
        headers(&session, session.deviceId)
    ));
    if (result.ok) {
        result.value.erase(std::remove_if(result.value.begin(), result.value.end(), [](const JellyfinItem& item) {
            return !isScopedVideoCollection(item);
        }), result.value.end());
    }
    return result;
}

ApiValueResult<std::vector<JellyfinItem>> JellyfinClient::browseLibrary(
    const JellyfinSession& session,
    const std::string& parentId,
    int startIndex,
    int limit
) const {
    if (!session.valid() || parentId.empty()) {
        ApiValueResult<std::vector<JellyfinItem>> result;
        result.error = "Library browse request is incomplete";
        return result;
    }
    const std::string url = session.server + "/Users/" + session.userId + "/Items"
        + "?ParentId=" + urlEncode(parentId)
        + "&Recursive=false&SortBy=SortName&SortOrder=Ascending"
        + "&IncludeItemTypes=Movie,Series,Episode,Season,Folder,BoxSet"
        + "&StartIndex=" + std::to_string(std::max(0, startIndex))
        + "&Limit=" + std::to_string(std::max(1, limit))
        + "&Fields=Overview,PrimaryImageAspectRatio,MediaSources,DateCreated,PremiereDate,ProductionYear"
          "&ImageTypeLimit=1&EnableImageTypes=Primary,Backdrop,Thumb"
          "&EnableTotalRecordCount=false";
    return parseItemList(http_.request("GET", url, headers(&session, session.deviceId)));
}

ApiValueResult<std::vector<JellyfinItem>> JellyfinClient::browseVideoFilter(
    const JellyfinSession& session,
    const JellyfinItem& library,
    int startIndex,
    int limit,
    bool favorites,
    const std::string& genre,
    const std::string& nameStartsWith
) const {
    if (!session.valid() || library.id.empty()) {
        ApiValueResult<std::vector<JellyfinItem>> result;
        result.error = "Filtered browse request is incomplete";
        return result;
    }
    std::string includeTypes = "Movie,Series";
    if (library.collectionType == "movies") includeTypes = "Movie";
    else if (library.collectionType == "tvshows") includeTypes = "Series";
    else if (library.collectionType == "boxsets") includeTypes = "BoxSet";

    std::string url = session.server + "/Users/" + session.userId + "/Items"
        + "?ParentId=" + urlEncode(library.id)
        + "&Recursive=true&SortBy=SortName&SortOrder=Ascending"
        + "&IncludeItemTypes=" + includeTypes
        + "&StartIndex=" + std::to_string(std::max(0, startIndex))
        + "&Limit=" + std::to_string(std::max(1, limit))
        + "&Fields=Overview,PrimaryImageAspectRatio,MediaSources,DateCreated,PremiereDate,ProductionYear"
          "&ImageTypeLimit=1&EnableImageTypes=Primary,Backdrop,Thumb"
          "&EnableTotalRecordCount=false";
    if (favorites) url += "&Filters=IsFavorite";
    if (!genre.empty()) url += "&Genres=" + urlEncode(genre);
    if (!nameStartsWith.empty()) url += "&NameStartsWith=" + urlEncode(nameStartsWith);
    return parseItemList(http_.request("GET", url, headers(&session, session.deviceId)));
}

ApiValueResult<std::vector<JellyfinItem>> JellyfinClient::listGenres(
    const JellyfinSession& session,
    const JellyfinItem& library,
    int limit
) const {
    if (!session.valid() || library.id.empty()) {
        ApiValueResult<std::vector<JellyfinItem>> result;
        result.error = "Genre request is incomplete";
        return result;
    }
    std::string includeTypes = "Movie,Series";
    if (library.collectionType == "movies") includeTypes = "Movie";
    else if (library.collectionType == "tvshows") includeTypes = "Series";
    const std::string url = session.server + "/Genres"
        + "?UserId=" + urlEncode(session.userId)
        + "&ParentId=" + urlEncode(library.id)
        + "&Recursive=true&IncludeItemTypes=" + includeTypes
        + "&SortBy=SortName&SortOrder=Ascending"
        + "&Limit=" + std::to_string(std::max(1, limit))
        + "&EnableTotalRecordCount=false";
    return parseItemList(http_.request("GET", url, headers(&session, session.deviceId)));
}

ApiValueResult<std::vector<JellyfinItem>> JellyfinClient::browseCollections(
    const JellyfinSession& session,
    int startIndex,
    int limit
) const {
    if (!session.valid()) {
        ApiValueResult<std::vector<JellyfinItem>> result;
        result.error = "Collections request requires login";
        return result;
    }
    const std::string url = session.server + "/Users/" + session.userId + "/Items"
        + "?Recursive=true&IncludeItemTypes=BoxSet&SortBy=SortName&SortOrder=Ascending"
        + "&StartIndex=" + std::to_string(std::max(0, startIndex))
        + "&Limit=" + std::to_string(std::max(1, limit))
        + "&Fields=Overview,PrimaryImageAspectRatio,ProductionYear,ProviderIds"
          "&ImageTypeLimit=1&EnableImageTypes=Primary,Backdrop,Thumb"
          "&EnableTotalRecordCount=false";
    return parseItemList(http_.request("GET", url, headers(&session, session.deviceId)));
}

ApiValueResult<std::vector<JellyfinItem>> JellyfinClient::browseCollectionMembersFallback(
    const JellyfinSession& session,
    const JellyfinItem& collection
) const {
    ApiValueResult<std::vector<JellyfinItem>> result;
    if (!session.valid() || collection.type != "BoxSet" || collection.tmdbId.empty()) {
        result.ok = true;
        return result;
    }

    const std::string url = session.server + "/Users/" + session.userId + "/Items"
        + "?Recursive=true&IncludeItemTypes=Movie&SortBy=SortName&SortOrder=Ascending"
          "&Fields=Overview,PrimaryImageAspectRatio,MediaSources,ProductionYear,ProviderIds"
          "&ImageTypeLimit=1&EnableImageTypes=Primary,Backdrop,Thumb"
          "&EnableTotalRecordCount=false&Limit=5000";
    auto allMovies = parseItemList(http_.request("GET", url, headers(&session, session.deviceId)));
    if (!allMovies.ok) return allMovies;

    result.value.reserve(allMovies.value.size());
    for (auto& movie : allMovies.value) {
        if (movie.tmdbCollectionId == collection.tmdbId) result.value.push_back(std::move(movie));
    }
    result.ok = true;
    return result;
}

ApiValueResult<std::vector<JellyfinItem>> JellyfinClient::search(const JellyfinSession& session, const std::string& query) const {
    if (query.empty()) {
        ApiValueResult<std::vector<JellyfinItem>> result;
        result.ok = true;
        return result;
    }
    const std::string url = session.server + "/Items?UserId=" + session.userId
        + "&Recursive=true&SearchTerm=" + urlEncode(query)
        + "&IncludeItemTypes=Movie,Series,Episode&Limit=60"
          "&Fields=Overview,PrimaryImageAspectRatio,MediaSources"
          "&ImageTypeLimit=1&EnableImageTypes=Primary,Backdrop,Thumb"
          "&EnableTotalRecordCount=false";
    return parseItemList(http_.request("GET", url, headers(&session, session.deviceId)));
}

ApiValueResult<JellyfinItem> JellyfinClient::getItem(const JellyfinSession& session, const std::string& itemId) const {
    ApiValueResult<JellyfinItem> result;
    const auto response = http_.request(
        "GET",
        session.server + "/Users/" + session.userId + "/Items/" + itemId
            + "?Fields=Chapters,MediaSources,MediaStreams,Overview,Genres,People,ProductionYear,CommunityRating,OfficialRating,CanDelete",
        headers(&session, session.deviceId)
    );
    if (!response.ok()) {
        result.error = apiError(response);
        return result;
    }
    try {
        result.value = parseItem(json::parse(response.body));
        result.ok = !result.value.id.empty();
        if (!result.ok) result.error = "Jellyfin returned an empty item";
    } catch (const std::exception& e) {
        result.error = std::string("Unable to parse item: ") + e.what();
    }
    return result;
}

ApiValueResult<std::vector<JellyfinItem>> JellyfinClient::getSimilar(
    const JellyfinSession& session,
    const std::string& itemId,
    int limit
) const {
    if (!session.valid() || itemId.empty()) {
        ApiValueResult<std::vector<JellyfinItem>> result;
        result.error = "Similar-items request is incomplete";
        return result;
    }
    const std::string url = session.server + "/Items/" + urlEncode(itemId) + "/Similar"
        + "?UserId=" + urlEncode(session.userId)
        + "&Limit=" + std::to_string(std::clamp(limit, 1, 60))
        + "&Fields=Overview,PrimaryImageAspectRatio,MediaSources,ProductionYear"
          "&ImageTypeLimit=1&EnableImageTypes=Primary,Backdrop,Thumb"
          "&EnableTotalRecordCount=false";
    auto result = parseItemList(http_.request("GET", url, headers(&session, session.deviceId)));
    if (result.ok) retainScopedVideoItems(result.value);
    return result;
}

ApiValueResult<std::vector<JellyfinItem>> JellyfinClient::getItemsForPerson(
    const JellyfinSession& session,
    const std::string& personId,
    int limit
) const {
    if (!session.valid() || personId.empty()) {
        ApiValueResult<std::vector<JellyfinItem>> result;
        result.error = "Person-items request is incomplete";
        return result;
    }
    const std::string url = session.server + "/Users/" + session.userId + "/Items"
        + "?Recursive=true&PersonIds=" + urlEncode(personId)
        + "&IncludeItemTypes=Movie,Series,Episode"
        + "&SortBy=ProductionYear,SortName&SortOrder=Descending"
        + "&Limit=" + std::to_string(std::clamp(limit, 1, 100))
        + "&Fields=Overview,PrimaryImageAspectRatio,MediaSources,ProductionYear"
          "&ImageTypeLimit=1&EnableImageTypes=Primary,Backdrop,Thumb"
          "&EnableTotalRecordCount=false";
    auto result = parseItemList(http_.request("GET", url, headers(&session, session.deviceId)));
    if (result.ok) retainScopedVideoItems(result.value);
    return result;
}

ApiValueResult<std::vector<JellyfinItem>> JellyfinClient::getSeasons(
    const JellyfinSession& session,
    const std::string& seriesId
) const {
    if (!session.valid() || seriesId.empty()) {
        ApiValueResult<std::vector<JellyfinItem>> result;
        result.error = "Season request is incomplete";
        return result;
    }
    const std::string url = session.server + "/Shows/" + urlEncode(seriesId) + "/Seasons"
        + "?UserId=" + urlEncode(session.userId)
        + "&Fields=Overview,PrimaryImageAspectRatio,MediaSources"
          "&ImageTypeLimit=1&EnableImageTypes=Primary,Backdrop,Thumb"
          "&EnableTotalRecordCount=false";
    return parseItemList(http_.request("GET", url, headers(&session, session.deviceId)));
}

ApiValueResult<std::vector<JellyfinItem>> JellyfinClient::getEpisodes(
    const JellyfinSession& session,
    const std::string& seriesId,
    const std::string& seasonId
) const {
    if (!session.valid() || seriesId.empty() || seasonId.empty()) {
        ApiValueResult<std::vector<JellyfinItem>> result;
        result.error = "Episode request is incomplete";
        return result;
    }
    const std::string url = session.server + "/Shows/" + urlEncode(seriesId) + "/Episodes"
        + "?UserId=" + urlEncode(session.userId)
        + "&SeasonId=" + urlEncode(seasonId)
        + "&Fields=Overview,PrimaryImageAspectRatio,MediaSources"
          "&ImageTypeLimit=1&EnableImageTypes=Primary,Backdrop,Thumb"
          "&EnableTotalRecordCount=false";
    return parseItemList(http_.request("GET", url, headers(&session, session.deviceId)));
}

ApiValueResult<std::vector<JellyfinMediaSegment>> JellyfinClient::getMediaSegments(
    const JellyfinSession& session,
    const std::string& itemId
) const {
    ApiValueResult<std::vector<JellyfinMediaSegment>> result;
    if (!session.valid() || itemId.empty()) {
        result.error = "Media-segment request is incomplete";
        return result;
    }
    const auto response = http_.request(
        "GET",
        session.server + "/MediaSegments/" + urlEncode(itemId),
        headers(&session, session.deviceId)
    );
    if (!response.ok()) {
        result.error = apiError(response);
        return result;
    }
    try {
        const auto data = json::parse(response.body);
        if (!data.contains("Items") || !data["Items"].is_array()) {
            result.error = "Jellyfin media-segment response did not contain Items";
            return result;
        }
        for (const auto& value : data["Items"]) {
            if (!value.is_object()) continue;
            JellyfinMediaSegment segment;
            segment.type = value.value("Type", std::string{});
            segment.startTicks = value.value("StartTicks", static_cast<int64_t>(0));
            segment.endTicks = value.value("EndTicks", static_cast<int64_t>(0));
            if (!segment.type.empty() && segment.endTicks > segment.startTicks) {
                result.value.push_back(std::move(segment));
            }
        }
        std::sort(result.value.begin(), result.value.end(), [](const auto& left, const auto& right) {
            return left.startTicks < right.startTicks;
        });
        result.ok = true;
    } catch (const std::exception& e) {
        result.error = std::string("Unable to parse media segments: ") + e.what();
    }
    return result;
}

ApiValueResult<JellyfinItem> JellyfinClient::getNextUpForSeries(const JellyfinSession& session, const std::string& seriesId) const {
    ApiValueResult<JellyfinItem> result;
    const std::string url = session.server + "/Shows/NextUp?UserId=" + session.userId
        + "&SeriesId=" + urlEncode(seriesId)
        + "&Limit=1&EnableResumable=true"
          "&Fields=Overview,PrimaryImageAspectRatio,MediaSources"
          "&ImageTypeLimit=1&EnableImageTypes=Primary,Backdrop,Thumb";
    auto items = parseItemList(http_.request("GET", url, headers(&session, session.deviceId)));
    if (!items.ok) {
        result.error = items.error;
        return result;
    }
    if (items.value.empty()) {
        result.error = "No next episode was returned for this series";
        return result;
    }
    result.value = std::move(items.value.front());
    result.ok = true;
    return result;
}

ApiValueResult<JellyfinItem> JellyfinClient::getFollowingEpisodeForSeries(
    const JellyfinSession& session,
    const std::string& seriesId,
    const std::string& currentItemId
) const {
    ApiValueResult<JellyfinItem> result;
    if (!session.valid() || seriesId.empty()) {
        result.error = "Following-episode request is incomplete";
        return result;
    }
    const std::string url = session.server + "/Shows/" + urlEncode(seriesId) + "/Episodes"
        + "?UserId=" + urlEncode(session.userId)
        + "&AdjacentTo=" + urlEncode(currentItemId)
        + "&Fields=Overview,PrimaryImageAspectRatio,MediaSources"
          "&ImageTypeLimit=1&EnableImageTypes=Primary,Backdrop,Thumb"
          "&EnableUserData=true&EnableTotalRecordCount=false";
    auto items = parseItemList(http_.request("GET", url, headers(&session, session.deviceId)));
    if (!items.ok) {
        result.error = items.error;
        return result;
    }
    const auto current = std::find_if(items.value.begin(), items.value.end(), [&](const JellyfinItem& item) {
        return item.id == currentItemId;
    });
    if (current == items.value.end() || std::next(current) == items.value.end()) {
        result.error = "No following episode was returned for this series";
        return result;
    }
    result.value = std::move(*std::next(current));
    result.ok = true;
    return result;
}

ApiResult JellyfinClient::setFavorite(
    const JellyfinSession& session,
    const JellyfinItem& item,
    bool favorite
) const {
    ApiResult result;
    if (!session.valid() || item.id.empty()) {
        result.error = "Favorite request is incomplete";
        return result;
    }
    const std::string url = session.server + "/UserFavoriteItems/" + urlEncode(item.id)
        + "?UserId=" + urlEncode(session.userId);
    const auto response = http_.request(
        favorite ? "POST" : "DELETE",
        url,
        headers(&session, session.deviceId)
    );
    result.ok = response.ok();
    if (!result.ok) result.error = apiError(response);
    return result;
}

ApiResult JellyfinClient::setPlayed(
    const JellyfinSession& session,
    const JellyfinItem& item,
    bool played
) const {
    ApiResult result;
    if (!session.valid() || item.id.empty()) {
        result.error = "Played-state request is incomplete";
        return result;
    }
    const std::string url = session.server + "/UserPlayedItems/" + urlEncode(item.id)
        + "?UserId=" + urlEncode(session.userId);
    const auto response = http_.request(
        played ? "POST" : "DELETE",
        url,
        headers(&session, session.deviceId)
    );
    result.ok = response.ok();
    if (!result.ok) result.error = apiError(response);
    return result;
}

ApiResult JellyfinClient::refreshMetadata(
    const JellyfinSession& session,
    const JellyfinItem& item
) const {
    ApiResult result;
    if (!session.valid() || item.id.empty()) {
        result.error = "Metadata refresh request is incomplete";
        return result;
    }
    const std::string url = session.server + "/Items/" + urlEncode(item.id) + "/Refresh"
        + "?metadataRefreshMode=Default&imageRefreshMode=Default"
          "&replaceAllMetadata=false&replaceAllImages=false&regenerateTrickplay=false";
    const auto response = http_.request("POST", url, headers(&session, session.deviceId));
    result.ok = response.ok();
    if (!result.ok) result.error = apiError(response);
    return result;
}

ApiResult JellyfinClient::deleteItem(
    const JellyfinSession& session,
    const JellyfinItem& item
) const {
    ApiResult result;
    if (!session.valid() || item.id.empty()) {
        result.error = "Delete request is incomplete";
        return result;
    }
    const auto response = http_.request(
        "DELETE",
        session.server + "/Items/" + urlEncode(item.id),
        headers(&session, session.deviceId)
    );
    result.ok = response.ok();
    if (!result.ok) result.error = apiError(response);
    return result;
}

ApiValueResult<PlaybackTarget> JellyfinClient::resolvePlayback(
    const JellyfinSession& session,
    const JellyfinItem& item,
    int maxStreamingBitrate,
    int maxAudioChannels,
    PlaybackOverrides overrides,
    int audioStreamIndex,
    int subtitleStreamIndex
) const {
    ApiValueResult<PlaybackTarget> result;
    maxAudioChannels = std::clamp(maxAudioChannels, 2, 8);

    auto videoCodecs = codecSupport_.jellyfinVideoCodecs();
    auto audioCodecs = codecSupport_.jellyfinAudioCodecs();

    auto lower = [](std::string value) {
        std::transform(value.begin(), value.end(), value.begin(), [](unsigned char c) {
            return static_cast<char>(std::tolower(c));
        });
        return value;
    };
    bool directVideoSupported = true;
    auto removeVideoCodec = [&](const std::string& codec, const char* reason) {
        const auto before = videoCodecs.size();
        videoCodecs.erase(std::remove(videoCodecs.begin(), videoCodecs.end(), codec), videoCodecs.end());
        if (videoCodecs.size() != before) {
            directVideoSupported = false;
            __android_log_print(ANDROID_LOG_INFO, kTag, "Not advertising direct %s for this item: %s", codec.c_str(), reason);
        }
    };

    const std::string itemCodec = lower(item.videoCodec);
    const std::string itemProfile = lower(item.videoProfile);
    const std::string range = lower(item.videoRangeType);
    const bool isDolbyVision = range.find("dovi") != std::string::npos;
    const bool isHdr10Plus = range.find("hdr10plus") != std::string::npos || range.find("hdr10+") != std::string::npos;
    const bool isHdr10 = !isHdr10Plus && range.find("hdr10") != std::string::npos;
    const bool isHlg = range.find("hlg") != std::string::npos;
    const bool unsupportedHdr = (isDolbyVision && !hdrCapabilityAllowed(codecSupport_.displayDolbyVision, overrides.hdrMode))
        || (!isDolbyVision && isHdr10Plus && !hdrCapabilityAllowed(codecSupport_.displayHdr10Plus, overrides.hdrMode))
        || (!isDolbyVision && isHdr10 && !hdrCapabilityAllowed(codecSupport_.displayHdr10, overrides.hdrMode))
        || (!isDolbyVision && isHlg && !hdrCapabilityAllowed(codecSupport_.displayHlg, overrides.hdrMode));
    __android_log_print(
        ANDROID_LOG_INFO,
        kTag,
        "Playback capability check: codec=%s profile=%s level=%d range=%s overrides(avc=%d hevc=%d hdr=%d)",
        itemCodec.c_str(), item.videoProfile.c_str(), item.videoLevel, item.videoRangeType.c_str(),
        overrides.maxAvcLevel, overrides.maxHevcLevel, static_cast<int>(overrides.hdrMode)
    );

    if (itemCodec == "hevc") {
        if (!codecLevelAllowed(item.videoLevel, overrides.maxHevcLevel)) {
            removeVideoCodec("hevc", "level exceeds user override");
        }
        if ((item.videoBitDepth > 8 || itemProfile.find("main 10") != std::string::npos) && !codecSupport_.hevcMain10) {
            removeVideoCodec("hevc", "HEVC Main10 unsupported");
        }
        if (codecSupport_.maxHevcWidth > 0 && (item.videoWidth > codecSupport_.maxHevcWidth || item.videoHeight > codecSupport_.maxHevcHeight)) {
            removeVideoCodec("hevc", "resolution exceeds decoder capability");
        }
        if (unsupportedHdr) removeVideoCodec("hevc", "HDR range unsupported by connected display");
    } else if (itemCodec == "h264") {
        if (!codecLevelAllowed(item.videoLevel, overrides.maxAvcLevel)) {
            removeVideoCodec("h264", "level exceeds user override");
        }
        if (itemProfile.find("high 10") != std::string::npos && !codecSupport_.h264High10) {
            removeVideoCodec("h264", "H.264 High10 unsupported");
        }
        if (codecSupport_.maxH264Width > 0 && (item.videoWidth > codecSupport_.maxH264Width || item.videoHeight > codecSupport_.maxH264Height)) {
            removeVideoCodec("h264", "resolution exceeds decoder capability");
        }
        if (unsupportedHdr) removeVideoCodec("h264", "HDR range unsupported by connected display");
    } else if (itemCodec == "av1") {
        if (item.videoBitDepth > 8 && !codecSupport_.av1Main10) removeVideoCodec("av1", "AV1 Main10 unsupported");
        if (codecSupport_.maxAv1Width > 0 && (item.videoWidth > codecSupport_.maxAv1Width || item.videoHeight > codecSupport_.maxAv1Height)) {
            removeVideoCodec("av1", "resolution exceeds decoder capability");
        }
        if (unsupportedHdr) removeVideoCodec("av1", "HDR range unsupported by connected display");
    } else if ((itemCodec == "vp9" || itemCodec == "vp8") && unsupportedHdr) {
        removeVideoCodec(itemCodec, "HDR range unsupported by connected display");
    }

    if (videoCodecs.empty()) videoCodecs.emplace_back("h264");
    if (audioCodecs.empty()) audioCodecs.emplace_back("aac");
    const std::string videoCodecList = joinCodecs(videoCodecs);
    const std::string audioCodecList = joinCodecs(audioCodecs);

    json profile = {
        {"Name", "sloppaTV-Native"},
        {"MaxStaticBitrate", 120000000},
        {"MaxStreamingBitrate", std::max(1000000, maxStreamingBitrate)},
        {"MusicStreamingTranscodingBitrate", 192000},
        {"DirectPlayProfiles", json::array({
            {
                {"Container", "mkv,matroska,mp4,m4v,mov,ts,mpegts,webm"},
                {"Type", "Video"},
                {"VideoCodec", videoCodecList},
                {"AudioCodec", audioCodecList}
            }
        })},
        {"TranscodingProfiles", json::array({
            {
                {"Container", "ts"},
                {"Type", "Video"},
                {"VideoCodec", "h264"},
                {"AudioCodec", "aac,ac3,eac3,mp3"},
                {"Protocol", "hls"},
                {"Context", "Streaming"},
                {"CopyTimestamps", false},
                {"EnableSubtitlesInManifest", true},
                {"MaxAudioChannels", std::to_string(maxAudioChannels)}
            }
        })},
        {"CodecProfiles", json::array({
            {
                {"Type", "VideoAudio"},
                {"Conditions", json::array({
                    {
                        {"Condition", "LessThanEqual"},
                        {"Property", "AudioChannels"},
                        {"Value", std::to_string(maxAudioChannels)},
                        {"IsRequired", false}
                    }
                })}
            }
        })},
        {"SubtitleProfiles", json::array({
            {{"Format", "vtt"}, {"Method", "Hls"}},
            {{"Format", "webvtt"}, {"Method", "Hls"}},
            {{"Format", "srt"}, {"Method", "Encode"}},
            {{"Format", "subrip"}, {"Method", "Encode"}},
            {{"Format", "ass"}, {"Method", "Encode"}},
            {{"Format", "ssa"}, {"Method", "Encode"}},
            {{"Format", "pgs"}, {"Method", "Encode"}}
        })}
    };

    const bool serverSubtitle = subtitleStreamIndex >= 0;
    const bool preferServerStream = preferServerStreamForStartup(item.container, item.positionTicks) || serverSubtitle;
    json body = {
        {"UserId", session.userId},
        {"StartTimeTicks", item.positionTicks},
        {"MediaSourceId", item.mediaSourceId.empty() ? json(nullptr) : json(item.mediaSourceId)},
        {"DeviceProfile", profile},
        // Until sloppaTV has client-side ASS/SSA rendering, do not let a server-default
        // embedded subtitle force an otherwise direct-playable video into a full transcode.
        {"AudioStreamIndex", audioStreamIndex >= 0 ? json(audioStreamIndex) : json(nullptr)},
        {"SubtitleStreamIndex", subtitleStreamIndex},
        {"MaxAudioChannels", maxAudioChannels},
        {"EnableDirectPlay", directVideoSupported && !preferServerStream},
        {"EnableDirectStream", directVideoSupported && !preferServerStream},
        {"EnableTranscoding", true},
        {"AllowVideoStreamCopy", directVideoSupported && !serverSubtitle},
        {"AllowAudioStreamCopy", true},
        {"AutoOpenLiveStream", true}
    };

    std::string url = session.server + "/Items/" + item.id + "/PlaybackInfo?UserId=" + session.userId
        + "&StartTimeTicks=" + std::to_string(item.positionTicks)
        + "&AudioStreamIndex=" + std::to_string(audioStreamIndex)
        + "&SubtitleStreamIndex=" + std::to_string(subtitleStreamIndex)
        + "&IsPlayback=true&AutoOpenLiveStream=true&MaxStreamingBitrate=" + std::to_string(std::max(1000000, maxStreamingBitrate));

    const auto response = http_.request("POST", url, headers(&session, session.deviceId), body.dump());
    if (!response.ok()) {
        result.error = apiError(response);
        return result;
    }

    try {
        const auto data = json::parse(response.body);
        if (!data.contains("MediaSources") || !data["MediaSources"].is_array() || data["MediaSources"].empty()) {
            result.error = "Jellyfin returned no playable media source";
            return result;
        }
        const auto& source = data["MediaSources"][0];
        PlaybackTarget target;
        target.playSessionId = data.value("PlaySessionId", std::string{});
        target.mediaSourceId = source.value("Id", std::string{});
        target.startTicks = item.positionTicks;

        const bool direct = source.value("SupportsDirectPlay", false);
        const bool directStream = source.value("SupportsDirectStream", false);
        const bool transcode = source.value("SupportsTranscoding", false);
        const std::string transcodingUrl = source.value("TranscodingUrl", std::string{});
        const std::string container = firstContainer(source.value("Container", item.container));
        if (!transcodingUrl.empty()) {
            target.fallbackTranscodeUrl = transcodingUrl.rfind("http://", 0) == 0 || transcodingUrl.rfind("https://", 0) == 0
                ? transcodingUrl
                : session.server + transcodingUrl;
            target.fallbackTranscodeUrl = addApiKey(std::move(target.fallbackTranscodeUrl), urlEncode(session.token));
        }

        if (direct) {
            target.url = session.server + "/Videos/" + item.id + "/stream." + container
                + "?Static=true&MediaSourceId=" + urlEncode(target.mediaSourceId)
                + "&api_key=" + urlEncode(session.token);
            target.playMethod = PlaybackMethod::DirectPlay;
        } else if ((directStream || transcode) && !target.fallbackTranscodeUrl.empty()) {
            target.url = target.fallbackTranscodeUrl;
            target.fallbackTranscodeUrl.clear();
            target.transcoding = true;
            target.playMethod = directStream ? PlaybackMethod::DirectStream : PlaybackMethod::Transcode;
        } else {
            result.error = "No direct-play, direct-stream or transcode path was offered by Jellyfin";
            return result;
        }

        __android_log_print(ANDROID_LOG_INFO, kTag, "Playback target: %s", playbackMethodName(target.playMethod));
        result.value = std::move(target);
        result.ok = true;
    } catch (const std::exception& e) {
        result.error = std::string("Unable to parse playback info: ") + e.what();
    }
    return result;
}

ApiResult JellyfinClient::reportPlaybackStart(
    const JellyfinSession& session,
    const JellyfinItem& item,
    const PlaybackTarget& target,
    int64_t positionTicks
) const {
    ApiResult result;
    json body = {
        {"ItemId", item.id},
        {"MediaSourceId", target.mediaSourceId},
        {"PlaySessionId", target.playSessionId},
        {"PositionTicks", positionTicks},
        {"CanSeek", true},
        {"IsPaused", false},
        {"IsMuted", false},
        {"VolumeLevel", 100},
        {"PlayMethod", playbackMethodName(target.playMethod)}
    };
    const auto response = http_.request("POST", session.server + "/Sessions/Playing", headers(&session, session.deviceId), body.dump());
    result.ok = response.ok();
    if (!result.ok) result.error = apiError(response);
    return result;
}

ApiResult JellyfinClient::reportPlaybackProgress(
    const JellyfinSession& session,
    const JellyfinItem& item,
    const PlaybackTarget& target,
    int64_t positionTicks,
    bool paused
) const {
    ApiResult result;
    json body = {
        {"ItemId", item.id},
        {"MediaSourceId", target.mediaSourceId},
        {"PlaySessionId", target.playSessionId},
        {"PositionTicks", positionTicks},
        {"CanSeek", true},
        {"IsPaused", paused},
        {"IsMuted", false},
        {"VolumeLevel", 100},
        {"PlayMethod", playbackMethodName(target.playMethod)}
    };
    const auto response = http_.request("POST", session.server + "/Sessions/Playing/Progress", headers(&session, session.deviceId), body.dump());
    result.ok = response.ok();
    if (!result.ok) result.error = apiError(response);
    return result;
}

ApiResult JellyfinClient::reportPlaybackStopped(
    const JellyfinSession& session,
    const JellyfinItem& item,
    const PlaybackTarget& target,
    int64_t positionTicks
) const {
    ApiResult result;
    json body = {
        {"ItemId", item.id},
        {"MediaSourceId", target.mediaSourceId},
        {"PlaySessionId", target.playSessionId},
        {"PositionTicks", positionTicks},
        {"Failed", false}
    };
    const auto response = http_.request("POST", session.server + "/Sessions/Playing/Stopped", headers(&session, session.deviceId), body.dump());
    result.ok = response.ok();
    if (!result.ok) result.error = apiError(response);
    return result;
}

std::string JellyfinClient::imageUrl(
    const JellyfinSession& session,
    const JellyfinItem& item,
    int width,
    int height
) const {
    if (item.id.empty()) return {};
    std::string url = session.server + "/Items/" + item.id + "/Images/Primary?maxWidth=" + std::to_string(width)
        + "&maxHeight=" + std::to_string(height) + "&quality=85";
    if (!item.imageTag.empty()) url += "&tag=" + urlEncode(item.imageTag);
    url += "&api_key=" + urlEncode(session.token);
    return url;
}

ApiValueResult<std::string> JellyfinClient::downloadPrimaryImage(
    const JellyfinSession& session,
    const JellyfinItem& item,
    int width,
    int height
) const {
    ApiValueResult<std::string> result;
    if (!session.valid() || item.id.empty()) {
        result.error = "Image request is incomplete";
        return result;
    }
    const std::string url = imageUrl(session, item, width, height);
    const auto response = http_.request("GET", url, headers(&session, session.deviceId));
    if (!response.ok()) {
        result.error = apiError(response);
        return result;
    }
    if (response.body.empty()) {
        result.error = "Jellyfin returned an empty image";
        return result;
    }
    result.value = response.body;
    result.ok = true;
    return result;
}

ApiValueResult<std::string> JellyfinClient::downloadBackdropImage(
    const JellyfinSession& session,
    const JellyfinItem& item,
    int width,
    int height
) const {
    ApiValueResult<std::string> result;
    if (!session.valid() || item.id.empty() || item.backdropTag.empty()) {
        result.error = "Backdrop request is incomplete";
        return result;
    }
    std::string url = session.server + "/Items/" + item.id + "/Images/Backdrop/0?maxWidth=" + std::to_string(width)
        + "&maxHeight=" + std::to_string(height) + "&quality=82"
        + "&tag=" + urlEncode(item.backdropTag)
        + "&api_key=" + urlEncode(session.token);
    const auto response = http_.request("GET", url, headers(&session, session.deviceId));
    if (!response.ok()) {
        result.error = apiError(response);
        return result;
    }
    if (response.body.empty()) {
        result.error = "Jellyfin returned an empty backdrop";
        return result;
    }
    result.value = response.body;
    result.ok = true;
    return result;
}

ApiValueResult<std::string> JellyfinClient::downloadHomeImage(
    const JellyfinSession& session,
    const JellyfinItem& item,
    int width,
    int height
) const {
    ApiValueResult<std::string> result;
    if (!session.valid() || item.id.empty()) {
        result.error = "Home image request is incomplete";
        return result;
    }

    std::string url;
    const ArtworkReference artwork = homeArtworkReference(
        item.id,
        item.imageTag,
        item.seriesId,
        item.seriesPrimaryImageTag,
        item.type == "Episode",
        item.thumbTag,
        item.backdropTag
    );
    const ArtworkKind kind = artwork.kind;
    if (kind == ArtworkKind::Primary) {
        url = session.server + "/Items/" + artwork.itemId + "/Images/Primary?maxWidth=" + std::to_string(width)
            + "&maxHeight=" + std::to_string(height) + "&quality=84&tag=" + urlEncode(artwork.tag);
    } else if (kind == ArtworkKind::Thumb) {
        url = session.server + "/Items/" + artwork.itemId + "/Images/Thumb?maxWidth=" + std::to_string(width)
            + "&maxHeight=" + std::to_string(height) + "&quality=84&tag=" + urlEncode(artwork.tag);
    } else if (kind == ArtworkKind::Backdrop) {
        url = session.server + "/Items/" + artwork.itemId + "/Images/Backdrop/0?maxWidth=" + std::to_string(width)
            + "&maxHeight=" + std::to_string(height) + "&quality=84&tag=" + urlEncode(artwork.tag);
    } else {
        result.error = "Item has no artwork";
        return result;
    }
    url += "&api_key=" + urlEncode(session.token);

    const auto response = http_.request("GET", url, headers(&session, session.deviceId));
    if (!response.ok()) {
        result.error = apiError(response);
        return result;
    }
    if (response.body.empty()) {
        result.error = "Jellyfin returned an empty home image";
        return result;
    }
    result.value = response.body;
    result.ok = true;
    return result;
}

ApiValueResult<std::string> JellyfinClient::downloadSubtitleSrt(
    const JellyfinSession& session,
    const JellyfinItem& item,
    int subtitleIndex
) const {
    ApiValueResult<std::string> result;
    if (!session.valid() || item.id.empty() || item.mediaSourceId.empty() || subtitleIndex < 0) {
        result.error = "Subtitle request is incomplete";
        return result;
    }
    const std::string url = session.server + "/Videos/" + urlEncode(item.id)
        + "/" + urlEncode(item.mediaSourceId)
        + "/Subtitles/" + std::to_string(subtitleIndex)
        + "/Stream.srt?api_key=" + urlEncode(session.token);
    const auto response = http_.request("GET", url, headers(&session, session.deviceId));
    if (!response.ok()) {
        result.error = apiError(response);
        return result;
    }
    if (response.body.empty()) {
        result.error = "Jellyfin returned an empty subtitle";
        return result;
    }
    result.value = response.body;
    result.ok = true;
    return result;
}
