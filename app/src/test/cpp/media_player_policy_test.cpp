#include "media_player_policy.hpp"

#include <cassert>
#include <cstdint>
#include <limits>

int main() {
    assert(clampSeekPositionMs(-1) == 0);
    assert(clampSeekPositionMs(0) == 0);
    assert(clampSeekPositionMs(1234) == 1234);
    assert(clampSeekPositionMs(std::numeric_limits<int64_t>::max()) == std::numeric_limits<int>::max());
    assert(playbackTicksFromPositionMs(-1) == 0);
    assert(playbackTicksFromPositionMs(1234) == 12'340'000);
    assert(playbackPositionMsFromTicks(-1) == 0);
    assert(playbackPositionMsFromTicks(12'340'000) == 1234);

    // Media3 accepts the logical resume position directly for both direct and
    // server-streamed playback; the old MediaPlayer/NuPlayer restart policy is gone.
    assert(initialPlayerSeekMs(12'340'000) == 1234);
    assert(subtitleStrategy("srt") == SubtitleStrategy::ClientText);
    assert(subtitleStrategy("ass") == SubtitleStrategy::ClientStyled);
    assert(subtitleStrategy("ssa") == SubtitleStrategy::ClientStyled);
    assert(subtitleStrategy("pgssub") == SubtitleStrategy::ServerTranscode);
    assert(preferExternalSubtitleDelivery(SubtitleStrategy::ClientStyled, true));
    assert(!preferExternalSubtitleDelivery(SubtitleStrategy::ClientStyled, false));
    assert(!preferExternalSubtitleDelivery(SubtitleStrategy::ClientText, true));
    assert(useNativeSubtitleRenderer(SubtitleStrategy::ClientStyled, false, true));
    assert(useNativeSubtitleRenderer(SubtitleStrategy::ClientStyled, true, true));
    assert(useNativeSubtitleRenderer(SubtitleStrategy::ClientText, false, true));
    assert(codecLevelAllowed(51, 0));
    assert(codecLevelAllowed(0, 41));
    assert(codecLevelAllowed(41, 41));
    assert(!codecLevelAllowed(51, 41));
    assert(hdrCapabilityAllowed(true, HdrOverrideMode::Auto));
    assert(!hdrCapabilityAllowed(false, HdrOverrideMode::Auto));
    assert(!hdrCapabilityAllowed(true, HdrOverrideMode::ForceSdr));
    assert(hdrCapabilityAllowed(false, HdrOverrideMode::AllowAllHdr));

    const auto autoBuffer = playbackBufferDurations(0);
    assert(!autoBuffer.custom());
    const auto largeBuffer = playbackBufferDurations(1);
    assert(largeBuffer.custom());
    assert(largeBuffer.minBufferMs == 50'000);
    assert(largeBuffer.maxBufferMs == 120'000);
    assert(largeBuffer.bufferForPlaybackMs == 2'500);
    assert(largeBuffer.bufferForPlaybackAfterRebufferMs == 5'000);
    const auto extraLargeBuffer = playbackBufferDurations(2);
    assert(extraLargeBuffer.custom());
    assert(extraLargeBuffer.minBufferMs == 80'000);
    assert(extraLargeBuffer.maxBufferMs == 240'000);
    assert(extraLargeBuffer.bufferForPlaybackMs == 5'000);
    assert(extraLargeBuffer.bufferForPlaybackAfterRebufferMs == 10'000);
    assert(!playbackBufferDurations(99).custom());

    assert(!shouldAutoplayNextEpisode(false, 0, 3));
    assert(shouldAutoplayNextEpisode(true, 0, 3));
    assert(shouldAutoplayNextEpisode(true, 2, 3));
    assert(!shouldAutoplayNextEpisode(true, 3, 3));
    assert(!shouldAutoplayNextEpisode(true, 4, 3));
    assert(!shouldAutoplayNextEpisode(true, 0, 0));
    assert(!shouldAutoplayNextEpisode(true, 0, -2));

    assert(transcodingReasonsFromUrl("/master.m3u8?TranscodeReasons=ContainerNotSupported") == "ContainerNotSupported");
    assert(transcodingReasonsFromUrl("/master.m3u8?x=1&TranscodeReasons=ContainerNotSupported%2CAudioCodecNotSupported&y=2")
        == "ContainerNotSupported,AudioCodecNotSupported");
    assert(transcodingUrlRepresentsDirectStream("/master.m3u8?TranscodeReasons=ContainerNotSupported"));
    assert(transcodingUrlRepresentsDirectStream("/master.m3u8?TranscodeReasons=ContainerNotSupported,AudioChannelsNotSupported"));
    assert(transcodingUrlRepresentsDirectStream("/master.m3u8?TranscodeReasons=AudioCodecNotSupported%2CVideoCodecTagNotSupported"));
    assert(!transcodingUrlRepresentsDirectStream("/master.m3u8?TranscodeReasons=SubtitleCodecNotSupported"));
    assert(!transcodingUrlRepresentsDirectStream("/master.m3u8?TranscodeReasons=VideoBitDepthNotSupported"));
    assert(!transcodingUrlRepresentsDirectStream("/master.m3u8?TranscodeReasons=ContainerNotSupported,SubtitleCodecNotSupported"));
    assert(!transcodingUrlRepresentsDirectStream("/master.m3u8?foo=bar"));

    assert(shouldAcceptPostSeekTelemetry(10'000, -1, 0));
    assert(shouldAcceptPostSeekTelemetry(20'800, 20'000, 50));
    assert(!shouldAcceptPostSeekTelemetry(10'000, 20'000, 100));
    assert(!shouldAcceptPostSeekTelemetry(10'000, 20'000, 749));
    assert(shouldAcceptPostSeekTelemetry(10'000, 20'000, 750));
    assert(shouldAcceptPostSeekTelemetry(30'000, 20'000, 900));

    assert(static_cast<int>(StartupStep::StartPlayback) < static_cast<int>(StartupStep::ReadTrackMetadata));
    return 0;
}
