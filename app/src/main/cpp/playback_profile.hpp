#pragma once

#include "audio_policy.hpp"
#include "media_player_policy.hpp"

#include <algorithm>
#include <cctype>
#include <string>
#include <utility>
#include <vector>

struct PlaybackVideoCapabilityInput {
    std::string codec;
    std::string profile;
    std::string rangeType;
    int bitDepth = 0;
    int level = 0;
    int width = 0;
    int height = 0;
};

struct PlaybackDeviceCapabilityInput {
    std::vector<std::string> videoCodecs;
    std::vector<std::string> audioCodecs;
    std::vector<std::string> transcodeAudioCodecs;
    bool h264High10 = false;
    bool hevcMain10 = false;
    bool av1Main10 = false;
    int maxH264Width = 0;
    int maxH264Height = 0;
    int maxHevcWidth = 0;
    int maxHevcHeight = 0;
    int maxAv1Width = 0;
    int maxAv1Height = 0;
    bool displayHdr10 = false;
    bool displayHdr10Plus = false;
    bool displayDolbyVision = false;
    bool displayHlg = false;
    int maxAudioOutputChannels = 2;
};

struct PlaybackAudioCapabilityInput {
    bool selected = false;
    std::string codec;
    int channels = 0;
};

struct PlaybackSubtitleCapabilityInput {
    bool selected = false;
    std::string codec;
};

struct PlaybackCodecRejection {
    std::string codec;
    std::string reason;
};

struct PlaybackProfilePlan {
    std::vector<std::string> videoCodecs;
    std::vector<std::string> audioCodecs;
    std::vector<std::string> transcodeAudioCodecs;
    std::vector<PlaybackCodecRejection> videoRejections;
    int maxAudioChannels = 2;
    bool directVideoSupported = true;
    bool allowAudioStreamCopy = false;
    bool clientSubtitle = false;
    bool serverSubtitle = false;
};

struct PlaybackRequestFlags {
    bool enableDirectPlay = false;
    bool enableDirectStream = false;
    bool allowVideoStreamCopy = false;
    bool allowAudioStreamCopy = false;
};

enum class PlaybackServerRoute {
    Unavailable,
    DirectPlay,
    DirectStream,
    Transcode,
};

struct PlaybackServerOfferInput {
    bool supportsDirectPlay = false;
    bool supportsDirectStream = false;
    bool supportsTranscoding = false;
    std::string transcodingUrl;
};

inline std::string normalizedPlaybackValue(std::string value) {
    std::transform(value.begin(), value.end(), value.begin(), [](unsigned char c) {
        return static_cast<char>(std::tolower(c));
    });
    return value;
}

inline PlaybackProfilePlan makePlaybackProfilePlan(
    const PlaybackDeviceCapabilityInput& device,
    const PlaybackVideoCapabilityInput& video,
    const PlaybackAudioCapabilityInput& audio,
    const PlaybackSubtitleCapabilityInput& subtitle,
    int requestedAudioChannels,
    PlaybackOverrides overrides
) {
    PlaybackProfilePlan plan;
    plan.maxAudioChannels = effectiveAudioChannels(requestedAudioChannels, device.maxAudioOutputChannels);
    plan.videoCodecs = device.videoCodecs;
    plan.audioCodecs = device.audioCodecs;
    plan.transcodeAudioCodecs = device.transcodeAudioCodecs;

    auto rejectVideoCodec = [&](const std::string& codec, std::string reason) {
        const auto before = plan.videoCodecs.size();
        std::erase(plan.videoCodecs, codec);
        if (plan.videoCodecs.size() == before) return;
        plan.directVideoSupported = false;
        plan.videoRejections.push_back({codec, std::move(reason)});
    };

    const std::string codec = normalizedPlaybackValue(video.codec);
    const std::string profile = normalizedPlaybackValue(video.profile);
    const std::string range = normalizedPlaybackValue(video.rangeType);
    const bool dolbyVision = range.find("dovi") != std::string::npos;
    const bool hdr10Plus = range.find("hdr10plus") != std::string::npos || range.find("hdr10+") != std::string::npos;
    const bool hdr10 = !hdr10Plus && range.find("hdr10") != std::string::npos;
    const bool hlg = range.find("hlg") != std::string::npos;
    const bool unsupportedHdr = (dolbyVision && !hdrCapabilityAllowed(device.displayDolbyVision, overrides.hdrMode))
        || (!dolbyVision && hdr10Plus && !hdrCapabilityAllowed(device.displayHdr10Plus, overrides.hdrMode))
        || (!dolbyVision && hdr10 && !hdrCapabilityAllowed(device.displayHdr10, overrides.hdrMode))
        || (!dolbyVision && hlg && !hdrCapabilityAllowed(device.displayHlg, overrides.hdrMode));

    if (codec == "hevc") {
        if (!codecLevelAllowed(video.level, overrides.maxHevcLevel)) rejectVideoCodec("hevc", "level exceeds user override");
        if ((video.bitDepth > 8 || profile.find("main 10") != std::string::npos) && !device.hevcMain10) {
            rejectVideoCodec("hevc", "HEVC Main10 unsupported");
        }
        if (device.maxHevcWidth > 0 && (video.width > device.maxHevcWidth || video.height > device.maxHevcHeight)) {
            rejectVideoCodec("hevc", "resolution exceeds decoder capability");
        }
        if (unsupportedHdr) rejectVideoCodec("hevc", "HDR range unsupported by connected display");
    } else if (codec == "h264") {
        if (!codecLevelAllowed(video.level, overrides.maxAvcLevel)) rejectVideoCodec("h264", "level exceeds user override");
        if (profile.find("high 10") != std::string::npos && !device.h264High10) rejectVideoCodec("h264", "H.264 High10 unsupported");
        if (device.maxH264Width > 0 && (video.width > device.maxH264Width || video.height > device.maxH264Height)) {
            rejectVideoCodec("h264", "resolution exceeds decoder capability");
        }
        if (unsupportedHdr) rejectVideoCodec("h264", "HDR range unsupported by connected display");
    } else if (codec == "av1") {
        if (video.bitDepth > 8 && !device.av1Main10) rejectVideoCodec("av1", "AV1 Main10 unsupported");
        if (device.maxAv1Width > 0 && (video.width > device.maxAv1Width || video.height > device.maxAv1Height)) {
            rejectVideoCodec("av1", "resolution exceeds decoder capability");
        }
        if (unsupportedHdr) rejectVideoCodec("av1", "HDR range unsupported by connected display");
    } else if ((codec == "vp9" || codec == "vp8") && unsupportedHdr) {
        rejectVideoCodec(codec, "HDR range unsupported by connected display");
    }

    if (plan.videoCodecs.empty()) plan.videoCodecs.emplace_back("h264");
    if (plan.audioCodecs.empty()) plan.audioCodecs.emplace_back("aac");
    if (plan.transcodeAudioCodecs.empty()) plan.transcodeAudioCodecs.emplace_back("aac");

    plan.allowAudioStreamCopy = audio.selected
        && audioStreamCopyAllowed(plan.audioCodecs, audio.codec, audio.channels, plan.maxAudioChannels);
    plan.clientSubtitle = subtitle.selected && subtitleStrategy(subtitle.codec) != SubtitleStrategy::ServerTranscode;
    plan.serverSubtitle = subtitle.selected && !plan.clientSubtitle;
    return plan;
}

inline PlaybackRequestFlags playbackRequestFlags(const PlaybackProfilePlan& plan, PlaybackOverrides overrides) {
    const bool streamCopyAllowed = !overrides.forceTranscode && plan.directVideoSupported && !plan.serverSubtitle;
    return {
        .enableDirectPlay = streamCopyAllowed && !overrides.forceServerStream,
        .enableDirectStream = streamCopyAllowed,
        .allowVideoStreamCopy = streamCopyAllowed,
        .allowAudioStreamCopy = plan.allowAudioStreamCopy,
    };
}

inline PlaybackServerRoute choosePlaybackServerRoute(
    const PlaybackServerOfferInput& offer,
    PlaybackOverrides overrides
) {
    if (!overrides.forceTranscode && !overrides.forceServerStream && offer.supportsDirectPlay) {
        return PlaybackServerRoute::DirectPlay;
    }
    if (offer.transcodingUrl.empty()) return PlaybackServerRoute::Unavailable;
    if (overrides.forceTranscode) {
        return offer.supportsTranscoding ? PlaybackServerRoute::Transcode : PlaybackServerRoute::Unavailable;
    }
    if (!offer.supportsDirectStream && !offer.supportsTranscoding) return PlaybackServerRoute::Unavailable;
    if (offer.supportsDirectStream || transcodingUrlRepresentsDirectStream(offer.transcodingUrl)) {
        return PlaybackServerRoute::DirectStream;
    }
    return PlaybackServerRoute::Transcode;
}
