#include "external_playback_executor.hpp"

#include <cassert>
#include <functional>
#include <optional>
#include <string>
#include <utility>
#include <variant>
#include <vector>

namespace {
using Completion = std::variant<ExternalPlaybackCompletion>;

struct ImmediateTaskRunner {
    bool submit(std::function<void()> task) {
        if (!accept) return false;
        ++submissions;
        task();
        return true;
    }

    bool accept = true;
    int submissions = 0;
};

struct CompletionSink {
    void push(Completion completion) { events.push_back(std::move(completion)); }

    std::vector<Completion> events;
};

struct Epoch {
    bool active(uint64_t generation) const { return generation == activeGeneration; }

    uint64_t activeGeneration = 7;
};

struct FakeClient {
    ApiValueResult<JellyfinItem> getNextUpForSeries(const JellyfinSession&, const std::string& seriesId) {
        calls.push_back("next:" + seriesId);
        return nextUp;
    }

    ApiValueResult<JellyfinItem> getItem(const JellyfinSession&, const std::string& itemId) {
        calls.push_back("item:" + itemId);
        return detailed;
    }

    ApiValueResult<std::vector<JellyfinMediaSegment>> getMediaSegments(const JellyfinSession&,
                                                                       const std::string& itemId) {
        calls.push_back("segments:" + itemId);
        return segments;
    }

    std::string staticVideoUrl(const JellyfinSession&, const JellyfinItem& item) {
        calls.push_back("video:" + item.id);
        lastVideoItem = item;
        return videoUrl;
    }

    std::string subtitleSrtUrl(const JellyfinSession&, const JellyfinItem& item, int subtitleIndex) {
        calls.push_back("subtitle:" + std::to_string(subtitleIndex));
        lastSubtitleItem = item;
        return subtitleUrl;
    }

    ApiResult reportExternalPlaybackStopped(const JellyfinSession&, const JellyfinItem& item,
                                            std::optional<int64_t> positionTicks) {
        calls.push_back("stopped:" + item.id);
        lastStoppedItem = item;
        lastStoppedPositionTicks = positionTicks;
        return stopResult;
    }

    ApiValueResult<JellyfinItem> nextUp;
    ApiValueResult<JellyfinItem> detailed;
    ApiValueResult<std::vector<JellyfinMediaSegment>> segments;
    std::string videoUrl = "https://media.example/video.mkv";
    std::string subtitleUrl = "https://media.example/subtitle.srt";
    ApiResult stopResult{.ok = true, .error = {}};
    std::vector<std::string> calls;
    JellyfinItem lastVideoItem;
    JellyfinItem lastSubtitleItem;
    JellyfinItem lastStoppedItem;
    std::optional<int64_t> lastStoppedPositionTicks;
};

ExternalPlaybackRequest episodeRequest(std::string packageName = "org.videolan.vlc") {
    JellyfinAudioStream audio;
    audio.index = 1;
    audio.language = "eng";
    audio.isDefault = true;

    JellyfinSubtitleStream subtitle;
    subtitle.index = 2;
    subtitle.codec = "srt";
    subtitle.language = "eng";

    return ExternalPlaybackRequest{
        .generation = 7,
        .selectedItemId = "episode-1",
        .selectedItemType = "Episode",
        .seriesId = "series-1",
        .container = "mkv",
        .mediaSourceId = "source-1",
        .audios = {audio},
        .subtitles = {subtitle},
        .player = ExternalPlayerApp{
            .componentName = packageName + "/Player",
            .packageName = std::move(packageName),
            .label = "Player",
        },
        .subtitlePreference = std::string{"eng"},
        .trackPolicy =
            PlaybackTrackSelectionPolicy{
                .autoSubtitles = false,
                .autoSubtitleLanguage = {},
                .autoSubtitleSourceLanguage = {},
                .allowedSubtitleLanguages = {"eng"},
            },
    };
}

JellyfinItem detailedEpisode(std::string id) {
    JellyfinItem item;
    item.id = std::move(id);
    item.type = "Episode";
    item.container = "mkv";
    item.mediaSourceId = "source-detailed";
    item.seriesId = "series-1";

    JellyfinAudioStream audio;
    audio.index = 1;
    audio.language = "eng";
    audio.isDefault = true;
    item.audios.push_back(std::move(audio));

    JellyfinSubtitleStream subtitle;
    subtitle.index = 2;
    subtitle.codec = "srt";
    subtitle.language = "eng";
    item.subtitles.push_back(std::move(subtitle));
    return item;
}
} // namespace

int main() {
    JellyfinSession session;
    session.server = "https://jellyfin.example.nz";
    session.userId = "user-1";

    FakeClient client;
    ImmediateTaskRunner tasks;
    CompletionSink completions;
    Epoch epoch;
    std::vector<ExternalPlaybackDiagnostic> diagnostics;
    ExternalPlaybackExecutor executor(
        client, tasks, completions, epoch,
        [&](const ExternalPlaybackDiagnostic& diagnostic) { diagnostics.push_back(diagnostic); });

    client.detailed.ok = false;
    assert(executor.prepare(session, episodeRequest()));
    assert(tasks.submissions == 1);
    assert((client.calls == std::vector<std::string>{"item:episode-1", "video:episode-1", "subtitle:2"}));
    const auto& fallbackLaunch = std::get<ExternalPlaybackCompletion>(completions.events.back());
    assert(fallbackLaunch.launch.has_value());
    assert(fallbackLaunch.selectedItemId == "episode-1");
    assert(fallbackLaunch.selectedSeriesId == "series-1");
    assert(fallbackLaunch.launch->item.mediaSourceId == "source-1");
    assert(fallbackLaunch.launch->item.seriesId == "series-1");
    assert(fallbackLaunch.launch->item.container == "mkv");
    assert(fallbackLaunch.launch->subtitleUrl == client.subtitleUrl);

    client.calls.clear();
    client.detailed.ok = true;
    client.detailed.value = detailedEpisode("episode-1");
    client.segments.ok = true;
    client.segments.value.clear();
    assert(executor.prepare(session, episodeRequest("app.mpvnova.player")));
    assert((client.calls ==
            std::vector<std::string>{"item:episode-1", "segments:episode-1", "video:episode-1", "subtitle:2"}));
    assert(std::get<ExternalPlaybackCompletion>(completions.events.back()).launch->skipSegmentsJson == "[]");

    client.calls.clear();
    client.segments.ok = true;
    client.segments.value = {
        JellyfinMediaSegment{.type = "Intro", .startTicks = 0, .endTicks = 40000000},
        JellyfinMediaSegment{.type = "Outro", .startTicks = 50000000, .endTicks = 100000000},
        JellyfinMediaSegment{.type = "Recap", .startTicks = 0, .endTicks = 10000000},
        JellyfinMediaSegment{.type = "Preview", .startTicks = 0, .endTicks = 50000000},
    };
    assert(executor.prepare(session, episodeRequest("app.mpvnova.player")));
    const auto& mpvLaunch = std::get<ExternalPlaybackCompletion>(completions.events.back());
    assert(mpvLaunch.launch->skipSegmentsJson ==
           "[{\"type\":\"intro\",\"start\":0,\"end\":4},{\"type\":\"outro\",\"start\":5,\"end\":10}]");

    client.calls.clear();
    client.segments.ok = false;
    client.segments.error = "segments unavailable";
    diagnostics.clear();
    assert(executor.prepare(session, episodeRequest("app.mpvnova.player")));
    assert(diagnostics.size() == 1);
    assert(diagnostics[0].itemId == "episode-1");
    assert(diagnostics[0].error == "segments unavailable");

    ExternalPlaybackRequest series = episodeRequest();
    series.selectedItemId = "series-1";
    series.selectedItemType = "Series";
    client.calls.clear();
    client.nextUp = {};
    client.nextUp.error = "next up unavailable";
    const size_t beforeSeriesFailure = completions.events.size();
    assert(executor.prepare(session, std::move(series)));
    assert((client.calls == std::vector<std::string>{"next:series-1"}));
    assert(completions.events.size() == beforeSeriesFailure + 1);
    const auto& seriesFailure = std::get<ExternalPlaybackCompletion>(completions.events.back());
    assert(!seriesFailure.launch.has_value());
    assert(seriesFailure.error == "EXTERNAL PLAYER: next up unavailable");

    client.calls.clear();
    client.videoUrl.clear();
    assert(executor.prepare(session, episodeRequest()));
    const auto& missingStream = std::get<ExternalPlaybackCompletion>(completions.events.back());
    assert(!missingStream.launch.has_value());
    assert(missingStream.error == "EXTERNAL PLAYER: NO STATIC STREAM");

    client.videoUrl = "https://media.example/video.mkv";
    const size_t completionCount = completions.events.size();
    epoch.activeGeneration = 8;
    assert(executor.prepare(session, episodeRequest()));
    assert(completions.events.size() == completionCount);

    client.calls.clear();
    client.stopResult = ApiResult{.ok = true, .error = {}};
    diagnostics.clear();
    assert(executor.reportStopped(session, ExternalPlaybackReportRequest{
                                               .itemId = "episode-1",
                                               .mediaSourceId = "source-stop",
                                               .positionTicks = 123456,
                                           }));
    assert((client.calls == std::vector<std::string>{"stopped:episode-1"}));
    assert(client.lastStoppedItem.id == "episode-1");
    assert(client.lastStoppedItem.mediaSourceId == "source-stop");
    assert(client.lastStoppedPositionTicks == 123456);
    assert(diagnostics.empty());

    client.calls.clear();
    client.stopResult = ApiResult{.ok = false, .error = "report failed"};
    assert(executor.reportStopped(session, ExternalPlaybackReportRequest{
                                               .itemId = "episode-2",
                                               .mediaSourceId = "source-2",
                                               .positionTicks = std::nullopt,
                                           }));
    assert((client.calls == std::vector<std::string>{"stopped:episode-2"}));
    assert(diagnostics.size() == 1);
    assert(diagnostics[0].kind == ExternalPlaybackDiagnosticKind::StopReport);
    assert(diagnostics[0].itemId == "episode-2");
    assert(diagnostics[0].error == "report failed");

    tasks.accept = false;
    assert(!executor.prepare(session, episodeRequest()));
    assert(!executor.reportStopped(session, ExternalPlaybackReportRequest{
                                                .itemId = "episode-3",
                                                .mediaSourceId = {},
                                                .positionTicks = std::nullopt,
                                            }));

    return 0;
}
