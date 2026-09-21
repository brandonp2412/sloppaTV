#pragma once

#include "account_flow.hpp"
#include "app_screen.hpp"
#include "app_settings.hpp"
#include "launch_intent.hpp"
#include "media_player.hpp"
#include "navigation_stack.hpp"
#include "request_epoch.hpp"
#include "screen_navigation_key.hpp"
#include "status_overlay_renderer.hpp"

#include <chrono>
#include <optional>
#include <string>
#include <unordered_set>
#include <utility>

template <typename Api, typename Player, typename PlaybackRequests, typename PlaybackCoordinatorT,
          typename ExternalPlaybackStateT, typename PlayerScreenStateT, typename TrickplayCoordinatorT,
          typename QueueState, typename Artwork, typename RendererT, typename HomeScreens, typename BrowseScreens,
          typename SearchScreens, typename DetailsScreens, typename ServerInfoScreens, typename SeerrApp>
class AccountSessionCoordinator {
public:
    AccountSessionCoordinator(Api& api, RequestEpochs& requestEpochs, Screen& screen,
                              NavigationStack<Screen>& navigation, Player& player, PlaybackRequests& playbackRequests,
                              PlaybackCoordinatorT& playbackCoordinator, ExternalPlaybackStateT& externalPlaybackState,
                              PlayerScreenStateT& playerScreenState, TrickplayCoordinatorT& trickplayCoordinator,
                              QueueState& queueState, Artwork& artwork, RendererT& renderer, AccountFlow& accountFlow,
                              JellyfinSession& session, AppSettings& settings,
                              std::unordered_set<std::string>& hiddenHomeItems, std::string& dataPath,
                              HomeScreens& homeScreens, BrowseScreens& browseScreens, SearchScreens& searchScreens,
                              DetailsScreens& detailsScreens, ServerInfoScreens& serverInfoScreens, SeerrApp& seerrApp,
                              bool& loading, std::string& error, StatusOverlayState& statusOverlayState,
                              bool& screensaverActive, std::chrono::steady_clock::time_point& lastInteraction,
                              std::string& pendingDeepLinkItemId, std::string& pendingSearchQuery,
                              std::optional<LaunchRequest>& pendingRuntimeLaunchRequest)
        : api_(api), requestEpochs_(requestEpochs), screen_(screen), navigation_(navigation), player_(player),
          playbackRequests_(playbackRequests), playbackCoordinator_(playbackCoordinator),
          externalPlaybackState_(externalPlaybackState), playerScreenState_(playerScreenState),
          trickplayCoordinator_(trickplayCoordinator), queueState_(queueState), artwork_(artwork), renderer_(renderer),
          accountFlow_(accountFlow), session_(session), settings_(settings), hiddenHomeItems_(hiddenHomeItems),
          dataPath_(dataPath), homeScreens_(homeScreens), browseScreens_(browseScreens), searchScreens_(searchScreens),
          detailsScreens_(detailsScreens), serverInfoScreens_(serverInfoScreens), seerrApp_(seerrApp),
          loading_(loading), error_(error), statusOverlayState_(statusOverlayState),
          screensaverActive_(screensaverActive), lastInteraction_(lastInteraction),
          pendingDeepLinkItemId_(pendingDeepLinkItemId), pendingSearchQuery_(pendingSearchQuery),
          pendingRuntimeLaunchRequest_(pendingRuntimeLaunchRequest) {}

    void load() {
        AccountPersistedState restored = accountFlow_.restore(dataPath_);
        session_ = std::move(restored.session);
        hiddenHomeItems_ = std::move(restored.hiddenHomeItems);
        settings_ = std::move(restored.settings);
        if (session_.valid()) artwork_.eraseProfile(session_, renderer_);
        playbackCoordinator_.setZoomMode(static_cast<VideoZoomMode>(settings_.zoomMode));
        warning_ = std::move(restored.warning);
    }

    [[nodiscard]] std::string takeWarning() { return std::exchange(warning_, {}); }

    void save() {
        if (session_.valid()) artwork_.eraseProfile(session_, renderer_);
        std::string warning;
        if (!accountFlow_.persist(dataPath_, session_, hiddenHomeItems_, settings_, warning) && !warning.empty())
            warning_ = std::move(warning);
    }

    void clearCurrentSessionUi() {
        const bool hadAuthenticatedSession = session_.valid();
        api_.cancelPendingRequests();
        requestEpochs_.invalidateAll();
        if (screen_ == Screen::Player || player_.status() != PlayerStatus::Idle ||
            playbackCoordinator_.activeItemAvailable()) {
            playbackRequests_.release(true);
        }

        queueState_.reset();
        playbackCoordinator_.resetSession();
        externalPlaybackState_.reset();
        playerScreenState_.resetSession();
        if (const auto texture = trickplayCoordinator_.clear(renderer_.generation())) renderer_.deleteTexture(*texture);

        if (hadAuthenticatedSession) {
            pendingDeepLinkItemId_.clear();
            pendingSearchQuery_.clear();
            pendingRuntimeLaunchRequest_.reset();
        }
        session_ = {};
        seerrApp_.resetForSessionChange();
        serverInfoScreens_.resetForSessionChange();
        homeScreens_.resetForSessionChange();
        browseScreens_.resetForSessionChange();
        searchScreens_.resetForSessionChange();
        detailsScreens_.resetForSessionChange();

        artwork_.clearSession(renderer_);
        accountFlow_.state().clearSessionUi();
        loading_ = false;
        error_.clear();
        statusOverlayState_.clearNotice();
        screensaverActive_ = false;
        lastInteraction_ = std::chrono::steady_clock::now();
    }

    void openProfiles() {
        if (!accountFlow_.beginProfiles()) {
            startAddAccount();
            return;
        }
        navigation_.push(Screen::Profiles);
        screen_ = navigation_.current();
        error_.clear();
    }

    void startAddAccount() {
        const std::string existingServer = session_.server;
        clearCurrentSessionUi();
        accountFlow_.beginAddAccount(existingServer);
        navigation_.reset(Screen::Login);
        screen_ = Screen::Login;
        save();
    }

    void handleProfiles(ScreenNavigationKey key) {
        AccountProfileCommand command = accountFlow_.handleProfiles(key, session_);
        switch (command.action) {
        case AccountProfileAction::None:
            return;
        case AccountProfileAction::Back:
            screen_ = navigation_.popOr(Screen::Login);
            return;
        case AccountProfileAction::AddAccount:
            startAddAccount();
            return;
        case AccountProfileAction::SwitchSession:
            if (!command.session) return;
            clearCurrentSessionUi();
            session_ = std::move(*command.session);
            accountFlow_.activateSession(session_);
            navigation_.reset(Screen::Home);
            screen_ = Screen::Home;
            homeScreens_.focusInitialForSession();
            save();
            loadHome();
            return;
        case AccountProfileAction::ForgetSession:
            if (!command.removedSession) return;
            artwork_.eraseProfile(*command.removedSession, renderer_);
            if (command.removedCurrent) {
                clearCurrentSessionUi();
                navigation_.reset(Screen::Profiles);
                screen_ = Screen::Profiles;
            }
            save();
            if (command.sessionsEmpty) startAddAccount();
            return;
        }
    }

    void applyAuthenticatedSession(std::optional<JellyfinSession> authenticatedSession) {
        if (!authenticatedSession) return;
        requestEpochs_.session.invalidate();
        session_ = std::move(*authenticatedSession);
        navigation_.reset(Screen::Home);
        screen_ = Screen::Home;
        homeScreens_.focusInitialForSession();
        save();
        loadHome();
    }

private:
    void loadHome() {
        serverInfoScreens_.requestNotice();
        homeScreens_.load();
    }

    Api& api_;
    RequestEpochs& requestEpochs_;
    Screen& screen_;
    NavigationStack<Screen>& navigation_;
    Player& player_;
    PlaybackRequests& playbackRequests_;
    PlaybackCoordinatorT& playbackCoordinator_;
    ExternalPlaybackStateT& externalPlaybackState_;
    PlayerScreenStateT& playerScreenState_;
    TrickplayCoordinatorT& trickplayCoordinator_;
    QueueState& queueState_;
    Artwork& artwork_;
    RendererT& renderer_;
    AccountFlow& accountFlow_;
    JellyfinSession& session_;
    AppSettings& settings_;
    std::unordered_set<std::string>& hiddenHomeItems_;
    std::string& dataPath_;
    HomeScreens& homeScreens_;
    BrowseScreens& browseScreens_;
    SearchScreens& searchScreens_;
    DetailsScreens& detailsScreens_;
    ServerInfoScreens& serverInfoScreens_;
    SeerrApp& seerrApp_;
    bool& loading_;
    std::string& error_;
    StatusOverlayState& statusOverlayState_;
    bool& screensaverActive_;
    std::chrono::steady_clock::time_point& lastInteraction_;
    std::string& pendingDeepLinkItemId_;
    std::string& pendingSearchQuery_;
    std::optional<LaunchRequest>& pendingRuntimeLaunchRequest_;
    std::string warning_;
};
