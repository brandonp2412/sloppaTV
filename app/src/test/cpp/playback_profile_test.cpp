#include "playback_profile.hpp"

#include <algorithm>
#include <cassert>

namespace {

PlaybackDeviceCapabilityInput capableDevice() {
    return {
        .videoCodecs = {"av1", "hevc", "h264", "vp9"},
        .audioCodecs = {"aac", "ac3", "eac3"},
        .transcodeAudioCodecs = {"aac", "ac3"},
        .h264High10 = false,
        .hevcMain10 = true,
        .av1Main10 = true,
        .maxH264Width = 3840,
        .maxH264Height = 2160,
        .maxHevcWidth = 3840,
        .maxHevcHeight = 2160,
        .maxAv1Width = 3840,
        .maxAv1Height = 2160,
        .displayHdr10 = true,
        .displayHdr10Plus = false,
        .displayDolbyVision = false,
        .displayHlg = true,
        .maxAudioOutputChannels = 8,
    };
}

bool containsCodec(const std::vector<std::string>& codecs, const std::string& codec) {
    return std::find(codecs.begin(), codecs.end(), codec) != codecs.end();
}

}

int main() {
    auto device = capableDevice();
    const PlaybackVideoCapabilityInput hevcHdr{
        .codec = "HEVC",
        .profile = "Main 10",
        .rangeType = "HDR10",
        .bitDepth = 10,
        .level = 153,
        .width = 3840,
        .height = 2160,
    };
    const PlaybackAudioCapabilityInput surround{
        .selected = true,
        .codec = "EAC3",
        .channels = 6,
    };

    auto plan = makePlaybackProfilePlan(device, hevcHdr, surround, {}, 8, {});
    assert(plan.directVideoSupported);
    assert(containsCodec(plan.videoCodecs, "hevc"));
    assert(plan.maxAudioChannels == 8);
    assert(plan.allowAudioStreamCopy);
    assert(plan.videoRejections.empty());

    PlaybackOverrides hevcLimit;
    hevcLimit.maxHevcLevel = 150;
    plan = makePlaybackProfilePlan(device, hevcHdr, surround, {}, 8, hevcLimit);
    assert(!plan.directVideoSupported);
    assert(!containsCodec(plan.videoCodecs, "hevc"));
    assert(plan.videoRejections.size() == 1);
    assert(plan.videoRejections.front().reason == "level exceeds user override");

    auto noHdrDisplay = device;
    noHdrDisplay.displayHdr10 = false;
    plan = makePlaybackProfilePlan(noHdrDisplay, hevcHdr, surround, {}, 8, {});
    assert(!plan.directVideoSupported);
    assert(!containsCodec(plan.videoCodecs, "hevc"));

    PlaybackOverrides allowHdr;
    allowHdr.hdrMode = HdrOverrideMode::AllowAllHdr;
    plan = makePlaybackProfilePlan(noHdrDisplay, hevcHdr, surround, {}, 8, allowHdr);
    assert(plan.directVideoSupported);
    assert(containsCodec(plan.videoCodecs, "hevc"));

    const PlaybackVideoCapabilityInput high10{
        .codec = "h264",
        .profile = "High 10",
        .rangeType = "",
        .bitDepth = 10,
        .level = 51,
        .width = 1920,
        .height = 1080,
    };
    plan = makePlaybackProfilePlan(device, high10, surround, {}, 8, {});
    assert(!plan.directVideoSupported);
    assert(!containsCodec(plan.videoCodecs, "h264"));

    auto stereoRoute = device;
    stereoRoute.maxAudioOutputChannels = 2;
    plan = makePlaybackProfilePlan(stereoRoute, {}, surround, {}, 8, {});
    assert(plan.maxAudioChannels == 2);
    assert(!plan.allowAudioStreamCopy);

    const PlaybackSubtitleCapabilityInput textSubtitle{.selected = true, .codec = "srt"};
    plan = makePlaybackProfilePlan(device, {}, {}, textSubtitle, 8, {});
    assert(plan.clientSubtitle);
    assert(!plan.serverSubtitle);

    const PlaybackSubtitleCapabilityInput imageSubtitle{.selected = true, .codec = "pgs"};
    plan = makePlaybackProfilePlan(device, {}, {}, imageSubtitle, 8, {});
    assert(!plan.clientSubtitle);
    assert(plan.serverSubtitle);
    auto flags = playbackRequestFlags(plan, {});
    assert(!flags.enableDirectPlay);
    assert(!flags.enableDirectStream);
    assert(!flags.allowVideoStreamCopy);

    plan = makePlaybackProfilePlan(device, hevcHdr, surround, {}, 8, {});
    flags = playbackRequestFlags(plan, {});
    assert(flags.enableDirectPlay);
    assert(flags.enableDirectStream);
    assert(flags.allowVideoStreamCopy);
    assert(flags.allowAudioStreamCopy);
    PlaybackOverrides forced;
    forced.forceTranscode = true;
    flags = playbackRequestFlags(plan, forced);
    assert(!flags.enableDirectPlay);
    assert(!flags.enableDirectStream);
    assert(!flags.allowVideoStreamCopy);
    assert(flags.allowAudioStreamCopy);

    PlaybackServerOfferInput offer{
        .supportsDirectPlay = true,
        .supportsDirectStream = true,
        .supportsTranscoding = true,
        .transcodingUrl = "/Videos/item/master.m3u8?TranscodeReasons=ContainerNotSupported",
    };
    assert(choosePlaybackServerRoute(offer, {}) == PlaybackServerRoute::DirectPlay);
    offer.supportsDirectPlay = false;
    offer.supportsDirectStream = false;
    assert(choosePlaybackServerRoute(offer, {}) == PlaybackServerRoute::DirectStream);
    offer.transcodingUrl = "/Videos/item/master.m3u8?TranscodeReasons=VideoCodecNotSupported";
    assert(choosePlaybackServerRoute(offer, {}) == PlaybackServerRoute::Transcode);
    assert(choosePlaybackServerRoute(offer, forced) == PlaybackServerRoute::Transcode);
    offer.supportsTranscoding = false;
    offer.supportsDirectStream = true;
    assert(choosePlaybackServerRoute(offer, forced) == PlaybackServerRoute::Unavailable);
    offer.transcodingUrl.clear();
    assert(choosePlaybackServerRoute(offer, {}) == PlaybackServerRoute::Unavailable);

    PlaybackDeviceCapabilityInput sparse;
    sparse.maxAudioOutputChannels = 2;
    plan = makePlaybackProfilePlan(sparse, {}, {}, {}, 8, {});
    assert(plan.videoCodecs == std::vector<std::string>{"h264"});
    assert(plan.audioCodecs == std::vector<std::string>{"aac"});
    assert(plan.transcodeAudioCodecs == std::vector<std::string>{"aac"});
    return 0;
}
