#include "playback_completion_controller.hpp"

#include <cassert>
#include <string>
#include <utility>
#include <vector>

namespace {
JellyfinItem item(std::string id, std::string type = "Episode") {
    JellyfinItem value;
    value.id = std::move(id);
    value.name = value.id;
    value.type = std::move(type);
    return value;
}

PlaybackTarget target(std::string url) {
    PlaybackTarget value;
    value.url = std::move(url);
    value.playMethod = PlaybackMethod::DirectPlay;
    return value;
}

PlaybackCoordinator coordinatorFor(const JellyfinItem& active) {
    PlaybackCoordinator coordinator;
    coordinator.activate(active, target("https://media.example/" + active.id),
                         PlaybackCoordinator::Clock::now());
    return coordinator;
}

void assertNoEffects(const PlaybackCompletionEffects& effects) {
    assert(!effects.finishLoading);
    assert(!effects.popToDetails);
    assert(!effects.stopPlayback);
    assert(!effects.error);
    assert(!effects.detailUpdate);
    assert(effects.restoreHomeVisibility.empty());
    assert(!effects.transition);
}
} // namespace

int main() {
    {
        PlaybackCoordinator coordinator;
        coordinator.beginPlaybackResolution(true);
        PlaybackQueueState queue;
        queue.replace({item("episode-1")}, 0);

        QueuedPlaybackResolutionCompletion<int> completion;
        completion.generation = 3;
        completion.originScreen = 7;
        completion.index = 0;
        completion.previousQueueIndex = 0;
        completion.item = item("episode-1");
        completion.result.ok = true;
        completion.result.value = target("https://media.example/episode-1");

        const auto effects =
            PlaybackCompletionController::apply(completion, false, true, false, queue, coordinator);
        assertNoEffects(effects);
        assert(coordinator.transitionLoading());
        assert(queue.currentIndex() == 0);
    }

    {
        PlaybackCoordinator coordinator;
        coordinator.beginPlaybackResolution(true);
        PlaybackQueueState queue;
        queue.replace({item("episode-1"), item("episode-2")}, 0);

        QueuedPlaybackResolutionCompletion<int> completion;
        completion.originScreen = 7;
        completion.index = 1;
        completion.previousQueueIndex = 0;
        completion.replacingPlayer = true;
        completion.item = item("episode-2");
        completion.result.ok = false;
        completion.result.error = "resolve failed";

        const auto effects = PlaybackCompletionController::apply(completion, true, true, true, queue, coordinator);
        assert(effects.finishLoading);
        assert(effects.popToDetails);
        assert(effects.error == "QUEUE: resolve failed");
        assert(!effects.transition);
        assert(!coordinator.transitionLoading());
        assert(queue.currentIndex() == 0);
    }

    {
        PlaybackCoordinator coordinator;
        coordinator.beginPlaybackResolution(true);
        PlaybackQueueState queue;
        queue.replace({item("episode-1"), item("episode-2")}, 0);

        QueuedPlaybackResolutionCompletion<int> completion;
        completion.index = 1;
        completion.previousQueueIndex = 0;
        completion.item = item("episode-2");
        completion.item.name = "Detailed episode";
        completion.result.ok = true;
        completion.result.value = target("https://media.example/resolved");

        const auto effects = PlaybackCompletionController::apply(completion, true, true, false, queue, coordinator);
        assert(effects.finishLoading);
        assert(!effects.error);
        assert(effects.transition);
        assert(effects.transition->kind == PlaybackCompletionTransitionKind::Resolved);
        assert(effects.transition->target.url == "https://media.example/resolved");
        assert(effects.transition->item.id == "episode-2");
        assert(queue.currentIndex() == 1);
        assert(queue.itemAt(1)->name == "Detailed episode");
    }

    {
        PlaybackCoordinator coordinator;
        coordinator.beginPlaybackResolution(true);

        PlayerItemPlaybackCompletion completion;
        completion.item = item("episode-3");
        completion.result.ok = true;
        completion.result.value = target("https://media.example/episode-3");

        const auto effects = PlaybackCompletionController::apply(completion, true, true, coordinator);
        assert(effects.finishLoading);
        assert(effects.detailUpdate);
        assert(effects.detailUpdate->id == "episode-3");
        assert(effects.transition);
        assert(effects.transition->item.id == "episode-3");
        assert(!coordinator.transitionLoading());
    }

    {
        PlaybackCoordinator coordinator;
        coordinator.beginPlaybackResolution(true);
        PlaybackQueueState queue;

        AutoplayPlaybackCompletion completion;
        completion.item = item("episode-4");
        completion.result.ok = false;
        completion.result.error = "autoplay failed";

        const auto effects = PlaybackCompletionController::apply(completion, true, queue, coordinator);
        assert(effects.finishLoading);
        assert(effects.popToDetails);
        assert(effects.error == "NEXT EPISODE: autoplay failed");
        assert(!effects.transition);
    }

    {
        PlaybackCoordinator coordinator;
        coordinator.beginPlaybackResolution(true);
        PlaybackQueueState queue;
        queue.replace({item("episode-1"), item("episode-2")}, 0);

        AutoplayPlaybackCompletion completion;
        completion.queuedNextIndex = 1;
        completion.item = item("episode-2");
        completion.item.name = "Resolved next";
        completion.result.ok = true;
        completion.result.value = target("https://media.example/next");

        const auto effects = PlaybackCompletionController::apply(completion, true, queue, coordinator);
        assert(effects.transition);
        assert(queue.currentIndex() == 1);
        assert(queue.itemAt(1)->name == "Resolved next");
    }

    {
        JellyfinItem active = item("episode-5");
        PlaybackCoordinator coordinator = coordinatorFor(active);

        StreamRestartCompletion completion;
        completion.item = active;
        completion.wasPaused = true;
        completion.audioStreamIndex = 4;
        completion.result.ok = true;
        completion.result.value = target("https://media.example/restart");

        const auto effects = PlaybackCompletionController::apply(completion, true, true, coordinator);
        assert(!effects.finishLoading);
        assert(effects.transition);
        assert(effects.transition->kind == PlaybackCompletionTransitionKind::StreamRestart);
        assert(effects.transition->restartPaused);
        assert(effects.transition->audioStreamIndex == 4);
    }

    {
        JellyfinItem active = item("episode-6");
        PlaybackCoordinator coordinator = coordinatorFor(active);
        coordinator.transition().setFallbackResolving(true);

        FallbackPlaybackCompletion completion;
        completion.item = active;
        completion.result.ok = false;
        completion.result.error = "fallback failed";

        const auto effects = PlaybackCompletionController::apply(completion, true, true, coordinator);
        assert(effects.finishLoading);
        assert(effects.stopPlayback);
        assert(effects.error == "TRANSCODE FALLBACK: fallback failed");
        assert(!effects.transition);
        assert(!coordinator.fallbackResolving());
    }

    {
        PlaybackQueueState queue;
        JellyfinItem queued = item("episode-7");
        queue.replace({queued}, 0);

        BeginPlaybackCompletion completion;
        completion.queuedPlaybackIndex = 0;
        completion.selected = item("series-1", "Series");
        completion.selected.seriesId = "series-parent";
        completion.playable = queued;
        completion.playable.name = "Resolved playable";
        completion.playable.seriesId = "series-1";
        completion.result.ok = true;
        completion.result.value = target("https://media.example/begin");

        const auto effects = PlaybackCompletionController::apply(completion, true, true, queue);
        assert(effects.finishLoading);
        assert(effects.restoreHomeVisibility.size() == 2);
        assert(effects.restoreHomeVisibility[0].itemId == "series-1");
        assert(effects.restoreHomeVisibility[0].seriesId == "series-parent");
        assert(effects.restoreHomeVisibility[1].itemId == "episode-7");
        assert(effects.restoreHomeVisibility[1].seriesId == "series-1");
        assert(effects.transition);
        assert(queue.itemAt(0)->name == "Resolved playable");
    }

    {
        PlaybackQueueState queue;
        queue.replace({item("old-episode")}, 0);

        SeriesPlayAllCompletion completion;
        completion.series = item("series-2", "Series");
        completion.episodes = {item("episode-8"), item("episode-9")};
        completion.first = completion.episodes.front();
        completion.first.seriesId = "series-2";
        completion.target = target("https://media.example/play-all");

        const auto effects = PlaybackCompletionController::apply(completion, true, true, queue);
        assert(effects.finishLoading);
        assert(!effects.error);
        assert(queue.size() == 2);
        assert(queue.currentIndex() == 0);
        assert(queue.itemAt(0)->id == "episode-8");
        assert(effects.restoreHomeVisibility.size() == 2);
        assert(effects.transition);
        assert(effects.transition->item.id == "episode-8");
        assert(effects.transition->target.url == "https://media.example/play-all");
    }

    {
        PlaybackQueueState queue;
        queue.replace({item("existing")}, 0);

        SeriesPlayAllCompletion completion;
        completion.series = item("series-3", "Series");
        completion.error = "PLAY ALL: failed";

        const auto effects = PlaybackCompletionController::apply(completion, true, true, queue);
        assert(effects.finishLoading);
        assert(effects.error == "PLAY ALL: failed");
        assert(queue.size() == 1);
        assert(queue.itemAt(0)->id == "existing");
        assert(!effects.transition);
    }

    return 0;
}
