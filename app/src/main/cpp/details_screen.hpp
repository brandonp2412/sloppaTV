#pragma once

#include "jellyfin_types.hpp"

#include <algorithm>
#include <string>
#include <utility>
#include <vector>

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
    }

    void beginDetails() {
        actionSelection_ = 0;
        similar_.clear();
        similarSelection_ = 0;
        similarFocused_ = false;
    }

    [[nodiscard]] std::vector<std::string> actions(const JellyfinItem& item, bool stillWatchingPrompt) const {
        std::vector<std::string> result;
        result.emplace_back(
            stillWatchingPrompt
                ? "KEEP WATCHING"
                : (item.type == "Series" ? "PLAY NEXT" : (item.positionTicks > 0 ? "RESUME" : "PLAY"))
        );
        if (item.type == "Series") result.emplace_back("EPISODES");
        result.emplace_back(item.favorite ? "UNFAVORITE" : "FAVORITE");
        result.emplace_back(item.played ? "MARK UNWATCHED" : "MARK WATCHED");
        if (!item.people.empty()) result.emplace_back("CAST");
        result.emplace_back("MORE");
        result.emplace_back("BACK");
        return result;
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
        if (similar_.empty() || similarSelection_ < 0 || similarSelection_ >= static_cast<int>(similar_.size())) return nullptr;
        return &similar_[static_cast<size_t>(similarSelection_)];
    }

    void beginItemMenu() {
        itemMenuSelection_ = 0;
        deleteConfirmation_ = false;
        deleteConfirmationSelection_ = 1;
    }
    [[nodiscard]] std::vector<std::string> itemMenuActions(
        const JellyfinItem& item,
        bool hasExternalPlayer,
        bool hasQueue,
        bool hiddenFromHome
    ) const {
        std::vector<std::string> result;
        if (item.type == "Series") result.emplace_back("PLAY ALL");
        if (hasExternalPlayer) result.emplace_back("PLAY EXTERNAL");
        if (hasQueue) result.emplace_back("VIEW QUEUE");
        result.emplace_back(item.favorite ? "UNFAVORITE" : "FAVORITE");
        result.emplace_back(item.played ? "MARK UNWATCHED" : "MARK WATCHED");
        result.emplace_back(hiddenFromHome ? "SHOW ON HOME" : "HIDE FROM HOME");
        result.emplace_back("REFRESH METADATA");
        if (item.canDelete) result.emplace_back("DELETE MEDIA");
        result.emplace_back("BACK");
        return result;
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
    [[nodiscard]] int castSelection() const { return castSelection_; }
    void moveCastSelection(const std::vector<JellyfinPerson>& people, int dx, int dy, int columns) {
        const int count = static_cast<int>(people.size());
        if (count <= 0 || columns <= 0) {
            castSelection_ = 0;
            return;
        }
        const int rows = (count + columns - 1) / columns;
        int row = castSelection_ / columns;
        int col = castSelection_ % columns;
        row = std::clamp(row + dy, 0, rows - 1);
        col = std::clamp(col + dx, 0, columns - 1);
        const int next = row * columns + col;
        if (next >= 0 && next < count) castSelection_ = next;
    }
    [[nodiscard]] const JellyfinPerson* selectedCastPerson(const std::vector<JellyfinPerson>& people) const {
        if (people.empty() || castSelection_ < 0 || castSelection_ >= static_cast<int>(people.size())) return nullptr;
        return &people[static_cast<size_t>(castSelection_)];
    }

private:
    int actionSelection_ = 0;
    std::vector<JellyfinItem> similar_;
    int similarSelection_ = 0;
    bool similarFocused_ = false;
    int itemMenuSelection_ = 0;
    bool deleteConfirmation_ = false;
    int deleteConfirmationSelection_ = 1;
    int castSelection_ = 0;
};
