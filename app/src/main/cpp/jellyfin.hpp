#pragma once

#include "device_capabilities.hpp"
#include "jni_http.hpp"

#include <jni.h>
#include <nlohmann/json.hpp>

#include <cstdint>
#include <string>
#include <vector>

struct JellyfinSession {
    std::string server;
    std::string username;
    std::string userId;
    std::string token;
    std::string deviceId;

    [[nodiscard]] bool valid() const {
        return !server.empty() && !userId.empty() && !token.empty();
    }
};

struct JellyfinChapter {
    std::string name;
    int64_t startTicks = 0;
};

struct JellyfinMediaSegment {
    std::string type;
    int64_t startTicks = 0;
    int64_t endTicks = 0;
};

struct JellyfinSubtitleStream {
    int index = -1;
    std::string codec;
    std::string language;
    std::string title;
    bool forced = false;
    bool isDefault = false;
};

struct JellyfinAudioStream {
    int index = -1;
    std::string codec;
    std::string language;
    std::string title;
    bool isDefault = false;
};

struct JellyfinItem {
    std::string id;
    std::string name;
    std::string type;
    std::string collectionType;
    std::string seriesId;
    std::string seriesName;
    std::string seriesPrimaryImageTag;
    std::string seasonName;
    std::string overview;
    std::string container;
    std::string mediaSourceId;
    std::string tmdbId;
    std::string tmdbCollectionId;
    std::string videoCodec;
    std::string videoProfile;
    std::string videoRangeType;
    std::string imageTag;
    std::string thumbTag;
    std::string backdropTag;
    std::string officialRating;
    std::vector<std::string> genres;
    std::vector<std::string> cast;
    std::vector<JellyfinChapter> chapters;
    std::vector<JellyfinAudioStream> audios;
    std::vector<JellyfinSubtitleStream> subtitles;
    int productionYear = 0;
    float communityRating = -1.0f;
    int videoWidth = 0;
    int videoHeight = 0;
    int videoBitDepth = 0;
    float videoFrameRate = 0.0f;
    int indexNumber = -1;
    int parentIndexNumber = -1;
    int64_t positionTicks = 0;
    int64_t runtimeTicks = 0;
    bool favorite = false;
    bool played = false;
};

struct JellyfinHomeRow {
    std::string title;
    std::vector<JellyfinItem> items;
};

struct JellyfinHomeData {
    std::vector<JellyfinHomeRow> rows;
    std::vector<JellyfinItem> views;
    std::string warning;
};

struct JellyfinServerInfo {
    std::string name;
    std::string version;
    std::string productName;
    std::string operatingSystem;
};

struct QuickConnectRequest {
    std::string server;
    std::string secret;
    std::string code;
};

struct PlaybackTarget {
    std::string url;
    std::string fallbackTranscodeUrl;
    std::string playSessionId;
    std::string mediaSourceId;
    bool transcoding = false;
    int64_t startTicks = 0;
};

struct ApiResult {
    bool ok = false;
    std::string error;
};

template <typename T>
struct ApiValueResult : ApiResult {
    T value{};
};

class JellyfinClient {
public:
    JellyfinClient(JavaVM* vm, jobject activity) : http_(vm), codecSupport_(queryDeviceCodecSupport(vm, activity)) {}

    ApiValueResult<JellyfinSession> login(
        std::string server,
        const std::string& username,
        const std::string& password,
        const std::string& deviceId
    ) const;
    ApiValueResult<QuickConnectRequest> initiateQuickConnect(std::string server, const std::string& deviceId) const;
    ApiValueResult<bool> pollQuickConnect(const QuickConnectRequest& request, const std::string& deviceId) const;
    ApiValueResult<JellyfinSession> completeQuickConnect(const QuickConnectRequest& request, const std::string& deviceId) const;

    ApiValueResult<JellyfinServerInfo> getServerInfo(const JellyfinSession& session) const;
    ApiValueResult<JellyfinHomeData> loadHome(const JellyfinSession& session) const;
    ApiValueResult<JellyfinHomeData> loadHomeCore(const JellyfinSession& session) const;
    ApiValueResult<JellyfinHomeData> loadHomeSecondary(
        const JellyfinSession& session,
        const std::vector<JellyfinItem>& views
    ) const;
    ApiValueResult<std::vector<JellyfinItem>> loadViews(const JellyfinSession& session) const;
    ApiValueResult<std::vector<JellyfinItem>> browseLibrary(
        const JellyfinSession& session,
        const std::string& parentId,
        int startIndex = 0,
        int limit = 100
    ) const;
    ApiValueResult<std::vector<JellyfinItem>> browseVideoFilter(
        const JellyfinSession& session,
        const JellyfinItem& library,
        int startIndex,
        int limit,
        bool favorites = false,
        const std::string& genre = {},
        const std::string& nameStartsWith = {}
    ) const;
    ApiValueResult<std::vector<JellyfinItem>> listGenres(
        const JellyfinSession& session,
        const JellyfinItem& library,
        int limit = 100
    ) const;
    ApiValueResult<std::vector<JellyfinItem>> browseCollections(
        const JellyfinSession& session,
        int startIndex = 0,
        int limit = 100
    ) const;
    ApiValueResult<std::vector<JellyfinItem>> browseCollectionMembersFallback(
        const JellyfinSession& session,
        const JellyfinItem& collection
    ) const;
    ApiValueResult<std::vector<JellyfinItem>> search(const JellyfinSession& session, const std::string& query) const;
    ApiValueResult<JellyfinItem> getItem(const JellyfinSession& session, const std::string& itemId) const;
    ApiValueResult<std::vector<JellyfinItem>> getSimilar(
        const JellyfinSession& session,
        const std::string& itemId,
        int limit = 18
    ) const;
    ApiValueResult<std::vector<JellyfinItem>> getSeasons(const JellyfinSession& session, const std::string& seriesId) const;
    ApiValueResult<std::vector<JellyfinItem>> getEpisodes(
        const JellyfinSession& session,
        const std::string& seriesId,
        const std::string& seasonId
    ) const;
    ApiValueResult<JellyfinItem> getNextUpForSeries(const JellyfinSession& session, const std::string& seriesId) const;
    ApiValueResult<std::vector<JellyfinMediaSegment>> getMediaSegments(
        const JellyfinSession& session,
        const std::string& itemId
    ) const;
    ApiValueResult<JellyfinItem> getFollowingEpisodeForSeries(
        const JellyfinSession& session,
        const std::string& seriesId,
        const std::string& currentItemId
    ) const;
    ApiResult setFavorite(const JellyfinSession& session, const JellyfinItem& item, bool favorite) const;
    ApiResult setPlayed(const JellyfinSession& session, const JellyfinItem& item, bool played) const;
    ApiValueResult<PlaybackTarget> resolvePlayback(
        const JellyfinSession& session,
        const JellyfinItem& item,
        int maxStreamingBitrate = 120000000,
        int audioStreamIndex = -1,
        int subtitleStreamIndex = -1
    ) const;
    ApiResult reportPlaybackStart(const JellyfinSession& session, const JellyfinItem& item, const PlaybackTarget& target, int64_t positionTicks) const;
    ApiResult reportPlaybackProgress(const JellyfinSession& session, const JellyfinItem& item, const PlaybackTarget& target, int64_t positionTicks, bool paused) const;
    ApiResult reportPlaybackStopped(const JellyfinSession& session, const JellyfinItem& item, const PlaybackTarget& target, int64_t positionTicks) const;

    [[nodiscard]] std::string imageUrl(
        const JellyfinSession& session,
        const JellyfinItem& item,
        int width = 360,
        int height = 540
    ) const;
    ApiValueResult<std::string> downloadPrimaryImage(
        const JellyfinSession& session,
        const JellyfinItem& item,
        int width = 360,
        int height = 540
    ) const;
    ApiValueResult<std::string> downloadBackdropImage(
        const JellyfinSession& session,
        const JellyfinItem& item,
        int width = 1280,
        int height = 720
    ) const;
    ApiValueResult<std::string> downloadHomeImage(
        const JellyfinSession& session,
        const JellyfinItem& item,
        int width = 480,
        int height = 270
    ) const;
    ApiValueResult<std::string> downloadSubtitleSrt(
        const JellyfinSession& session,
        const JellyfinItem& item,
        int subtitleIndex
    ) const;

    [[nodiscard]] const DeviceCodecSupport& deviceCodecSupport() const { return codecSupport_; }

private:
    std::string normalizeServer(std::string value) const;
    std::string discoverServerBase(const std::string& value, const std::string& deviceId) const;
    std::string authorization(const JellyfinSession* session, const std::string& deviceId) const;
    std::map<std::string, std::string> headers(const JellyfinSession* session, const std::string& deviceId) const;
    std::string urlEncode(const std::string& value) const;
    JellyfinItem parseItem(const nlohmann::json& value) const;
    ApiValueResult<JellyfinSession> parseAuthenticationResult(
        const HttpResponse& response,
        const std::string& server,
        const std::string& deviceId,
        const std::string& fallbackUsername
    ) const;
    ApiValueResult<std::vector<JellyfinItem>> parseItemList(const HttpResponse& response) const;

    JniHttpClient http_;
    DeviceCodecSupport codecSupport_;
};
