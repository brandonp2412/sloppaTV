#pragma once

#include "jellyfin_types.hpp"
#include "playback_track_selection.hpp"

#include <optional>
#include <string>
#include <map>
#include <utility>

struct PlaybackResolutionOptions {
    int maxStreamingBitrate = 0;
    int maxAudioChannels = 0;
    bool isRemux = false;
    PlaybackOverrides overrides;
    std::map<std::string, std::string> preferredMediaSources;
    std::optional<std::string> audioLanguagePreference;
    std::optional<std::string> subtitleLanguagePreference;
    PlaybackTrackSelectionPolicy trackPolicy;
};

struct PlaybackResolutionResult {
    JellyfinItem item;
    ApiValueResult<PlaybackTarget> target;
};


inline JellyfinItem playbackNegotiationItem(const JellyfinItem& item, bool heterogeneousRemuxPool) {
    JellyfinItem negotiation = item;
    if (!heterogeneousRemuxPool) return negotiation;

    // Remux can return many torrent versions with different codecs, profiles,
    // HDR modes, and resolutions. The hydrated item's video fields describe
    // only one source, so using them to prune the DeviceProfile can disable
    // direct play for a different healthy fallback before Remux evaluates it.
    negotiation.videoCodec.clear();
    negotiation.videoProfile.clear();
    negotiation.videoRangeType.clear();
    negotiation.videoBitDepth = 0;
    negotiation.videoLevel = 0;
    negotiation.videoWidth = 0;
    negotiation.videoHeight = 0;
    negotiation.videoFrameRate = 0.0f;
    return negotiation;
}

template <typename Client>
PlaybackResolutionResult resolvePreferredPlayback(Client& client, const JellyfinSession& session, JellyfinItem item,
                                                  const PlaybackResolutionOptions& options) {
    auto detailed = client.getItem(session, item.id);
    if (detailed.ok) item = std::move(detailed.value);
    if (const auto preferred = options.preferredMediaSources.find(item.id);
        preferred != options.preferredMediaSources.end()) {
        item.mediaSourceId = preferred->second;
    } else if (options.isRemux) {
        // Item metadata names its first source even without a user choice. Sending
        // that ID to Remux restricts PlaybackInfo to ONE version, preventing
        // recovery from an unhealthy torrent. Ask for the whole ranked pool.
        item.mediaSourceId.clear();
    }
    const auto tracks = selectPlaybackTracks(item, options.audioLanguagePreference, options.subtitleLanguagePreference,
                                             options.trackPolicy);
    const JellyfinItem negotiation =
        playbackNegotiationItem(item, options.isRemux && item.mediaSourceId.empty());
    auto target = client.resolvePlayback(session, negotiation, options.maxStreamingBitrate, options.maxAudioChannels,
                                         options.overrides, tracks.audioStreamIndex, tracks.subtitleStreamIndex);
    return {
        .item = std::move(item),
        .target = std::move(target),
    };
}
