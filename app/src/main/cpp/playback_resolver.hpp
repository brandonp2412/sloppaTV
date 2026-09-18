#pragma once

#include "jellyfin_types.hpp"
#include "playback_track_selection.hpp"

#include <optional>
#include <string>
#include <utility>

struct PlaybackResolutionOptions {
    int maxStreamingBitrate = 0;
    int maxAudioChannels = 0;
    PlaybackOverrides overrides;
    std::optional<std::string> audioLanguagePreference;
    std::optional<std::string> subtitleLanguagePreference;
    PlaybackTrackSelectionPolicy trackPolicy;
};

struct PlaybackResolutionResult {
    JellyfinItem item;
    ApiValueResult<PlaybackTarget> target;
};

template <typename Client>
PlaybackResolutionResult resolvePreferredPlayback(Client& client, const JellyfinSession& session, JellyfinItem item,
                                                  const PlaybackResolutionOptions& options) {
    auto detailed = client.getItem(session, item.id);
    if (detailed.ok) item = std::move(detailed.value);
    const auto tracks = selectPlaybackTracks(item, options.audioLanguagePreference, options.subtitleLanguagePreference,
                                             options.trackPolicy);
    auto target = client.resolvePlayback(session, item, options.maxStreamingBitrate, options.maxAudioChannels,
                                         options.overrides, tracks.audioStreamIndex, tracks.subtitleStreamIndex);
    return {
        .item = std::move(item),
        .target = std::move(target),
    };
}
