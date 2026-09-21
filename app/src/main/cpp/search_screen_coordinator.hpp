#pragma once

#include "app_screen.hpp"
#include "home_screen.hpp"
#include "media_grid_renderer.hpp"
#include "navigation_stack.hpp"
#include "search_flow.hpp"
#include "virtual_keyboard.hpp"

#include <chrono>
#include <optional>
#include <string>
#include <utility>

struct SearchScreenEffects {
    bool hideTextInput = false;
    bool openTextInput = false;
    bool activateKeyboard = false;
    bool refreshSeerrStorage = false;
    bool reconnectSeerr = false;
    bool syncSeerrHome = false;
    std::optional<JellyfinItem> openContextItem;
    std::optional<JellyfinItem> openDetailsItem;
    std::optional<JellyfinItem> prefetchSimilarItem;
    std::optional<SeerrMediaItem> requestSeerrItem;
};

template <typename SearchFlowType> class SearchScreenCoordinator {
public:
    SearchScreenCoordinator(SearchFlowType& search, VirtualKeyboardState& keyboard, NavigationStack<Screen>& navigation,
                            Screen& screen, HomeScreenState& homeState, JellyfinHomeData& home,
                            JellyfinSession& session, std::string& error)
        : search_(search), keyboard_(keyboard), navigation_(navigation), screen_(screen), homeState_(homeState),
          home_(home), session_(session), error_(error) {}

    void resetForSessionChange() { search_.reset(); }

    [[nodiscard]] SearchScreenEffects handle(ScreenNavigationKey key, const SeerrEndpoint& endpoint,
                                             std::chrono::steady_clock::time_point now) {
        SearchScreenEffects effects;
        const SearchNavigationAction navigation = search_.handleNavigation(key, mediaGridColumns());
        if (const JellyfinItem* selected = search_.selectedResult()) effects.prefetchSimilarItem = *selected;

        switch (navigation.type) {
        case SearchNavigationActionType::None:
            return effects;
        case SearchNavigationActionType::Exit:
            effects.hideTextInput = true;
            screen_ = navigation_.popOr(Screen::Home);
            if (screen_ == Screen::Home) {
                homeState_.setRow(0);
                homeState_.updateViewport(static_cast<int>(home_.rows.size()));
            }
            return effects;
        case SearchNavigationActionType::SubmitSearch:
            merge(effects, applyDispatch(search_.search(session_, endpoint, true, now)));
            return effects;
        case SearchNavigationActionType::MoveKeyboard:
            keyboard_.move(navigation.dx, navigation.dy);
            return effects;
        case SearchNavigationActionType::ActivateKeyboard:
            effects.activateKeyboard = true;
            return effects;
        case SearchNavigationActionType::OpenTextInput:
            effects.openTextInput = true;
            return effects;
        case SearchNavigationActionType::OpenContext:
            effects.openContextItem = navigation.item;
            return effects;
        case SearchNavigationActionType::OpenDetails:
            effects.openDetailsItem = navigation.item;
            return effects;
        case SearchNavigationActionType::RequestSeerr:
            effects.requestSeerrItem = navigation.seerrItem;
            return effects;
        }
        return effects;
    }

    [[nodiscard]] bool scheduleLive(bool seerrConfigured, std::chrono::steady_clock::time_point now) {
        const SearchScheduleEffects effects = search_.scheduleLive(now, seerrConfigured);
        if (effects.clearError) error_.clear();
        return true;
    }

    [[nodiscard]] SearchScreenEffects runDue(const SeerrEndpoint& endpoint, std::chrono::steady_clock::time_point now) {
        return applyDispatch(search_.runDue(screen_ == Screen::Search, session_, endpoint, now));
    }

    [[nodiscard]] SearchScreenEffects search(const SeerrEndpoint& endpoint, bool includeSeerrImmediately,
                                             std::chrono::steady_clock::time_point now) {
        return applyDispatch(search_.search(session_, endpoint, includeSeerrImmediately, now));
    }

    [[nodiscard]] SearchScreenEffects searchSeerr(const SeerrEndpoint& endpoint, bool immediate,
                                                  std::chrono::steady_clock::time_point now) {
        return applyDispatch(search_.searchSeerr(endpoint, immediate, now));
    }

    [[nodiscard]] SearchScreenEffects complete(SeerrSearchCompletion& completion, bool hasSeerrSession,
                                               std::chrono::steady_clock::time_point now) {
        return applyCompletion(search_.complete(completion, screen_ == Screen::Search, hasSeerrSession, now));
    }

    [[nodiscard]] SearchScreenEffects complete(JellyfinSearchCompletion& completion) {
        return applyCompletion(search_.complete(completion, screen_ == Screen::Search));
    }

    void applyTextInputShown(bool shown) {
        search_.state().setKeyboard(!shown);
        if (search_.state().keyboard()) keyboard_.reset();
    }

private:
    [[nodiscard]] SearchScreenEffects applyDispatch(const SearchDispatchEffects& dispatch) {
        SearchScreenEffects effects;
        if (dispatch.clearError) error_.clear();
        effects.refreshSeerrStorage = dispatch.refreshSeerrStorage;
        return effects;
    }

    [[nodiscard]] SearchScreenEffects applyCompletion(SearchCompletionEffects completion) {
        SearchScreenEffects effects;
        if (completion.error) error_ = std::move(*completion.error);
        if (completion.clearError) error_.clear();
        effects.reconnectSeerr = completion.reconnectSeerr;
        effects.syncSeerrHome = completion.syncSeerrHome;
        if (const JellyfinItem* selected = search_.selectedResult()) effects.prefetchSimilarItem = *selected;
        return effects;
    }

    static void merge(SearchScreenEffects& target, SearchScreenEffects source) {
        target.refreshSeerrStorage = target.refreshSeerrStorage || source.refreshSeerrStorage;
        target.reconnectSeerr = target.reconnectSeerr || source.reconnectSeerr;
        target.syncSeerrHome = target.syncSeerrHome || source.syncSeerrHome;
    }

    SearchFlowType& search_;
    VirtualKeyboardState& keyboard_;
    NavigationStack<Screen>& navigation_;
    Screen& screen_;
    HomeScreenState& homeState_;
    JellyfinHomeData& home_;
    JellyfinSession& session_;
    std::string& error_;
};
