#pragma once

#include "subtitle_policy.hpp"

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
    bool isExternal = false;
};

struct JellyfinPerson {
    std::string id;
    std::string name;
    std::string imageTag;
    std::string role;
};

struct JellyfinAudioStream {
    int index = -1;
    int channels = 0;
    std::string codec;
    std::string language;
    std::string title;
    bool isDefault = false;
};

struct JellyfinTrickplayInfo {
    std::string mediaSourceId;
    int width = 0;
    int height = 0;
    int tileWidth = 0;
    int tileHeight = 0;
    int thumbnailCount = 0;
    int intervalMs = 0;

    [[nodiscard]] bool valid() const {
        return !mediaSourceId.empty() && width > 0 && height > 0 && tileWidth > 0 && tileHeight > 0
            && thumbnailCount > 0 && intervalMs > 0;
    }
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
    std::string logoTag;
    std::string logoItemId;
    std::string backdropTag;
    std::string backdropItemId;
    std::string officialRating;
    std::vector<std::string> genres;
    std::vector<std::string> cast;
    std::vector<JellyfinPerson> people;
    std::vector<JellyfinChapter> chapters;
    std::vector<JellyfinAudioStream> audios;
    std::vector<JellyfinSubtitleStream> subtitles;
    JellyfinTrickplayInfo trickplay;
    int productionYear = 0;
    float communityRating = -1.0f;
    int videoWidth = 0;
    int videoHeight = 0;
    int videoBitDepth = 0;
    int videoLevel = 0;
    float videoFrameRate = 0.0f;
    int indexNumber = -1;
    int parentIndexNumber = -1;
    int64_t positionTicks = 0;
    int64_t runtimeTicks = 0;
    bool favorite = false;
    bool played = false;
    bool canDelete = false;
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

enum class PlaybackMethod {
    DirectPlay,
    DirectStream,
    Transcode,
};

inline const char* playbackMethodName(PlaybackMethod method) {
    switch (method) {
        case PlaybackMethod::DirectPlay: return "DirectPlay";
        case PlaybackMethod::DirectStream: return "DirectStream";
        case PlaybackMethod::Transcode: return "Transcode";
    }
    return "DirectPlay";
}

struct PlaybackTarget {
    std::string url;
    std::string fallbackTranscodeUrl;
    std::string playSessionId;
    std::string mediaSourceId;
    bool transcoding = false;
    PlaybackMethod playMethod = PlaybackMethod::DirectPlay;
    int audioStreamIndex = -1;
    int subtitleStreamIndex = kSubtitleOffIndex;
    std::string subtitleUrl;
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
