#include "playback_completion_controller.hpp"

namespace {
PlaybackHomeVisibilityRestore visibilityRestore(const JellyfinItem& item) {
    return {
        .itemId = item.id,
        .seriesId = item.seriesId,
    };
}

PlaybackCompletionTransition resolvedTransition(PlaybackTarget target, JellyfinItem item) {
    return {
        .kind = PlaybackCompletionTransitionKind::Resolved,
        .target = std::move(target),
        .item = std::move(item),
    };
}
} // namespace

PlaybackCompletionEffects PlaybackCompletionController::apply(PlayerItemPlaybackCompletion& completion,
                                                              bool activeGeneration, bool playerScreen,
                                                              PlaybackCoordinator& coordinator) {
    if (!activeGeneration) return {};

    PlaybackCompletionEffects effects;
    effects.finishLoading = true;
    coordinator.finishPlaybackResolution();
    if (!playerScreen) return effects;

    if (!completion.result.ok) {
        effects.error = "EPISODE: " + completion.result.error;
        return effects;
    }

    effects.detailUpdate = completion.item;
    effects.transition = resolvedTransition(std::move(completion.result.value), std::move(completion.item));
    return effects;
}

PlaybackCompletionEffects PlaybackCompletionController::apply(AutoplayPlaybackCompletion& completion,
                                                              bool activeGeneration, PlaybackQueueState& queue,
                                                              PlaybackCoordinator& coordinator) {
    if (!activeGeneration) return {};

    PlaybackCompletionEffects effects;
    effects.finishLoading = true;
    coordinator.finishPlaybackResolution();
    if (!completion.result.ok) {
        effects.popToDetails = true;
        effects.error = "NEXT EPISODE: " + completion.result.error;
        return effects;
    }

    if (completion.queuedNextIndex >= 0 && queue.currentIndex() + 1 == completion.queuedNextIndex &&
        queue.itemMatches(completion.queuedNextIndex, completion.item.id)) {
        queue.setCurrentIndex(completion.queuedNextIndex);
        queue.setItemAt(completion.queuedNextIndex, completion.item);
    }

    effects.transition = resolvedTransition(std::move(completion.result.value), std::move(completion.item));
    return effects;
}

PlaybackCompletionEffects PlaybackCompletionController::apply(StreamRestartCompletion& completion,
                                                              bool activeGeneration, bool playerScreen,
                                                              PlaybackCoordinator& coordinator) {
    if (!activeGeneration) return {};

    PlaybackCompletionEffects effects;
    const bool sameItem = coordinator.completeStreamRestartRequest(completion.item.id);
    if (!playerScreen || !sameItem) return effects;

    if (!completion.result.ok) {
        effects.error = completion.result.error;
        return effects;
    }

    effects.transition = PlaybackCompletionTransition{
        .kind = PlaybackCompletionTransitionKind::StreamRestart,
        .target = std::move(completion.result.value),
        .item = std::move(completion.item),
        .restartPaused = completion.wasPaused,
        .audioStreamIndex = completion.audioStreamIndex,
    };
    return effects;
}

PlaybackCompletionEffects PlaybackCompletionController::apply(FallbackPlaybackCompletion& completion,
                                                              bool activeGeneration, bool playerScreen,
                                                              PlaybackCoordinator& coordinator) {
    if (!activeGeneration) return {};

    PlaybackCompletionEffects effects;
    effects.finishLoading = true;
    const bool sameItem = coordinator.completeFallbackResolution(completion.item.id);
    if (!playerScreen || !sameItem) return effects;

    if (!completion.result.ok) {
        effects.error = "TRANSCODE FALLBACK: " + completion.result.error;
        effects.stopPlayback = true;
        return effects;
    }

    effects.transition = PlaybackCompletionTransition{
        .kind = PlaybackCompletionTransitionKind::ResolvedFallback,
        .target = std::move(completion.result.value),
        .item = std::move(completion.item),
        .audioStreamIndex = completion.audioStreamIndex,
    };
    return effects;
}

PlaybackCompletionEffects PlaybackCompletionController::apply(BeginPlaybackCompletion& completion,
                                                              bool activeGeneration, bool activeDetailsSelection,
                                                              PlaybackQueueState& queue) {
    if (!activeGeneration) return {};

    PlaybackCompletionEffects effects;
    effects.finishLoading = true;
    if (!activeDetailsSelection) return effects;

    if (!completion.result.ok) {
        effects.error = completion.result.error;
        return effects;
    }

    if (completion.queuedPlaybackIndex >= 0 &&
        queue.itemMatches(completion.queuedPlaybackIndex, completion.playable.id)) {
        queue.setCurrentIndex(completion.queuedPlaybackIndex);
        queue.setItemAt(completion.queuedPlaybackIndex, completion.playable);
    }

    effects.restoreHomeVisibility.push_back(visibilityRestore(completion.selected));
    effects.restoreHomeVisibility.push_back(visibilityRestore(completion.playable));
    effects.transition = resolvedTransition(std::move(completion.result.value), std::move(completion.playable));
    return effects;
}

PlaybackCompletionEffects PlaybackCompletionController::apply(SeriesPlayAllCompletion& completion,
                                                              bool activeGeneration, bool activeDetailsSelection,
                                                              PlaybackQueueState& queue) {
    if (!activeGeneration) return {};

    PlaybackCompletionEffects effects;
    effects.finishLoading = true;
    if (!activeDetailsSelection) return effects;

    if (!completion.error.empty()) {
        effects.error = std::move(completion.error);
        return effects;
    }
    if (!completion.target) return effects;

    queue.replace(std::move(completion.episodes), 0);
    queue.setItemAt(0, completion.first);
    effects.restoreHomeVisibility.push_back(visibilityRestore(completion.series));
    effects.restoreHomeVisibility.push_back(visibilityRestore(completion.first));
    effects.transition = resolvedTransition(std::move(*completion.target), std::move(completion.first));
    return effects;
}
