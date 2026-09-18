#include "player_completion_controller.hpp"

#include <cassert>
#include <chrono>
#include <string>

namespace {
using namespace std::chrono_literals;

JellyfinItem episode(std::string id = "episode-1") {
    JellyfinItem item;
    item.id = std::move(id);
    item.type = "Episode";
    item.seriesId = "series-1";
    item.parentIndexNumber = 1;
    item.indexNumber = 2;
    return item;
}

PlaybackTarget target() {
    PlaybackTarget value;
    value.url = "https://media.example/item";
    value.playMethod = PlaybackMethod::DirectPlay;
    return value;
}

PlaybackCoordinator activeCoordinator(const JellyfinItem& item = episode()) {
    PlaybackCoordinator coordinator;
    coordinator.activate(item, target(), PlaybackCoordinator::Clock::now());
    return coordinator;
}

void assertNoEffects(const PlayerCompletionEffects& effects) {
    assert(!effects.notice);
    assert(!effects.diagnostic);
    assert(!effects.showSubtitleOverlay);
    assert(!effects.reportProgress);
    assert(!effects.playItem);
}
} // namespace

int main() {
    {
        PlaybackCoordinator coordinator = activeCoordinator();
        coordinator.prepareNativeSubtitleLoad(3);

        SubtitleLoadCompletion completion;
        completion.generation = 7;
        completion.itemId = "episode-1";
        completion.requestedSubtitleIndex = 3;
        completion.loadedSubtitle.index = 3;
        completion.loadedSubtitle.language = "eng";
        completion.loadedSubtitle.codec = "srt";
        completion.cues.push_back({});

        const auto stale = PlayerCompletionController::apply(completion, false, coordinator);
        assertNoEffects(stale);
        assert(coordinator.subtitleLoadMatches("episode-1", 3));
    }

    {
        PlaybackCoordinator coordinator = activeCoordinator();
        coordinator.prepareNativeSubtitleLoad(4);

        SubtitleLoadCompletion completion;
        completion.itemId = "episode-1";
        completion.requestedSubtitleIndex = 4;

        const auto effects = PlayerCompletionController::apply(completion, true, coordinator);
        assert(effects.notice);
        assert(effects.notice->text == "SUBTITLES UNAVAILABLE FOR THIS FILE");
        assert(effects.notice->duration == 6s);
        assert(!effects.diagnostic);
        assert(!effects.showSubtitleOverlay);
        assert(!effects.reportProgress);
    }

    {
        PlaybackCoordinator coordinator = activeCoordinator();
        coordinator.prepareNativeSubtitleLoad(5);

        SubtitleLoadCompletion completion;
        completion.itemId = "episode-1";
        completion.requestedSubtitleIndex = 5;
        completion.loadedSubtitle.index = 5;
        completion.loadedSubtitle.language = "eng";
        completion.loadedSubtitle.codec = "ass";
        completion.cues.resize(2);

        const auto effects = PlayerCompletionController::apply(completion, true, coordinator);
        assert(effects.diagnostic);
        assert(effects.diagnostic->kind == PlayerCompletionDiagnosticKind::SubtitleLoaded);
        assert(effects.diagnostic->itemId == "episode-1");
        assert(effects.diagnostic->streamIndex == 5);
        assert(effects.diagnostic->codec == "ass");
        assert(effects.diagnostic->count == 2);
        assert(effects.showSubtitleOverlay);
        assert(effects.reportProgress);
        assert(coordinator.tracks().selectedSubtitleServerIndex() == 5);
    }

    {
        PlaybackCoordinator coordinator = activeCoordinator();
        const auto now = PlaybackCoordinator::Clock::now();
        assert(coordinator.beginMediaSegmentsRequest(now));

        MediaSegmentsCompletion completion;
        completion.itemId = "episode-1";
        completion.ok = false;
        completion.error = "segments failed";
        completion.completedAt = now + 1s;

        const auto effects = PlayerCompletionController::apply(completion, true, coordinator);
        assert(effects.diagnostic);
        assert(effects.diagnostic->kind == PlayerCompletionDiagnosticKind::MediaSegmentsUnavailable);
        assert(effects.diagnostic->error == "segments failed");
    }

    {
        PlaybackCoordinator coordinator = activeCoordinator();
        const auto now = PlaybackCoordinator::Clock::now();
        assert(coordinator.beginMediaSegmentsRequest(now));

        MediaSegmentsCompletion completion;
        completion.itemId = "episode-1";
        completion.ok = true;
        completion.completedAt = now + 1s;
        completion.segments.push_back({
            .type = "Intro",
            .startTicks = 10'000'000,
            .endTicks = 40'000'000,
        });

        const auto effects = PlayerCompletionController::apply(completion, true, coordinator);
        assert(effects.diagnostic);
        assert(effects.diagnostic->kind == PlayerCompletionDiagnosticKind::MediaSegmentsLoaded);
        assert(effects.diagnostic->count == 1);
        assert(coordinator.session().mediaSegments().size() == 1);
    }

    {
        PlaybackCoordinator coordinator = activeCoordinator();
        const auto now = PlaybackCoordinator::Clock::now();
        assert(coordinator.beginNextEpisodeRequest(now));

        NextEpisodeCompletion completion;
        completion.currentItemId = "episode-1";
        completion.ok = false;
        completion.error = "lookup failed";
        completion.completedAt = now + 1s;

        const auto effects = PlayerCompletionController::apply(completion, true, coordinator);
        assert(effects.diagnostic);
        assert(effects.diagnostic->kind == PlayerCompletionDiagnosticKind::NextEpisodeUnavailable);
        assert(effects.diagnostic->error == "lookup failed");
    }

    {
        PlaybackCoordinator coordinator = activeCoordinator();
        const auto now = PlaybackCoordinator::Clock::now();
        assert(coordinator.beginNextEpisodeRequest(now));

        NextEpisodeCompletion completion;
        completion.currentItemId = "episode-1";
        completion.ok = true;
        completion.item = episode("episode-2");

        const auto effects = PlayerCompletionController::apply(completion, true, coordinator);
        assertNoEffects(effects);
        assert(coordinator.continuation().nextItem());
        assert(coordinator.continuation().nextItem()->id == "episode-2");
    }

    {
        PlaybackCoordinator coordinator = activeCoordinator();
        assert(coordinator.beginAdjacentEpisodeLookup());

        PlaybackAdjacentCompletion completion;
        completion.currentItemId = "episode-1";
        completion.direction = -1;
        completion.ok = false;

        const auto effects = PlayerCompletionController::apply(completion, true, coordinator);
        assert(effects.notice);
        assert(effects.notice->text == "EPISODE LIST UNAVAILABLE");
        assert(effects.notice->duration == 2s);
    }

    {
        PlaybackCoordinator coordinator = activeCoordinator();
        assert(coordinator.beginAdjacentEpisodeLookup());

        PlaybackAdjacentCompletion completion;
        completion.currentItemId = "episode-1";
        completion.direction = 1;
        completion.ok = true;

        const auto effects = PlayerCompletionController::apply(completion, true, coordinator);
        assert(effects.notice);
        assert(effects.notice->text == "NO NEXT EPISODE");
        assert(effects.notice->duration == 2s);
    }

    {
        PlaybackCoordinator coordinator = activeCoordinator();
        assert(coordinator.beginAdjacentEpisodeLookup());

        PlaybackAdjacentCompletion completion;
        completion.currentItemId = "episode-1";
        completion.direction = 1;
        completion.ok = true;
        completion.item = episode("episode-2");

        const auto effects = PlayerCompletionController::apply(completion, true, coordinator);
        assert(effects.playItem);
        assert(effects.playItem->id == "episode-2");
    }

    {
        PlaybackCoordinator coordinator = activeCoordinator();
        assert(coordinator.beginAdjacentEpisodeLookup());

        PlaybackAdjacentCompletion completion;
        completion.currentItemId = "episode-1";
        completion.direction = 1;
        completion.ok = true;
        completion.item = episode("episode-2");

        const auto effects = PlayerCompletionController::apply(completion, false, coordinator);
        assertNoEffects(effects);
        assert(coordinator.beginAdjacentEpisodeLookup());
    }

    {
        assert(std::string(PlayerCompletionController::reportStage(PlaybackReportKind::Start)) == "start");
        assert(std::string(PlayerCompletionController::reportStage(PlaybackReportKind::Progress)) == "progress");
        assert(std::string(PlayerCompletionController::reportStage(PlaybackReportKind::PausedProgress)) ==
               "paused-progress");
        assert(std::string(PlayerCompletionController::reportStage(PlaybackReportKind::Stop)) == "stop");

        PlaybackCoordinator coordinator = activeCoordinator();
        PlaybackReportCompletion completion;
        completion.kind = PlaybackReportKind::Stop;
        completion.server = "https://jellyfin.example";
        completion.userId = "user-1";
        completion.result.ok = true;

        PlayerCompletionController::apply(completion, "https://jellyfin.example", "user-1", false, coordinator);
        assert(coordinator.consumeHomeRefreshRequest());
        assert(!coordinator.consumeHomeRefreshRequest());

        PlayerCompletionController::apply(completion, "https://jellyfin.example", "user-1", true, coordinator);
        assert(!coordinator.consumeHomeRefreshRequest());

        PlayerCompletionController::apply(completion, "https://other.example", "user-1", false, coordinator);
        assert(!coordinator.consumeHomeRefreshRequest());

        completion.result.ok = false;
        PlayerCompletionController::apply(completion, "https://jellyfin.example", "user-1", false, coordinator);
        assert(!coordinator.consumeHomeRefreshRequest());
    }

    return 0;
}
