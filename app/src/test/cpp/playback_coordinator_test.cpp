#include "playback_coordinator.hpp"

#include <cassert>
#include <chrono>

int main() {
    using namespace std::chrono_literals;

    PlaybackSessionState session;
    PlaybackTelemetryState telemetry;
    PlaybackContinuationState continuation;
    PlaybackQueueState queue;
    const auto now = PlaybackTelemetryState::Clock::now();

    PlaybackCoordinator coordinator;
    JellyfinItem coordinatedItem;
    coordinatedItem.id = "coordinated-item";
    coordinatedItem.type = "Episode";
    coordinatedItem.runtimeTicks = 600'000'000;
    PlaybackTarget coordinatedTarget;
    coordinatedTarget.url = "https://media.example/coordinated-item";
    coordinatedTarget.playMethod = PlaybackMethod::DirectStream;
    coordinator.activate(coordinatedItem, coordinatedTarget, now - 11s);
    assert(coordinator.session().activeItem().id == coordinatedItem.id);
    assert(coordinator.session().activeTarget().url == coordinatedTarget.url);
    assert(coordinator.session().lastPlaybackSummary() == "DirectStream");
    assert(coordinator.tickPlan(false, true, 35000, "Episode", now).reportPlaybackStart);
    assert(coordinator.telemetry().markPlaybackStartReported());
    const auto coordinatedRelease = coordinator.releasePlan(true, false, true, 12345);
    assert(coordinatedRelease.reportStop);
    assert(coordinatedRelease.reportTicks == 123'450'000);
    auto coordinatedProgress = coordinator.progressPlan(true, true, false, false, true, 23456);
    assert(coordinatedProgress.report);
    assert(coordinatedProgress.paused);
    assert(coordinatedProgress.ticks == 234'560'000);
    coordinatedProgress = coordinator.progressPlan(true, true, false, true, false, 23456);
    assert(!coordinatedProgress.report);
    coordinatedProgress = coordinator.progressPlan(true, true, true, true, false, 23456);
    assert(coordinatedProgress.report);
    coordinatedProgress = coordinator.progressPlan(false, true, true, false, false, 23456);
    assert(!coordinatedProgress.report);

    auto restorePlan = coordinator.windowRestorePlan(true, true, true, true, true, true, true);
    assert(restorePlan.restore);
    assert(restorePlan.preservePlayer);
    assert(restorePlan.resumePlayback);
    assert(!restorePlan.pauseAfterRestart);
    restorePlan = coordinator.windowRestorePlan(true, true, true, false, true, true, false);
    assert(restorePlan.restore);
    assert(!restorePlan.preservePlayer);
    assert(!restorePlan.resumePlayback);
    assert(restorePlan.pauseAfterRestart);
    restorePlan = coordinator.windowRestorePlan(false, true, true, true, true, true, true);
    assert(!restorePlan.restore);
    coordinator.session().activeTarget().url.clear();
    restorePlan = coordinator.windowRestorePlan(true, true, true, true, true, true, true);
    assert(!restorePlan.restore);
    coordinator.session().activeTarget() = coordinatedTarget;

    auto suspendPlan = coordinator.windowSuspendPlan(true, true);
    assert(suspendPlan.suspend);
    assert(suspendPlan.resumePlayback);
    suspendPlan = coordinator.windowSuspendPlan(true, false);
    assert(suspendPlan.suspend);
    assert(!suspendPlan.resumePlayback);
    suspendPlan = coordinator.windowSuspendPlan(false, true);
    assert(!suspendPlan.suspend);
    coordinator.session().activeTarget().url.clear();
    suspendPlan = coordinator.windowSuspendPlan(true, true);
    assert(!suspendPlan.suspend);
    coordinator.session().activeTarget() = coordinatedTarget;

    assert(shouldPausePlaybackForFocusLoss(true, true));
    assert(!shouldPausePlaybackForFocusLoss(true, false));
    assert(!shouldPausePlaybackForFocusLoss(false, true));
    const auto mediaSegmentsRequest = coordinator.beginMediaSegmentsRequest(now);
    assert(mediaSegmentsRequest);
    assert(*mediaSegmentsRequest == coordinatedItem.id);
    assert(!coordinator.beginMediaSegmentsRequest(now));
    assert(!coordinator.failMediaSegmentsRequest("stale-item", now));
    assert(coordinator.session().mediaSegmentsRequested());
    assert(coordinator.completeMediaSegmentsRequest(
        coordinatedItem.id, {{.type = "Intro", .startTicks = 10'000'000, .endTicks = 50'000'000}}));
    assert(coordinator.session().mediaSegments().size() == 1);
    assert(coordinator.session().mediaSegments().front().type == "Intro");

    PlaybackCoordinator mediaSegmentsFailureCoordinator;
    mediaSegmentsFailureCoordinator.activate(coordinatedItem, coordinatedTarget, now);
    assert(mediaSegmentsFailureCoordinator.beginMediaSegmentsRequest(now));
    assert(mediaSegmentsFailureCoordinator.failMediaSegmentsRequest(coordinatedItem.id, now));
    assert(!mediaSegmentsFailureCoordinator.beginMediaSegmentsRequest(now + 4999ms));
    assert(mediaSegmentsFailureCoordinator.beginMediaSegmentsRequest(now + 5000ms));

    assert(coordinator.continuation().beginNextEpisodeRequest(now));
    coordinator.transition().setFallbackResolving(true);
    assert(coordinator.tracks().beginSubtitleWork());
    coordinator.finishRelease();
    assert(coordinator.session().activeItem().id.empty());
    assert(!coordinator.telemetry().playbackStartReported());
    assert(!coordinator.session().mediaSegmentsRequested());
    assert(!coordinator.continuation().nextEpisodeRequested());
    assert(!coordinator.transition().fallbackResolving());
    assert(!coordinator.tracks().subtitleBusy());
    assert(!coordinator.consumeHomeRefreshRequest());
    coordinator.markPlaybackStopReported();
    assert(coordinator.consumeHomeRefreshRequest());
    assert(!coordinator.consumeHomeRefreshRequest());

    coordinator.activate(coordinatedItem, coordinatedTarget, now);
    assert(!coordinator.progressPlan(true, true, true, false, false, 1000).report);
    assert(coordinator.telemetry().markPlaybackStartReported());
    coordinator.session().activeTarget().url.clear();
    assert(!coordinator.progressPlan(true, true, true, false, false, 1000).report);

    PlaybackTarget coordinatedFallbackTarget;
    coordinatedFallbackTarget.url = "https://media.example/direct";
    coordinatedFallbackTarget.fallbackTranscodeUrl = "/master.m3u8?TranscodeReasons=ContainerNotSupported";
    coordinator.activate(coordinatedItem, coordinatedFallbackTarget, now - 11s);
    coordinator.beginPreparing(now - 1s);
    assert(coordinator.telemetry().markPlaybackStartReported());
    coordinator.telemetry().markPlaybackRead(now);
    const auto coordinatedFallback = coordinator.fallbackPlan(true, 43210, true);
    assert(coordinatedFallback.retry);
    assert(coordinatedFallback.reportPrevious);
    assert(coordinatedFallback.useOfferedTarget);
    assert(coordinatedFallback.resumeTicks == 432'100'000);
    coordinator.beginFallback();
    assert(!coordinator.session().preparing());
    assert(coordinator.session().fallbackAttempted());
    assert(!coordinator.telemetry().playbackStartReported());
    assert(coordinator.telemetry().shouldReadPlayback(now, false));
    coordinator.useOfferedFallback(coordinatedFallback);
    assert(coordinator.session().activeTarget().url == coordinatedFallbackTarget.fallbackTranscodeUrl);
    assert(coordinator.session().activeTarget().fallbackTranscodeUrl.empty());
    assert(coordinator.session().activeTarget().playMethod == PlaybackMethod::DirectStream);

    PlaybackCoordinator prepareCoordinator;
    PlaybackTarget prepareTarget;
    prepareTarget.url = "https://media.example/prepare";
    prepareTarget.playMethod = PlaybackMethod::DirectPlay;
    prepareTarget.transcoding = false;
    prepareCoordinator.activate(coordinatedItem, prepareTarget, now);
    prepareCoordinator.beginPreparing(now - 15s);
    auto preparePlan = prepareCoordinator.preparePlan(now);
    assert(preparePlan.elapsedMs == 15000);
    assert(!preparePlan.transcoding);
    assert(preparePlan.timedOut);
    assert(preparePlan.retryWithTranscodeFallback);
    prepareCoordinator.finishPreparing();
    assert(!prepareCoordinator.session().preparing());
    prepareTarget.playMethod = PlaybackMethod::Transcode;
    prepareTarget.transcoding = true;
    prepareCoordinator.activate(coordinatedItem, prepareTarget, now);
    prepareCoordinator.beginPreparing(now - 30s);
    preparePlan = prepareCoordinator.preparePlan(now);
    assert(preparePlan.elapsedMs == 30000);
    assert(preparePlan.transcoding);
    assert(preparePlan.timedOut);
    assert(!preparePlan.retryWithTranscodeFallback);

    PlaybackCoordinator subtitleCoordinator;
    JellyfinItem subtitleFallbackItem;
    subtitleFallbackItem.id = "subtitle-fallback";
    subtitleFallbackItem.subtitles = {
        {.index = 7, .codec = "unknown-subtitle-codec", .language = "eng", .title = "English"},
    };
    PlaybackTarget subtitleFallbackTarget;
    subtitleFallbackTarget.url = "https://media.example/subtitle-fallback";
    subtitleFallbackTarget.playMethod = PlaybackMethod::Transcode;
    subtitleCoordinator.activate(subtitleFallbackItem, subtitleFallbackTarget, now);
    subtitleCoordinator.tracks().setSelectedAudioServerIndex(3);
    subtitleCoordinator.tracks().setSelectedSubtitleServerIndex(7);
    auto subtitleFallbackPlan = subtitleCoordinator.subtitleFallbackPlan();
    assert(subtitleFallbackPlan.retry);
    assert(subtitleFallbackPlan.failedSubtitleStreamIndex == 7);
    assert(subtitleFallbackPlan.audioStreamIndex == 3);
    subtitleCoordinator.session().activeTarget().playMethod = PlaybackMethod::DirectPlay;
    assert(!subtitleCoordinator.subtitleFallbackPlan().retry);

    PlaybackCoordinator preferenceCoordinator;
    JellyfinItem preferenceItem;
    preferenceItem.id = "preferences";
    preferenceItem.audios = {
        {.index = 2, .channels = 2, .codec = "aac", .language = "EN", .title = "English", .isDefault = true},
        {.index = 3, .channels = 2, .codec = "aac", .language = "", .title = "Unlabelled", .isDefault = false},
    };
    preferenceItem.subtitles = {
        {.index = 4,
         .codec = "srt",
         .language = "English",
         .title = "English",
         .forced = false,
         .isDefault = true,
         .isExternal = true},
        {.index = 5,
         .codec = "srt",
         .language = "",
         .title = "Unlabelled",
         .forced = false,
         .isDefault = false,
         .isExternal = true},
    };
    preferenceCoordinator.activate(preferenceItem, coordinatedTarget, now);
    preferenceCoordinator.session().activeTarget().playMethod = PlaybackMethod::DirectPlay;
    preferenceCoordinator.selectAudioStream(2);
    preferenceCoordinator.selectSubtitleStream(4);
    const PlaybackTrackSelectionPolicy cyclePolicy{
        .autoSubtitles = false,
        .autoSubtitleLanguage = {},
        .autoSubtitleSourceLanguage = {},
        .allowedSubtitleLanguages = {},
    };
    auto audioCyclePlan = preferenceCoordinator.audioTrackCyclePlan(cyclePolicy);
    assert(audioCyclePlan.available);
    assert(audioCyclePlan.audioStreamIndex == 3);
    assert(audioCyclePlan.subtitleStreamIndex == 4);
    assert(audioCyclePlan.tryEmbeddedSwitch);
    auto subtitleCyclePlan = preferenceCoordinator.subtitleTrackCyclePlan({});
    assert(subtitleCyclePlan.action == PlaybackSubtitleCycleAction::LoadNative);
    assert(subtitleCyclePlan.subtitleStreamIndex == 5);
    preferenceCoordinator.session().activeTarget().playMethod = PlaybackMethod::DirectStream;
    audioCyclePlan = preferenceCoordinator.audioTrackCyclePlan(cyclePolicy);
    assert(!audioCyclePlan.tryEmbeddedSwitch);

    preferenceCoordinator.rememberAudioLanguagePreference(2);
    assert(preferenceCoordinator.tracks().audioLanguagePreference() == std::optional<std::string>{"en"});
    preferenceCoordinator.rememberAudioLanguagePreference(3);
    assert(!preferenceCoordinator.tracks().audioLanguagePreference().has_value());
    preferenceCoordinator.rememberSubtitleLanguagePreference(4);
    assert(preferenceCoordinator.tracks().subtitleLanguagePreference() == std::optional<std::string>{"eng"});
    preferenceCoordinator.rememberSubtitleLanguagePreference(5);
    assert(!preferenceCoordinator.tracks().subtitleLanguagePreference().has_value());
    preferenceCoordinator.rememberSubtitleLanguagePreference(kSubtitleOffIndex);
    assert(preferenceCoordinator.tracks().subtitleLanguagePreference() == std::optional<std::string>{""});

    PlaybackCoordinator subtitleLoadCoordinator;
    subtitleLoadCoordinator.activate(preferenceItem, coordinatedTarget, now);
    subtitleLoadCoordinator.prepareNativeSubtitleLoad(4);
    assert(subtitleLoadCoordinator.tracks().selectedSubtitleServerIndex() == 4);
    assert(subtitleLoadCoordinator.session().activeTarget().subtitleStreamIndex == 4);
    assert(!subtitleLoadCoordinator.tracks().subtitleEnabled());
    assert(subtitleLoadCoordinator.beginSubtitleLoad());
    assert(subtitleLoadCoordinator.tracks().subtitleBusy());
    assert(subtitleLoadCoordinator.subtitleLoadMatches(preferenceItem.id, 4));
    assert(!subtitleLoadCoordinator.subtitleLoadMatches("other-item", 4));
    assert(!subtitleLoadCoordinator.subtitleLoadMatches(preferenceItem.id, 5));
    subtitleLoadCoordinator.completeSubtitleLoad(5, "English", {{.startMs = 100, .endMs = 500, .text = "Hello"}});
    assert(!subtitleLoadCoordinator.tracks().subtitleBusy());
    assert(subtitleLoadCoordinator.tracks().selectedSubtitleServerIndex() == 5);
    assert(subtitleLoadCoordinator.session().activeTarget().subtitleStreamIndex == 5);
    assert(subtitleLoadCoordinator.tracks().subtitleEnabled());
    assert(subtitleLoadCoordinator.tracks().activeSubtitleServerIndex() == 5);
    assert(subtitleLoadCoordinator.tracks().subtitleLanguage() == "English");
    assert(subtitleLoadCoordinator.tracks().subtitleCues().size() == 1);
    subtitleLoadCoordinator.disableSubtitleRendering();
    assert(subtitleLoadCoordinator.tracks().selectedSubtitleServerIndex() == kSubtitleOffIndex);
    assert(subtitleLoadCoordinator.session().activeTarget().subtitleStreamIndex == kSubtitleOffIndex);
    assert(!subtitleLoadCoordinator.tracks().subtitleEnabled());
    assert(subtitleLoadCoordinator.beginSubtitleLoad());
    subtitleLoadCoordinator.failSubtitleLoad();
    assert(!subtitleLoadCoordinator.tracks().subtitleBusy());
    assert(subtitleLoadCoordinator.tracks().selectedSubtitleServerIndex() == kSubtitleOffIndex);

    PlaybackCoordinator restartCoordinator;
    restartCoordinator.activate(coordinatedItem, coordinatedTarget, now);
    assert(restartCoordinator.telemetry().markPlaybackStartReported());
    auto restartPlan = restartCoordinator.beginStreamRestart();
    assert(restartPlan);
    assert(restartPlan->reportPrevious);
    assert(restartPlan->previousTarget.url == coordinatedTarget.url);
    assert(!restartCoordinator.telemetry().playbackStartReported());
    assert(restartCoordinator.tracks().subtitleBusy());
    assert(restartCoordinator.transitionLoading());
    assert(!restartCoordinator.beginStreamRestart());
    restartCoordinator.finishStreamRestartRequest();
    assert(!restartCoordinator.tracks().subtitleBusy());
    assert(!restartCoordinator.transitionLoading());
    restartPlan = restartCoordinator.beginStreamRestart();
    assert(restartPlan);
    assert(!restartPlan->reportPrevious);
    restartCoordinator.finishStreamRestartRequest();

    PlaybackTarget restartTarget;
    restartTarget.url = "https://media.example/restart";
    JellyfinItem restartItem;
    restartItem.id = "restart-item";
    restartCoordinator.stageStreamRestart(restartTarget, restartItem, true, 6);
    auto stagedRestart = restartCoordinator.takePendingTransition();
    assert(stagedRestart);
    assert(stagedRestart->target.url == restartTarget.url);
    assert(stagedRestart->item.id == restartItem.id);
    assert(stagedRestart->streamRestart);
    assert(stagedRestart->restartPaused);
    assert(stagedRestart->audioStreamIndex == 6);
    assert(!restartCoordinator.takePendingTransition());
    restartCoordinator.setPauseAfterRestart(true);
    assert(!restartCoordinator.consumePauseAfterRestart(false));
    assert(restartCoordinator.consumePauseAfterRestart(true));
    assert(!restartCoordinator.consumePauseAfterRestart(true));

    PlaybackCoordinator lifecycleCoordinator;
    lifecycleCoordinator.activate(coordinatedItem, coordinatedTarget, now);
    lifecycleCoordinator.tracks().setAudioLanguagePreference(std::string{"eng"});
    lifecycleCoordinator.tracks().setSubtitleLanguagePreference(std::string{"spa"});
    lifecycleCoordinator.selectAudioStream(11);
    assert(lifecycleCoordinator.tracks().selectedAudioServerIndex() == 11);
    assert(lifecycleCoordinator.session().activeTarget().audioStreamIndex == 11);
    lifecycleCoordinator.selectSubtitleStream(12);
    assert(lifecycleCoordinator.tracks().selectedSubtitleServerIndex() == 12);
    assert(lifecycleCoordinator.session().activeTarget().subtitleStreamIndex == 12);

    PlaybackCoordinator ownershipCoordinator;
    ownershipCoordinator.activate(coordinatedItem, coordinatedTarget, now);
    ownershipCoordinator.setZoomMode(VideoZoomMode::Fill);
    assert(ownershipCoordinator.session().zoomMode() == VideoZoomMode::Fill);
    ownershipCoordinator.recordExternalPlayback("VLC");
    assert(ownershipCoordinator.session().lastPlaybackSummary() == "EXTERNAL / VLC");
    ownershipCoordinator.clearActivePlayback();
    assert(ownershipCoordinator.session().activeItem().id.empty());
    assert(ownershipCoordinator.session().activeTarget().url.empty());
    assert(ownershipCoordinator.session().lastPlaybackSummary() == "EXTERNAL / VLC");

    lifecycleCoordinator.transition().setLoading(true);
    lifecycleCoordinator.finishStop();
    assert(!lifecycleCoordinator.tracks().audioLanguagePreference().has_value());
    assert(!lifecycleCoordinator.tracks().subtitleLanguagePreference().has_value());
    assert(!lifecycleCoordinator.transition().loading());

    lifecycleCoordinator.continuation().setStillWatchingPrompt(true);
    lifecycleCoordinator.transition().setFallbackResolving(true);
    lifecycleCoordinator.tracks().setSelectedAudioServerIndex(4);
    assert(lifecycleCoordinator.telemetry().markPlaybackStartReported());
    lifecycleCoordinator.resetSession();
    assert(lifecycleCoordinator.session().activeItem().id.empty());
    assert(!lifecycleCoordinator.telemetry().playbackStartReported());
    assert(!lifecycleCoordinator.continuation().stillWatchingPrompt());
    assert(!lifecycleCoordinator.transition().fallbackResolving());
    assert(lifecycleCoordinator.tracks().selectedAudioServerIndex() == -1);

    PlaybackCoordinator continuationCoordinator;
    JellyfinItem continuationEpisode;
    continuationEpisode.id = "episode-current";
    continuationEpisode.type = "Episode";
    continuationEpisode.seriesId = "series-1";
    continuationEpisode.parentIndexNumber = 2;
    continuationEpisode.indexNumber = 3;
    continuationCoordinator.activate(continuationEpisode, coordinatedTarget, now);

    JellyfinItem queuedCurrent = continuationEpisode;
    JellyfinItem queuedNext;
    queuedNext.id = "episode-next";
    PlaybackQueueState continuationQueue;
    continuationQueue.replace({queuedCurrent, queuedNext}, 0);
    continuationCoordinator.syncQueueContinuation(continuationQueue);
    assert(continuationCoordinator.continuation().nextItem());
    assert(continuationCoordinator.continuation().nextItem()->id == queuedNext.id);
    assert(continuationCoordinator.useQueueContinuation(continuationQueue));
    assert(continuationCoordinator.continuation().nextEpisodeRequested());
    assert(continuationCoordinator.continuation().nextItem()->id == queuedNext.id);

    continuationCoordinator.continuation().clearNextEpisode();
    const auto nextEpisodeRequest = continuationCoordinator.beginNextEpisodeRequest(now);
    assert(nextEpisodeRequest);
    assert(nextEpisodeRequest->seriesId == continuationEpisode.seriesId);
    assert(nextEpisodeRequest->currentItemId == continuationEpisode.id);
    JellyfinItem resolvedNext;
    resolvedNext.id = "episode-resolved-next";
    assert(!continuationCoordinator.completeNextEpisodeRequest("stale-item", resolvedNext));
    assert(!continuationCoordinator.continuation().nextItem());
    assert(continuationCoordinator.completeNextEpisodeRequest(continuationEpisode.id, resolvedNext));
    assert(continuationCoordinator.continuation().nextItem());
    assert(continuationCoordinator.continuation().nextItem()->id == resolvedNext.id);

    continuationCoordinator.continuation().clearNextEpisode();
    assert(continuationCoordinator.beginNextEpisodeRequest(now));
    assert(!continuationCoordinator.failNextEpisodeRequest("stale-item", now));
    assert(continuationCoordinator.continuation().nextEpisodeRequested());
    assert(continuationCoordinator.failNextEpisodeRequest(continuationEpisode.id, now));
    assert(!continuationCoordinator.continuation().nextEpisodeRequested());
    assert(!continuationCoordinator.beginNextEpisodeRequest(now));
    assert(!continuationCoordinator.beginNextEpisodeRequest(now + 9999ms));
    assert(continuationCoordinator.beginNextEpisodeRequest(now + 10000ms));

    const auto adjacentRequest = continuationCoordinator.beginAdjacentEpisodeLookup();
    assert(adjacentRequest);
    assert(adjacentRequest->currentItemId == continuationEpisode.id);
    assert(adjacentRequest->seriesId == continuationEpisode.seriesId);
    assert(adjacentRequest->currentSeason == continuationEpisode.parentIndexNumber);
    assert(adjacentRequest->currentEpisode == continuationEpisode.indexNumber);
    assert(!continuationCoordinator.beginAdjacentEpisodeLookup());
    assert(!continuationCoordinator.finishAdjacentEpisodeLookup("stale-item"));
    assert(!continuationCoordinator.continuation().adjacentEpisodeLookupInProgress());
    assert(continuationCoordinator.beginAdjacentEpisodeLookup());
    assert(continuationCoordinator.finishAdjacentEpisodeLookup(continuationEpisode.id));
    assert(!continuationCoordinator.continuation().adjacentEpisodeLookupInProgress());

    PlaybackCoordinator telemetryCoordinator;
    JellyfinItem telemetryItem = coordinatedItem;
    telemetryItem.runtimeTicks = 600'000'000;
    telemetryCoordinator.activate(telemetryItem, coordinatedTarget, now - 11s);
    auto telemetryRead = telemetryCoordinator.consumeTelemetryRead(now, false, 0);
    assert(telemetryRead.read);
    assert(!telemetryRead.probeDuration);
    assert(telemetryRead.knownDurationMs == 60000);
    assert(!telemetryCoordinator.consumeTelemetryRead(now, false, 0).read);

    JellyfinItem unknownDurationItem = telemetryItem;
    unknownDurationItem.runtimeTicks = 0;
    telemetryCoordinator.activate(unknownDurationItem, coordinatedTarget, now - 11s);
    telemetryRead = telemetryCoordinator.consumeTelemetryRead(now, false, 0);
    assert(telemetryRead.read);
    assert(telemetryRead.probeDuration);
    assert(telemetryRead.knownDurationMs == 0);
    telemetryRead = telemetryCoordinator.consumeTelemetryRead(now + 300ms, false, 0);
    assert(telemetryRead.read);
    assert(!telemetryRead.probeDuration);
    telemetryRead = telemetryCoordinator.consumeTelemetryRead(now + 3s, false, 0);
    assert(telemetryRead.read);
    assert(telemetryRead.probeDuration);

    PlaybackCoordinator tickCoordinator;
    tickCoordinator.activate(coordinatedItem, coordinatedTarget, now - 11s);
    auto consumedTick = tickCoordinator.consumeTickPlan(false, true, 35000, "Episode", now);
    assert(consumedTick.reportPlaybackStart);
    assert(consumedTick.reportProgress);
    assert(tickCoordinator.telemetry().playbackStartReported());
    consumedTick = tickCoordinator.consumeTickPlan(false, true, 36000, "Episode", now);
    assert(!consumedTick.reportPlaybackStart);
    assert(!consumedTick.reportProgress);

    telemetry.beginPlayback(now - 11s);

    auto plan = planPlaybackTick(false, true, 35000, "Episode", session, telemetry, continuation, now);
    assert(plan.refreshTelemetry);
    assert(plan.requestMediaSegments);
    assert(plan.reportPlaybackStart);
    assert(plan.reportProgress);
    assert(plan.requestNextEpisode);

    assert(telemetry.markPlaybackStartReported());
    telemetry.markProgressReport(now);
    assert(session.beginMediaSegmentsRequest(now));
    assert(continuation.beginNextEpisodeRequest(now));
    plan = planPlaybackTick(false, false, 36000, "Episode", session, telemetry, continuation, now + 1s);
    assert(plan.refreshTelemetry);
    assert(!plan.requestMediaSegments);
    assert(!plan.reportPlaybackStart);
    assert(!plan.reportProgress);
    assert(!plan.requestNextEpisode);

    JellyfinItem releaseItem;
    releaseItem.id = "movie-1";
    releaseItem.runtimeTicks = 900'000'000;
    PlaybackTarget releaseTarget;
    releaseTarget.url = "https://media.example/movie-1";

    JellyfinItem summaryItem;
    PlaybackTarget summaryTarget;
    assert(playbackSummary(summaryTarget, summaryItem) == "DirectPlay");
    summaryTarget.playMethod = PlaybackMethod::DirectStream;
    summaryItem.videoCodec = "hevc";
    assert(playbackSummary(summaryTarget, summaryItem) == "DirectStream / hevc");
    summaryItem.videoWidth = 3840;
    summaryItem.videoHeight = 2160;
    assert(playbackSummary(summaryTarget, summaryItem) == "DirectStream / hevc / 3840X2160");

    auto releasePlan = planPlaybackRelease(true, false, true, true, releaseItem, releaseTarget, 12345);
    assert(releasePlan.reportTicks == 123'450'000);
    assert(releasePlan.cachedPositionTicks == 123'450'000);
    assert(!releasePlan.markPlayed);
    assert(releasePlan.reportStop);

    releasePlan = planPlaybackRelease(true, true, true, true, releaseItem, releaseTarget, 12345);
    assert(releasePlan.reportTicks == releaseItem.runtimeTicks);
    assert(releasePlan.cachedPositionTicks == 0);
    assert(releasePlan.markPlayed);
    assert(releasePlan.reportStop);

    releasePlan = planPlaybackRelease(true, false, false, true, releaseItem, releaseTarget, 12345);
    assert(!releasePlan.reportStop);

    releaseItem.runtimeTicks = 0;
    releasePlan = planPlaybackRelease(false, true, true, true, releaseItem, releaseTarget, 54321);
    assert(releasePlan.reportTicks == 543'210'000);
    assert(releasePlan.cachedPositionTicks == 0);
    assert(releasePlan.markPlayed);
    assert(!releasePlan.reportStop);

    PlaybackTarget fallbackTarget;
    fallbackTarget.url = "https://media.example/direct";
    fallbackTarget.fallbackTranscodeUrl = "/master.m3u8?TranscodeReasons=ContainerNotSupported";
    fallbackTarget.playSessionId = "play-session";
    fallbackTarget.mediaSourceId = "media-source";
    fallbackTarget.audioStreamIndex = 3;
    fallbackTarget.subtitleStreamIndex = 7;

    auto fallbackPlan = planPlaybackFallback(false, PlaybackMethod::DirectPlay, true, "movie-1", fallbackTarget.url,
                                             fallbackTarget.fallbackTranscodeUrl, true, 43210, true);
    assert(fallbackPlan.retry);
    assert(fallbackPlan.resumeTicks == 432'100'000);
    assert(fallbackPlan.reportPrevious);
    assert(fallbackPlan.useOfferedTarget);
    assert(fallbackPlan.offeredDirectStream);
    assert(!fallbackPlan.forceServerStream);
    assert(!fallbackPlan.forceTranscode);

    auto offeredTarget = offeredPlaybackFallbackTarget(fallbackTarget, fallbackPlan);
    assert(offeredTarget.url == fallbackTarget.fallbackTranscodeUrl);
    assert(offeredTarget.fallbackTranscodeUrl.empty());
    assert(offeredTarget.transcoding);
    assert(offeredTarget.playMethod == PlaybackMethod::DirectStream);
    assert(offeredTarget.startTicks == fallbackPlan.resumeTicks);
    assert(offeredTarget.playSessionId == fallbackTarget.playSessionId);
    assert(offeredTarget.mediaSourceId == fallbackTarget.mediaSourceId);
    assert(offeredTarget.audioStreamIndex == fallbackTarget.audioStreamIndex);
    assert(offeredTarget.subtitleStreamIndex == fallbackTarget.subtitleStreamIndex);

    fallbackPlan = planPlaybackFallback(false, PlaybackMethod::DirectPlay, true, "movie-1", fallbackTarget.url, {},
                                        false, 12345, false);
    assert(fallbackPlan.retry);
    assert(!fallbackPlan.reportPrevious);
    assert(!fallbackPlan.useOfferedTarget);
    assert(!fallbackPlan.offeredDirectStream);
    assert(!fallbackPlan.forceServerStream);
    assert(fallbackPlan.forceTranscode);

    fallbackPlan = planPlaybackFallback(false, PlaybackMethod::DirectStream, true, "movie-1", fallbackTarget.url, {},
                                        true, 12345, true);
    assert(fallbackPlan.retry);
    assert(fallbackPlan.forceServerStream);
    assert(!fallbackPlan.forceTranscode);

    assert(!planPlaybackFallback(true, PlaybackMethod::DirectPlay, true, "movie-1", fallbackTarget.url,
                                 fallbackTarget.fallbackTranscodeUrl, true, 1000, false)
                .retry);
    assert(!planPlaybackFallback(false, PlaybackMethod::Transcode, true, "movie-1", fallbackTarget.url, {}, true, 1000,
                                 false)
                .retry);
    assert(!planPlaybackFallback(false, PlaybackMethod::DirectPlay, false, "movie-1", fallbackTarget.url, {}, true,
                                 1000, false)
                .retry);
    assert(!planPlaybackFallback(false, PlaybackMethod::DirectPlay, true, {}, fallbackTarget.url, {}, true, 1000, false)
                .retry);

    PlaybackCoordinator fallbackCoordinator;
    fallbackCoordinator.beginFallbackResolution();
    assert(fallbackCoordinator.fallbackResolving());
    fallbackCoordinator.finishFallbackResolution();
    assert(!fallbackCoordinator.fallbackResolving());
    JellyfinItem fallbackItem;
    fallbackItem.id = "fallback-item";
    PlaybackTarget resolvedFallbackTarget;
    resolvedFallbackTarget.url = "https://media.example/fallback";
    fallbackCoordinator.beginFallbackResolution();
    fallbackCoordinator.stageResolvedFallback(resolvedFallbackTarget, fallbackItem, 4);
    assert(!fallbackCoordinator.fallbackResolving());
    auto fallbackTransition = fallbackCoordinator.takePendingTransition();
    assert(fallbackTransition);
    assert(fallbackTransition->streamRestart);
    assert(!fallbackTransition->restartPaused);
    assert(fallbackTransition->audioStreamIndex == 4);
    assert(fallbackTransition->target.url == resolvedFallbackTarget.url);
    assert(fallbackTransition->item.id == fallbackItem.id);

    JellyfinItem transitionItem;
    transitionItem.runtimeTicks = 900'000'000;
    transitionItem.audios = {
        {.index = 1, .channels = 2, .codec = "aac", .language = "eng", .title = "Track 1", .isDefault = false},
        {.index = 3, .channels = 2, .codec = "aac", .language = "eng", .title = "Track 3", .isDefault = true},
    };
    PlaybackTarget transitionTarget;
    transitionTarget.startTicks = 420'000'000;
    transitionTarget.audioStreamIndex = -1;
    transitionTarget.subtitleStreamIndex = 8;

    auto transitionPlan = planPlaybackTransition(transitionTarget, transitionItem, false, true, -1);
    assert(transitionPlan.startPositionMs == 42000);
    assert(transitionPlan.durationMs == 90000);
    assert(transitionPlan.selectedAudioServerIndex == 3);
    assert(transitionPlan.selectedSubtitleServerIndex == 8);
    assert(!transitionPlan.pauseAfterRestart);
    assert(transitionPlan.resetContinuation);
    assert(transitionPlan.resetMediaSegments);

    PlaybackCoordinator transitionCoordinator;
    transitionItem.id = "transition-item";
    transitionTarget.url = "https://media.example/transition";
    assert(transitionCoordinator.session().beginMediaSegmentsRequest(now));
    assert(transitionCoordinator.continuation().beginNextEpisodeRequest(now));
    transitionCoordinator.transition().setLoading(true);
    transitionCoordinator.tracks().setSelectedAudioServerIndex(99);
    auto activatedTransition = transitionCoordinator.activateTransition(transitionItem, transitionTarget, false, true,
                                                                        -1, VideoZoomMode::Fill, now);
    assert(transitionCoordinator.session().activeItem().id == transitionItem.id);
    assert(transitionCoordinator.session().activeTarget().url == transitionTarget.url);
    assert(transitionCoordinator.session().zoomMode() == VideoZoomMode::Fill);
    assert(!transitionCoordinator.session().mediaSegmentsRequested());
    assert(!transitionCoordinator.continuation().nextEpisodeRequested());
    assert(!transitionCoordinator.transition().loading());
    assert(!transitionCoordinator.transition().pauseAfterRestart());
    assert(transitionCoordinator.tracks().selectedAudioServerIndex() == activatedTransition.selectedAudioServerIndex);
    assert(transitionCoordinator.tracks().selectedSubtitleServerIndex() ==
           activatedTransition.selectedSubtitleServerIndex);

    transitionPlan = planPlaybackTransition(transitionTarget, transitionItem, true, true, 7);
    assert(transitionPlan.selectedAudioServerIndex == 7);
    assert(transitionPlan.pauseAfterRestart);
    assert(!transitionPlan.resetContinuation);
    assert(!transitionPlan.resetMediaSegments);

    PlaybackCoordinator restartTransitionCoordinator;
    assert(restartTransitionCoordinator.session().beginMediaSegmentsRequest(now));
    assert(restartTransitionCoordinator.continuation().beginNextEpisodeRequest(now));
    restartTransitionCoordinator.transition().setLoading(true);
    auto activatedRestart = restartTransitionCoordinator.activateTransition(transitionItem, transitionTarget, true,
                                                                            true, 7, VideoZoomMode::Stretch, now);
    assert(activatedRestart.pauseAfterRestart);
    assert(restartTransitionCoordinator.transition().pauseAfterRestart());
    assert(restartTransitionCoordinator.transition().loading() == false);
    assert(restartTransitionCoordinator.session().mediaSegmentsRequested());
    assert(restartTransitionCoordinator.continuation().nextEpisodeRequested());
    assert(restartTransitionCoordinator.session().zoomMode() == VideoZoomMode::Stretch);
    assert(restartTransitionCoordinator.tracks().selectedAudioServerIndex() == 7);

    PlaybackCoordinator resolutionCoordinator;
    resolutionCoordinator.continuation().incrementAutoplayChain();
    resolutionCoordinator.continuation().setStillWatchingPrompt(true);
    resolutionCoordinator.resetContinuationPrompt();
    assert(resolutionCoordinator.continuation().autoplayChainCount() == 0);
    assert(!resolutionCoordinator.continuation().stillWatchingPrompt());
    resolutionCoordinator.continuation().incrementAutoplayChain();
    resolutionCoordinator.showStillWatchingPrompt();
    assert(resolutionCoordinator.continuation().autoplayChainCount() == 0);
    assert(resolutionCoordinator.continuation().stillWatchingPrompt());
    resolutionCoordinator.continuation().incrementAutoplayChain();
    resolutionCoordinator.dismissStillWatchingPrompt();
    assert(resolutionCoordinator.continuation().autoplayChainCount() == 1);
    assert(!resolutionCoordinator.continuation().stillWatchingPrompt());
    resolutionCoordinator.resetAutoplayChain();
    assert(resolutionCoordinator.continuation().autoplayChainCount() == 0);

    resolutionCoordinator.tracks().setAudioLanguagePreference(std::string{"eng"});
    resolutionCoordinator.tracks().setSubtitleLanguagePreference(std::string{"spa"});
    resolutionCoordinator.continuation().incrementAutoplayChain();
    resolutionCoordinator.continuation().setStillWatchingPrompt(true);
    resolutionCoordinator.beginUserPlayback(true);
    assert(resolutionCoordinator.tracks().audioLanguagePreference() == std::optional<std::string>{"eng"});
    assert(resolutionCoordinator.tracks().subtitleLanguagePreference() == std::optional<std::string>{"spa"});
    assert(resolutionCoordinator.continuation().autoplayChainCount() == 0);
    assert(!resolutionCoordinator.continuation().stillWatchingPrompt());
    resolutionCoordinator.continuation().setStillWatchingPrompt(true);
    resolutionCoordinator.beginUserPlayback(false);
    assert(!resolutionCoordinator.tracks().audioLanguagePreference().has_value());
    assert(!resolutionCoordinator.tracks().subtitleLanguagePreference().has_value());
    assert(!resolutionCoordinator.continuation().stillWatchingPrompt());

    resolutionCoordinator.continuation().setStillWatchingPrompt(true);
    resolutionCoordinator.beginPlaybackResolution(false);
    assert(!resolutionCoordinator.transition().loading());
    assert(!resolutionCoordinator.continuation().stillWatchingPrompt());
    resolutionCoordinator.beginPlaybackResolution(true);
    assert(resolutionCoordinator.transition().loading());
    resolutionCoordinator.finishPlaybackResolution();
    assert(!resolutionCoordinator.transition().loading());

    PlaybackTarget resolvedTarget;
    resolvedTarget.url = "https://media.example/resolved";
    JellyfinItem resolvedItem;
    resolvedItem.id = "resolved-item";
    resolutionCoordinator.beginAutoplayResolution();
    assert(resolutionCoordinator.transition().loading());
    assert(resolutionCoordinator.continuation().autoplayChainCount() == 1);
    resolutionCoordinator.finishPlaybackResolution();
    resolutionCoordinator.stageResolvedPlayback(resolvedTarget, resolvedItem);
    assert(!resolutionCoordinator.transition().loading());
    assert(resolutionCoordinator.transition().hasPending());
    auto resolvedTransition = resolutionCoordinator.transition().take();
    assert(resolvedTransition);
    assert(resolvedTransition->target.url == resolvedTarget.url);
    assert(resolvedTransition->item.id == resolvedItem.id);

    transitionTarget.audioStreamIndex = 9;
    transitionPlan = planPlaybackTransition(transitionTarget, transitionItem, true, false, -1);
    assert(transitionPlan.selectedAudioServerIndex == 9);
    assert(!transitionPlan.pauseAfterRestart);

    transitionTarget.audioStreamIndex = -1;
    transitionItem.audios = {
        {.index = 5, .channels = 2, .codec = "aac", .language = "eng", .title = "Track 5", .isDefault = false},
        {.index = 6, .channels = 2, .codec = "aac", .language = "eng", .title = "Track 6", .isDefault = false},
    };
    transitionPlan = planPlaybackTransition(transitionTarget, transitionItem, false, false, -1);
    assert(transitionPlan.selectedAudioServerIndex == 5);

    transitionItem.audios.clear();
    transitionPlan = planPlaybackTransition(transitionTarget, transitionItem, false, false, -1);
    assert(transitionPlan.selectedAudioServerIndex == -1);

    JellyfinItem special;
    special.id = "special-1";
    special.name = "Special";
    special.parentIndexNumber = 0;
    special.indexNumber = 1;
    JellyfinItem currentEpisode;
    currentEpisode.id = "episode-1";
    currentEpisode.name = "Episode 1";
    currentEpisode.parentIndexNumber = 1;
    currentEpisode.indexNumber = 1;
    JellyfinItem duplicateCurrent;
    duplicateCurrent.id = "episode-1-alt";
    duplicateCurrent.name = "Episode 1 alternate";
    duplicateCurrent.parentIndexNumber = 1;
    duplicateCurrent.indexNumber = 1;
    JellyfinItem nextEpisode;
    nextEpisode.id = "episode-2";
    nextEpisode.name = "Episode 2";
    nextEpisode.parentIndexNumber = 1;
    nextEpisode.indexNumber = 2;

    auto adjacent =
        selectAdjacentPlaybackEpisode({nextEpisode, duplicateCurrent, special, currentEpisode}, currentEpisode.id,
                                      currentEpisode.parentIndexNumber, currentEpisode.indexNumber, 1);
    assert(adjacent);
    assert(adjacent->id == nextEpisode.id);

    adjacent =
        selectAdjacentPlaybackEpisode({nextEpisode, duplicateCurrent, special, currentEpisode}, "missing-current-id",
                                      currentEpisode.parentIndexNumber, currentEpisode.indexNumber, 1);
    assert(adjacent);
    assert(adjacent->id == nextEpisode.id);

    adjacent = selectAdjacentPlaybackEpisode({special, currentEpisode}, currentEpisode.id,
                                             currentEpisode.parentIndexNumber, currentEpisode.indexNumber, -1);
    assert(!adjacent);
    assert(!selectAdjacentPlaybackEpisode({currentEpisode, nextEpisode}, currentEpisode.id,
                                          currentEpisode.parentIndexNumber, currentEpisode.indexNumber, 0));
    assert(!selectAdjacentPlaybackEpisode({currentEpisode, nextEpisode}, "missing", -1, -1, 1));

    JellyfinItem episode1;
    episode1.id = "episode-1";
    JellyfinItem episode2;
    episode2.id = "episode-2";
    queue.replace({episode1, episode2}, 0);
    queue.setRepeatMode(QueueRepeatMode::One);
    auto continuationPlan = planPlaybackContinuation(true, 120000, 120000, queue, continuation, true, 3);
    assert(continuationPlan.action == PlaybackContinuationAction::PlayQueueIndex);
    assert(continuationPlan.queueIndex == 0);
    assert(continuationPlan.repeatCurrentQueueItem);
    assert(!continuationPlan.resetAutoplayChain);

    queue.setRepeatMode(QueueRepeatMode::All);
    queue.setCurrentIndex(1);
    continuationPlan = planPlaybackContinuation(false, 119500, 120000, queue, continuation, true, 3);
    assert(continuationPlan.action == PlaybackContinuationAction::PlayQueueIndex);
    assert(continuationPlan.queueIndex == 0);
    assert(!continuationPlan.repeatCurrentQueueItem);

    queue.setRepeatMode(QueueRepeatMode::Off);
    continuation.clearNextEpisodeRequest();
    continuation.setNextItem(episode2);
    continuationPlan = planPlaybackContinuation(false, 119500, 120000, queue, continuation, true, 3);
    assert(continuationPlan.action == PlaybackContinuationAction::AutoplayNext);

    continuation.incrementAutoplayChain();
    continuation.incrementAutoplayChain();
    continuation.incrementAutoplayChain();
    continuationPlan = planPlaybackContinuation(false, 119500, 120000, queue, continuation, true, 3);
    assert(continuationPlan.action == PlaybackContinuationAction::ShowStillWatching);

    continuation.clearNextItem();
    continuationPlan = planPlaybackContinuation(true, 0, 0, queue, continuation, true, 3);
    assert(continuationPlan.action == PlaybackContinuationAction::Stop);
    assert(continuationPlan.resetAutoplayChain);

    queue.reset();
    continuationPlan = planPlaybackContinuation(false, 0, 1000, queue, continuation, true, 3);
    assert(continuationPlan.action == PlaybackContinuationAction::None);
    assert(!continuationPlan.resetAutoplayChain);

    coordinator.continuation().setNextItem(episode2);
    continuationPlan = coordinator.continuationPlan(true, 120000, 120000, queue, true, 3);
    assert(continuationPlan.action == PlaybackContinuationAction::AutoplayNext);

    return 0;
}
