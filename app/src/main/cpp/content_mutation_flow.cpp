#include "content_mutation_flow.hpp"

#include <utility>

void ContentMutationFlow::reset() {
    loading_ = false;
    playedRollback_.reset();
}

PlayedMutationPreparation ContentMutationFlow::preparePlayedToggle(JellyfinHomeData& home, HomeScreenState& homeState,
                                                                   BrowseScreenState& browseState,
                                                                   SearchScreenState& searchState,
                                                                   DetailsScreenState& detailsState,
                                                                   PlaybackQueueState& queueState, JellyfinItem& detail,
                                                                   bool hiddenFromHome, uint64_t sessionEpoch) {
    PlayedMutationPreparation preparation = ItemMutationController::preparePlayedToggle(
        home, homeState, browseState, searchState, detailsState, queueState, detail, hiddenFromHome, sessionEpoch);
    playedRollback_ = std::move(preparation.rollback);
    return preparation;
}

void ContentMutationFlow::rejectPlayedToggle(const JellyfinItem& original, bool hiddenFromHome, JellyfinHomeData& home,
                                             HomeScreenState& homeState, BrowseScreenState& browseState,
                                             SearchScreenState& searchState, DetailsScreenState& detailsState,
                                             PlaybackQueueState& queueState, JellyfinItem& detail) {
    loading_ = false;
    ItemMutationCompletionEffects effects;
    effects.cacheUpdate = original;
    effects.playedRollback.swap(playedRollback_);
    if (detail.id == original.id) detail = original;
    static_cast<void>(
        apply(std::move(effects), hiddenFromHome, home, homeState, browseState, searchState, detailsState, queueState));
}

ContentMutationHostEffects ContentMutationFlow::complete(FavoriteCompletion& completion, bool activeSession,
                                                         bool detailVisible, bool updatedHiddenFromHome,
                                                         JellyfinHomeData& home, HomeScreenState& homeState,
                                                         BrowseScreenState& browseState, SearchScreenState& searchState,
                                                         DetailsScreenState& detailsState,
                                                         PlaybackQueueState& queueState, JellyfinItem& detail) {
    return apply(ItemMutationController::apply(completion, activeSession, detailVisible, detail), updatedHiddenFromHome,
                 home, homeState, browseState, searchState, detailsState, queueState);
}

ContentMutationHostEffects ContentMutationFlow::complete(PlayedCompletion& completion, bool activeSession,
                                                         bool detailVisible, bool replacementHiddenFromHome,
                                                         bool updatedHiddenFromHome, JellyfinHomeData& home,
                                                         HomeScreenState& homeState, BrowseScreenState& browseState,
                                                         SearchScreenState& searchState,
                                                         DetailsScreenState& detailsState,
                                                         PlaybackQueueState& queueState, JellyfinItem& detail) {
    return apply(ItemMutationController::apply(completion, activeSession, detailVisible, replacementHiddenFromHome,
                                               playedRollback_, home, homeState, detail),
                 updatedHiddenFromHome, home, homeState, browseState, searchState, detailsState, queueState);
}

ContentMutationHostEffects ContentMutationFlow::complete(MetadataRefreshCompletion& completion, bool activeSession) {
    ItemMutationCompletionEffects effects = ItemMutationController::apply(completion, activeSession);
    if (effects.finishLoading) loading_ = false;
    ContentMutationHostEffects host;
    host.error = std::move(effects.error);
    host.notice = std::move(effects.notice);
    return host;
}

ContentMutationHostEffects ContentMutationFlow::complete(DeleteItemCompletion& completion, bool activeSession,
                                                         bool activeItemMenu, JellyfinHomeData& home,
                                                         HomeScreenState& homeState, BrowseScreenState& browseState,
                                                         SearchScreenState& searchState,
                                                         DetailsScreenState& detailsState,
                                                         PlaybackQueueState& queueState, JellyfinItem& detail) {
    return apply(ItemMutationController::apply(completion, activeSession, activeItemMenu, detail, detailsState), false,
                 home, homeState, browseState, searchState, detailsState, queueState);
}

ContentMutationHostEffects ContentMutationFlow::apply(ItemMutationCompletionEffects effects, bool updatedHiddenFromHome,
                                                      JellyfinHomeData& home, HomeScreenState& homeState,
                                                      BrowseScreenState& browseState, SearchScreenState& searchState,
                                                      DetailsScreenState& detailsState,
                                                      PlaybackQueueState& queueState) {
    if (effects.finishLoading) loading_ = false;
    if (effects.cacheUpdate) {
        ItemMutationController::updateCachedUserData(home, homeState, browseState, searchState, detailsState,
                                                     queueState, *effects.cacheUpdate, updatedHiddenFromHome);
    }
    ItemMutationController::restorePlayedRollback(effects, home, homeState);
    if (effects.removeCachedItemId) {
        ItemMutationController::removeCachedItem(home, browseState, searchState, detailsState,
                                                 *effects.removeCachedItemId);
    }

    ContentMutationHostEffects host;
    host.error = std::move(effects.error);
    host.notice = std::move(effects.notice);
    host.closeDeletedItem = effects.closeDeletedItem;
    if (effects.nextUpAnimation) {
        nextUpReplacementFadeIndex_ = effects.nextUpAnimation->index;
        nextUpReplacementFadeItemId_ = std::move(effects.nextUpAnimation->itemId);
        nextUpReplacementFadeStarted_ = std::chrono::steady_clock::now();
        host.renderAnimationStarted = true;
    }
    return host;
}
