#include "playback_info.hpp"
#include "audio_policy.hpp"

#include "media_player_policy.hpp"
#include "subtitle_policy.hpp"

#include <algorithm>
#include <iterator>
#include <optional>
#include <vector>
#include <nlohmann/json.hpp>
#include <sstream>

using nlohmann::json;

namespace {
template <typename T> T scalarValueOr(const json& value, const char* key, T fallback) {
    const auto match = value.find(key);
    if (match == value.end() || match->is_null()) return fallback;
    try {
        return match->get<T>();
    } catch (const json::exception&) {
        return fallback;
    }
}

bool isNoStreamsPlaceholder(const json& source) {
    // A gateway can return the Remux placeholder as an absolute or query-bearing URL.
    // Never report its intentionally playable clip as decoded media content.
    if (!source.is_object()) return false;
    const auto isSentinelUrl = [](std::string_view url) {
        const auto scheme = url.find("://");
        if (scheme != std::string_view::npos) {
            const auto path = url.find('/', scheme + 3);
            if (path == std::string_view::npos) return false;
            url.remove_prefix(path);
        }
        url = url.substr(0, url.find_first_of("?#"));
        return url == "/videos/no-streams";
    };
    return isSentinelUrl(scalarValueOr(source, "Path", std::string{})) ||
           isSentinelUrl(scalarValueOr(source, "DirectStreamUrl", std::string{}));
}

std::string joinCodecs(const std::vector<std::string>& codecs) {
    std::ostringstream out;
    for (size_t index = 0; index < codecs.size(); ++index) {
        if (index) out << ',';
        out << codecs[index];
    }
    return out.str();
}
} // namespace

std::string buildPlaybackInfoRequestBody(const JellyfinSession& session, const JellyfinItem& item,
                                         const PlaybackProfilePlan& plan, PlaybackOverrides overrides,
                                         int maxStreamingBitrate, int maxAudioChannels, int audioStreamIndex,
                                         int subtitleStreamIndex) {
    // Embedded libmpv renders these formats itself. Advertise embedded support so
    // Jellyfin does not burn PGS/ASS/etc. into the video and force a transcode.
    // Text formats may still be delivered externally when Jellyfin prefers that.
    json subtitleProfiles = json::array({
        {{"Format", "vtt"}, {"Method", "External"}},
        {{"Format", "vtt"}, {"Method", "Embed"}},
        {{"Format", "webvtt"}, {"Method", "External"}},
        {{"Format", "webvtt"}, {"Method", "Embed"}},
        {{"Format", "srt"}, {"Method", "External"}},
        {{"Format", "srt"}, {"Method", "Embed"}},
        {{"Format", "subrip"}, {"Method", "External"}},
        {{"Format", "subrip"}, {"Method", "Embed"}},
        {{"Format", "mov_text"}, {"Method", "External"}},
        {{"Format", "mov_text"}, {"Method", "Embed"}},
        {{"Format", "ass"}, {"Method", "External"}},
        {{"Format", "ass"}, {"Method", "Embed"}},
        {{"Format", "ssa"}, {"Method", "External"}},
        {{"Format", "ssa"}, {"Method", "Embed"}},
        {{"Format", "pgs"}, {"Method", "Embed"}},
        {{"Format", "pgssub"}, {"Method", "Embed"}},
        {{"Format", "hdmv_pgs_subtitle"}, {"Method", "Embed"}},
        {{"Format", "dvdsub"}, {"Method", "Embed"}},
        {{"Format", "dvd_subtitle"}, {"Method", "Embed"}},
        {{"Format", "dvbsub"}, {"Method", "Embed"}},
        {{"Format", "dvb_subtitle"}, {"Method", "Embed"}},
        {{"Format", "xsub"}, {"Method", "Embed"}},
    });

    const std::string videoCodecList = joinCodecs(plan.videoCodecs);
    const std::string serverStreamVideoCodecs = serverStreamVideoCodecList(plan.videoCodecs);
    const std::string audioCodecList = joinCodecs(plan.audioCodecs);
    const std::string transcodeAudioCodecList = joinCodecs(plan.transcodeAudioCodecs);
    json profile = {
        {"Name", "sloppaTV-Native"},
        {"MaxStaticBitrate", 120000000},
        {"MaxStreamingBitrate", std::max(1000000, maxStreamingBitrate)},
        {"MusicStreamingTranscodingBitrate", 192000},
        {"DirectPlayProfiles", json::array({
                                   {
                                       {"Container", "mkv,matroska,mp4,m4v,mov,ts,mpegts,webm"},
                                       {"Type", "Video"},
                                       {"VideoCodec", videoCodecList},
                                       {"AudioCodec", audioCodecList},
                                   },
                               })},
        {"TranscodingProfiles", json::array({
                                    {
                                        {"Container", "ts"},
                                        {"Type", "Video"},
                                        {"VideoCodec", serverStreamVideoCodecs},
                                        {"AudioCodec", transcodeAudioCodecList},
                                        {"Protocol", "hls"},
                                        {"Context", "Streaming"},
                                        {"CopyTimestamps", false},
                                        {"EnableSubtitlesInManifest", true},
                                        {"MaxAudioChannels", std::to_string(maxAudioChannels)},
                                    },
                                })},
        {"CodecProfiles", json::array({
                              {
                                  {"Type", "VideoAudio"},
                                  {"Conditions", json::array({
                                                     {
                                                         {"Condition", "LessThanEqual"},
                                                         {"Property", "AudioChannels"},
                                                         {"Value", std::to_string(maxAudioChannels)},
                                                         {"IsRequired", false},
                                                     },
                                                 })},
                              },
                          })},
        {"SubtitleProfiles", std::move(subtitleProfiles)},
    };

    const PlaybackRequestFlags requestFlags = playbackRequestFlags(plan, overrides);
    json body = {
        {"UserId", session.userId},
        {"StartTimeTicks", item.positionTicks},
        {"MediaSourceId", item.mediaSourceId.empty() ? json(nullptr) : json(item.mediaSourceId)},
        {"DeviceProfile", std::move(profile)},
        {"AudioStreamIndex", audioStreamIndex >= 0 ? json(audioStreamIndex) : json(nullptr)},
        {"MaxAudioChannels", maxAudioChannels},
        {"EnableDirectPlay", requestFlags.enableDirectPlay},
        {"EnableDirectStream", requestFlags.enableDirectStream},
        {"EnableTranscoding", true},
        {"AllowVideoStreamCopy", requestFlags.allowVideoStreamCopy},
        {"AllowAudioStreamCopy", requestFlags.allowAudioStreamCopy},
        {"AutoOpenLiveStream", true},
    };
    if (subtitleStreamIndex != kSubtitleServerDefaultIndex) {
        body["SubtitleStreamIndex"] = subtitleStreamIndex;
    }
    return body.dump();
}

ApiValueResult<PlaybackInfoOffer> parsePlaybackInfoOffer(std::string_view responseBody, int audioStreamIndex,
                                                         int subtitleStreamIndex, PlaybackOverrides overrides,
                                                         std::string_view requestedMediaSourceId,
                                                         std::string_view preferredAudioLanguage) {
    ApiValueResult<PlaybackInfoOffer> result;
    try {
        const auto data = json::parse(responseBody);
        if (!data.contains("MediaSources") || !data["MediaSources"].is_array() || data["MediaSources"].empty()) {
            result.error = "Jellyfin returned no playable media source";
            return result;
        }

        const auto& sources = data["MediaSources"];
        const auto firstObject = std::find_if(sources.begin(), sources.end(), [](const json& candidate) {
            return candidate.is_object() && !isNoStreamsPlaceholder(candidate);
        });
        if (firstObject == sources.end()) {
            result.error = "Jellyfin returned no playable media source";
            return result;
        }
        // Jellyfin can return multiple media versions. Rank each source with the same
        // route policy used by resolvePlayback so forced server-stream/transcode modes
        // cannot accidentally select a direct-play-only version and report a false
        // "no playback path" failure. Stable ranking preserves the first server result
        // when routes are equivalent or when older/noisy servers omit all capability flags.
        std::vector<json::const_iterator> candidates;
        for (auto candidate = firstObject; candidate != sources.end(); ++candidate) {
            if (candidate->is_object() && !isNoStreamsPlaceholder(*candidate)) candidates.push_back(candidate);
        }
        // Preserve the server's ordering. Remux may have spent the PlaybackInfo
        // budget proving the first torrent (or one of its internal fallbacks)
        // readable; re-sorting by advertised seeders or preferring an unprobed
        // later DirectPlay flag can make the URL SloppaTV opens differ from the
        // source whose bytes were actually verified.
        auto source = candidates.front();
        if (!requestedMediaSourceId.empty()) {
            const auto requested = std::find_if(candidates.begin(), candidates.end(), [&](const auto& candidate) {
                if (scalarValueOr(*candidate, "Id", std::string{}) != requestedMediaSourceId) return false;
                const std::string transcodingUrl = scalarValueOr(*candidate, "TranscodingUrl", std::string{});
                return choosePlaybackServerRoute(
                           {
                               .supportsDirectPlay = scalarValueOr(*candidate, "SupportsDirectPlay", false),
                               .supportsDirectStream = scalarValueOr(*candidate, "SupportsDirectStream", false),
                               .supportsTranscoding = scalarValueOr(*candidate, "SupportsTranscoding", false),
                               .transcodingUrl = transcodingUrl,
                           },
                           overrides) != PlaybackServerRoute::Unavailable;
            });
            if (requested != candidates.end()) source = *requested;
        }
        // A stale requested source must not suppress route-compatible fallback.
        // Preserve the response's availability-ranked order for all candidates.
        const std::string selectedTranscodingUrl = scalarValueOr(*source, "TranscodingUrl", std::string{});
        const PlaybackServerRoute selectedRoute = choosePlaybackServerRoute(
            {
                .supportsDirectPlay = scalarValueOr(*source, "SupportsDirectPlay", false),
                .supportsDirectStream = scalarValueOr(*source, "SupportsDirectStream", false),
                .supportsTranscoding = scalarValueOr(*source, "SupportsTranscoding", false),
                .transcodingUrl = selectedTranscodingUrl,
            },
            overrides);
        if (selectedRoute == PlaybackServerRoute::Unavailable) {
            const auto usable = std::find_if(candidates.begin(), candidates.end(), [&](const auto& candidate) {
                const std::string transcodingUrl = scalarValueOr(*candidate, "TranscodingUrl", std::string{});
                return choosePlaybackServerRoute(
                           {
                               .supportsDirectPlay = scalarValueOr(*candidate, "SupportsDirectPlay", false),
                               .supportsDirectStream = scalarValueOr(*candidate, "SupportsDirectStream", false),
                               .supportsTranscoding = scalarValueOr(*candidate, "SupportsTranscoding", false),
                               .transcodingUrl = transcodingUrl,
                           },
                           overrides) != PlaybackServerRoute::Unavailable;
            });
            if (usable != candidates.end()) source = *usable;
        }

        PlaybackInfoOffer offer;
        offer.sourceCount = static_cast<int>(sources.size());
        offer.directPlaySourceCount =
            static_cast<int>(std::count_if(sources.begin(), sources.end(), [](const json& candidate) {
                return candidate.is_object() && !isNoStreamsPlaceholder(candidate) &&
                       scalarValueOr(candidate, "SupportsDirectPlay", false);
            }));
        offer.playSessionId = scalarValueOr(data, "PlaySessionId", std::string{});
        offer.mediaSourceId = scalarValueOr(*source, "Id", std::string{});
        // Fall back through this request's availability-ranked ordering, never
        // backwards to an already attempted version (A -> B -> A loop).
        for (auto candidate = std::next(std::find(candidates.begin(), candidates.end(), source));
             candidate != candidates.end(); ++candidate) {
            if (!scalarValueOr(**candidate, "SupportsDirectPlay", false)) continue;
            const std::string id = scalarValueOr(**candidate, "Id", std::string{});
            if (id.empty() || id == offer.mediaSourceId) continue;
            offer.alternativeMediaSourceId = id;
            break;
        }
        // Stream indexes are scoped to a media file. After choosing a different
        // torrent, index 1 can be Italian where index 1 was English on the
        // previous source. Match the intended language against THIS source's
        // audio tracks and carry the native ordinal to libmpv.
        offer.audioStreamIndex = scalarValueOr(*source, "DefaultAudioStreamIndex", -1);
        if (source->contains("MediaStreams") && (*source)["MediaStreams"].is_array()) {
            for (const auto& track : (*source)["MediaStreams"]) {
                if (!track.is_object() || scalarValueOr(track, "Type", std::string{}) != "Audio") continue;
                JellyfinAudioStream audio;
                audio.index = scalarValueOr(track, "Index", -1);
                audio.language = scalarValueOr(track, "Language", std::string{});
                audio.codec = scalarValueOr(track, "Codec", std::string{});
                audio.title = scalarValueOr(track, "DisplayTitle", std::string{});
                audio.channels = scalarValueOr(track, "Channels", 0);
                audio.isDefault = scalarValueOr(track, "IsDefault", false);
                if (audio.index < 0) continue;
                offer.audioTracks.push_back(std::move(audio));
            }
        }
        auto chosenAudio = offer.audioTracks.end();
        if (!preferredAudioLanguage.empty()) {
            chosenAudio =
                std::find_if(offer.audioTracks.begin(), offer.audioTracks.end(), [&](const JellyfinAudioStream& track) {
                    return audioLanguageEquals(track.language, preferredAudioLanguage);
                });
        }
        if (chosenAudio == offer.audioTracks.end() && audioStreamIndex >= 0 && preferredAudioLanguage.empty()) {
            chosenAudio =
                std::find_if(offer.audioTracks.begin(), offer.audioTracks.end(),
                             [&](const JellyfinAudioStream& track) { return track.index == audioStreamIndex; });
        }
        if (chosenAudio == offer.audioTracks.end()) {
            chosenAudio = std::find_if(offer.audioTracks.begin(), offer.audioTracks.end(),
                                       [](const JellyfinAudioStream& track) { return track.isDefault; });
        }
        if (chosenAudio == offer.audioTracks.end() && !offer.audioTracks.empty())
            chosenAudio = offer.audioTracks.begin();
        if (chosenAudio != offer.audioTracks.end()) {
            offer.audioOrdinal = static_cast<int>(std::distance(offer.audioTracks.begin(), chosenAudio));
            offer.audioStreamIndex = chosenAudio->index;
        } else if (audioStreamIndex >= 0) {
            // Metadata-poor servers: retain the explicit request when there
            // are no source-local tracks to re-map.
            offer.audioStreamIndex = audioStreamIndex;
        }
        offer.subtitleStreamIndex = resolvedSubtitleIndex(
            subtitleStreamIndex, scalarValueOr(*source, "DefaultSubtitleStreamIndex", kSubtitleOffIndex));
        offer.transcodingUrl = scalarValueOr(*source, "TranscodingUrl", std::string{});
        offer.container = scalarValueOr(*source, "Container", std::string{});
        offer.supportsDirectPlay = scalarValueOr(*source, "SupportsDirectPlay", false);
        offer.supportsDirectStream = scalarValueOr(*source, "SupportsDirectStream", false);
        offer.supportsTranscoding = scalarValueOr(*source, "SupportsTranscoding", false);

        if (offer.subtitleStreamIndex >= 0 && source->contains("MediaStreams") &&
            (*source)["MediaStreams"].is_array()) {
            const auto stream = std::find_if(
                (*source)["MediaStreams"].begin(), (*source)["MediaStreams"].end(), [&](const json& candidate) {
                    return candidate.is_object() && scalarValueOr(candidate, "Type", std::string{}) == "Subtitle" &&
                           scalarValueOr(candidate, "Index", -1) == offer.subtitleStreamIndex;
                });
            if (stream != (*source)["MediaStreams"].end()) {
                const std::string subtitleCodec = scalarValueOr(*stream, "Codec", std::string{});
                const std::string delivery = scalarValueOr(*stream, "DeliveryMethod", std::string{});
                const std::string deliveryUrl = scalarValueOr(*stream, "DeliveryUrl", std::string{});
                if (delivery == "External" && !deliveryUrl.empty() &&
                    subtitleStrategy(subtitleCodec) != SubtitleStrategy::ServerTranscode) {
                    offer.subtitleDeliveryUrl = deliveryUrl;
                }
            }
        }

        result.value = std::move(offer);
        result.ok = true;
    } catch (const std::exception& e) {
        result.error = std::string("Unable to parse playback info: ") + e.what();
    }
    return result;
}
