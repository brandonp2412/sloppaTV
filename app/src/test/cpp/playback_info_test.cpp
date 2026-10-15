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
    bool hasPgsEmbed = false;
    bool hasPgsEncode = false;
    bool hasAssEmbed = false;
    for (const auto& subtitleProfile : body["DeviceProfile"]["SubtitleProfiles"]) {
        const std::string format = subtitleProfile.value("Format", std::string{});
        const std::string method = subtitleProfile.value("Method", std::string{});
        if ((format == "pgs" || format == "pgssub") && method == "Embed") hasPgsEmbed = true;
        if ((format == "pgs" || format == "pgssub") && method == "Encode") hasPgsEncode = true;
        if (format == "ass" && method == "Embed") hasAssEmbed = true;
    }
    assert(hasPgsEmbed);
    assert(!hasPgsEncode);
    assert(hasAssEmbed);

    PlaybackOverrides forceTranscode;
    forceTranscode.forceTranscode = true;
    const auto forced = json::parse(
        buildPlaybackInfoRequestBody(session, item, plan, forceTranscode, 500000, 2, -1, kSubtitleServerDefaultIndex));
    assert(forced["DeviceProfile"]["MaxStreamingBitrate"] == 1000000);
    assert(!forced["EnableDirectPlay"].get<bool>());
    assert(!forced["EnableDirectStream"].get<bool>());
    assert(forced["MediaSourceId"] == "source");
    assert(forced["AudioStreamIndex"].is_null());
    assert(!forced.contains("SubtitleStreamIndex"));

    PlaybackOverrides forceServerStream;
    forceServerStream.forceServerStream = true;
    const auto streamed =
        json::parse(buildPlaybackInfoRequestBody(session, item, plan, forceServerStream, 50000000, 6, 2, 5));
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
    assert(offer.value.subtitleDeliveryUrl == "/subtitle");

    const auto noisyOffer = parsePlaybackInfoOffer(R"({
        "PlaySessionId": null,
        "MediaSources": [
            null,
            {
                "Id": "usable-source",
                "Container": null,
                "DefaultAudioStreamIndex": null,
                "DefaultSubtitleStreamIndex": null,
                "SupportsDirectPlay": true,
                "SupportsDirectStream": null,
                "SupportsTranscoding": true,
                "TranscodingUrl": null,
                "MediaStreams": [null, {"Type":"Subtitle","Index":4,"Codec":null}]
            }
        ]
    })",
                                                   -1, kSubtitleServerDefaultIndex);
    assert(noisyOffer.ok);
    assert(noisyOffer.value.mediaSourceId == "usable-source");
    assert(noisyOffer.value.audioStreamIndex == -1);
    assert(noisyOffer.value.subtitleStreamIndex == kSubtitleOffIndex);
    assert(noisyOffer.value.transcodingUrl.empty());
    assert(noisyOffer.value.supportsDirectPlay);

    const auto alternateVersionOffer = parsePlaybackInfoOffer(R"({
        "MediaSources": [
            {
                "Id": "offline-version",
                "Container": "mkv",
                "SupportsDirectPlay": false,
                "SupportsDirectStream": false,
                "SupportsTranscoding": false
            },
            {
                "Id": "playable-version",
                "Container": "mp4",
                "DefaultAudioStreamIndex": 2,
                "SupportsDirectPlay": true
            }
        ]
    })",
                                                              -1, kSubtitleServerDefaultIndex);
    assert(alternateVersionOffer.ok);
    assert(alternateVersionOffer.value.mediaSourceId == "playable-version");
    assert(alternateVersionOffer.value.container == "mp4");
    assert(alternateVersionOffer.value.audioStreamIndex == 2);
    assert(alternateVersionOffer.value.supportsDirectPlay);
    const auto selectedVersionOffer = parsePlaybackInfoOffer(R"({
        "MediaSources": [
            {"Id":"first","Container":"mkv","SupportsDirectPlay":true},
            {"Id":"known-good","Container":"mp4","SupportsDirectPlay":true}
        ]
    })",
                                                             -1, kSubtitleServerDefaultIndex, {}, "known-good");
    assert(selectedVersionOffer.ok);
    assert(selectedVersionOffer.value.mediaSourceId == "known-good");
    assert(selectedVersionOffer.value.container == "mp4");
    assert(selectedVersionOffer.value.alternativeMediaSourceId.empty());
    const std::string threeVersionResponse = R"({
        "MediaSources": [
            {"Id":"slow", "SupportsDirectPlay":true},
            {"Id":"cached", "SupportsDirectPlay":true},
            {"Id":"backup", "SupportsDirectPlay":true},
            {"Id":"unavailable", "SupportsDirectPlay":false}
        ]
    })";
    const auto firstChoice = parsePlaybackInfoOffer(threeVersionResponse, -1, kSubtitleOffIndex);
    const auto secondChoice = parsePlaybackInfoOffer(threeVersionResponse, -1, kSubtitleOffIndex, {}, "cached");
    const auto lastChoice = parsePlaybackInfoOffer(threeVersionResponse, -1, kSubtitleOffIndex, {}, "backup");
    assert(firstChoice.ok && firstChoice.value.mediaSourceId == "slow" &&
           firstChoice.value.alternativeMediaSourceId == "cached");
    assert(firstChoice.value.sourceCount == 4 && firstChoice.value.directPlaySourceCount == 3);
    assert(secondChoice.ok && secondChoice.value.mediaSourceId == "cached" &&
           secondChoice.value.alternativeMediaSourceId == "backup");
    assert(lastChoice.ok && lastChoice.value.mediaSourceId == "backup" &&
           lastChoice.value.alternativeMediaSourceId.empty());
    // The fallback file can tag Italian as the default track despite the
    // failed (English) source using the same stream index for English.
    // Keep the server-probed source order even when advertised seeder hints differ.
    const auto seededOffer = parsePlaybackInfoOffer(R"({"MediaSources":[
        {"Id":"4k-zero","SupportsDirectPlay":true,"Remux":{"ProviderInfo":{"seeders":0}}},
        {"Id":"hd-healthy","SupportsDirectPlay":true,"Remux":{"ProviderInfo":{"seeders":46}}},
        {"Id":"hd-backup","SupportsDirectPlay":true,"Remux":{"ProviderInfo":{"seeders":2}}}
    ]})",
                                                    -1, kSubtitleOffIndex);
    assert(seededOffer.ok && seededOffer.value.mediaSourceId == "4k-zero");
    assert(seededOffer.value.alternativeMediaSourceId == "hd-healthy");
    const auto mixedSources = parsePlaybackInfoOffer(R"({"MediaSources":[
        {"Id":"debrid-cached","SupportsDirectPlay":true,"Remux":{"ProviderInfo":{"service_cached":true}}},
        {"Id":"torrent-seeded","SupportsDirectPlay":true,"Remux":{"ProviderInfo":{"seeders":82}}}
    ]})",
                                                     -1, kSubtitleOffIndex);
    assert(mixedSources.ok && mixedSources.value.mediaSourceId == "debrid-cached");
    assert(mixedSources.value.alternativeMediaSourceId == "torrent-seeded");
    const std::string multilingual = R"({"MediaSources":[{
        "Id":"fallback-file","SupportsDirectPlay":true,
        "DefaultAudioStreamIndex":1,
        "MediaStreams":[
            {"Type":"Video","Index":0,"Codec":"h264"},
            {"Type":"Audio","Index":1,"Codec":"eac3","Language":"ita","IsDefault":true,"Channels":2},
            {"Type":"Audio","Index":2,"Codec":"eac3","Language":"eng","IsDefault":false,"Channels":6},
            {"Type":"Subtitle","Index":3,"Language":"eng"}
        ]}]})";
    const auto english = parsePlaybackInfoOffer(multilingual, 1, kSubtitleOffIndex, {}, {}, "eng");
    assert(english.ok);
    assert(english.value.audioStreamIndex == 2);
    assert(english.value.audioOrdinal == 1);
    assert(english.value.audioTracks.size() == 2);
    assert(english.value.audioTracks[0].language == "ita");
    assert(english.value.audioTracks[1].language == "eng");
    const auto italian = parsePlaybackInfoOffer(multilingual, -1, kSubtitleOffIndex, {}, {}, "ita");
    assert(italian.value.audioStreamIndex == 1);
    assert(italian.value.audioOrdinal == 0);
    const auto unconfigured = parsePlaybackInfoOffer(multilingual, -1, kSubtitleOffIndex);
    assert(unconfigured.value.audioStreamIndex == 1);
    const auto absentVersionOffer = parsePlaybackInfoOffer(R"({
        "MediaSources": [
            {"Id":"first","SupportsDirectPlay":true},
            {"Id":"other","SupportsDirectPlay":true}
        ]
    })",
                                                           -1, kSubtitleServerDefaultIndex, {}, "not-present");
    assert(absentVersionOffer.ok);
    assert(absentVersionOffer.value.mediaSourceId == "first");
    assert(absentVersionOffer.value.alternativeMediaSourceId == "other");
    assert(alternateVersionOffer.value.alternativeMediaSourceId.empty());
    const auto incompatiblePreferred = parsePlaybackInfoOffer(R"({
        "MediaSources": [
            {"Id":"healthy","SupportsDirectPlay":true},
            {"Id":"preferred","SupportsDirectPlay":false,"SupportsDirectStream":false,"SupportsTranscoding":false}
        ]
    })",
                                                              -1, kSubtitleServerDefaultIndex, {}, "preferred");
    assert(incompatiblePreferred.ok);
    assert(incompatiblePreferred.value.mediaSourceId == "healthy");

    const auto preferDirectPlayOffer = parsePlaybackInfoOffer(R"({
        "MediaSources": [
            {
                "Id": "transcode-version",
                "Container": "mkv",
                "SupportsTranscoding": true,
                "TranscodingUrl": "/Videos/item/master.m3u8"
            },
            {
                "Id": "direct-version",
                "Container": "mp4",
                "SupportsDirectPlay": true
            }
        ]
    })",
                                                              -1, kSubtitleServerDefaultIndex);
    assert(preferDirectPlayOffer.ok);
    assert(preferDirectPlayOffer.value.mediaSourceId == "transcode-version");
    assert(!preferDirectPlayOffer.value.supportsDirectPlay);
    assert(!preferDirectPlayOffer.value.transcodingUrl.empty());

    const auto preferDirectStreamOffer = parsePlaybackInfoOffer(R"({
        "MediaSources": [
            {
                "Id": "transcode-version",
                "SupportsTranscoding": true,
                "TranscodingUrl": "/Videos/item/master.m3u8"
            },
            {
                "Id": "direct-stream-version",
                "SupportsDirectStream": true,
                "TranscodingUrl": "/Videos/item/directstream.m3u8?TranscodeReasons=ContainerNotSupported"
            }
        ]
    })",
                                                                -1, kSubtitleServerDefaultIndex);
    assert(preferDirectStreamOffer.ok);
    assert(preferDirectStreamOffer.value.mediaSourceId == "transcode-version");
    assert(!preferDirectStreamOffer.value.supportsDirectStream);
    assert(preferDirectStreamOffer.value.supportsTranscoding);

    PlaybackOverrides forceTranscodeOffer;
    forceTranscodeOffer.forceTranscode = true;
    const auto forcedTranscodeOffer = parsePlaybackInfoOffer(R"({
        "MediaSources": [
            {
                "Id": "direct-only-version",
                "Container": "mp4",
                "SupportsDirectPlay": true
            },
            {
                "Id": "transcode-version",
                "Container": "mkv",
                "SupportsTranscoding": true,
                "TranscodingUrl": "/Videos/item/transcode.m3u8?TranscodeReasons=VideoCodecNotSupported"
            }
        ]
    })",
                                                             -1, kSubtitleServerDefaultIndex, forceTranscodeOffer);
    assert(forcedTranscodeOffer.ok);
    assert(forcedTranscodeOffer.value.mediaSourceId == "transcode-version");
    assert(forcedTranscodeOffer.value.supportsTranscoding);
    assert(!forcedTranscodeOffer.value.transcodingUrl.empty());

    PlaybackOverrides forceServerStreamOffer;
    forceServerStreamOffer.forceServerStream = true;
    const auto forcedServerStream = parsePlaybackInfoOffer(R"({
        "MediaSources": [
            {
                "Id": "direct-only-version",
                "Container": "mp4",
                "SupportsDirectPlay": true
            },
            {
                "Id": "stream-version",
                "Container": "mkv",
                "SupportsDirectStream": true,
                "TranscodingUrl": "/Videos/item/directstream.m3u8?TranscodeReasons=ContainerNotSupported"
            }
        ]
    })",
                                                           -1, kSubtitleServerDefaultIndex, forceServerStreamOffer);
    assert(forcedServerStream.ok);
    assert(forcedServerStream.value.mediaSourceId == "stream-version");
    assert(forcedServerStream.value.supportsDirectStream);

    const auto missingDirectStreamUrl = parsePlaybackInfoOffer(R"({
        "MediaSources": [
            {
                "Id": "broken-stream-version",
                "SupportsDirectStream": true
            },
            {
                "Id": "working-transcode-version",
                "SupportsTranscoding": true,
                "TranscodingUrl": "/Videos/item/master.m3u8?TranscodeReasons=VideoCodecNotSupported"
            }
        ]
    })",
                                                               -1, kSubtitleServerDefaultIndex);
    assert(missingDirectStreamUrl.ok);
    assert(missingDirectStreamUrl.value.mediaSourceId == "working-transcode-version");
    assert(missingDirectStreamUrl.value.supportsTranscoding);

    const auto noSources = parsePlaybackInfoOffer(R"({"MediaSources":[]})", -1, kSubtitleServerDefaultIndex);
    assert(!noSources.ok);
    assert(noSources.error == "Jellyfin returned no playable media source");

    const auto remuxNoStreamsPlaceholder = parsePlaybackInfoOffer(R"({
        "MediaSources": [{
            "Id": "episode",
            "Name": "No streams found",
            "Path": "/videos/no-streams",
            "Container": "mp4",
            "SupportsDirectPlay": true,
            "SupportsDirectStream": true,
            "SupportsTranscoding": true
        }]
    })",
                                                                  -1, kSubtitleServerDefaultIndex);
    assert(!remuxNoStreamsPlaceholder.ok);
    assert(remuxNoStreamsPlaceholder.error == "Jellyfin returned no playable media source");

    const auto placeholderBeforeRealSource = parsePlaybackInfoOffer(R"({
        "MediaSources": [
            {
                "Id": "episode",
                "Name": "No streams found",
                "Path": "/videos/no-streams",
                "Container": "mp4",
                "SupportsDirectPlay": true
            },
            {
                "Id": "real-source",
                "Name": "Torrentio",
                "Path": "/stream/real-source",
                "Container": "mkv",
                "SupportsDirectPlay": true
            }
        ]
    })",
                                                                    -1, kSubtitleServerDefaultIndex);
    assert(placeholderBeforeRealSource.ok);
    assert(placeholderBeforeRealSource.value.mediaSourceId == "real-source");
    assert(placeholderBeforeRealSource.value.directPlaySourceCount == 1);
    const auto noObjectSources =
        parsePlaybackInfoOffer(R"({"MediaSources":[null,"bad"]})", -1, kSubtitleServerDefaultIndex);
    assert(!noObjectSources.ok);
    assert(noObjectSources.error == "Jellyfin returned no playable media source");

    const auto malformed = parsePlaybackInfoOffer("not-json", -1, kSubtitleServerDefaultIndex);
    assert(!malformed.ok);
    return 0;
}
