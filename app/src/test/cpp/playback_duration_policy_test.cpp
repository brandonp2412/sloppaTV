#include "playback_duration_policy.hpp"

#include <cassert>

int main() {
    // Real media file runtimes versus rounded Jellyfin item metadata.
    assert(preferredPlaybackDurationMs(2460000, 2573988, true) == 2573988);
    assert(preferredPlaybackDurationMs(2520000, 2638293, true) == 2638293);
    // Do not allow a transcoded or DirectStream segment to replace episode duration.
    assert(preferredPlaybackDurationMs(2460000, 90000, false) == 2460000);
    assert(preferredPlaybackDurationMs(2460000, 0, true) == 2460000);
    assert(preferredPlaybackDurationMs(0, 2573988, true) == 2573988);
    assert(preferredPlaybackDurationMs(-1, 0, false) == 0);
    // Avoid tiny runtime jitter when metadata is already accurate.
    assert(preferredPlaybackDurationMs(2573988, 2574200, true) == 2573988);
}
