#pragma once

#include "seerr_media.hpp"
#include "seerr_storage.hpp"
#include "screen_navigation_key.hpp"

#include <algorithm>
#include <cstdint>
#include <optional>
#include <string>
#include <vector>

struct SeerrSeason {
    int number = 0;
    int episodes = 0;
    std::string name;
    std::string airDate;
    int status = 0;
    bool requested = false;
    bool selected = false;

    [[nodiscard]] bool selectable() const { return episodes > 0 && !requested && status != 5 && status != 6; }

    [[nodiscard]] std::string statusLabel() const {
        if (status == 5) return "Available";
        if (status == 6) return "Blocklisted";
        if (requested) return status == 3 ? "Processing" : "Requested";
        if (episodes <= 0) return "No episodes yet";
        if (status == 4) return "Partially available";
        return selected ? "Selected" : "Not selected";
    }
};

class SeerrSeasonPickerState {
public:
    SeerrMediaItem item;
    std::optional<SeerrStorageTarget> target;
    std::vector<SeerrSeason> seasons;
    std::string error;
    bool loading = false;
    bool submitting = false;
    int selection = 0; // Continue, select/clear all, then seasons.
    uint64_t generation = 0;

    void open(SeerrMediaItem value, std::optional<SeerrStorageTarget> storage) {
        item = std::move(value);
        target = std::move(storage);
        seasons.clear();
        error.clear();
        selection = 0;
        loading = true;
        submitting = false;
        ++generation;
    }

    void cancel() {
        ++generation;
        loading = false;
    }

    void complete(uint64_t token, std::vector<SeerrSeason> values, std::string failure) {
        if (token != generation || !loading) return;
        loading = false;
        seasons = std::move(values);
        error = std::move(failure);
        selection = seasons.empty() ? 0 : 2;
    }

    [[nodiscard]] std::vector<int> selected() const {
        std::vector<int> result;
        for (const auto& season : seasons)
            if (season.selected && season.selectable()) result.push_back(season.number);
        return result;
    }

    [[nodiscard]] bool allSelected() const {
        bool any = false;
        for (const auto& season : seasons) {
            if (!season.selectable() || season.number == 0) continue;
            any = true;
            if (!season.selected) return false;
        }
        return any;
    }

    void navigate(ScreenNavigationKey key) {
        if (loading || !error.empty() || seasons.empty()) return;
        if (key == ScreenNavigationKey::Up) selection = std::max(0, selection - 1);
        if (key == ScreenNavigationKey::Down) selection = std::min(static_cast<int>(seasons.size()) + 1, selection + 1);
        if (key == ScreenNavigationKey::Left) selection = 0;
        if (key == ScreenNavigationKey::Right) selection = 2;
        if (key != ScreenNavigationKey::Activate && key != ScreenNavigationKey::Submit) return;
        if (selection == 1) {
            const bool select = !allSelected();
            for (auto& season : seasons)
                if (season.selectable() && (!select || season.number != 0)) season.selected = select;
        } else if (selection >= 2) {
            auto& season = seasons[static_cast<size_t>(selection - 2)];
            if (season.selectable()) season.selected = !season.selected;
        }
    }
};
