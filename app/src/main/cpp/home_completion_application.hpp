#pragma once

#include "account_flow.hpp"
#include "app_screen.hpp"
#include "app_settings.hpp"
#include "content_mutation_flow.hpp"
#include "details_flow.hpp"
#include "home_screen_coordinator.hpp"
#include "navigation_stack.hpp"
#include "request_epoch.hpp"
#include "seerr_domain.hpp"

#include <optional>
#include <string>
#include <utility>

struct HomeCompletionApplicationEffects {
    std::optional<int> retryDelaySeconds;
    std::optional<std::string> retryError;
    std::optional<long long> primaryReadyMs;
    std::optional<long long> secondaryReadyMs;
    std::optional<JellyfinSession> eraseProfile;
    bool persistSession = false;
    std::optional<std::string> openedItemId;
    bool openedSearch = false;
    bool triggerSearch = false;
    bool refreshSeerrPending = false;
    bool sessionExpired = false;
};

template <typename HomeScreens, typename DetailsScreens> class HomeCompletionApplication {
public:
    HomeCompletionApplication(HomeScreens& homeScreens, DetailsScreens& detailsScreens, JellyfinSession& session,
                              AccountFlow& account, RequestEpochs& epochs, ContentMutationFlow& mutations,
                              SearchScreenState& search, AppSettings& settings, SeerrDomainState& seerr,
                              NavigationStack<Screen>& navigation, Screen& screen, bool& loading, std::string& error)
        : homeScreens_(homeScreens), detailsScreens_(detailsScreens), session_(session), account_(account),
          epochs_(epochs), mutations_(mutations), search_(search), settings_(settings), seerr_(seerr),
          navigation_(navigation), screen_(screen), loading_(loading), error_(error) {}

    [[nodiscard]] HomeCompletionApplicationEffects complete(HomeCoreCompletion& completion) {
        HomeCompletionApplicationEffects host;
        HomeCoreScreenEffects effects = homeScreens_.complete(completion);
        if (!effects.active) return host;
        host.retryDelaySeconds = effects.retryDelaySeconds;
        if (effects.retryDelaySeconds) host.retryError = completion.result.error;

        if (effects.sessionExpired) {
            host.eraseProfile = session_;
            (void)account_.removeIdentity(session_);
            epochs_.invalidateAll();
            loading_ = false;
            mutations_.reset();
            search_.setLoading(false);
            session_.token.clear();
            session_.userId.clear();
            settings_.seerrSessionCookie.clear();
            seerr_.invalidateStorageTargets();
            navigation_.reset(Screen::Login);
            screen_ = Screen::Login;
            error_ = "SESSION EXPIRED - LOG IN AGAIN";
            host.persistSession = true;
            host.sessionExpired = true;
            return host;
        }

        host.primaryReadyMs = effects.readyMs;
        if (effects.openDetailsItem) {
            host.openedItemId = effects.openDetailsItem->id;
            detailsScreens_.openDetails(*effects.openDetailsItem);
            return host;
        }
        if (effects.searchQuery) {
            search_.setQuery(std::move(*effects.searchQuery));
            search_.setKeyboard(false);
            navigation_.push(Screen::Search);
            screen_ = navigation_.current();
            host.openedSearch = true;
            host.triggerSearch = true;
            return host;
        }

        if (effects.refreshSeerrPending) {
            host.refreshSeerrPending = true;
            homeScreens_.loadSecondary(completion, std::move(effects));
        }
        return host;
    }

    [[nodiscard]] HomeCompletionApplicationEffects complete(HomeSecondaryCompletion& completion) {
        HomeCompletionApplicationEffects host;
        HomeSecondaryScreenEffects effects = homeScreens_.complete(completion);
        if (effects.active) host.secondaryReadyMs = effects.readyMs;
        return host;
    }

private:
    HomeScreens& homeScreens_;
    DetailsScreens& detailsScreens_;
    JellyfinSession& session_;
    AccountFlow& account_;
    RequestEpochs& epochs_;
    ContentMutationFlow& mutations_;
    SearchScreenState& search_;
    AppSettings& settings_;
    SeerrDomainState& seerr_;
    NavigationStack<Screen>& navigation_;
    Screen& screen_;
    bool& loading_;
    std::string& error_;
};
