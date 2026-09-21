#pragma once

#include "account_screen_coordinator.hpp"
#include "account_session_coordinator.hpp"
#include "browse_screen_coordinator.hpp"
#include "details_screen_coordinator.hpp"
#include "home_screen_coordinator.hpp"
#include "input_navigation.hpp"
#include "native_text_input_bridge.hpp"
#include "search_screen_coordinator.hpp"
#include "seerr_app_coordinator.hpp"
#include "server_info_screen_coordinator.hpp"
#include "settings_screen_coordinator.hpp"
#include "similar_prefetch_controller.hpp"
#include "system_text_input_controller.hpp"

#include <algorithm>
#include <array>
#include <chrono>
#include <optional>
#include <string>
#include <utility>
#include <vector>

struct ScreenInteractionNotice {
    std::string message;
    std::chrono::seconds duration{6};
    bool persistent = false;
};

struct ScreenInteractionHostEffects {
    bool finishActivity = false;
    bool wakeLooper = false;
    bool persistSession = false;
    std::vector<ScreenInteractionNotice> notices;
    std::vector<SeerrCompletionHostEffects> seerrEffects;

    void merge(ScreenInteractionHostEffects other) {
        finishActivity = finishActivity || other.finishActivity;
        wakeLooper = wakeLooper || other.wakeLooper;
        persistSession = persistSession || other.persistSession;
        for (auto& notice : other.notices) notices.push_back(std::move(notice));
        for (auto& effect : other.seerrEffects) seerrEffects.push_back(std::move(effect));
    }
};

template <typename Api, typename TextInput, typename AccountScreens, typename AccountSession, typename HomeScreens,
          typename BrowseScreens, typename SearchScreens, typename SearchFlowT, typename SettingsScreens,
          typename ServerInfoScreens, typename DetailsScreens, typename SeerrApp, typename PlaybackRequests,
          typename ExternalPlayback, typename SimilarPrefetch, typename ExternalPlayer>
class AppScreenInteractionCoordinator {
public:
    AppScreenInteractionCoordinator(Api& api, TextInput& textInput, AccountScreens& accountScreens,
                                    AccountSession& accountSession, HomeScreens& homeScreens,
                                    BrowseScreens& browseScreens, SearchScreens& searchScreens, SearchFlowT& searchFlow,
                                    SettingsScreens& settingsScreens, ServerInfoScreens& serverInfoScreens,
                                    DetailsScreens& detailsScreens, SeerrApp& seerrApp,
                                    PlaybackRequests& playbackRequests, ExternalPlayback& externalPlayback,
                                    SimilarPrefetch& similarPrefetch, ExternalPlayer& externalPlayer, Screen& screen,
                                    NavigationStack<Screen>& navigation, JellyfinSession& session,
                                    AppSettings& settings, SettingsFlow& settingsFlow, DetailsFlow& detailsFlow,
                                    VirtualKeyboardState& keyboard, AccountFlow& accountFlow, bool& loading,
                                    std::string& error, std::chrono::steady_clock::time_point& renderBurstUntil)
        : api_(api), textInput_(textInput), accountScreens_(accountScreens), accountSession_(accountSession),
          homeScreens_(homeScreens), browseScreens_(browseScreens), searchScreens_(searchScreens),
          searchFlow_(searchFlow), settingsScreens_(settingsScreens), serverInfoScreens_(serverInfoScreens),
          detailsScreens_(detailsScreens), seerrApp_(seerrApp), playbackRequests_(playbackRequests),
          externalPlayback_(externalPlayback), similarPrefetch_(similarPrefetch), externalPlayer_(externalPlayer),
          screen_(screen), navigation_(navigation), session_(session), settings_(settings), settingsFlow_(settingsFlow),
          detailsFlow_(detailsFlow), keyboard_(keyboard), accountFlow_(accountFlow), loading_(loading), error_(error),
          renderBurstUntil_(renderBurstUntil) {}

    [[nodiscard]] ScreenInteractionHostEffects scheduleLiveSearch() {
        ScreenInteractionHostEffects host;
        if (searchScreens_.scheduleLive(seerrApp_.endpoint().configured(), std::chrono::steady_clock::now()))
            host.wakeLooper = true;
        return host;
    }

    [[nodiscard]] ScreenInteractionHostEffects runDueLiveSearch() {
        return applySearch(searchScreens_.runDue(seerrApp_.endpoint(), std::chrono::steady_clock::now()));
    }

    [[nodiscard]] ScreenInteractionHostEffects search(bool includeSeerrImmediately = true) {
        return applySearch(
            searchScreens_.search(seerrApp_.endpoint(), includeSeerrImmediately, std::chrono::steady_clock::now()));
    }

    [[nodiscard]] ScreenInteractionHostEffects applySystemTextInput(SystemTextInputEffects effects) {
        ScreenInteractionHostEffects host;
        if (effects.scheduleSearch) host.merge(scheduleLiveSearch());
        if (effects.cancelSeerrSearch) searchFlow_.cancelSeerrSearch();
        if (effects.submitSearch) host.merge(search());
        if (effects.invalidateSeerrStorage) seerrApp_.invalidateStorage();
        if (effects.saveSession) host.persistSession = true;

        if (auto notice = systemNotice(effects.notice)) host.notices.push_back(std::move(*notice));
        if (effects.refreshSeerr) {
            host.seerrEffects.push_back(seerrApp_.refreshPending());
            seerrApp_.refreshStorage(true);
        }
        if (effects.renderBurst.count() > 0) renderBurstUntil_ = std::chrono::steady_clock::now() + effects.renderBurst;
        return host;
    }

    [[nodiscard]] ScreenInteractionHostEffects applySearch(SearchScreenEffects effects) {
        ScreenInteractionHostEffects host;
        if (effects.hideTextInput) textInput_.hide();
        if (effects.openTextInput) {
            const bool shown =
                textInput_.show(searchFlow_.state().query(), "Search Jellyfin & Seerr", kTextInputSearch);
            searchScreens_.applyTextInputShown(shown);
        }
        if (effects.activateKeyboard) host.merge(activateKeyboard(true));
        if (effects.refreshSeerrStorage) seerrApp_.refreshStorage();
        if (effects.reconnectSeerr) host.seerrEffects.push_back(seerrApp_.connect(false));
        if (effects.syncSeerrHome) homeScreens_.syncSeerrHome();
        if (effects.prefetchSimilarItem) schedulePrefetch(*effects.prefetchSimilarItem, host);
        if (effects.openContextItem) detailsScreens_.openItemMenuForItem(*effects.openContextItem);
        if (effects.openDetailsItem) detailsScreens_.openDetails(*effects.openDetailsItem);
        if (effects.requestSeerrItem) host.seerrEffects.push_back(seerrApp_.requestMedia(*effects.requestSeerrItem));
        return host;
    }

    [[nodiscard]] ScreenInteractionHostEffects applyAccount(AccountScreenEffects effects) {
        ScreenInteractionHostEffects host;
        switch (effects.action) {
        case AccountScreenHostAction::None:
            return host;
        case AccountScreenHostAction::FinishActivity:
            host.finishActivity = true;
            return host;
        case AccountScreenHostAction::CancelPendingRequests:
            api_.cancelPendingRequests();
            return host;
        case AccountScreenHostAction::ActivateKeyboard:
            return activateKeyboard(false);
        case AccountScreenHostAction::EditField: {
            const int field = effects.field;
            const int mode = kTextInputLoginServer + field;
            static constexpr std::array<const char*, 3> hints{"Jellyfin server URL", "Jellyfin username",
                                                              "Jellyfin password"};
            accountFlow_.state().setKeyboardActive(!textInput_.show(accountFlow_.state().field(field),
                                                                    hints[static_cast<size_t>(field)], mode,
                                                                    field == AccountScreenState::kPasswordField));
            if (accountFlow_.state().keyboardActive()) keyboard_.reset();
            return host;
        }
        case AccountScreenHostAction::OpenProfiles:
            accountSession_.openProfiles();
            return host;
        }
        return host;
    }

    [[nodiscard]] ScreenInteractionHostEffects applyBrowse(BrowseScreenEffects effects) {
        ScreenInteractionHostEffects host;
        if (effects.openContextItem) detailsScreens_.openItemMenuForItem(*effects.openContextItem);
        if (effects.openDetailsItem) detailsScreens_.openDetails(*effects.openDetailsItem);
        if (effects.prefetchSimilarItem) schedulePrefetch(*effects.prefetchSimilarItem, host);
        return host;
    }

    [[nodiscard]] ScreenInteractionHostEffects applyDetails(DetailsScreenEffects effects) {
        ScreenInteractionHostEffects host;
        if (effects.reloadBrowse) browseScreens_.loadPage(false);
        if (effects.prefetchItem) schedulePrefetch(*effects.prefetchItem, host);
        if (effects.persistSession) host.persistSession = true;
        if (effects.notice)
            host.notices.push_back({std::move(effects.notice->message), effects.notice->duration, false});
        switch (effects.action) {
        case DetailsScreenHostAction::None:
            break;
        case DetailsScreenHostAction::BeginPlayback:
            beginPlayback();
            break;
        case DetailsScreenHostAction::BeginSeriesPlayAll:
            playbackRequests_.beginSeriesPlayAll();
            break;
        case DetailsScreenHostAction::DeleteSeerrRequest:
            host.seerrEffects.push_back(seerrApp_.deleteCurrentRequest());
            break;
        case DetailsScreenHostAction::PlayExternal:
            externalPlayback_.prepare(settingsFlow_.selectedExternalPlayer());
            break;
        case DetailsScreenHostAction::ViewQueue:
            playbackRequests_.openQueue();
            break;
        }
        return host;
    }

    [[nodiscard]] ScreenInteractionHostEffects handleHome(ScreenNavigationKey key) {
        ScreenInteractionHostEffects host;
        HomeScreenEffects effects = homeScreens_.handle(key);
        if (effects.rowSlideStarted)
            renderBurstUntil_ = std::max(renderBurstUntil_, *effects.rowSlideStarted + std::chrono::milliseconds(240));
        if (effects.prefetchSimilarItem) schedulePrefetch(*effects.prefetchSimilarItem, host);
        if (effects.openContextItem) {
            detailsScreens_.openItemMenuForItem(*effects.openContextItem);
            return host;
        }
        if (effects.openLibraryItem) {
            browseScreens_.openLibrary(*effects.openLibraryItem);
            return host;
        }
        if (effects.openDetailsItem) {
            detailsScreens_.openDetails(*effects.openDetailsItem);
            return host;
        }
        switch (effects.action) {
        case HomeScreenHostAction::None:
            break;
        case HomeScreenHostAction::FinishActivity:
            host.finishActivity = true;
            break;
        case HomeScreenHostAction::OpenProfiles:
            accountSession_.openProfiles();
            break;
        case HomeScreenHostAction::OpenSearch:
            host.merge(openSearch());
            break;
        case HomeScreenHostAction::OpenSettings:
            settingsScreens_.open(externalPlayer_.availablePlayers());
            break;
        }
        return host;
    }

    [[nodiscard]] ScreenInteractionHostEffects applySettings(SettingsScreenEffects effects) {
        ScreenInteractionHostEffects host;
        if (effects.persistSession) host.persistSession = true;
        if (effects.refreshSeerrStorage) seerrApp_.refreshStorage(true);
        switch (effects.hostAction) {
        case SettingsHostAction::None:
        case SettingsHostAction::Exit:
            break;
        case SettingsHostAction::EditSearch:
            textInput_.show(settingsFlow_.state().searchQuery(), "Search settings", kTextInputSettingsSearch);
            break;
        case SettingsHostAction::OpenDiagnostics:
            serverInfoScreens_.openDiagnostics();
            break;
        case SettingsHostAction::SwitchUser:
            accountSession_.openProfiles();
            break;
        case SettingsHostAction::EditSeerrServer:
            textInput_.show(settings_.seerrServer, "Seerr server URL", kTextInputSeerrServer);
            break;
        case SettingsHostAction::ConnectSeerr:
            host.seerrEffects.push_back(seerrApp_.connect());
            break;
        case SettingsHostAction::EditSeerrApiKey:
            textInput_.show(settings_.seerrApiKey, "Seerr API key", kTextInputSeerrApiKey, true);
            break;
        }
        return host;
    }

    [[nodiscard]] ScreenInteractionHostEffects dispatch(ScreenNavigationKey key, DetailsNavigationKey detailsKey) {
        switch (screen_) {
        case Screen::Login:
            return applyAccount(accountScreens_.handleLogin(key));
        case Screen::Profiles:
            accountSession_.handleProfiles(key);
            return {};
        case Screen::Home:
            return handleHome(key);
        case Screen::Browse:
            return applyBrowse(browseScreens_.handle(key));
        case Screen::Search:
            return applySearch(searchScreens_.handle(key, seerrApp_.endpoint(), std::chrono::steady_clock::now()));
        case Screen::Settings:
            return applySettings(settingsScreens_.handle(key));
        case Screen::Diagnostics:
            serverInfoScreens_.handleDiagnostics(key);
            return {};
        case Screen::Details:
            return applyDetails(detailsScreens_.handleDetails(detailsKey));
        case Screen::Cast:
            return applyDetails(detailsScreens_.handleCast(detailsKey, mediaGridColumns()));
        case Screen::PersonItems:
            return applyDetails(detailsScreens_.handlePersonItems(detailsKey, mediaGridColumns()));
        case Screen::ItemMenu:
            return applyDetails(detailsScreens_.handleItemMenu(detailsKey));
        case Screen::Seasons:
            return applyDetails(detailsScreens_.handleSeasons(detailsKey, mediaGridColumns()));
        case Screen::Episodes:
            return applyDetails(detailsScreens_.handleEpisodes(detailsKey, mediaGridColumns()));
        case Screen::SeerrDrivePicker: {
            ScreenInteractionHostEffects host;
            host.seerrEffects.push_back(seerrApp_.handleDrivePicker(key));
            return host;
        }
        case Screen::Player:
            return {};
        }
        return {};
    }

private:
    [[nodiscard]] ScreenInteractionHostEffects activateKeyboard(bool forSearch) {
        ScreenInteractionHostEffects host;
        const VirtualKeyboardEffects effects = keyboard_.activate(forSearch, searchFlow_.state(), accountFlow_.state());
        if (effects.searchChanged) host.merge(scheduleLiveSearch());
        if (effects.submitSearch) host.merge(search());
        return host;
    }

    [[nodiscard]] ScreenInteractionHostEffects openSearch() {
        navigation_.push(Screen::Search);
        screen_ = navigation_.current();
        const bool shown = textInput_.show(searchFlow_.state().query(), "Search Jellyfin & Seerr", kTextInputSearch);
        searchScreens_.applyTextInputShown(shown);
        keyboard_.reset();
        error_.clear();
        return {};
    }

    void beginPlayback() {
        if (loading_ || detailsFlow_.item().id.empty()) return;
        if (const auto player = settingsFlow_.selectedExternalPlayer())
            externalPlayback_.prepare(*player);
        else
            playbackRequests_.beginPlayback();
    }

    void schedulePrefetch(const JellyfinItem& item, ScreenInteractionHostEffects& host) {
        if (similarPrefetch_.schedule(session_, item)) host.wakeLooper = true;
    }

    static std::optional<ScreenInteractionNotice> systemNotice(SystemTextInputNotice notice) {
        switch (notice) {
        case SystemTextInputNotice::None:
            return std::nullopt;
        case SystemTextInputNotice::SeerrDisconnected:
            return ScreenInteractionNotice{"SEERR DISCONNECTED", std::chrono::seconds(3), false};
        case SystemTextInputNotice::SeerrServerSaved:
            return ScreenInteractionNotice{"SEERR SERVER SAVED", std::chrono::seconds(3), false};
        case SystemTextInputNotice::SeerrApiKeyCleared:
            return ScreenInteractionNotice{"SEERR API KEY CLEARED", std::chrono::seconds(3), false};
        case SystemTextInputNotice::SeerrApiKeySaved:
            return ScreenInteractionNotice{"SEERR API KEY SAVED", std::chrono::seconds(3), false};
        }
        return std::nullopt;
    }

    Api& api_;
    TextInput& textInput_;
    AccountScreens& accountScreens_;
    AccountSession& accountSession_;
    HomeScreens& homeScreens_;
    BrowseScreens& browseScreens_;
    SearchScreens& searchScreens_;
    SearchFlowT& searchFlow_;
    SettingsScreens& settingsScreens_;
    ServerInfoScreens& serverInfoScreens_;
    DetailsScreens& detailsScreens_;
    SeerrApp& seerrApp_;
    PlaybackRequests& playbackRequests_;
    ExternalPlayback& externalPlayback_;
    SimilarPrefetch& similarPrefetch_;
    ExternalPlayer& externalPlayer_;
    Screen& screen_;
    NavigationStack<Screen>& navigation_;
    JellyfinSession& session_;
    AppSettings& settings_;
    SettingsFlow& settingsFlow_;
    DetailsFlow& detailsFlow_;
    VirtualKeyboardState& keyboard_;
    AccountFlow& accountFlow_;
    bool& loading_;
    std::string& error_;
    std::chrono::steady_clock::time_point& renderBurstUntil_;
};
