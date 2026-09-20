#pragma once

#include "app_screen.hpp"
#include "content_mutation_flow.hpp"
#include "details_completion_controller.hpp"
#include "details_flow.hpp"
#include "home_visibility.hpp"
#include "request_epoch.hpp"
#include "similar_prefetch_controller.hpp"

#include <chrono>
#include <optional>
#include <string>
#include <type_traits>
#include <utility>

template <typename T>
inline constexpr bool isContentCompletionV =
    std::is_same_v<std::remove_cvref_t<T>, ItemMenuDetailCompletion> ||
    std::is_same_v<std::remove_cvref_t<T>, PersonItemsCompletion> ||
    std::is_same_v<std::remove_cvref_t<T>, SeasonsCompletion> ||
    std::is_same_v<std::remove_cvref_t<T>, EpisodesCompletion> ||
    std::is_same_v<std::remove_cvref_t<T>, DetailsItemCompletion> ||
    std::is_same_v<std::remove_cvref_t<T>, DetailsSimilarCompletion> ||
    std::is_same_v<std::remove_cvref_t<T>, SimilarPrefetchCompletion> ||
    std::is_same_v<std::remove_cvref_t<T>, EpisodeSeriesContextRequestCompletion> ||
    std::is_same_v<std::remove_cvref_t<T>, EpisodeSeriesContextCompletion> ||
    std::is_same_v<std::remove_cvref_t<T>, FavoriteCompletion> ||
    std::is_same_v<std::remove_cvref_t<T>, PlayedCompletion> ||
    std::is_same_v<std::remove_cvref_t<T>, MetadataRefreshCompletion> ||
    std::is_same_v<std::remove_cvref_t<T>, DeleteItemCompletion>;

struct ContentCompletionHostEffects {
    bool closeDeletedItem = false;
    std::optional<std::string> notice;
    std::optional<std::chrono::steady_clock::time_point> renderAnimationStarted;
};

template <typename DetailsAsync> class ContentCompletionCoordinator {
public:
    ContentCompletionCoordinator(RequestEpoch& contentEpoch, RequestEpoch& sessionEpoch, Screen& screen, bool& loading,
                                 std::string& error, DetailsFlow& details, ContentMutationFlow& mutations,
                                 HomeVisibility& homeVisibility, JellyfinHomeData& home, HomeScreenState& homeState,
                                 BrowseScreenState& browseState, SearchScreenState& searchState,
                                 PlaybackQueueState& queueState, DetailsAsync& detailsAsync,
                                 SimilarPrefetchController& similarPrefetch)
        : contentEpoch_(contentEpoch), sessionEpoch_(sessionEpoch), screen_(screen), loading_(loading), error_(error),
          details_(details), mutations_(mutations), homeVisibility_(homeVisibility), home_(home), homeState_(homeState),
          browseState_(browseState), searchState_(searchState), queueState_(queueState), detailsAsync_(detailsAsync),
          similarPrefetch_(similarPrefetch) {}

    [[nodiscard]] ContentCompletionHostEffects complete(ItemMenuDetailCompletion& completion) {
        applyDetails(DetailsCompletionController::apply(completion, screen_ == Screen::ItemMenu, details_.item()));
        return {};
    }

    [[nodiscard]] ContentCompletionHostEffects complete(PersonItemsCompletion& completion) {
        applyDetails(DetailsCompletionController::apply(completion, contentEpoch_.active(completion.generation),
                                                        screen_ == Screen::PersonItems, details_.state()));
        return {};
    }

    [[nodiscard]] ContentCompletionHostEffects complete(SeasonsCompletion& completion) {
        applyDetails(DetailsCompletionController::apply(completion, contentEpoch_.active(completion.generation),
                                                        screen_ == Screen::Seasons, details_.state()));
        return {};
    }

    [[nodiscard]] ContentCompletionHostEffects complete(EpisodesCompletion& completion) {
        applyDetails(DetailsCompletionController::apply(completion, contentEpoch_.active(completion.generation),
                                                        screen_ == Screen::Episodes, details_.state()));
        return {};
    }

    [[nodiscard]] ContentCompletionHostEffects complete(DetailsItemCompletion& completion) {
        applyDetails(DetailsCompletionController::apply(completion, contentEpoch_.active(completion.generation),
                                                        screen_ == Screen::Details, details_.item()));
        return {};
    }

    [[nodiscard]] ContentCompletionHostEffects complete(DetailsSimilarCompletion& completion) {
        applyDetails(DetailsCompletionController::apply(completion, contentEpoch_.active(completion.generation),
                                                        screen_ == Screen::Details, details_.item(), details_.state()));
        return {};
    }

    [[nodiscard]] ContentCompletionHostEffects complete(EpisodeSeriesContextRequestCompletion& completion) {
        if (!contentEpoch_.active(completion.generation)) return {};
        if (screen_ != Screen::Details || !completion.request.matches(details_.item())) return {};
        if (!detailsAsync_.loadSeriesContext(std::move(completion.session), std::move(completion.request),
                                             contentEpoch_.token(completion.generation))) {
            error_ = "EPISODE CONTEXT COULD NOT BE STARTED";
        }
        return {};
    }

    [[nodiscard]] ContentCompletionHostEffects complete(EpisodeSeriesContextCompletion& completion) {
        applyDetails(DetailsCompletionController::apply(completion, contentEpoch_.active(completion.generation),
                                                        screen_ == Screen::Details, details_.item(), details_.state()));
        return {};
    }

    [[nodiscard]] ContentCompletionHostEffects complete(SimilarPrefetchCompletion& completion) {
        auto items = similarPrefetch_.complete(completion);
        if (!items || screen_ != Screen::Details || details_.item().id != completion.itemId ||
            !details_.state().similar().empty())
            return {};
        details_.state().setSimilar(std::move(*items));
        return {};
    }

    [[nodiscard]] ContentCompletionHostEffects complete(FavoriteCompletion& completion) {
        return applyMutation(mutations_.complete(
            completion, sessionEpoch_.active(completion.sessionEpoch),
            screen_ == Screen::Details || screen_ == Screen::ItemMenu, homeVisibility_.isHidden(completion.item), home_,
            homeState_, browseState_, searchState_, details_.state(), queueState_, details_.item()));
    }

    [[nodiscard]] ContentCompletionHostEffects complete(PlayedCompletion& completion) {
        const bool replacementHidden =
            completion.nextUpReplacement && homeVisibility_.isHidden(*completion.nextUpReplacement);
        return applyMutation(mutations_.complete(completion, sessionEpoch_.active(completion.sessionEpoch),
                                                 screen_ == Screen::Details || screen_ == Screen::ItemMenu,
                                                 replacementHidden, homeVisibility_.isHidden(completion.item), home_,
                                                 homeState_, browseState_, searchState_, details_.state(), queueState_,
                                                 details_.item()));
    }

    [[nodiscard]] ContentCompletionHostEffects complete(MetadataRefreshCompletion& completion) {
        return applyMutation(mutations_.complete(completion, sessionEpoch_.active(completion.sessionEpoch)));
    }

    [[nodiscard]] ContentCompletionHostEffects complete(DeleteItemCompletion& completion) {
        return applyMutation(mutations_.complete(completion, sessionEpoch_.active(completion.sessionEpoch),
                                                 screen_ == Screen::ItemMenu, home_, homeState_, browseState_,
                                                 searchState_, details_.state(), queueState_, details_.item()));
    }

private:
    void applyDetails(DetailsCompletionEffects effects) {
        if (effects.finishLoading) loading_ = false;
        if (effects.error) error_ = std::move(*effects.error);
    }

    [[nodiscard]] ContentCompletionHostEffects applyMutation(ContentMutationHostEffects effects) {
        ContentCompletionHostEffects host;
        if (effects.error) error_ = std::move(*effects.error);
        host.closeDeletedItem = effects.closeDeletedItem;
        host.notice = std::move(effects.notice);
        if (effects.renderAnimationStarted) host.renderAnimationStarted = mutations_.nextUpReplacementFadeStarted();
        return host;
    }

    RequestEpoch& contentEpoch_;
    RequestEpoch& sessionEpoch_;
    Screen& screen_;
    bool& loading_;
    std::string& error_;
    DetailsFlow& details_;
    ContentMutationFlow& mutations_;
    HomeVisibility& homeVisibility_;
    JellyfinHomeData& home_;
    HomeScreenState& homeState_;
    BrowseScreenState& browseState_;
    SearchScreenState& searchState_;
    PlaybackQueueState& queueState_;
    DetailsAsync& detailsAsync_;
    SimilarPrefetchController& similarPrefetch_;
};
