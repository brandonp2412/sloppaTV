#include "playback_info.hpp"

#include "media_player_policy.hpp"
#include "subtitle_policy.hpp"

#include <algorithm>
#include <nlohmann/json.hpp>
#include <sstream>

using nlohmann::json;

namespace {
std::string joinCodecs(const std::vector<std::string>& codecs) {
    std::ostringstream out;
    for (size_t index = 0; index < codecs.size(); ++index) {
        if (index) out << ',';
        out << codecs[index];
    }
    return out.str();
}
}

std::string buildPlaybackInfoRequestBody(
    const JellyfinSession& session,
    const JellyfinItem& item,
    const PlaybackProfilePlan& plan,
    PlaybackOverrides overrides,
    int maxStreamingBitrate,
    int maxAudioChannels,
    int audioStreamIndex,
    int subtitleStreamIndex
) {
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

ApiValueResult<PlaybackInfoOffer> parsePlaybackInfoOffer(
    std::string_view responseBody,
    int audioStreamIndex,
    int subtitleStreamIndex
) {
    ApiValueResult<PlaybackInfoOffer> result;
    try {
        const auto data = json::parse(responseBody);
        if (!data.contains("MediaSources") || !data["MediaSources"].is_array() || data["MediaSources"].empty()) {
            result.error = "Jellyfin returned no playable media source";
            return result;
        }

        const auto& source = data["MediaSources"][0];
        PlaybackInfoOffer offer;
        offer.playSessionId = data.value("PlaySessionId", std::string{});
        offer.mediaSourceId = source.value("Id", std::string{});
        offer.audioStreamIndex = audioStreamIndex >= 0
            ? audioStreamIndex
            : source.value("DefaultAudioStreamIndex", -1);
        offer.subtitleStreamIndex = resolvedSubtitleIndex(
            subtitleStreamIndex,
            source.value("DefaultSubtitleStreamIndex", kSubtitleOffIndex)
        );
        offer.transcodingUrl = source.value("TranscodingUrl", std::string{});
        offer.container = source.value("Container", std::string{});
        offer.supportsDirectPlay = source.value("SupportsDirectPlay", false);
        offer.supportsDirectStream = source.value("SupportsDirectStream", false);
        offer.supportsTranscoding = source.value("SupportsTranscoding", false);

        if (offer.subtitleStreamIndex >= 0 && source.contains("MediaStreams") && source["MediaStreams"].is_array()) {
            const auto stream = std::find_if(
                source["MediaStreams"].begin(),
                source["MediaStreams"].end(),
                [&](const json& candidate) {
                    return candidate.is_object()
                        && candidate.value("Type", std::string{}) == "Subtitle"
                        && candidate.value("Index", -1) == offer.subtitleStreamIndex;
                }
            );
            if (stream != source["MediaStreams"].end()) {
                const std::string subtitleCodec = stream->value("Codec", std::string{});
                const std::string delivery = stream->value("DeliveryMethod", std::string{});
                const std::string deliveryUrl = stream->value("DeliveryUrl", std::string{});
                if (delivery == "External" && !deliveryUrl.empty()
                    && subtitleStrategy(subtitleCodec) != SubtitleStrategy::ServerTranscode) {
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
