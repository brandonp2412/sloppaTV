#include "playback_resolver.hpp"

#include <cassert>
#include <string>

namespace {
struct FakeClient {
    ApiValueResult<JellyfinItem> getItem(const JellyfinSession& session, const std::string& itemId) {
        lastServer = session.server;
        lastRequestedItemId = itemId;
        ApiValueResult<JellyfinItem> result;
        result.ok = detailOk;
        if (!detailOk) {
            result.error = "detail failed";
            return result;
        }

        JellyfinItem item;
        item.id = itemId;
        item.name = "Detailed";
        item.mediaSourceId = "server-first-source";
        item.audios = {
            {.index = 2, .channels = 2, .codec = "aac", .language = "eng", .title = "English", .isDefault = true},
            {.index = 3, .channels = 2, .codec = "aac", .language = "jpn", .title = "Japanese", .isDefault = false},
        };
        item.subtitles = {
            {.index = 4,
             .codec = "srt",
             .language = "eng",
             .title = "English",
             .forced = false,
             .isDefault = true,
             .isExternal = true},
        };
        result.value = std::move(item);
        return result;
    }

    ApiValueResult<PlaybackTarget> resolvePlayback(const JellyfinSession& session, const JellyfinItem& item,
                                                   int maxStreamingBitrate, int maxAudioChannels,
                                                   PlaybackOverrides overrides, int audioStreamIndex,
                                                   int subtitleStreamIndex) {
        lastServer = session.server;
        resolvedItemName = item.name;
        resolvedMediaSourceId = item.mediaSourceId;
        lastMaxStreamingBitrate = maxStreamingBitrate;
        lastMaxAudioChannels = maxAudioChannels;
        lastOverrides = overrides;
        lastAudioStreamIndex = audioStreamIndex;
        lastSubtitleStreamIndex = subtitleStreamIndex;

        ApiValueResult<PlaybackTarget> result;
        result.ok = true;
        result.value.url = "https://media.example/stream";
        result.value.audioStreamIndex = audioStreamIndex;
        result.value.subtitleStreamIndex = subtitleStreamIndex;
        return result;
    }

    bool detailOk = true;
    std::string lastServer;
    std::string lastRequestedItemId;
    std::string resolvedItemName;
    std::string resolvedMediaSourceId;
    int lastMaxStreamingBitrate = 0;
    int lastMaxAudioChannels = 0;
    PlaybackOverrides lastOverrides;
    int lastAudioStreamIndex = -1;
    int lastSubtitleStreamIndex = -1;
};
} // namespace

int main() {
    JellyfinSession session;
    session.server = "https://jellyfin.example.nz";

    JellyfinItem item;
    item.id = "episode-1";
    item.name = "Summary";

    PlaybackResolutionOptions options;
    options.maxStreamingBitrate = 20'000'000;
    options.maxAudioChannels = 6;
    options.audioLanguagePreference = std::string{"jpn"};
    options.subtitleLanguagePreference = std::string{"eng"};

    FakeClient client;
    auto resolved = resolvePreferredPlayback(client, session, item, options);
    assert(client.lastServer == session.server);
    assert(client.lastRequestedItemId == item.id);
    assert(client.resolvedItemName == "Detailed");
    assert(client.lastMaxStreamingBitrate == 20'000'000);
    assert(client.lastMaxAudioChannels == 6);
    assert(client.lastAudioStreamIndex == 3);
    assert(client.lastSubtitleStreamIndex == 4);
    assert(resolved.item.name == "Detailed");
    assert(resolved.target.ok);
    assert(resolved.target.value.audioStreamIndex == 3);
    assert(resolved.target.value.subtitleStreamIndex == 4);
    options.isRemux = true;
    resolved = resolvePreferredPlayback(client, session, item, options);
    assert(resolved.item.mediaSourceId.empty());
    assert(client.resolvedMediaSourceId.empty());
    options.preferredMediaSources[item.id] = "known-good-source";
    resolved = resolvePreferredPlayback(client, session, item, options);
    assert(resolved.item.mediaSourceId.empty());
    assert(client.resolvedMediaSourceId.empty());
    options.isRemux = false;
    resolved = resolvePreferredPlayback(client, session, item, options);
    assert(resolved.item.mediaSourceId == "known-good-source");
    assert(client.resolvedMediaSourceId == "known-good-source");
    options.isRemux = true;

    client.detailOk = false;
    item.audios = {
        {.index = 8, .channels = 2, .codec = "aac", .language = "jpn", .title = "Japanese", .isDefault = true},
    };
    item.subtitles.clear();
    options.subtitleLanguagePreference = std::string{};
    resolved = resolvePreferredPlayback(client, session, item, options);
    assert(resolved.item.name == "Summary");
    assert(client.resolvedItemName == "Summary");
    assert(client.lastAudioStreamIndex == 8);
    assert(client.lastSubtitleStreamIndex == kSubtitleOffIndex);

    return 0;
}
