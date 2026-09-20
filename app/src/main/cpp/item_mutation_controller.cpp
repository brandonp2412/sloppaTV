#include "item_mutation_controller.hpp"

#include <algorithm>
#include <cstddef>
#include <utility>
#include <vector>

namespace {
void clampHomeSelections(JellyfinHomeData& home, HomeScreenState& state) {
    std::vector<int> itemCounts;
    itemCounts.reserve(home.rows.size());
    for (const auto& row : home.rows) itemCounts.push_back(static_cast<int>(row.items.size()));
    state.clampSelections(itemCounts);
}
} // namespace

void ItemMutationController::updateCachedUserData(JellyfinHomeData& home, HomeScreenState& homeState,
                                                  BrowseScreenState& browseState, SearchScreenState& searchState,
                                                  DetailsScreenState& detailsState, PlaybackQueueState& queueState,
                                                  const JellyfinItem& updated, bool hiddenFromHome) {
    auto apply = [&](JellyfinItem& item) {
        if (item.id != updated.id) return;
        item.favorite = updated.favorite;
        item.played = updated.played;
        item.positionTicks = updated.positionTicks;
    };
    for (auto& row : home.rows)
        for (auto& item : row.items) apply(item);
    for (auto& item : browseState.items()) apply(item);
    for (auto& item : searchState.results()) apply(item);
    detailsState.updateCachedUserData(updated);
    queueState.updateCachedUserData(updated);

    for (auto& row : home.rows) {
        if (row.title == "Favorites") {
            const auto existing = std::find_if(row.items.begin(), row.items.end(),
                                               [&](const JellyfinItem& item) { return item.id == updated.id; });
            if (updated.favorite && existing == row.items.end() && !hiddenFromHome)
                row.items.push_back(updated);
            else if (!updated.favorite && existing != row.items.end())
                row.items.erase(existing);
        } else if (row.title == "Continue Watching" && updated.played) {
            std::erase_if(row.items, [&](const JellyfinItem& item) { return item.id == updated.id; });
        }
    }
    clampHomeSelections(home, homeState);
}

void ItemMutationController::removeCachedItem(JellyfinHomeData& home, BrowseScreenState& browseState,
                                              SearchScreenState& searchState, DetailsScreenState& detailsState,
                                              const std::string& itemId) {
    if (itemId.empty()) return;
    for (auto& row : home.rows) {
        std::erase_if(row.items, [&](const JellyfinItem& item) { return item.id == itemId; });
    }
    browseState.removeItem(itemId);
    searchState.removeItem(itemId);
    detailsState.removeItem(itemId);
}

PlayedMutationPreparation
ItemMutationController::preparePlayedToggle(JellyfinHomeData& home, HomeScreenState& homeState,
                                            BrowseScreenState& browseState, SearchScreenState& searchState,
                                            DetailsScreenState& detailsState, PlaybackQueueState& queueState,
                                            JellyfinItem& detail, bool hiddenFromHome, uint64_t sessionEpoch) {
    PlayedMutationPreparation preparation;
    preparation.item = detail;
    preparation.desired = !detail.played;
    preparation.rollback = PlayedRollbackState{
        .itemId = detail.id,
        .sessionEpoch = sessionEpoch,
        .previousHome = home,
        .previousHomeSelection = homeState.snapshot(home.rows),
    };

    for (const auto& row : home.rows) {
        if (row.title != "Next Up") continue;
        const auto current = std::find_if(row.items.begin(), row.items.end(),
                                          [&](const JellyfinItem& homeItem) { return homeItem.id == detail.id; });
        if (current != row.items.end()) {
            preparation.nextUpReplacementIndex = static_cast<int>(std::distance(row.items.begin(), current));
        }
        break;
    }

    JellyfinItem updated = detail;
    updated.played = preparation.desired;
    if (preparation.desired) updated.positionTicks = 0;
    updateCachedUserData(home, homeState, browseState, searchState, detailsState, queueState, updated, hiddenFromHome);
    detail.played = preparation.desired;
    if (preparation.desired) detail.positionTicks = 0;

    if (preparation.desired) {
        for (auto& row : home.rows) {
            if (!homeRowDropsItemWhenPlayed(row.title)) continue;
            if (row.title == "Next Up" && preparation.nextUpReplacementIndex >= 0) continue;
            std::erase_if(row.items, [&](const JellyfinItem& homeItem) { return homeItem.id == detail.id; });
        }
        clampHomeSelections(home, homeState);
    }
    return preparation;
}

ItemMutationCompletionEffects ItemMutationController::apply(FavoriteCompletion& completion, bool activeSession,
                                                            bool detailVisible, JellyfinItem& detail) {
    if (!activeSession) return {};

    ItemMutationCompletionEffects effects;
    effects.finishLoading = true;
    if (!completion.result.ok) {
        effects.error = completion.result.error;
        return effects;
    }

    JellyfinItem updated = completion.item;
    updated.favorite = completion.desired;
    effects.cacheUpdate = updated;
    if (detailVisible && detail.id == completion.item.id) detail.favorite = completion.desired;
    return effects;
}

ItemMutationCompletionEffects ItemMutationController::apply(PlayedCompletion& completion, bool activeSession,
                                                            bool detailVisible, bool replacementHidden,
                                                            std::optional<PlayedRollbackState>& rollbackState,
                                                            JellyfinHomeData& home, HomeScreenState& homeState,
                                                            JellyfinItem& detail) {
    const bool rollbackMatches = rollbackState && rollbackState->sessionEpoch == completion.sessionEpoch &&
                                 rollbackState->itemId == completion.item.id;
    if (!activeSession) {
        if (rollbackMatches) rollbackState.reset();
        return {};
    }

    ItemMutationCompletionEffects effects;
    effects.finishLoading = true;
    if (rollbackMatches) effects.playedRollback.swap(rollbackState);

    if (!completion.result.ok) {
        effects.cacheUpdate = completion.item;
        if (detailVisible && detail.id == completion.item.id) detail = completion.item;
        effects.error = completion.result.error;
        return effects;
    }

    auto nextUpRow = std::find_if(home.rows.begin(), home.rows.end(),
                                  [](const JellyfinHomeRow& candidate) { return candidate.title == "Next Up"; });
    if (completion.nextUpReplacementIndex >= 0 && nextUpRow != home.rows.end()) {
        const size_t replacementIndex = static_cast<size_t>(completion.nextUpReplacementIndex);
        const bool slotStillMatches =
            replacementIndex < nextUpRow->items.size() && nextUpRow->items[replacementIndex].id == completion.item.id;
        if (completion.nextUpReplacement && slotStillMatches && !replacementHidden) {
            const std::string replacementId = completion.nextUpReplacement->id;
            nextUpRow->items[replacementIndex] = std::move(*completion.nextUpReplacement);
            effects.nextUpAnimation =
                NextUpReplacementAnimation{.index = completion.nextUpReplacementIndex, .itemId = replacementId};
        } else if (slotStillMatches) {
            nextUpRow->items.erase(nextUpRow->items.begin() + static_cast<std::ptrdiff_t>(replacementIndex));
            clampHomeSelections(home, homeState);
        }
    }

    if (detailVisible && detail.id == completion.item.id) {
        detail.played = completion.desired;
        if (completion.desired) detail.positionTicks = 0;
    }
    return effects;
}

ItemMutationCompletionEffects ItemMutationController::apply(MetadataRefreshCompletion& completion, bool activeSession) {
    if (!activeSession) return {};

    ItemMutationCompletionEffects effects;
    effects.finishLoading = true;
    if (!completion.result.ok)
        effects.error = completion.result.error;
    else
        effects.notice = "METADATA REFRESH REQUESTED";
    return effects;
}

ItemMutationCompletionEffects ItemMutationController::apply(DeleteItemCompletion& completion, bool activeSession,
                                                            bool activeItemMenu, JellyfinItem& detail,
                                                            DetailsScreenState& detailsState) {
    if (!activeSession) return {};

    ItemMutationCompletionEffects effects;
    effects.finishLoading = true;
    if (completion.result.ok) effects.removeCachedItemId = completion.itemId;
    if (!activeItemMenu || detail.id != completion.itemId) return effects;

    detailsState.setDeleteConfirmation(false);
    if (!completion.result.ok) {
        effects.error = completion.result.error;
        return effects;
    }

    detail = {};
    effects.closeDeletedItem = true;
    effects.notice = "MEDIA DELETED";
    return effects;
}

void ItemMutationController::restorePlayedRollback(ItemMutationCompletionEffects& effects, JellyfinHomeData& home,
                                                   HomeScreenState& homeState) {
    if (!effects.playedRollback) return;

    home = std::move(effects.playedRollback->previousHome);
    HomeRestorePlan rollbackPlan =
        HomeScreenState::restorePlan(effects.playedRollback->previousHomeSelection, home.rows);
    homeState.setSelections(std::move(rollbackPlan.selections));
    homeState.setRow(rollbackPlan.focusedRow);
    homeState.updateViewport(static_cast<int>(home.rows.size()));
    effects.playedRollback.reset();
}
