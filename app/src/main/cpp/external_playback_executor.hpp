#pragma once

#include "external_playback_state.hpp"
#include "jellyfin_types.hpp"
#include "playback_track_selection.hpp"

#include <algorithm>
#include <cstdint>
#include <functional>
#include <optional>
#include <sstream>
#include <string>
#include <utility>
#include <vector>

struct ExternalPlaybackRequest {
    uint64_t generation = 0;
    std::string selectedItemId;
    std::string selectedItemType;
    std::string seriesId;
    std::string container;
    std::string mediaSourceId;
    std::vector<JellyfinAudioStream> audios;
    std::vector<JellyfinSubtitleStream> subtitles;
    ExternalPlayerApp player;
    std::optional<std::string> subtitlePreference;
    PlaybackTrackSelectionPolicy trackPolicy;
};

struct ExternalPlaybackCompletion {
    uint64_t generation = 0;
    std::string selectedItemId;
    std::string selectedSeriesId;
    std::optional<ExternalPlaybackLaunch> launch;
    std::string error;
};

struct ExternalPlaybackReportRequest {
    std::string itemId;
    std::string mediaSourceId;
    std::optional<int64_t> positionTicks;
};

enum class ExternalPlaybackDiagnosticKind {
    MediaSegments,
    StopReport,
};

struct ExternalPlaybackDiagnostic {
    ExternalPlaybackDiagnosticKind kind = ExternalPlaybackDiagnosticKind::MediaSegments;
    std::string itemId;
    std::string error;
};

inline std::string externalSkipSegmentsJson(const std::vector<JellyfinMediaSegment>& segments) {
    std::ostringstream json;
    json << '[';
    bool first = true;
    for (const auto& segment : segments) {
        if (segment.endTicks - segment.startTicks < 30000000) continue;
        std::string type;
        if (segment.type == "Intro")
            type = "intro";
        else if (segment.type == "Outro")
            type = "outro";
        else if (segment.type == "Recap")
            type = "recap";
        else
            continue;
        if (!first) json << ',';
        first = false;
        json << "{\"type\":\"" << type << "\",\"start\":" << (static_cast<double>(segment.startTicks) / 10000000.0)
             << ",\"end\":" << (static_cast<double>(segment.endTicks) / 10000000.0) << '}';
    }
    json << ']';
    return json.str();
}

template <typename Client, typename TaskRunner, typename CompletionSink, typename Epoch>
class ExternalPlaybackExecutor {
public:
    using DiagnosticSink = std::function<void(const ExternalPlaybackDiagnostic&)>;

    ExternalPlaybackExecutor(Client& client, TaskRunner& tasks, CompletionSink& completions, const Epoch& epoch,
                             DiagnosticSink diagnostics = {})
        : client_(client), tasks_(tasks), completions_(completions), epoch_(epoch),
          diagnostics_(std::move(diagnostics)) {}

    bool prepare(JellyfinSession session, ExternalPlaybackRequest request) {
        return tasks_.submit([this, session = std::move(session), request = std::move(request)]() mutable {
            JellyfinItem playable;
            playable.id = request.selectedItemId;
            playable.type = request.selectedItemType;
            playable.seriesId = request.seriesId;
            playable.container = request.container;
            playable.mediaSourceId = request.mediaSourceId;
            playable.audios = std::move(request.audios);
            playable.subtitles = std::move(request.subtitles);

            if (playable.type == "Series") {
                auto next = client_.getNextUpForSeries(session, playable.id);
                if (!next.ok) {
                    if (!epoch_.active(request.generation)) return;
                    completions_.push(ExternalPlaybackCompletion{
                        .generation = request.generation,
                        .selectedItemId = request.selectedItemId,
                        .selectedSeriesId = request.seriesId,
                        .launch = std::nullopt,
                        .error = "EXTERNAL PLAYER: " + next.error,
                    });
                    return;
                }
                playable = std::move(next.value);
            }

            auto detailed = client_.getItem(session, playable.id);
            if (detailed.ok) playable = std::move(detailed.value);

            std::string skipSegmentsJson;
            if (request.player.packageName == "app.mpvnova.player") {
                auto segments = client_.getMediaSegments(session, playable.id);
                if (segments.ok) {
                    skipSegmentsJson = externalSkipSegmentsJson(segments.value);
                } else if (diagnostics_) {
                    diagnostics_(ExternalPlaybackDiagnostic{
                        .kind = ExternalPlaybackDiagnosticKind::MediaSegments,
                        .itemId = playable.id,
                        .error = std::move(segments.error),
                    });
                }
            }

            const std::string videoUrl = client_.staticVideoUrl(session, playable);
            std::string subtitleUrl;
            const auto tracks =
                selectPlaybackTracks(playable, std::nullopt, request.subtitlePreference, request.trackPolicy);
            const int subtitleIndex = tracks.subtitleStreamIndex;
            if (subtitleIndex >= 0) {
                const auto subtitle = std::find_if(
                    playable.subtitles.begin(), playable.subtitles.end(),
                    [&](const JellyfinSubtitleStream& candidate) { return candidate.index == subtitleIndex; });
                if (subtitle != playable.subtitles.end()) {
                    subtitleUrl = client_.subtitleSrtUrl(session, playable, subtitle->index);
                }
            }

            if (!epoch_.active(request.generation)) return;
            if (videoUrl.empty()) {
                completions_.push(ExternalPlaybackCompletion{
                    .generation = request.generation,
                    .selectedItemId = request.selectedItemId,
                    .selectedSeriesId = request.seriesId,
                    .launch = std::nullopt,
                    .error = "EXTERNAL PLAYER: NO STATIC STREAM",
                });
                return;
            }
            completions_.push(ExternalPlaybackCompletion{
                .generation = request.generation,
                .selectedItemId = request.selectedItemId,
                .selectedSeriesId = request.seriesId,
                .launch =
                    ExternalPlaybackLaunch{
                        .item = std::move(playable),
                        .player = std::move(request.player),
                        .url = videoUrl,
                        .subtitleUrl = std::move(subtitleUrl),
                        .skipSegmentsJson = std::move(skipSegmentsJson),
                    },
                .error = {},
            });
        });
    }

    bool reportStopped(JellyfinSession session, ExternalPlaybackReportRequest request) {
        return tasks_.submit([this, session = std::move(session), request = std::move(request)] {
            JellyfinItem item;
            item.id = request.itemId;
            item.mediaSourceId = request.mediaSourceId;
            const auto result = client_.reportExternalPlaybackStopped(session, item, request.positionTicks);
            if (!result.ok && diagnostics_) {
                diagnostics_(ExternalPlaybackDiagnostic{
                    .kind = ExternalPlaybackDiagnosticKind::StopReport,
                    .itemId = request.itemId,
                    .error = result.error,
                });
            }
        });
    }

private:
    Client& client_;
    TaskRunner& tasks_;
    CompletionSink& completions_;
    const Epoch& epoch_;
    DiagnosticSink diagnostics_;
};
