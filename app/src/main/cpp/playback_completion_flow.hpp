#pragma once

#include "app_screen.hpp"
#include "details_flow.hpp"
#include "playback_completion_controller.hpp"
#include "player_completion_controller.hpp"
#include "player_screen.hpp"

#include <optional>
#include <string>
#include <vector>

using AppQueuedPlaybackCompletion = QueuedPlaybackResolutionCompletion<Screen>;

struct PlaybackCompletionHostEffects {
    bool finishLoading = false;
    bool popToDetails = false;
    bool stopPlayback = false;
    bool reportProgress = false;
    std::optional<std::string> error;
    std::vector<PlaybackHomeVisibilityRestore> restoreHomeVisibility;
    std::optional<PlayerCompletionNotice> notice;
    std::optional<PlayerCompletionDiagnostic> diagnostic;
    std::optional<JellyfinItem> playItem;
};

class PlaybackCompletionFlow {
public:
    PlaybackCompletionFlow(PlaybackCoordinator& coordinator, PlaybackQueueState& queueState, DetailsFlow& detailsFlow,
                           PlayerScreenState& playerScreenState)
        : coordinator_(coordinator), queueState_(queueState), detailsFlow_(detailsFlow),
          playerScreenState_(playerScreenState) {}

    [[nodiscard]] PlaybackCompletionHostEffects complete(AppQueuedPlaybackCompletion& completion, bool activeGeneration,
                                                         Screen screen);
    [[nodiscard]] PlaybackCompletionHostEffects complete(PlayerItemPlaybackCompletion& completion,
                                                         bool activeGeneration, bool playerScreen);
    [[nodiscard]] PlaybackCompletionHostEffects complete(AutoplayPlaybackCompletion& completion, bool activeGeneration);
    [[nodiscard]] PlaybackCompletionHostEffects complete(StreamRestartCompletion& completion, bool activeGeneration,
                                                         bool playerScreen);
    [[nodiscard]] PlaybackCompletionHostEffects complete(FallbackPlaybackCompletion& completion, bool activeGeneration,
                                                         bool playerScreen);
    [[nodiscard]] PlaybackCompletionHostEffects complete(BeginPlaybackCompletion& completion, bool activeGeneration,
                                                         bool activeDetailsSelection);
    [[nodiscard]] PlaybackCompletionHostEffects complete(SeriesPlayAllCompletion& completion, bool activeGeneration,
                                                         bool activeDetailsSelection);

    [[nodiscard]] PlaybackCompletionHostEffects complete(SubtitleLoadCompletion& completion, bool activeGeneration);
    [[nodiscard]] PlaybackCompletionHostEffects complete(MediaSegmentsCompletion& completion, bool playerScreen);
    [[nodiscard]] PlaybackCompletionHostEffects complete(NextEpisodeCompletion& completion, bool playerScreen);
    [[nodiscard]] PlaybackCompletionHostEffects complete(PlaybackAdjacentCompletion& completion, bool playerScreen);

    void applyReport(const PlaybackReportCompletion& completion, const JellyfinSession& session, bool playerScreen);

private:
    [[nodiscard]] PlaybackCompletionHostEffects apply(PlaybackCompletionEffects effects);
    [[nodiscard]] PlaybackCompletionHostEffects apply(PlayerCompletionEffects effects);

    PlaybackCoordinator& coordinator_;
    PlaybackQueueState& queueState_;
    DetailsFlow& detailsFlow_;
    PlayerScreenState& playerScreenState_;
};
