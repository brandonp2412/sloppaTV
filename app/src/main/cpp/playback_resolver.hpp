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
    auto target = client.resolvePlayback(session, item, options.maxStreamingBitrate, options.maxAudioChannels,
                                         options.overrides, tracks.audioStreamIndex, tracks.subtitleStreamIndex);
    return {
        .item = std::move(item),
        .target = std::move(target),
    };
}
