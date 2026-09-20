#include "player_completion_controller.hpp"

#include <utility>

PlayerCompletionEffects PlayerCompletionController::apply(SubtitleLoadCompletion& completion, bool activeGeneration,
                                                          PlaybackCoordinator& coordinator) {
    if (!activeGeneration || !coordinator.subtitleLoadMatches(completion.itemId, completion.requestedSubtitleIndex)) {
        return {};
    }

    PlayerCompletionEffects effects;
    if (completion.cues.empty()) {
        coordinator.failSubtitleLoad();
        effects.notice = PlayerCompletionNotice{.text = "SUBTITLES UNAVAILABLE FOR THIS FILE"};
        return effects;
    }

    PlayerCompletionDiagnostic diagnostic;
    diagnostic.kind = PlayerCompletionDiagnosticKind::SubtitleLoaded;
    diagnostic.itemId = completion.itemId;
    diagnostic.streamIndex = completion.loadedSubtitle.index;
    diagnostic.codec = completion.loadedSubtitle.codec;
    diagnostic.count = completion.cues.size();
    effects.diagnostic = std::move(diagnostic);
    coordinator.completeSubtitleLoad(completion.loadedSubtitle.index, completion.loadedSubtitle.language,
                                     std::move(completion.cues));
    effects.showSubtitleOverlay = true;
    effects.reportProgress = true;
    return effects;
}

PlayerCompletionEffects PlayerCompletionController::apply(MediaSegmentsCompletion& completion, bool playerScreen,
                                                          PlaybackCoordinator& coordinator) {
    if (!playerScreen) return {};

    PlayerCompletionEffects effects;
    if (!completion.ok) {
        if (coordinator.failMediaSegmentsRequest(completion.itemId, completion.completedAt)) {
            PlayerCompletionDiagnostic diagnostic;
            diagnostic.kind = PlayerCompletionDiagnosticKind::MediaSegmentsUnavailable;
            diagnostic.itemId = completion.itemId;
            diagnostic.error = completion.error;
            effects.diagnostic = std::move(diagnostic);
        }
        return effects;
    }

    const std::size_t segmentCount = completion.segments.size();
    if (!coordinator.completeMediaSegmentsRequest(completion.itemId, std::move(completion.segments))) return effects;
    PlayerCompletionDiagnostic diagnostic;
    diagnostic.kind = PlayerCompletionDiagnosticKind::MediaSegmentsLoaded;
    diagnostic.itemId = completion.itemId;
    diagnostic.count = segmentCount;
    effects.diagnostic = std::move(diagnostic);
    return effects;
}

PlayerCompletionEffects PlayerCompletionController::apply(NextEpisodeCompletion& completion, bool playerScreen,
                                                          PlaybackCoordinator& coordinator) {
    if (!playerScreen) return {};

    PlayerCompletionEffects effects;
    if (!completion.ok) {
        if (coordinator.failNextEpisodeRequest(completion.currentItemId, completion.completedAt)) {
            PlayerCompletionDiagnostic diagnostic;
            diagnostic.kind = PlayerCompletionDiagnosticKind::NextEpisodeUnavailable;
            diagnostic.itemId = completion.currentItemId;
            diagnostic.error = completion.error;
            effects.diagnostic = std::move(diagnostic);
        }
        return effects;
    }

    coordinator.completeNextEpisodeRequest(completion.currentItemId, std::move(completion.item));
    return effects;
}

PlayerCompletionEffects PlayerCompletionController::apply(PlaybackAdjacentCompletion& completion, bool playerScreen,
                                                          PlaybackCoordinator& coordinator) {
    PlayerCompletionEffects effects;
    const bool sameItem = coordinator.finishAdjacentEpisodeLookup(completion.currentItemId);
    if (!playerScreen || !sameItem) return effects;

    if (!completion.ok) {
        effects.notice = PlayerCompletionNotice{
            .text = "EPISODE LIST UNAVAILABLE",
            .duration = std::chrono::seconds(2),
        };
        return effects;
    }

    if (!completion.item) {
        effects.notice = PlayerCompletionNotice{
            .text = completion.direction < 0 ? "NO PREVIOUS EPISODE" : "NO NEXT EPISODE",
            .duration = std::chrono::seconds(2),
        };
        return effects;
    }

    effects.playItem = std::move(*completion.item);
    return effects;
}

const char* PlayerCompletionController::reportStage(PlaybackReportKind kind) {
    switch (kind) {
    case PlaybackReportKind::Start:
        return "start";
    case PlaybackReportKind::Progress:
        return "progress";
    case PlaybackReportKind::PausedProgress:
        return "paused-progress";
    case PlaybackReportKind::Stop:
        return "stop";
    }
    return "progress";
}

void PlayerCompletionController::apply(const PlaybackReportCompletion& completion, std::string_view server,
                                       std::string_view userId, bool playerScreen, PlaybackCoordinator& coordinator) {
    if (completion.kind != PlaybackReportKind::Stop || !completion.result.ok || playerScreen) return;
    if (server != completion.server || userId != completion.userId) return;
    coordinator.markPlaybackStopReported();
}
