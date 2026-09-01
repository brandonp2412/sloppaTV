#pragma once

#include <cstddef>
#include <vector>

template <typename Screen>
class NavigationStack {
public:
    explicit NavigationStack(Screen root) : entries_{root} {}

    [[nodiscard]] Screen current() const { return entries_.back(); }
    [[nodiscard]] Screen previousOr(Screen fallback) const {
        return entries_.size() > 1 ? entries_[entries_.size() - 2] : fallback;
    }
    [[nodiscard]] std::size_t depth() const { return entries_.size(); }

    void reset(Screen root) {
        entries_.clear();
        entries_.push_back(root);
    }

    void push(Screen screen) {
        if (entries_.empty()) {
            entries_.push_back(screen);
            return;
        }
        if (entries_.back() != screen) entries_.push_back(screen);
    }

    void replace(Screen screen) {
        if (entries_.empty()) entries_.push_back(screen);
        else entries_.back() = screen;
    }

    Screen popOr(Screen fallback) {
        if (entries_.size() > 1) entries_.pop_back();
        else if (entries_.empty()) entries_.push_back(fallback);
        return entries_.back();
    }

private:
    std::vector<Screen> entries_;
};
