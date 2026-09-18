#pragma once

#include "playback_continuation_executor.hpp"
#include "playback_coordinator.hpp"
#include "playback_telemetry_executor.hpp"
#include "subtitle_load_executor.hpp"

#include <chrono>
#include <cstddef>
#include <optional>
#include <string>
#include <string_view>

enum class PlayerCompletionDiagnosticKind {
    None,
    SubtitleLoaded,
    MediaSegmentsUnavailable,
    MediaSegmentsLoaded,
    NextEpisodeUnavailable,
};

struct PlayerCompletionDiagnostic {
    PlayerCompletionDiagnosticKind kind = PlayerCompletionDiagnosticKind::None;
    std::string itemId;
    int streamIndex = -1;
    std::string codec;
    std::size_t count = 0;
    std::string error;
};

struct PlayerCompletionNotice {
    std::string text;
    std::chrono::seconds duration{6};
};

struct PlayerCompletionEffects {
    std::optional<PlayerCompletionNotice> notice;
    std::optional<PlayerCompletionDiagnostic> diagnostic;
    bool showSubtitleOverlay = false;
    bool reportProgress = false;
    std::optional<JellyfinItem> playItem;
};

class PlayerCompletionController {
public:
    [[nodiscard]] static PlayerCompletionEffects apply(SubtitleLoadCompletion& completion, bool activeGeneration,
                                                       PlaybackCoordinator& coordinator);
    [[nodiscard]] static PlayerCompletionEffects apply(MediaSegmentsCompletion& completion, bool playerScreen,
                                                       PlaybackCoordinator& coordinator);
    [[nodiscard]] static PlayerCompletionEffects apply(NextEpisodeCompletion& completion, bool playerScreen,
                                                       PlaybackCoordinator& coordinator);
    [[nodiscard]] static PlayerCompletionEffects apply(PlaybackAdjacentCompletion& completion, bool playerScreen,
                                                       PlaybackCoordinator& coordinator);

    [[nodiscard]] static const char* reportStage(PlaybackReportKind kind);
    static void apply(const PlaybackReportCompletion& completion, std::string_view server, std::string_view userId,
                      bool playerScreen, PlaybackCoordinator& coordinator);
};
