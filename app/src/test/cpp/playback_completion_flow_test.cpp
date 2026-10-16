#include "playback_completion_flow.hpp"

#include <cassert>

int main() {
    PlaybackCoordinator coordinator;
    PlaybackQueueState queue;
    DetailsFlow details;
    PlayerScreenState player;
    PlaybackCompletionFlow flow(coordinator, queue, details, player);
    JellyfinItem series;
    series.id = "series";
    series.type = "Series";

    // Cancelling the initial player invalidates its generation; late results
    // must leave a subsequent playback request untouched.
    coordinator.beginPlaybackResolution(true);
    BeginPlaybackCompletion stale;
    auto ignored = flow.complete(stale, false, true);
    assert(!ignored.finishLoading);
    assert(coordinator.transitionLoading());

    coordinator.session().setActive(series, {});
    BeginPlaybackCompletion failure;
    failure.result.error = "Remux unavailable";
    auto failed = flow.complete(failure, true, true);
    assert(failed.finishLoading);
    assert(failed.popToDetails);
    assert(failed.error == "Remux unavailable");
    assert(!coordinator.transitionLoading());
    assert(!coordinator.activeItemAvailable());

    coordinator.beginPlaybackResolution(true);
    SeriesPlayAllCompletion allFailure;
    allFailure.error = "No episodes available";
    auto allFailed = flow.complete(allFailure, true, true);
    assert(allFailed.popToDetails);
    assert(!coordinator.transitionLoading());

    coordinator.beginPlaybackResolution(true);
    BeginPlaybackCompletion success;
    success.result.ok = true;
    success.result.value.url = "https://example.test/stream";
    success.playable.id = "episode";
    success.playable.type = "Episode";
    auto started = flow.complete(success, true, true);
    assert(started.finishLoading);
    assert(!started.popToDetails);
    assert(!coordinator.transitionLoading());
    auto transition = coordinator.takePendingTransition();
    assert(transition);
    assert(transition->item.id == "episode");
    assert(transition->target.url == "https://example.test/stream");
}
