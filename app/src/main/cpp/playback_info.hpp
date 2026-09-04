#pragma once

#include "jellyfin_types.hpp"
#include "playback_profile.hpp"

#include <string>
#include <string_view>

struct PlaybackInfoOffer {
    std::string playSessionId;
    std::string mediaSourceId;
    int audioStreamIndex = -1;
    int subtitleStreamIndex = kSubtitleOffIndex;
    std::string subtitleDeliveryUrl;
    std::string transcodingUrl;
    std::string container;
    bool supportsDirectPlay = false;
    bool supportsDirectStream = false;
    bool supportsTranscoding = false;
};

std::string buildPlaybackInfoRequestBody(
    const JellyfinSession& session,
    const JellyfinItem& item,
    const PlaybackProfilePlan& plan,
    PlaybackOverrides overrides,
    int maxStreamingBitrate,
    int maxAudioChannels,
    int audioStreamIndex,
    int subtitleStreamIndex
);

ApiValueResult<PlaybackInfoOffer> parsePlaybackInfoOffer(
    std::string_view responseBody,
    int audioStreamIndex,
    int subtitleStreamIndex
);
