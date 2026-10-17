#pragma once

#include "jellyfin_types.hpp"
#include "playback_profile.hpp"

#include <string>
#include <string_view>

struct PlaybackInfoOffer {
    std::string playSessionId;
    std::string mediaSourceId;
    std::string alternativeMediaSourceId;
    int sourceCount = 0;
    int directPlaySourceCount = 0;
    int audioStreamIndex = -1;
    int audioOrdinal = -1;
    std::vector<JellyfinAudioStream> audioTracks;
    int subtitleStreamIndex = kSubtitleOffIndex;
    std::vector<JellyfinSubtitleStream> subtitleTracks;
    std::string subtitleDeliveryUrl;
    std::string transcodingUrl;
    std::string container;
    bool supportsDirectPlay = false;
    bool supportsDirectStream = false;
    bool supportsTranscoding = false;
};

std::string buildPlaybackInfoRequestBody(const JellyfinSession& session, const JellyfinItem& item,
                                         const PlaybackProfilePlan& plan, PlaybackOverrides overrides,
                                         int maxStreamingBitrate, int maxAudioChannels, int audioStreamIndex,
                                         int subtitleStreamIndex);

ApiValueResult<PlaybackInfoOffer> parsePlaybackInfoOffer(std::string_view responseBody, int audioStreamIndex,
                                                         int subtitleStreamIndex, PlaybackOverrides overrides = {},
                                                         std::string_view requestedMediaSourceId = {},
                                                         std::string_view preferredAudioLanguage = {});
