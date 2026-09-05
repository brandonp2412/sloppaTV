#include "playback_info.hpp"

#include <cassert>
#include <nlohmann/json.hpp>

using nlohmann::json;

int main() {
    JellyfinSession session;
    session.userId = "user";

    JellyfinItem item;
    item.id = "episode";
    item.mediaSourceId = "source";
    item.positionTicks = 12345;

    PlaybackProfilePlan plan;
    plan.videoCodecs = {"hevc", "h264", "mpeg4"};
    plan.audioCodecs = {"aac", "eac3", "mp2", "pcm_s16le"};
    plan.transcodeAudioCodecs = {"aac"};
    plan.maxAudioChannels = 6;
    plan.allowAudioStreamCopy = true;

    const std::string request = buildPlaybackInfoRequestBody(session, item, plan, {}, 50000000, 6, 2, 5);
    const auto body = json::parse(request);
    assert(body["UserId"] == "user");
    assert(body["StartTimeTicks"] == 12345);
    assert(body["MediaSourceId"] == "source");
    assert(body["AudioStreamIndex"] == 2);
    assert(body["SubtitleStreamIndex"] == 5);
    assert(body["MaxAudioChannels"] == 6);
    assert(body["EnableDirectPlay"].get<bool>());
    assert(body["AllowAudioStreamCopy"].get<bool>());
    assert(body["DeviceProfile"]["MaxStreamingBitrate"] == 50000000);
    assert(body["DeviceProfile"]["DirectPlayProfiles"][0]["VideoCodec"] == "hevc,h264,mpeg4");
    assert(body["DeviceProfile"]["DirectPlayProfiles"][0]["AudioCodec"] == "aac,eac3,mp2,pcm_s16le");

    PlaybackOverrides forceTranscode;
    forceTranscode.forceTranscode = true;
    const auto forced = json::parse(buildPlaybackInfoRequestBody(session, item, plan, forceTranscode, 500000, 2, -1, kSubtitleServerDefaultIndex));
    assert(forced["DeviceProfile"]["MaxStreamingBitrate"] == 1000000);
    assert(!forced["EnableDirectPlay"].get<bool>());
    assert(!forced["EnableDirectStream"].get<bool>());
    assert(forced["MediaSourceId"] == "source");
    assert(forced["AudioStreamIndex"].is_null());
    assert(!forced.contains("SubtitleStreamIndex"));

    PlaybackOverrides forceServerStream;
    forceServerStream.forceServerStream = true;
    const auto streamed = json::parse(buildPlaybackInfoRequestBody(
        session,
        item,
        plan,
        forceServerStream,
        50000000,
        6,
        2,
        5
    ));
    assert(!streamed["EnableDirectPlay"].get<bool>());
    assert(streamed["EnableDirectStream"].get<bool>());
    assert(streamed["AllowVideoStreamCopy"].get<bool>());

    const std::string response = R"({
        "PlaySessionId":"play-session",
        "MediaSources":[{
            "Id":"resolved-source",
            "Container":"mkv",
            "DefaultAudioStreamIndex":3,
            "DefaultSubtitleStreamIndex":7,
            "SupportsDirectPlay":true,
            "SupportsDirectStream":true,
            "SupportsTranscoding":true,
            "TranscodingUrl":"/Videos/episode/master.m3u8",
            "MediaStreams":[
                {"Type":"Subtitle","Index":7,"Codec":"srt","DeliveryMethod":"External","DeliveryUrl":"/Videos/episode/subtitles/7/0/Stream.srt"}
            ]
        }]
    })";
    auto offer = parsePlaybackInfoOffer(response, -1, kSubtitleServerDefaultIndex);
    assert(offer.ok);
    assert(offer.value.playSessionId == "play-session");
    assert(offer.value.mediaSourceId == "resolved-source");
    assert(offer.value.audioStreamIndex == 3);
    assert(offer.value.subtitleStreamIndex == 7);
    assert(offer.value.subtitleDeliveryUrl == "/Videos/episode/subtitles/7/0/Stream.srt");
    assert(offer.value.transcodingUrl == "/Videos/episode/master.m3u8");
    assert(offer.value.container == "mkv");
    assert(offer.value.supportsDirectPlay);

    const std::string pgsResponse = R"({
        "MediaSources":[{
            "Id":"source",
            "DefaultSubtitleStreamIndex":4,
            "MediaStreams":[
                {"Type":"Subtitle","Index":4,"Codec":"pgs","DeliveryMethod":"External","DeliveryUrl":"/subtitle"}
            ]
        }]
    })";
    offer = parsePlaybackInfoOffer(pgsResponse, 8, 4);
    assert(offer.ok);
    assert(offer.value.audioStreamIndex == 8);
    assert(offer.value.subtitleStreamIndex == 4);
    assert(offer.value.subtitleDeliveryUrl.empty());

    const auto noSources = parsePlaybackInfoOffer(R"({"MediaSources":[]})", -1, kSubtitleServerDefaultIndex);
    assert(!noSources.ok);
    assert(noSources.error == "Jellyfin returned no playable media source");

    const auto malformed = parsePlaybackInfoOffer("not-json", -1, kSubtitleServerDefaultIndex);
    assert(!malformed.ok);
    return 0;
}
