#pragma once

#include "device_capabilities.hpp"
#include "jellyfin_types.hpp"
#include "jni_http.hpp"
#include "media_player_policy.hpp"

#include <jni.h>
#include <nlohmann/json.hpp>

#include <cstdint>
#include <mutex>
#include <optional>
#include <string>
#include <vector>

class JellyfinClient {
public:
    JellyfinClient(JavaVM* vm, jobject activity);
    ~JellyfinClient();

    JellyfinClient(const JellyfinClient&) = delete;
    JellyfinClient& operator=(const JellyfinClient&) = delete;

    void cancelPendingRequests() const { http_.cancelPending(); }
    void warmDeviceCodecSupport() const;

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
    ApiValueResult<std::vector<JellyfinItem>> getItemsForPerson(
        const JellyfinSession& session,
        const std::string& personId,
        int limit = 60
    ) const;
    ApiValueResult<std::vector<JellyfinItem>> getSeriesEpisodes(
        const JellyfinSession& session,
        const std::string& seriesId,
        int limit = 500
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
    ApiResult refreshMetadata(const JellyfinSession& session, const JellyfinItem& item) const;
    ApiResult deleteItem(const JellyfinSession& session, const JellyfinItem& item) const;
    [[nodiscard]] bool isStaticStreamAvailable(const JellyfinSession& session, const JellyfinItem& item) const;
    ApiValueResult<PlaybackTarget> resolvePlayback(
        const JellyfinSession& session,
        const JellyfinItem& item,
        int maxStreamingBitrate = 120000000,
        int maxAudioChannels = 8,
        PlaybackOverrides overrides = {},
        int audioStreamIndex = -1,
        int subtitleStreamIndex = kSubtitleServerDefaultIndex
    ) const;
    ApiResult reportPlaybackStart(const JellyfinSession& session, const JellyfinItem& item, const PlaybackTarget& target, int64_t positionTicks) const;
    ApiResult reportPlaybackProgress(const JellyfinSession& session, const JellyfinItem& item, const PlaybackTarget& target, int64_t positionTicks, bool paused) const;
    ApiResult reportPlaybackStopped(const JellyfinSession& session, const JellyfinItem& item, const PlaybackTarget& target, int64_t positionTicks) const;
    ApiResult reportExternalPlaybackStopped(
        const JellyfinSession& session,
        const JellyfinItem& item,
        std::optional<int64_t> positionTicks
    ) const;

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
    ApiValueResult<std::string> downloadUserImage(
        const JellyfinSession& session,
        int width = 160,
        int height = 160
    ) const;
    ApiValueResult<std::string> downloadBackdropImage(
        const JellyfinSession& session,
        const JellyfinItem& item,
        int width = 1280,
        int height = 720
    ) const;
    ApiValueResult<std::string> downloadLogoImage(
        const JellyfinSession& session,
        const JellyfinItem& item,
        int width = 800,
        int height = 240
    ) const;
    ApiValueResult<std::string> downloadHomeImage(
        const JellyfinSession& session,
        const JellyfinItem& item,
        int width = 480,
        int height = 270
    ) const;
    [[nodiscard]] std::string staticVideoUrl(
        const JellyfinSession& session,
        const JellyfinItem& item
    ) const;
    [[nodiscard]] std::string subtitleSrtUrl(
        const JellyfinSession& session,
        const JellyfinItem& item,
        int subtitleIndex
    ) const;
    ApiValueResult<std::string> downloadSubtitleSrt(
        const JellyfinSession& session,
        const JellyfinItem& item,
        int subtitleIndex
    ) const;
    ApiValueResult<std::string> downloadSubtitleUrl(
        const JellyfinSession& session,
        const std::string& url
    ) const;
    ApiValueResult<std::string> downloadTrickplayTile(
        const JellyfinSession& session,
        const JellyfinItem& item,
        int tileIndex
    ) const;

    [[nodiscard]] DeviceCodecSupport deviceCodecSupport() const;

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

    DeviceCodecSupport ensureDeviceCodecSupport() const;

    JniHttpClient http_;
    JavaVM* vm_ = nullptr;
    jobject activity_ = nullptr;
    mutable std::once_flag codecSupportOnce_;
    mutable std::mutex codecSupportMutex_;
    mutable DeviceCodecSupport codecSupport_;
};
