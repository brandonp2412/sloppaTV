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

    // Exact-frame seeks have frozen the video decoder on some Android TV devices.
    // Playback seeks should use the platform's decoder-safe legacy/keyframe path.
    assert(seekApiForPlayback() == SeekApi::Legacy);
    assert(!forceAudioClockSync());
    assert(preferServerStreamForStartup("mkv", 1));
    assert(preferServerStreamForStartup("matroska", 50'000'000));
    assert(!preferServerStreamForStartup("mp4", 0));
    assert(initialPlayerSeekMs(true, 12'340'000) == 1234);
    assert(initialPlayerSeekMs(false, 12'340'000) == 1234);
    assert(seekStrategy(false) == SeekStrategy::RestartServerStream);
    assert(seekStrategy(true) == SeekStrategy::InPlace);
    assert(subtitleStrategy("pgssub") == SubtitleStrategy::ServerTranscode);
    assert(subtitleStrategy("ass") == SubtitleStrategy::ServerTranscode);

    assert(static_cast<int>(StartupStep::StartPlayback) < static_cast<int>(StartupStep::ReadTrackMetadata));
    return 0;
}
