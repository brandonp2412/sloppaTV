#pragma once

#include "jellyfin_types.hpp"

#include <algorithm>
#include <array>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

class DetailActionList {
public:
    void add(std::string_view action) { actions_[size_++] = action; }
    [[nodiscard]] size_t size() const { return size_; }
    [[nodiscard]] bool empty() const { return size_ == 0; }
    [[nodiscard]] std::string_view front() const { return actions_.front(); }
    [[nodiscard]] std::string_view back() const { return actions_[size_ - 1]; }
    [[nodiscard]] std::string_view operator[](size_t index) const { return actions_[index]; }

private:
    std::array<std::string_view, 9> actions_{};
    size_t size_ = 0;
};

class DetailsScreenState {
public:
    void reset() {
        beginDetails();
        beginItemMenu();
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
    }

    void beginDetails() {
        actionSelection_ = 0;
        similar_.clear();
        similarSelection_ = 0;
        similarFocused_ = false;
    }

    [[nodiscard]] DetailActionList actions(const JellyfinItem& item, bool stillWatchingPrompt) const {
        DetailActionList result;
        result.add(
            stillWatchingPrompt
                ? "KEEP WATCHING"
                : (item.type == "Series" ? "PLAY NEXT" : (item.positionTicks > 0 ? "RESUME" : "PLAY"))
        );
        if (item.type == "Series") result.add("EPISODES");
        result.add(item.favorite ? "UNFAVORITE" : "FAVORITE");
        result.add(item.played ? "MARK UNWATCHED" : "MARK WATCHED");
        if (!item.people.empty()) result.add("CAST");
        result.add("MORE");
        result.add("BACK");
        return result;
    }

    [[nodiscard]] int actionSelection() const { return actionSelection_; }
    void moveAction(int direction, int count) {
        moveLinearSelection(actionSelection_, direction, count);
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
        return selectedItem(similar_, similarSelection_);
    }

    void beginItemMenu() {
        itemMenuSelection_ = 0;
        deleteConfirmation_ = false;
        deleteConfirmationSelection_ = 1;
    }
    [[nodiscard]] DetailActionList itemMenuActions(
        const JellyfinItem& item,
        bool hasExternalPlayer,
        bool hasQueue,
        bool hiddenFromHome
    ) const {
        DetailActionList result;
        if (item.type == "Series") result.add("PLAY ALL");
        if (hasExternalPlayer) result.add("PLAY EXTERNAL");
        if (hasQueue) result.add("VIEW QUEUE");
        result.add(item.favorite ? "UNFAVORITE" : "FAVORITE");
        result.add(item.played ? "MARK UNWATCHED" : "MARK WATCHED");
        result.add(hiddenFromHome ? "SHOW ON HOME" : "HIDE FROM HOME");
        result.add("REFRESH METADATA");
        if (item.canDelete) result.add("DELETE MEDIA");
        result.add("BACK");
        return result;
    }
    [[nodiscard]] int itemMenuSelection() const { return itemMenuSelection_; }
    void moveItemMenu(int direction, int count) {
        moveLinearSelection(itemMenuSelection_, direction, count);
    }
    [[nodiscard]] bool deleteConfirmation() const { return deleteConfirmation_; }
    void setDeleteConfirmation(bool enabled) {
        deleteConfirmation_ = enabled;
        if (!enabled) deleteConfirmationSelection_ = 1;
    }
    [[nodiscard]] int deleteConfirmationSelection() const { return deleteConfirmationSelection_; }
    void setDeleteConfirmationSelection(int selection) { deleteConfirmationSelection_ = selection <= 0 ? 0 : 1; }

    void resetCastSelection() { castSelection_ = 0; }
    [[nodiscard]] int castSelection() const { return castSelection_; }
    void moveCastSelection(const std::vector<JellyfinPerson>& people, int dx, int dy, int columns) {
        moveGridSelection(castSelection_, static_cast<int>(people.size()), dx, dy, columns);
    }
    [[nodiscard]] const JellyfinPerson* selectedCastPerson(const std::vector<JellyfinPerson>& people) const {
        return selectedItem(people, castSelection_);
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
    [[nodiscard]] int seasonSelection() const { return seasonSelection_; }
    void moveSeason(int dx, int dy, int columns) {
        moveGridSelection(seasonSelection_, static_cast<int>(seasons_.size()), dx, dy, columns);
    }
    [[nodiscard]] const JellyfinItem* selectedSeasonItem() const {
        return selectedItem(seasons_, seasonSelection_);
    }

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
    [[nodiscard]] int episodeSelection() const { return episodeSelection_; }
    void moveEpisode(int dx, int dy, int columns) {
        moveGridSelection(episodeSelection_, static_cast<int>(episodes_.size()), dx, dy, columns);
    }
    [[nodiscard]] const JellyfinItem* selectedEpisodeItem() const {
        return selectedItem(episodes_, episodeSelection_);
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
        auto remove = [&](auto& items) {
            std::erase_if(items, [&](const JellyfinItem& item) { return item.id == itemId; });
        };
        remove(similar_);
        remove(personItems_);
        remove(seasons_);
        remove(episodes_);
        clampSelection(personItemSelection_, personItems_.size());
        clampSelection(seasonSelection_, seasons_.size());
        clampSelection(episodeSelection_, episodes_.size());
        clampSelection(similarSelection_, similar_.size());
    }

private:
    static void moveLinearSelection(int& selection, int direction, int count) {
        selection = count <= 0 ? 0 : std::clamp(selection + direction, 0, count - 1);
    }

    static void clampSelection(int& selection, size_t count) {
        selection = count == 0 ? 0 : std::clamp(selection, 0, static_cast<int>(count) - 1);
    }

    static void moveGridSelection(int& selection, int count, int dx, int dy, int columns) {
        if (count <= 0 || columns <= 0) {
            selection = 0;
            return;
        }
        const int rows = (count + columns - 1) / columns;
        int row = selection / columns;
        int col = selection % columns;
        row = std::clamp(row + dy, 0, rows - 1);
        col = std::clamp(col + dx, 0, columns - 1);
        const int next = row * columns + col;
        if (next >= 0 && next < count) selection = next;
    }

    template <typename T>
    static const T* selectedItem(const std::vector<T>& items, int selection) {
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
};
