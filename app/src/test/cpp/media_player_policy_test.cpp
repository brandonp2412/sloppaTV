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
    assert(codecLevelAllowed(51, 0));
    assert(codecLevelAllowed(0, 41));
    assert(codecLevelAllowed(41, 41));
    assert(!codecLevelAllowed(51, 41));
    assert(hdrCapabilityAllowed(true, HdrOverrideMode::Auto));
    assert(!hdrCapabilityAllowed(false, HdrOverrideMode::Auto));
    assert(!hdrCapabilityAllowed(true, HdrOverrideMode::ForceSdr));
    assert(hdrCapabilityAllowed(false, HdrOverrideMode::AllowAllHdr));

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

    assert(static_cast<int>(StartupStep::StartPlayback) < static_cast<int>(StartupStep::ReadTrackMetadata));
    return 0;
}
