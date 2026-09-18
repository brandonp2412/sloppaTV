#pragma once

#include "playback_coordinator.hpp"
#include "playback_queue.hpp"
#include "playback_resolution_executor.hpp"
#include "playback_stream_executor.hpp"
#include "series_playback_executor.hpp"

#include <optional>
#include <string>
#include <utility>
#include <vector>

struct PlaybackHomeVisibilityRestore {
    std::string itemId;
    std::string seriesId;
};

enum class PlaybackCompletionTransitionKind {
    Resolved,
    StreamRestart,
    ResolvedFallback,
};

struct PlaybackCompletionTransition {
    PlaybackCompletionTransitionKind kind = PlaybackCompletionTransitionKind::Resolved;
    PlaybackTarget target;
    JellyfinItem item;
    bool restartPaused = false;
    int audioStreamIndex = -1;
};

struct PlaybackCompletionEffects {
    bool finishLoading = false;
    bool popToDetails = false;
    bool stopPlayback = false;
    std::optional<std::string> error;
    std::optional<JellyfinItem> detailUpdate;
    std::vector<PlaybackHomeVisibilityRestore> restoreHomeVisibility;
    std::optional<PlaybackCompletionTransition> transition;
};

class PlaybackCompletionController {
public:
    template <typename Origin>
    [[nodiscard]] static PlaybackCompletionEffects apply(QueuedPlaybackResolutionCompletion<Origin>& completion,
                                                         bool activeGeneration, bool activeQueueContext,
                                                         bool playerScreen, PlaybackQueueState& queue,
                                                         PlaybackCoordinator& coordinator) {
        if (!activeGeneration) return {};

        PlaybackCompletionEffects effects;
        effects.finishLoading = true;
        coordinator.finishPlaybackResolution();
        if (!activeQueueContext) return effects;

        if (!completion.result.ok) {
            effects.popToDetails = completion.replacingPlayer && playerScreen;
            effects.error = "QUEUE: " + completion.result.error;
            return effects;
        }

        queue.setCurrentIndex(completion.index);
        queue.setItemAt(completion.index, completion.item);
        effects.transition = PlaybackCompletionTransition{
            .kind = PlaybackCompletionTransitionKind::Resolved,
            .target = std::move(completion.result.value),
            .item = std::move(completion.item),
        };
        return effects;
    }

    [[nodiscard]] static PlaybackCompletionEffects apply(PlayerItemPlaybackCompletion& completion,
                                                         bool activeGeneration, bool playerScreen,
                                                         PlaybackCoordinator& coordinator);
    [[nodiscard]] static PlaybackCompletionEffects apply(AutoplayPlaybackCompletion& completion,
                                                         bool activeGeneration, PlaybackQueueState& queue,
                                                         PlaybackCoordinator& coordinator);
    [[nodiscard]] static PlaybackCompletionEffects apply(StreamRestartCompletion& completion, bool activeGeneration,
                                                         bool playerScreen, PlaybackCoordinator& coordinator);
    [[nodiscard]] static PlaybackCompletionEffects apply(FallbackPlaybackCompletion& completion,
                                                         bool activeGeneration, bool playerScreen,
                                                         PlaybackCoordinator& coordinator);
    [[nodiscard]] static PlaybackCompletionEffects apply(BeginPlaybackCompletion& completion, bool activeGeneration,
                                                         bool activeDetailsSelection, PlaybackQueueState& queue);
    [[nodiscard]] static PlaybackCompletionEffects apply(SeriesPlayAllCompletion& completion, bool activeGeneration,
                                                         bool activeDetailsSelection, PlaybackQueueState& queue);
};
