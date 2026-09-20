#include "external_playback_state.hpp"

#include <cassert>
#include <limits>
#include <utility>

int main() {
    ExternalPlayerResult extremeResult;
    extremeResult.success = true;
    extremeResult.positionMs = std::numeric_limits<int64_t>::max();

    ExternalPlaybackLaunch boundedLaunch;
    boundedLaunch.item.runtimeTicks = 100'000;
    const ExternalPlaybackFinishPlan bounded = planExternalPlaybackFinish(boundedLaunch, extremeResult);
    assert(bounded.positionTicks == boundedLaunch.item.runtimeTicks);
    assert(bounded.updatedItem);
    assert(bounded.updatedItem->positionTicks == boundedLaunch.item.runtimeTicks);

    ExternalPlaybackLaunch unboundedLaunch;
    const ExternalPlaybackFinishPlan saturated = planExternalPlaybackFinish(unboundedLaunch, extremeResult);
    assert(saturated.positionTicks == std::numeric_limits<int64_t>::max());
    assert(saturated.updatedItem);
    assert(saturated.updatedItem->positionTicks == std::numeric_limits<int64_t>::max());

    ExternalPlaybackState state;
    assert(!state.hasPending());
    assert(!state.hasActive());

    ExternalPlaybackLaunch launch;
    launch.item.id = "episode-1";
    launch.player.packageName = "org.videolan.vlc";
    launch.player.label = "VLC";
    launch.url = "https://example.test/video";
    launch.subtitleUrl = "https://example.test/subtitle.srt";
    state.stage(launch);
    assert(state.hasPending());

    auto pending = state.takePending();
    assert(pending.has_value());
    assert(!state.hasPending());
    assert(pending->item.id == "episode-1");
    assert(pending->player.label == "VLC");
    assert(pending->url == launch.url);
    assert(pending->subtitleUrl == launch.subtitleUrl);

    state.beginActive(std::move(*pending));
    assert(state.hasActive());
    auto active = state.takeActive();
    assert(active.has_value());
    assert(!state.hasActive());
    assert(active->item.id == "episode-1");

    state.stage(launch);
    state.beginActive(launch);
    state.reset();
    assert(!state.hasPending());
    assert(!state.hasActive());
    return 0;
}
