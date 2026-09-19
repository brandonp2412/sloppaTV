#pragma once
#include "browse_screen.hpp"
#include "details_flow.hpp"
#include "home_screen.hpp"
#include "display_mode.hpp"
#include "item_mutation_controller.hpp"
#include "media_player.hpp"
#include "media_session.hpp"
#include "playback_coordinator.hpp"
#include "playback_queue.hpp"
#include "playback_runtime_controller.hpp"
#include "search_screen.hpp"
#include "request_epoch.hpp"
#include "renderer.hpp"
#include "trickplay_preview.hpp"
#include "video_surface.hpp"

#include <string>
#include <unordered_set>

template <typename TelemetryExecutor> class PlaybackReleaseFlow {
public:
    PlaybackReleaseFlow(RequestEpoch& epoch, NativeMediaPlayer& player, VideoSurface& videoSurface,
                        DisplayModeController& displayMode, NativeMediaSession& mediaSession,
                        PlaybackCoordinator& coordinator, PlayerScreenState& playerScreen, JellyfinSession& session,
                        JellyfinHomeData& home, HomeScreenState& homeState, BrowseScreenState& browse,
                        SearchScreenState& search, DetailsFlow& details, PlaybackQueueState& queue,
                        TelemetryExecutor& telemetry, const std::unordered_set<std::string>& hiddenItems,
                        Renderer& renderer, TrickplayPreviewState& trickplay)
        : epoch_(epoch), player_(player), videoSurface_(videoSurface), displayMode_(displayMode),
          mediaSession_(mediaSession), coordinator_(coordinator), playerScreen_(playerScreen), session_(session),
          home_(home), homeState_(homeState), browse_(browse), search_(search), details_(details), queue_(queue),
          telemetry_(telemetry), hiddenItems_(hiddenItems), renderer_(renderer), trickplay_(trickplay) {}

    void release(bool reportStop, bool completed, PlaybackRuntimeController& runtime) {
        epoch_.invalidate();
        if (player_.status() == PlayerStatus::Playing || player_.status() == PlayerStatus::Paused)
            runtime.refreshTelemetry(true);
        const PlaybackReleaseContext release =
            coordinator_.releaseContext(reportStop, completed, session_.valid(), playerScreen_.positionMs());
        const PlaybackReleasePlan plan = release.plan;
        const auto& item = release.item;
        const auto& target = release.target;
        if (!item.id.empty()) {
            JellyfinItem updated = item;
            updated.positionTicks = plan.cachedPositionTicks;
            if (plan.markPlayed) updated.played = true;
            ItemMutationController::updateCachedUserData(home_, homeState_, browse_, search_, details_.state(), queue_,
                                                         updated, excludedFromHome(updated));
            if (details_.item().id == item.id) {
                details_.item().played = updated.played;
                details_.item().positionTicks = updated.positionTicks;
            }
        }
        player_.stop();
        videoSurface_.release();
        displayMode_.restore();
        mediaSession_.clear();
        if (trickplay_.texture() != 0 && trickplay_.textureGeneration() == renderer_.generation())
            renderer_.deleteTexture(trickplay_.texture());
        trickplay_.reset();
        coordinator_.finishRelease();
        playerScreen_.resetPosition();
        if (plan.reportStop) telemetry_.reportStop(session_, item, target, plan.reportTicks);
    }

private:
    bool excludedFromHome(const JellyfinItem& item) const {
        if (item.id.empty()) return false;
        const std::string key = session_.server + "\n" + session_.userId + "\n" + item.id;
        return hiddenItems_.contains(key);
    }

    RequestEpoch& epoch_;
    NativeMediaPlayer& player_;
    VideoSurface& videoSurface_;
    DisplayModeController& displayMode_;
    NativeMediaSession& mediaSession_;
    PlaybackCoordinator& coordinator_;
    PlayerScreenState& playerScreen_;
    JellyfinSession& session_;
    JellyfinHomeData& home_;
    HomeScreenState& homeState_;
    BrowseScreenState& browse_;
    SearchScreenState& search_;
    DetailsFlow& details_;
    PlaybackQueueState& queue_;
    TelemetryExecutor& telemetry_;
    const std::unordered_set<std::string>& hiddenItems_;
    Renderer& renderer_;
    TrickplayPreviewState& trickplay_;
};
