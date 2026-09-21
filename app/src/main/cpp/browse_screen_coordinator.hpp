#pragma once

#include "app_screen.hpp"
#include "browse_async_executor.hpp"
#include "browse_completion_controller.hpp"
#include "browse_navigation_controller.hpp"
#include "browse_screen.hpp"
#include "home_screen.hpp"
#include "media_grid_renderer.hpp"
#include "navigation_stack.hpp"
#include "request_epoch.hpp"

#include <optional>
#include <string>
#include <utility>

struct BrowseScreenEffects {
    std::optional<JellyfinItem> openContextItem;
    std::optional<JellyfinItem> openDetailsItem;
    std::optional<JellyfinItem> prefetchSimilarItem;
};

template <typename BrowseAsync, typename UiPresentation> class BrowseScreenCoordinator {
public:
    static constexpr int kPageSize = 60;

    BrowseScreenCoordinator(BrowseScreenState& browse, HomeScreenState& homeState, JellyfinSession& session,
                            NavigationStack<Screen>& navigation, Screen& screen, bool& loading, std::string& error,
                            RequestEpoch& contentEpoch, BrowseAsync& browseAsync, UiPresentation& uiPresentation)
        : browse_(browse), homeState_(homeState), session_(session), navigation_(navigation), screen_(screen),
          loading_(loading), error_(error), contentEpoch_(contentEpoch), browseAsync_(browseAsync),
          uiPresentation_(uiPresentation) {}

    void resetForSessionChange() { browse_.clear(); }

    [[nodiscard]] BrowseScreenEffects handle(ScreenNavigationKey key) {
        BrowseScreenEffects effects;
        if (key == ScreenNavigationKey::Back) cancelContentLoadForNavigation();

        const BrowseNavigationAction navigation =
            BrowseNavigationController::handle(browse_, key, mediaGridColumns(), loading_);
        switch (navigation.type) {
        case BrowseNavigationActionType::None:
            return effects;
        case BrowseNavigationActionType::ReloadPage:
            loadPage(false);
            return effects;
        case BrowseNavigationActionType::LocalPage:
            loading_ = false;
            error_.clear();
            return effects;
        case BrowseNavigationActionType::Exit:
            screen_ = navigation_.popOr(Screen::Home);
            if (screen_ == Screen::Home) homeState_.focusToolbar(1);
            return effects;
        case BrowseNavigationActionType::ApplyFilter:
            applyFilter(navigation.filterSelection);
            return effects;
        case BrowseNavigationActionType::OpenContext:
            effects.openContextItem = navigation.item;
            return effects;
        case BrowseNavigationActionType::OpenContainer:
            if (navigation.item) openContainer(*navigation.item, true);
            return effects;
        case BrowseNavigationActionType::OpenDetails:
            effects.openDetailsItem = navigation.item;
            return effects;
        case BrowseNavigationActionType::SelectionChanged: {
            uiPresentation_.prefetchBrowseArtworkAhead(session_, browse_);
            const auto& items = browse_.items();
            if (browse_.selection() >= 0 && browse_.selection() < static_cast<int>(items.size())) {
                effects.prefetchSimilarItem = items[static_cast<size_t>(browse_.selection())];
            }
            if (navigation.loadMore) loadMore();
            return effects;
        }
        }
        return effects;
    }

    void openLibrary(const JellyfinItem& library) {
        if (loading_ || library.id.empty()) return;
        browse_.resetForLibrary(library);
        pushScreen(Screen::Browse);
        loadPage(false);
    }

    void loadPage(bool append) {
        if (loading_ || browse_.activeContainer().id.empty()) return;
        loading_ = true;
        error_.clear();
        if (browseAsync_.load(BrowsePageRequest{
                .session = session_,
                .container = browse_.activeContainer(),
                .startIndex = append ? browse_.nextIndex() : 0,
                .append = append,
                .generation = contentEpoch_.begin(),
                .mode = browse_.mode(),
                .genre = browse_.genre(),
                .letter = browse_.letter(),
                .nested = browse_.nested(),
                .pageSize = kPageSize,
            }))
            return;
        contentEpoch_.invalidate();
        loading_ = false;
        error_ = "BROWSE LOAD COULD NOT BE STARTED";
    }

    void complete(BrowsePageCompletion& completion) {
        BrowseCompletionEffects effects = BrowseCompletionController::apply(
            completion, contentEpoch_.active(completion.generation), screen_ == Screen::Browse,
            browse_.activeContainer().id, browse_, kPageSize);
        if (effects.finishLoading) loading_ = false;
        if (effects.error) error_ = std::move(*effects.error);
        if (effects.clearError) error_.clear();
        if (effects.prefetchArtwork) uiPresentation_.prefetchBrowseArtworkAhead(session_, browse_);
    }

private:
    void pushScreen(Screen screen) {
        navigation_.push(screen);
        screen_ = navigation_.current();
    }

    void cancelContentLoadForNavigation() {
        if (!loading_) return;
        contentEpoch_.invalidate();
        loading_ = false;
    }

    void applyFilter(int selection) {
        if (loading_) return;
        if (browse_.applyFilter(selection)) {
            loadPage(false);
        } else {
            loading_ = false;
            error_.clear();
        }
    }

    void openContainer(const JellyfinItem& container, bool pushCurrent) {
        if (loading_ || container.id.empty()) return;
        browse_.openContainer(container, pushCurrent);
        pushScreen(Screen::Browse);
        loadPage(false);
    }

    void loadMore() {
        if (loading_ || !browse_.hasMore() || browse_.activeContainer().id.empty()) return;
        loadPage(true);
    }

    BrowseScreenState& browse_;
    HomeScreenState& homeState_;
    JellyfinSession& session_;
    NavigationStack<Screen>& navigation_;
    Screen& screen_;
    bool& loading_;
    std::string& error_;
    RequestEpoch& contentEpoch_;
    BrowseAsync& browseAsync_;
    UiPresentation& uiPresentation_;
};
