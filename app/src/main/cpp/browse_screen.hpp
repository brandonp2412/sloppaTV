#pragma once

#include "jellyfin_types.hpp"

#include <algorithm>
#include <iterator>
#include <string>
#include <utility>
#include <vector>

enum class BrowseContentMode {
    All,
    Favorites,
    Genres,
    GenreItems,
    Letters,
    LetterItems,
    Collections,
};

struct BrowseSnapshot {
    JellyfinItem container;
    std::vector<JellyfinItem> items;
    int selection = 0;
    int nextIndex = 0;
    bool hasMore = false;
};

enum class BrowseBackAction {
    RestoredSnapshot,
    Reload,
    LocalPage,
    Exit,
};

class BrowseScreenState {
public:
    void resetForLibrary(const JellyfinItem& library) {
        stack_.clear();
        filterFocused_ = false;
        filterSelection_ = 0;
        mode_ = BrowseContentMode::All;
        genre_.clear();
        letter_.clear();
        activeContainer_ = library;
        clearPage();
    }

    void clear() {
        activeContainer_ = {};
        stack_.clear();
        filterFocused_ = false;
        filterSelection_ = 0;
        mode_ = BrowseContentMode::All;
        genre_.clear();
        letter_.clear();
        clearPage();
    }

    void openContainer(const JellyfinItem& container, bool pushCurrent) {
        if (pushCurrent && !activeContainer_.id.empty()) {
            stack_.push_back(BrowseSnapshot{
                activeContainer_,
                std::move(items_),
                selection_,
                nextIndex_,
                hasMore_,
            });
        }
        activeContainer_ = container;
        clearPage();
    }

    BrowseBackAction back() {
        filterFocused_ = false;
        if (!stack_.empty()) {
            auto previous = std::move(stack_.back());
            stack_.pop_back();
            activeContainer_ = std::move(previous.container);
            items_ = std::move(previous.items);
            selection_ = previous.selection;
            nextIndex_ = previous.nextIndex;
            hasMore_ = previous.hasMore;
            return BrowseBackAction::RestoredSnapshot;
        }
        if (mode_ == BrowseContentMode::GenreItems) {
            mode_ = BrowseContentMode::Genres;
            genre_.clear();
            clearPage();
            return BrowseBackAction::Reload;
        }
        if (mode_ == BrowseContentMode::LetterItems) {
            mode_ = BrowseContentMode::Letters;
            letter_.clear();
            populateLetters();
            return BrowseBackAction::LocalPage;
        }
        clearPage();
        return BrowseBackAction::Exit;
    }

    [[nodiscard]] bool hasFilterBar() const {
        return stack_.empty() && (activeContainer_.collectionType == "movies"
            || activeContainer_.collectionType == "tvshows" || activeContainer_.collectionType == "mixed");
    }

    [[nodiscard]] std::vector<std::string> filterLabels() const {
        std::vector<std::string> labels{"ALL", "FAVORITES", "GENRES", "A-Z"};
        if (activeContainer_.collectionType == "movies") labels.emplace_back("COLLECTIONS");
        return labels;
    }

    void moveFilter(int delta) {
        const auto labels = filterLabels();
        if (labels.empty()) {
            filterSelection_ = 0;
            return;
        }
        filterSelection_ = std::clamp(filterSelection_ + delta, 0, static_cast<int>(labels.size()) - 1);
    }

    [[nodiscard]] bool applyFilter(int selection) {
        const auto labels = filterLabels();
        if (!hasFilterBar() || labels.empty()) return false;
        filterSelection_ = std::clamp(selection, 0, static_cast<int>(labels.size()) - 1);
        genre_.clear();
        letter_.clear();
        clearPage();
        switch (filterSelection_) {
            case 1: mode_ = BrowseContentMode::Favorites; break;
            case 2: mode_ = BrowseContentMode::Genres; break;
            case 3:
                mode_ = BrowseContentMode::Letters;
                populateLetters();
                return false;
            case 4: mode_ = BrowseContentMode::Collections; break;
            default: mode_ = BrowseContentMode::All; break;
        }
        return true;
    }

    void selectGenre(std::string genre) {
        genre_ = std::move(genre);
        mode_ = BrowseContentMode::GenreItems;
        clearPage();
    }

    void selectLetter(std::string letter) {
        letter_ = std::move(letter);
        mode_ = BrowseContentMode::LetterItems;
        clearPage();
    }

    void replacePage(std::vector<JellyfinItem> items, int pageSize) {
        const int received = static_cast<int>(items.size());
        items_ = std::move(items);
        selection_ = 0;
        nextIndex_ = received;
        hasMore_ = mode_ != BrowseContentMode::Genres && received == pageSize;
    }

    void appendPage(std::vector<JellyfinItem> items, int startIndex, int pageSize) {
        const int received = static_cast<int>(items.size());
        items_.insert(items_.end(), std::make_move_iterator(items.begin()), std::make_move_iterator(items.end()));
        nextIndex_ = startIndex + received;
        hasMore_ = mode_ != BrowseContentMode::Genres && received == pageSize;
    }

    void removeItem(const std::string& itemId) {
        if (itemId.empty()) return;
        auto remove = [&](std::vector<JellyfinItem>& items) {
            std::erase_if(items, [&](const JellyfinItem& item) { return item.id == itemId; });
        };
        remove(items_);
        for (auto& snapshot : stack_) remove(snapshot.items);
        setSelection(selection_);
    }

    [[nodiscard]] std::string heading() const {
        std::string value = activeContainer_.name.empty() ? "LIBRARY" : activeContainer_.name;
        if (mode_ == BrowseContentMode::Favorites) value += " - FAVORITES";
        else if (mode_ == BrowseContentMode::Genres) value += " - GENRES";
        else if (mode_ == BrowseContentMode::GenreItems && !genre_.empty()) value += " - " + genre_;
        else if (mode_ == BrowseContentMode::Letters) value += " - A-Z";
        else if (mode_ == BrowseContentMode::LetterItems && !letter_.empty()) value += " - " + letter_;
        else if (mode_ == BrowseContentMode::Collections) value = "COLLECTIONS";
        return value;
    }

    [[nodiscard]] bool syntheticPage() const {
        return mode_ == BrowseContentMode::Genres || mode_ == BrowseContentMode::Letters;
    }

    [[nodiscard]] const JellyfinItem& activeContainer() const { return activeContainer_; }
    [[nodiscard]] const std::vector<JellyfinItem>& items() const { return items_; }
    [[nodiscard]] std::vector<JellyfinItem>& items() { return items_; }
    [[nodiscard]] int selection() const { return selection_; }
    void setSelection(int value) {
        selection_ = items_.empty() ? 0 : std::clamp(value, 0, static_cast<int>(items_.size()) - 1);
    }
    [[nodiscard]] int nextIndex() const { return nextIndex_; }
    [[nodiscard]] bool hasMore() const { return hasMore_; }
    [[nodiscard]] bool nested() const { return !stack_.empty(); }
    [[nodiscard]] BrowseContentMode mode() const { return mode_; }
    [[nodiscard]] const std::string& genre() const { return genre_; }
    [[nodiscard]] const std::string& letter() const { return letter_; }
    [[nodiscard]] bool filterFocused() const { return filterFocused_; }
    void setFilterFocused(bool value) { filterFocused_ = value; }
    [[nodiscard]] int filterSelection() const { return filterSelection_; }

private:
    void clearPage() {
        items_.clear();
        selection_ = 0;
        nextIndex_ = 0;
        hasMore_ = false;
    }

    void populateLetters() {
        items_.clear();
        items_.reserve(26);
        for (char c = 'A'; c <= 'Z'; ++c) {
            JellyfinItem item;
            item.id = std::string(1, c);
            item.name = item.id;
            item.type = "Letter";
            items_.push_back(std::move(item));
        }
        selection_ = 0;
        nextIndex_ = static_cast<int>(items_.size());
        hasMore_ = false;
    }

    JellyfinItem activeContainer_;
    std::vector<JellyfinItem> items_;
    int selection_ = 0;
    int nextIndex_ = 0;
    bool hasMore_ = false;
    std::vector<BrowseSnapshot> stack_;
    BrowseContentMode mode_ = BrowseContentMode::All;
    bool filterFocused_ = false;
    int filterSelection_ = 0;
    std::string genre_;
    std::string letter_;
};
