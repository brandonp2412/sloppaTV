#pragma once

#include "account_flow.hpp"
#include "app_screen.hpp"
#include "launch_intent.hpp"
#include "navigation_stack.hpp"
#include "playback_queue.hpp"
#include "request_epoch.hpp"
#include "search_screen.hpp"

#include <chrono>
#include <optional>
#include <string>

struct RuntimeLaunchEffects {
    bool hideTextInput = false;
    bool releasePlayback = false;
    bool triggerSearch = false;
    std::optional<std::string> openedItemId;
    bool openedSearch = false;
};

template <typename Api, typename DetailsScreens> class RuntimeLaunchCoordinator {
public:
    RuntimeLaunchCoordinator(Api& api, RequestEpochs& epochs, JellyfinSession& session, AccountFlow& account,
                             PlaybackQueueState& queue, SearchScreenState& search, DetailsScreens& detailsScreens,
                             NavigationStack<Screen>& navigation, Screen& screen, bool& loading, bool& homeLoading,
                             std::string& pendingDeepLinkItemId, std::string& pendingSearchQuery, std::string& error,
                             std::chrono::steady_clock::time_point& lastInteraction, bool& screensaverActive)
        : api_(api), epochs_(epochs), session_(session), account_(account), queue_(queue), search_(search),
          detailsScreens_(detailsScreens), navigation_(navigation), screen_(screen), loading_(loading),
          homeLoading_(homeLoading), pendingDeepLinkItemId_(pendingDeepLinkItemId),
          pendingSearchQuery_(pendingSearchQuery), error_(error), lastInteraction_(lastInteraction),
          screensaverActive_(screensaverActive) {}

    [[nodiscard]] RuntimeLaunchEffects apply(const LaunchRequest& request, bool playbackActive) {
        RuntimeLaunchEffects effects;
        if (!session_.valid()) {
            if (!request.itemId.empty()) pendingDeepLinkItemId_ = request.itemId;
            if (!request.searchQuery.empty()) pendingSearchQuery_ = request.searchQuery;
            return effects;
        }

        api_.cancelPendingRequests();
        epochs_.invalidateTransient();
        loading_ = false;
        homeLoading_ = false;
        account_.state().endQuickConnect();
        effects.hideTextInput = true;
        effects.releasePlayback = playbackActive;
        navigation_.reset(Screen::Home);
        screen_ = Screen::Home;
        queue_.closeOverlay();
        lastInteraction_ = std::chrono::steady_clock::now();
        screensaverActive_ = false;
        error_.clear();

        if (!request.itemId.empty()) {
            JellyfinItem linked;
            linked.id = request.itemId;
            effects.openedItemId = request.itemId;
            detailsScreens_.openDetails(linked);
        } else if (!request.searchQuery.empty()) {
            search_.setQuery(request.searchQuery);
            search_.setKeyboard(false);
            navigation_.push(Screen::Search);
            screen_ = navigation_.current();
            effects.openedSearch = true;
            effects.triggerSearch = true;
        }
        return effects;
    }

private:
    Api& api_;
    RequestEpochs& epochs_;
    JellyfinSession& session_;
    AccountFlow& account_;
    PlaybackQueueState& queue_;
    SearchScreenState& search_;
    DetailsScreens& detailsScreens_;
    NavigationStack<Screen>& navigation_;
    Screen& screen_;
    bool& loading_;
    bool& homeLoading_;
    std::string& pendingDeepLinkItemId_;
    std::string& pendingSearchQuery_;
    std::string& error_;
    std::chrono::steady_clock::time_point& lastInteraction_;
    bool& screensaverActive_;
};
