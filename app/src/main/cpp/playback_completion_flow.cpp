#include "playback_completion_flow.hpp"

#include <chrono>
#include <utility>

using namespace std::chrono_literals;

PlaybackCompletionHostEffects PlaybackCompletionFlow::complete(AppQueuedPlaybackCompletion& completion,
                                                               bool activeGeneration, Screen screen) {
    const bool activeQueueContext = screen == completion.originScreen &&
                                    queueState_.currentIndex() == completion.previousQueueIndex &&
                                    queueState_.itemMatches(completion.index, completion.item.id);
    return apply(PlaybackCompletionController::apply(completion, activeGeneration, activeQueueContext,
                                                     screen == Screen::Player, queueState_, coordinator_));
}

PlaybackCompletionHostEffects PlaybackCompletionFlow::complete(PlayerItemPlaybackCompletion& completion,
                                                               bool activeGeneration, bool playerScreen) {
    return apply(PlaybackCompletionController::apply(completion, activeGeneration, playerScreen, coordinator_));
}

PlaybackCompletionHostEffects PlaybackCompletionFlow::complete(AutoplayPlaybackCompletion& completion,
                                                               bool activeGeneration) {
    return apply(PlaybackCompletionController::apply(completion, activeGeneration, queueState_, coordinator_));
}

PlaybackCompletionHostEffects PlaybackCompletionFlow::complete(StreamRestartCompletion& completion,
                                                               bool activeGeneration, bool playerScreen) {
    return apply(PlaybackCompletionController::apply(completion, activeGeneration, playerScreen, coordinator_));
}

PlaybackCompletionHostEffects PlaybackCompletionFlow::complete(FallbackPlaybackCompletion& completion,
                                                               bool activeGeneration, bool playerScreen) {
    return apply(PlaybackCompletionController::apply(completion, activeGeneration, playerScreen, coordinator_));
}

PlaybackCompletionHostEffects PlaybackCompletionFlow::complete(BeginPlaybackCompletion& completion,
                                                               bool activeGeneration, bool activeDetailsSelection) {
    return apply(
        PlaybackCompletionController::apply(completion, activeGeneration, activeDetailsSelection, queueState_));
}

PlaybackCompletionHostEffects PlaybackCompletionFlow::complete(SeriesPlayAllCompletion& completion,
                                                               bool activeGeneration, bool activeDetailsSelection) {
    return apply(
        PlaybackCompletionController::apply(completion, activeGeneration, activeDetailsSelection, queueState_));
}

PlaybackCompletionHostEffects PlaybackCompletionFlow::complete(SubtitleLoadCompletion& completion,
                                                               bool activeGeneration) {
    return apply(PlayerCompletionController::apply(completion, activeGeneration, coordinator_));
}

PlaybackCompletionHostEffects PlaybackCompletionFlow::complete(MediaSegmentsCompletion& completion, bool playerScreen) {
    return apply(PlayerCompletionController::apply(completion, playerScreen, coordinator_));
}

PlaybackCompletionHostEffects PlaybackCompletionFlow::complete(NextEpisodeCompletion& completion, bool playerScreen) {
    return apply(PlayerCompletionController::apply(completion, playerScreen, coordinator_));
}

PlaybackCompletionHostEffects PlaybackCompletionFlow::complete(PlaybackAdjacentCompletion& completion,
                                                               bool playerScreen) {
    return apply(PlayerCompletionController::apply(completion, playerScreen, coordinator_));
}

void PlaybackCompletionFlow::applyReport(const PlaybackReportCompletion& completion, const JellyfinSession& session,
                                         bool playerScreen) {
    PlayerCompletionController::apply(completion, session.server, session.userId, playerScreen, coordinator_);
}

PlaybackCompletionHostEffects PlaybackCompletionFlow::apply(PlaybackCompletionEffects effects) {
    PlaybackCompletionHostEffects host;
    host.finishLoading = effects.finishLoading;
    host.popToDetails = effects.popToDetails;
    host.stopPlayback = effects.stopPlayback;
    host.error = std::move(effects.error);
    host.restoreHomeVisibility = std::move(effects.restoreHomeVisibility);
    if (effects.detailUpdate) detailsFlow_.item() = std::move(*effects.detailUpdate);

    if (effects.transition) {
        auto transition = std::move(*effects.transition);
        switch (transition.kind) {
        case PlaybackCompletionTransitionKind::Resolved:
            coordinator_.stageResolvedPlayback(std::move(transition.target), std::move(transition.item));
            break;
        case PlaybackCompletionTransitionKind::StreamRestart:
            coordinator_.stageStreamRestart(std::move(transition.target), std::move(transition.item),
                                            transition.restartPaused, transition.audioStreamIndex);
            break;
        case PlaybackCompletionTransitionKind::ResolvedFallback:
            coordinator_.stageResolvedFallback(std::move(transition.target), std::move(transition.item),
                                               transition.audioStreamIndex);
            break;
        }
    }
    return host;
}

PlaybackCompletionHostEffects PlaybackCompletionFlow::apply(PlayerCompletionEffects effects) {
    PlaybackCompletionHostEffects host;
    host.notice = std::move(effects.notice);
    host.diagnostic = std::move(effects.diagnostic);
    host.reportProgress = effects.reportProgress;
    host.playItem = std::move(effects.playItem);
    if (effects.showSubtitleOverlay) playerScreenState_.showOverlayFor(std::chrono::steady_clock::now(), 4s);
    return host;
}
