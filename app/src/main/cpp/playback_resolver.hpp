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
                                                  const PlaybackResolutionOptions& options,
                                                  bool startFromBeginning = false) {
    auto detailed = client.getItem(session, item.id);
    if (detailed.ok) {
        const int64_t selectedPositionTicks = item.positionTicks;
        item = std::move(detailed.value);
        // The server can lag a just-reported local stop, but an explicit restart
        // must still override stale server resume data.
        item.positionTicks = startFromBeginning ? 0 : std::max(item.positionTicks, selectedPositionTicks);
    }
    if (options.isRemux) {
        // A remembered source is an automatic prewarm hint, not a user request to
        // lock playback to that release. Sending it as MediaSourceId makes Remux
        // restrict fallback to the same resolution. Keep foreground negotiation
        // automatic so an unhealthy 4K source can recover to a viable 1080p/720p.
        item.mediaSourceId.clear();
    } else if (const auto preferred = options.preferredMediaSources.find(item.id);
               preferred != options.preferredMediaSources.end()) {
        item.mediaSourceId = preferred->second;
    }
    const auto tracks = selectPlaybackTracks(item, options.audioLanguagePreference, options.subtitleLanguagePreference,
                                             options.trackPolicy);
    const JellyfinItem negotiation = playbackNegotiationItem(item, options.isRemux && item.mediaSourceId.empty());
    auto target = client.resolvePlayback(session, negotiation, options.maxStreamingBitrate, options.maxAudioChannels,
                                         options.overrides, tracks.audioStreamIndex, tracks.subtitleStreamIndex);
    return {
        .item = std::move(item),
        .target = std::move(target),
    };
}
