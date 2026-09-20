#pragma once

#include "grid_navigation.hpp"
#include "jellyfin_types.hpp"

#include <algorithm>
#include <optional>
#include <string>
#include <utility>
#include <vector>

struct EpisodeSeriesContextRequest {
    std::string itemId;
    std::string seriesId;

    [[nodiscard]] bool matches(const JellyfinItem& item) const { return item.id == itemId && item.type == "Episode"; }
};

inline std::optional<EpisodeSeriesContextRequest> episodeSeriesContextRequest(const JellyfinItem& item) {
    if (item.type != "Episode" || item.seriesId.empty()) return std::nullopt;
    return EpisodeSeriesContextRequest{
        .itemId = item.id,
        .seriesId = item.seriesId,
    };
}

enum class DetailsScreenInput {
    None,
    Back,
    Context,
    Left,
    Right,
    Up,
    Down,
    Activate,
};

enum class DetailsScreenCommandType {
    None,
    Back,
    OpenContext,
    OpenEpisodeSeries,
    OpenEpisodeSeason,
    OpenSimilar,
    ActivateAction,
};

struct DetailsScreenCommand {
    DetailsScreenCommandType type = DetailsScreenCommandType::None;
};

enum class DetailsAction {
    StartPlayback,
    OpenEpisodes,
    PlayAll,
    ToggleFavorite,
    TogglePlayed,
    OpenCast,
    OpenItemMenu,
    Back,
};

inline std::vector<DetailsAction> detailActionIdsFor(const JellyfinItem& item) {
    std::vector<DetailsAction> result;
    result.reserve(7);
    result.push_back(DetailsAction::StartPlayback);
    if (item.type == "Series") {
        result.push_back(DetailsAction::OpenEpisodes);
        result.push_back(DetailsAction::PlayAll);
    }
    result.push_back(DetailsAction::ToggleFavorite);
    result.push_back(DetailsAction::TogglePlayed);
    if (!item.people.empty()) result.push_back(DetailsAction::OpenCast);
    if (item.type != "Series") result.push_back(DetailsAction::OpenItemMenu);
    result.push_back(DetailsAction::Back);
    return result;
}

inline std::string detailsActionLabel(DetailsAction action, const JellyfinItem& item, bool stillWatchingPrompt) {
    switch (action) {
    case DetailsAction::StartPlayback:
        return stillWatchingPrompt
                   ? "KEEP WATCHING"
                   : (item.type == "Series" ? "PLAY NEXT" : (item.positionTicks > 0 ? "RESUME" : "PLAY"));
    case DetailsAction::OpenEpisodes:
        return "EPISODES";
    case DetailsAction::PlayAll:
        return "PLAY ALL";
    case DetailsAction::ToggleFavorite:
        return item.favorite ? "UNFAVORITE" : "FAVORITE";
    case DetailsAction::TogglePlayed:
        return item.played ? "MARK UNWATCHED" : "MARK WATCHED";
    case DetailsAction::OpenCast:
        return "CAST";
    case DetailsAction::OpenItemMenu:
        return "MORE";
    case DetailsAction::Back:
        return "BACK";
    }
    return {};
}

enum class CastScreenInput {
    None,
    Back,
    Left,
    Right,
    Up,
    Down,
    Activate,
};

enum class CastScreenCommandType {
    None,
    Back,
    OpenPerson,
};

struct CastScreenCommand {
    CastScreenCommandType type = CastScreenCommandType::None;
};

enum class DetailGridScreenInput {
    None,
    Back,
    Context,
    Left,
    Right,
    Up,
    Down,
    Activate,
};

enum class DetailGridScreenCommandType {
    None,
    Back,
    OpenContext,
    OpenSelected,
};

struct DetailGridScreenCommand {
    DetailGridScreenCommandType type = DetailGridScreenCommandType::None;
};

enum class ItemMenuScreenInput {
    None,
    Back,
    Left,
    Right,
    Up,
    Down,
    Activate,
};

enum class ItemMenuScreenCommandType {
    None,
    Back,
    ActivateAction,
    ConfirmDelete,
};

struct ItemMenuScreenCommand {
    ItemMenuScreenCommandType type = ItemMenuScreenCommandType::None;
};

enum class ItemMenuAction {
    PlayAll,
    PlayExternal,
    ViewQueue,
    ToggleFavorite,
    TogglePlayed,
    ToggleHomeVisibility,
    RefreshMetadata,
    DeleteMedia,
    DeleteRequest,
    Back,
};

inline std::vector<ItemMenuAction> itemMenuActionIdsFor(const JellyfinItem& item, bool seerrRequest,
                                                        bool hasExternalPlayer, bool hasQueue) {
    if (seerrRequest) return {ItemMenuAction::DeleteRequest, ItemMenuAction::Back};

    std::vector<ItemMenuAction> result;
    result.reserve(9);
    if (item.type == "Series") result.push_back(ItemMenuAction::PlayAll);
    if (hasExternalPlayer) result.push_back(ItemMenuAction::PlayExternal);
    if (hasQueue) result.push_back(ItemMenuAction::ViewQueue);
    result.push_back(ItemMenuAction::ToggleFavorite);
    result.push_back(ItemMenuAction::TogglePlayed);
    result.push_back(ItemMenuAction::ToggleHomeVisibility);
    result.push_back(ItemMenuAction::RefreshMetadata);
    if (item.canDelete) result.push_back(ItemMenuAction::DeleteMedia);
    result.push_back(ItemMenuAction::Back);
    return result;
}

inline std::string itemMenuActionLabel(ItemMenuAction action, const JellyfinItem& item, bool hiddenFromHome) {
    switch (action) {
    case ItemMenuAction::PlayAll:
        return "PLAY ALL";
    case ItemMenuAction::PlayExternal:
        return "PLAY EXTERNAL";
    case ItemMenuAction::ViewQueue:
        return "VIEW QUEUE";
    case ItemMenuAction::ToggleFavorite:
        return item.favorite ? "UNFAVORITE" : "FAVORITE";
    case ItemMenuAction::TogglePlayed:
        return item.played ? "MARK UNWATCHED" : "MARK WATCHED";
    case ItemMenuAction::ToggleHomeVisibility:
        return hiddenFromHome ? "SHOW ON HOME" : "HIDE FROM HOME";
    case ItemMenuAction::RefreshMetadata:
        return "REFRESH METADATA";
    case ItemMenuAction::DeleteMedia:
        return "DELETE MEDIA";
    case ItemMenuAction::DeleteRequest:
        return "DELETE REQUEST";
    case ItemMenuAction::Back:
        return "BACK";
    }
    return {};
}

class DetailsScreenState {
public:
    void reset() {
        actionSelection_ = 0;
        similar_.clear();
        similarSelection_ = 0;
        similarFocused_ = false;
        itemMenuSelection_ = 0;
        deleteConfirmation_ = false;
        deleteConfirmationSelection_ = 1;
        castSelection_ = 0;
        selectedPerson_ = {};
        personItems_.clear();
        personItemSelection_ = 0;
        seriesDetail_ = {};
        seasons_.clear();
        seasonSelection_ = 0;
        selectedSeason_ = {};
        episodes_.clear();
        episodeSelection_ = 0;
        episodeContextFocused_ = false;
        episodeContextSelection_ = 0;
    }

    void beginDetails() {
        actionSelection_ = 0;
        similar_.clear();
        similarSelection_ = 0;
        similarFocused_ = false;
        seriesDetail_ = {};
        seasons_.clear();
        selectedSeason_ = {};
        episodes_.clear();
        episodeContextFocused_ = false;
        episodeContextSelection_ = 0;
    }

    [[nodiscard]] std::vector<std::string> actions(const JellyfinItem& item, bool stillWatchingPrompt) const {
        const auto ids = detailActionIdsFor(item);
        std::vector<std::string> result;
        result.reserve(ids.size());
        for (DetailsAction action : ids) result.push_back(detailsActionLabel(action, item, stillWatchingPrompt));
        return result;
    }

    [[nodiscard]] std::optional<DetailsAction> selectedAction(const JellyfinItem& item) const {
        const auto ids = detailActionIdsFor(item);
        if (actionSelection_ < 0 || actionSelection_ >= static_cast<int>(ids.size())) return std::nullopt;
        return ids[static_cast<size_t>(actionSelection_)];
    }

    [[nodiscard]] DetailsScreenCommand handleInput(DetailsScreenInput input, int actionCount, bool episodeDetail) {
        if (input == DetailsScreenInput::Back) return {.type = DetailsScreenCommandType::Back};
        if (input == DetailsScreenInput::Context) return {.type = DetailsScreenCommandType::OpenContext};

        if (episodeContextFocused_) {
            if (input == DetailsScreenInput::Up)
                setEpisodeContextFocused(false);
            else if (input == DetailsScreenInput::Left)
                moveEpisodeContext(-1);
            else if (input == DetailsScreenInput::Right)
                moveEpisodeContext(1);
            else if (input == DetailsScreenInput::Activate)
                return {.type = episodeContextSelection_ == 0 ? DetailsScreenCommandType::OpenEpisodeSeries
                                                              : DetailsScreenCommandType::OpenEpisodeSeason};
            return {};
        }

        if (similarFocused_) {
            if (input == DetailsScreenInput::Up)
                setSimilarFocused(false);
            else if (input == DetailsScreenInput::Left)
                moveSimilar(-1);
            else if (input == DetailsScreenInput::Right)
                moveSimilar(1);
            else if (input == DetailsScreenInput::Activate)
                return {.type = DetailsScreenCommandType::OpenSimilar};
            return {};
        }

        if (input == DetailsScreenInput::Left)
            moveAction(-1, actionCount);
        else if (input == DetailsScreenInput::Right)
            moveAction(1, actionCount);
        else if (input == DetailsScreenInput::Down && episodeDetail && hasEpisodeSeriesContext())
            setEpisodeContextFocused(true);
        else if (input == DetailsScreenInput::Down && !similar_.empty())
            setSimilarFocused(true);
        else if (input == DetailsScreenInput::Activate)
            return {.type = DetailsScreenCommandType::ActivateAction};
        return {};
    }

    [[nodiscard]] int actionSelection() const { return actionSelection_; }

    void moveAction(int direction, int count) {
        if (count <= 0) {
            actionSelection_ = 0;
            return;
        }
        actionSelection_ = std::clamp(actionSelection_ + direction, 0, count - 1);
    }

    [[nodiscard]] const std::vector<JellyfinItem>& similar() const { return similar_; }

    [[nodiscard]] std::vector<JellyfinItem>& similar() { return similar_; }

    void setSimilar(std::vector<JellyfinItem> items) {
        similar_ = std::move(items);
        similarSelection_ = 0;
        if (similar_.empty()) similarFocused_ = false;
    }

    [[nodiscard]] bool similarFocused() const { return similarFocused_; }

    void setSimilarFocused(bool focused) { similarFocused_ = focused && !similar_.empty(); }

    [[nodiscard]] int similarSelection() const { return similarSelection_; }

    void moveSimilar(int direction) {
        if (similar_.empty()) {
            similarSelection_ = 0;
            return;
        }
        similarSelection_ = std::clamp(similarSelection_ + direction, 0, static_cast<int>(similar_.size()) - 1);
    }

    [[nodiscard]] const JellyfinItem* selectedSimilar() const {
        if (similar_.empty() || similarSelection_ < 0 || similarSelection_ >= static_cast<int>(similar_.size()))
            return nullptr;
        return &similar_[static_cast<size_t>(similarSelection_)];
    }

    void beginItemMenu() {
        itemMenuSelection_ = 0;
        deleteConfirmation_ = false;
        deleteConfirmationSelection_ = 1;
    }

    [[nodiscard]] std::vector<std::string> itemMenuActions(const JellyfinItem& item, bool seerrRequest,
                                                           bool hasExternalPlayer, bool hasQueue,
                                                           bool hiddenFromHome) const {
        const auto ids = itemMenuActionIdsFor(item, seerrRequest, hasExternalPlayer, hasQueue);
        std::vector<std::string> result;
        result.reserve(ids.size());
        for (ItemMenuAction action : ids) result.push_back(itemMenuActionLabel(action, item, hiddenFromHome));
        return result;
    }

    [[nodiscard]] std::optional<ItemMenuAction> selectedItemMenuAction(const JellyfinItem& item, bool seerrRequest,
                                                                       bool hasExternalPlayer, bool hasQueue) const {
        const auto ids = itemMenuActionIdsFor(item, seerrRequest, hasExternalPlayer, hasQueue);
        if (itemMenuSelection_ < 0 || itemMenuSelection_ >= static_cast<int>(ids.size())) return std::nullopt;
        return ids[static_cast<size_t>(itemMenuSelection_)];
    }

    [[nodiscard]] ItemMenuScreenCommand handleItemMenuInput(ItemMenuScreenInput input, int actionCount) {
        if (input == ItemMenuScreenInput::Back) {
            if (deleteConfirmation_) {
                setDeleteConfirmation(false);
                return {};
            }
            return {.type = ItemMenuScreenCommandType::Back};
        }

        if (deleteConfirmation_) {
            if (input == ItemMenuScreenInput::Up || input == ItemMenuScreenInput::Left)
                setDeleteConfirmationSelection(0);
            else if (input == ItemMenuScreenInput::Down || input == ItemMenuScreenInput::Right)
                setDeleteConfirmationSelection(1);
            else if (input == ItemMenuScreenInput::Activate) {
                if (deleteConfirmationSelection_ == 0) return {.type = ItemMenuScreenCommandType::ConfirmDelete};
                setDeleteConfirmation(false);
            }
            return {};
        }

        if (input == ItemMenuScreenInput::Up)
            moveItemMenu(-1, actionCount);
        else if (input == ItemMenuScreenInput::Down)
            moveItemMenu(1, actionCount);
        else if (input == ItemMenuScreenInput::Activate)
            return {.type = ItemMenuScreenCommandType::ActivateAction};
        return {};
    }

    [[nodiscard]] int itemMenuSelection() const { return itemMenuSelection_; }

    void moveItemMenu(int direction, int count) {
        if (count <= 0) {
            itemMenuSelection_ = 0;
            return;
        }
        itemMenuSelection_ = std::clamp(itemMenuSelection_ + direction, 0, count - 1);
    }

    [[nodiscard]] bool deleteConfirmation() const { return deleteConfirmation_; }

    void setDeleteConfirmation(bool enabled) {
        deleteConfirmation_ = enabled;
        if (!enabled) deleteConfirmationSelection_ = 1;
    }

    [[nodiscard]] int deleteConfirmationSelection() const { return deleteConfirmationSelection_; }

    void setDeleteConfirmationSelection(int selection) { deleteConfirmationSelection_ = selection <= 0 ? 0 : 1; }

    void resetCastSelection() { castSelection_ = 0; }

    [[nodiscard]] CastScreenCommand handleCastInput(CastScreenInput input, const std::vector<JellyfinPerson>& people,
                                                    int columns) {
        if (input == CastScreenInput::Back) return {.type = CastScreenCommandType::Back};
        if (input == CastScreenInput::Activate) return {.type = CastScreenCommandType::OpenPerson};

        int dx = 0;
        int dy = 0;
        if (input == CastScreenInput::Left)
            dx = -1;
        else if (input == CastScreenInput::Right)
            dx = 1;
        else if (input == CastScreenInput::Up)
            dy = -1;
        else if (input == CastScreenInput::Down)
            dy = 1;
        else
            return {};

        moveCastSelection(people, dx, dy, columns);
        return {};
    }

    [[nodiscard]] int castSelection() const { return castSelection_; }

    void moveCastSelection(const std::vector<JellyfinPerson>& people, int dx, int dy, int columns) {
        moveGridSelection(castSelection_, static_cast<int>(people.size()), dx, dy, columns);
    }

    [[nodiscard]] const JellyfinPerson* selectedCastPerson(const std::vector<JellyfinPerson>& people) const {
        if (people.empty() || castSelection_ < 0 || castSelection_ >= static_cast<int>(people.size())) return nullptr;
        return &people[static_cast<size_t>(castSelection_)];
    }

    void beginPerson(JellyfinPerson person) {
        selectedPerson_ = std::move(person);
        personItems_.clear();
        personItemSelection_ = 0;
    }

    [[nodiscard]] const JellyfinPerson& selectedPerson() const { return selectedPerson_; }

    [[nodiscard]] const std::vector<JellyfinItem>& personItems() const { return personItems_; }

    [[nodiscard]] std::vector<JellyfinItem>& personItems() { return personItems_; }

    void setPersonItems(std::vector<JellyfinItem> items) {
        personItems_ = std::move(items);
        personItemSelection_ = 0;
    }

    [[nodiscard]] DetailGridScreenCommand handlePersonItemsInput(DetailGridScreenInput input, int columns) {
        return handleItemGridInput(input, static_cast<int>(personItems_.size()), personItemSelection_, columns, true);
    }

    [[nodiscard]] int personItemSelection() const { return personItemSelection_; }

    void movePersonItem(int dx, int dy, int columns) {
        moveGridSelection(personItemSelection_, static_cast<int>(personItems_.size()), dx, dy, columns);
    }

    [[nodiscard]] const JellyfinItem* selectedPersonItem() const {
        return selectedItem(personItems_, personItemSelection_);
    }

    void beginSeries(JellyfinItem series) {
        seriesDetail_ = std::move(series);
        seasons_.clear();
        seasonSelection_ = 0;
        selectedSeason_ = {};
        episodes_.clear();
        episodeSelection_ = 0;
    }

    [[nodiscard]] const JellyfinItem& seriesDetail() const { return seriesDetail_; }

    [[nodiscard]] const std::vector<JellyfinItem>& seasons() const { return seasons_; }

    [[nodiscard]] std::vector<JellyfinItem>& seasons() { return seasons_; }

    void setSeasons(std::vector<JellyfinItem> seasons) {
        seasons_ = std::move(seasons);
        seasonSelection_ = 0;
    }

    [[nodiscard]] DetailGridScreenCommand handleSeasonsInput(DetailGridScreenInput input, int columns) {
        return handleItemGridInput(input, static_cast<int>(seasons_.size()), seasonSelection_, columns, false);
    }

    [[nodiscard]] int seasonSelection() const { return seasonSelection_; }

    void moveSeason(int dx, int dy, int columns) {
        moveGridSelection(seasonSelection_, static_cast<int>(seasons_.size()), dx, dy, columns);
    }

    [[nodiscard]] const JellyfinItem* selectedSeasonItem() const { return selectedItem(seasons_, seasonSelection_); }

    void beginSeason(JellyfinItem season) {
        selectedSeason_ = std::move(season);
        episodes_.clear();
        episodeSelection_ = 0;
    }

    [[nodiscard]] const JellyfinItem& selectedSeason() const { return selectedSeason_; }

    [[nodiscard]] const std::vector<JellyfinItem>& episodes() const { return episodes_; }

    [[nodiscard]] std::vector<JellyfinItem>& episodes() { return episodes_; }

    void setEpisodes(std::vector<JellyfinItem> episodes) {
        episodes_ = std::move(episodes);
        episodeSelection_ = 0;
    }

    [[nodiscard]] DetailGridScreenCommand handleEpisodesInput(DetailGridScreenInput input, int columns) {
        return handleItemGridInput(input, static_cast<int>(episodes_.size()), episodeSelection_, columns, true);
    }

    [[nodiscard]] int episodeSelection() const { return episodeSelection_; }

    void moveEpisode(int dx, int dy, int columns) {
        moveGridSelection(episodeSelection_, static_cast<int>(episodes_.size()), dx, dy, columns);
    }

    [[nodiscard]] const JellyfinItem* selectedEpisodeItem() const { return selectedItem(episodes_, episodeSelection_); }

    void setEpisodeSeriesContext(JellyfinItem series, std::vector<JellyfinItem> seasons) {
        seriesDetail_ = std::move(series);
        seasons_ = std::move(seasons);
        episodeContextSelection_ = 0;
        episodeContextFocused_ = false;
    }

    [[nodiscard]] bool hasEpisodeSeriesContext() const { return !seriesDetail_.id.empty(); }

    [[nodiscard]] bool episodeContextFocused() const { return episodeContextFocused_; }

    void setEpisodeContextFocused(bool focused) { episodeContextFocused_ = focused && hasEpisodeSeriesContext(); }

    [[nodiscard]] int episodeContextSelection() const { return episodeContextSelection_; }

    [[nodiscard]] int episodeContextCount() const {
        return hasEpisodeSeriesContext() ? static_cast<int>(seasons_.size()) + 1 : 0;
    }

    void moveEpisodeContext(int direction) {
        const int count = episodeContextCount();
        if (count <= 0) {
            episodeContextSelection_ = 0;
            return;
        }
        episodeContextSelection_ = std::clamp(episodeContextSelection_ + direction, 0, count - 1);
    }

    [[nodiscard]] const JellyfinItem* selectedEpisodeContextSeason() const {
        const int index = episodeContextSelection_ - 1;
        return selectedItem(seasons_, index);
    }

    void updateCachedUserData(const JellyfinItem& updated) {
        auto apply = [&](JellyfinItem& item) {
            if (item.id != updated.id) return;
            item.favorite = updated.favorite;
            item.played = updated.played;
            item.positionTicks = updated.positionTicks;
        };
        for (auto& item : similar_) apply(item);
        for (auto& item : personItems_) apply(item);
        for (auto& item : seasons_) apply(item);
        for (auto& item : episodes_) apply(item);
    }

    void removeItem(const std::string& itemId) {
        if (itemId.empty()) return;
        auto remove = [&](auto& items, int& selection) {
            const size_t selectedOffset = selection <= 0 ? 0 : std::min(static_cast<size_t>(selection), items.size());
            const int removedBeforeSelection = static_cast<int>(
                std::count_if(items.begin(), items.begin() + static_cast<std::ptrdiff_t>(selectedOffset),
                              [&](const JellyfinItem& item) { return item.id == itemId; }));
            std::erase_if(items, [&](const JellyfinItem& item) { return item.id == itemId; });
            selection = items.empty()
                            ? 0
                            : std::clamp(selection - removedBeforeSelection, 0, static_cast<int>(items.size()) - 1);
        };
        remove(similar_, similarSelection_);
        remove(personItems_, personItemSelection_);
        remove(seasons_, seasonSelection_);
        remove(episodes_, episodeSelection_);
    }

private:
    static DetailGridScreenCommand handleItemGridInput(DetailGridScreenInput input, int itemCount, int& selection,
                                                       int columns, bool allowContext) {
        if (input == DetailGridScreenInput::Back) return {.type = DetailGridScreenCommandType::Back};
        if (input == DetailGridScreenInput::Context) {
            return allowContext ? DetailGridScreenCommand{.type = DetailGridScreenCommandType::OpenContext}
                                : DetailGridScreenCommand{};
        }
        if (input == DetailGridScreenInput::Activate) {
            return {.type = DetailGridScreenCommandType::OpenSelected};
        }

        int dx = 0;
        int dy = 0;
        if (input == DetailGridScreenInput::Left)
            dx = -1;
        else if (input == DetailGridScreenInput::Right)
            dx = 1;
        else if (input == DetailGridScreenInput::Up)
            dy = -1;
        else if (input == DetailGridScreenInput::Down)
            dy = 1;
        else
            return {};

        moveGridSelection(selection, itemCount, dx, dy, columns);
        return {};
    }

    static void moveGridSelection(int& selection, int count, int dx, int dy, int columns) {
        selection = gridSelectionAfterMove(selection, count, dx, dy, columns);
    }

    static const JellyfinItem* selectedItem(const std::vector<JellyfinItem>& items, int selection) {
        if (items.empty() || selection < 0 || selection >= static_cast<int>(items.size())) return nullptr;
        return &items[static_cast<size_t>(selection)];
    }

    int actionSelection_ = 0;
    std::vector<JellyfinItem> similar_;
    int similarSelection_ = 0;
    bool similarFocused_ = false;
    int itemMenuSelection_ = 0;
    bool deleteConfirmation_ = false;
    int deleteConfirmationSelection_ = 1;
    int castSelection_ = 0;
    JellyfinPerson selectedPerson_;
    std::vector<JellyfinItem> personItems_;
    int personItemSelection_ = 0;
    JellyfinItem seriesDetail_;
    std::vector<JellyfinItem> seasons_;
    int seasonSelection_ = 0;
    JellyfinItem selectedSeason_;
    std::vector<JellyfinItem> episodes_;
    int episodeSelection_ = 0;
    bool episodeContextFocused_ = false;
    int episodeContextSelection_ = 0;
};
