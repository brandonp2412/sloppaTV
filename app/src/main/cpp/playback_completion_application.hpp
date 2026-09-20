#pragma once

#include "app_screen.hpp"
#include "details_flow.hpp"
#include "playback_completion_flow.hpp"
#include "playback_runtime_controller.hpp"
#include "request_epoch.hpp"

#include <optional>
#include <string>
#include <type_traits>
#include <utility>

template <typename T>
inline constexpr bool isPlaybackApplicationCompletionV =
    std::is_same_v<std::remove_cvref_t<T>, SubtitleLoadCompletion> ||
    std::is_same_v<std::remove_cvref_t<T>, MediaSegmentsCompletion> ||
    std::is_same_v<std::remove_cvref_t<T>, NextEpisodeCompletion> ||
    std::is_same_v<std::remove_cvref_t<T>, PlaybackAdjacentCompletion> ||
    std::is_same_v<std::remove_cvref_t<T>, AppQueuedPlaybackCompletion> ||
    std::is_same_v<std::remove_cvref_t<T>, PlayerItemPlaybackCompletion> ||
    std::is_same_v<std::remove_cvref_t<T>, AutoplayPlaybackCompletion> ||
    std::is_same_v<std::remove_cvref_t<T>, StreamRestartCompletion> ||
    std::is_same_v<std::remove_cvref_t<T>, FallbackPlaybackCompletion> ||
    std::is_same_v<std::remove_cvref_t<T>, BeginPlaybackCompletion> ||
    std::is_same_v<std::remove_cvref_t<T>, SeriesPlayAllCompletion>;

struct PlaybackCompletionApplicationEffects {
    bool popToDetails = false;
    bool stopPlayback = false;
    int saveSessionCount = 0;
    std::optional<PlayerCompletionNotice> notice;
    std::optional<PlayerCompletionDiagnostic> diagnostic;
    std::optional<JellyfinItem> playItem;
};

template <typename DetailsScreens, typename TelemetryAsync> class PlaybackCompletionApplication {
public:
    PlaybackCompletionApplication(PlaybackCompletionFlow& flow, RequestEpoch& epoch, Screen& screen,
                                  DetailsFlow& details, DetailsScreens& detailsScreens, JellyfinSession& session,
                                  bool& loading, std::string& error, PlaybackRuntimeController& runtime,
                                  TelemetryAsync& telemetry)
        : flow_(flow), epoch_(epoch), screen_(screen), details_(details), detailsScreens_(detailsScreens),
          session_(session), loading_(loading), error_(error), runtime_(runtime), telemetry_(telemetry) {}

    [[nodiscard]] PlaybackCompletionApplicationEffects complete(SubtitleLoadCompletion& completion) {
        return apply(flow_.complete(completion, epoch_.active(completion.generation)));
    }

    [[nodiscard]] PlaybackCompletionApplicationEffects complete(MediaSegmentsCompletion& completion) {
        return apply(flow_.complete(completion, screen_ == Screen::Player));
    }

    [[nodiscard]] PlaybackCompletionApplicationEffects complete(NextEpisodeCompletion& completion) {
        return apply(flow_.complete(completion, screen_ == Screen::Player));
    }

    [[nodiscard]] PlaybackCompletionApplicationEffects complete(PlaybackAdjacentCompletion& completion) {
        return apply(flow_.complete(completion, screen_ == Screen::Player));
    }

    [[nodiscard]] PlaybackCompletionApplicationEffects complete(AppQueuedPlaybackCompletion& completion) {
        return apply(flow_.complete(completion, epoch_.active(completion.generation), screen_));
    }

    [[nodiscard]] PlaybackCompletionApplicationEffects complete(PlayerItemPlaybackCompletion& completion) {
        return apply(flow_.complete(completion, epoch_.active(completion.generation), screen_ == Screen::Player));
    }

    [[nodiscard]] PlaybackCompletionApplicationEffects complete(AutoplayPlaybackCompletion& completion) {
        return apply(flow_.complete(completion, epoch_.active(completion.generation)));
    }

    [[nodiscard]] PlaybackCompletionApplicationEffects complete(StreamRestartCompletion& completion) {
        return apply(flow_.complete(completion, epoch_.active(completion.generation), screen_ == Screen::Player));
    }

    [[nodiscard]] PlaybackCompletionApplicationEffects complete(FallbackPlaybackCompletion& completion) {
        return apply(flow_.complete(completion, epoch_.active(completion.generation), screen_ == Screen::Player));
    }

    [[nodiscard]] PlaybackCompletionApplicationEffects complete(BeginPlaybackCompletion& completion) {
        const bool activeDetailsSelection = screen_ == Screen::Details && details_.item().id == completion.selected.id;
        return apply(flow_.complete(completion, epoch_.active(completion.generation), activeDetailsSelection));
    }

    [[nodiscard]] PlaybackCompletionApplicationEffects complete(SeriesPlayAllCompletion& completion) {
        const bool activeDetailsSelection = screen_ == Screen::Details && details_.item().id == completion.series.id;
        return apply(flow_.complete(completion, epoch_.active(completion.generation), activeDetailsSelection));
    }

private:
    [[nodiscard]] PlaybackCompletionApplicationEffects apply(PlaybackCompletionHostEffects effects) {
        PlaybackCompletionApplicationEffects host;
        if (effects.finishLoading) loading_ = false;
        if (effects.error) error_ = std::move(*effects.error);
        host.popToDetails = effects.popToDetails;
        host.stopPlayback = effects.stopPlayback;
        host.notice = std::move(effects.notice);
        host.diagnostic = std::move(effects.diagnostic);
        host.playItem = std::move(effects.playItem);

        for (const auto& restore : effects.restoreHomeVisibility) {
            JellyfinItem item;
            item.id = restore.itemId;
            item.seriesId = restore.seriesId;
            if (detailsScreens_.restoreHomeVisibilityForPlayback(item)) ++host.saveSessionCount;
        }
        if (effects.reportProgress) runtime_.reportProgress(telemetry_, screen_ == Screen::Player, false);
        return host;
    }

    PlaybackCompletionFlow& flow_;
    RequestEpoch& epoch_;
    Screen& screen_;
    DetailsFlow& details_;
    DetailsScreens& detailsScreens_;
    JellyfinSession& session_;
    bool& loading_;
    std::string& error_;
    PlaybackRuntimeController& runtime_;
    TelemetryAsync& telemetry_;
};
