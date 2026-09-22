#include "seerr_seasons.hpp"
#include "seerr_season_parser.hpp"

#include <cassert>

int main() {
    const auto data = nlohmann::json::parse(R"({
        "seasons": [
            {"seasonNumber": 4, "episodeCount": 6, "name": "Season 4", "airDate": null},
            {"seasonNumber": 0, "episodeCount": 2, "name": "Specials"},
            {"seasonNumber": 1, "episodeCount": 10, "name": "Season 1"},
            {"seasonNumber": 2, "episodeCount": 8, "name": "Season 2"},
            {"seasonNumber": 3, "episodeCount": 8, "name": "Season 3"},
            {"seasonNumber": 5, "episodeCount": 0, "name": "Season 5"},
            {"seasonNumber": 4, "episodeCount": 6},
            {"seasonNumber": -1, "episodeCount": 6}
        ],
        "mediaInfo": {
            "status": 4,
            "seasons": [
                {"seasonNumber": 1, "status": 5, "status4k": 1},
                {"seasonNumber": 2, "status": 3, "status4k": 5},
                {"seasonNumber": 4, "status": 4, "status4k": 1}
            ],
            "requests": [
                {"status": 1, "is4k": false, "seasons": [{"seasonNumber": 3}]},
                {"status": 3, "is4k": false, "seasons": [{"seasonNumber": 4}]}
            ]
        }
    })");
    auto seasons = parseSeerrSeasons(data, false);
    assert(seasons.size() == 6);
    assert(seasons[0].number == 0 && seasons[0].selectable());
    assert(!seasons[1].selectable() && seasons[1].statusLabel() == "Available");
    assert(!seasons[2].selectable() && seasons[2].statusLabel() == "Processing");
    assert(!seasons[3].selectable() && seasons[3].statusLabel() == "Requested");
    assert(seasons[4].selectable() && seasons[4].statusLabel() == "Partially available");
    assert(!seasons[5].selectable());
    const auto uhd = parseSeerrSeasons(data, true);
    assert(uhd[1].selectable() && !uhd[2].selectable() && uhd[3].selectable());
    assert(parseSeerrSeasons(nlohmann::json::object(), false).empty());

    SeerrSeasonPickerState picker;
    SeerrMediaItem item;
    item.name = "Example";
    picker.open(item, std::nullopt);
    const auto first = picker.generation;
    picker.cancel();
    picker.complete(first, seasons, {});
    assert(picker.seasons.empty());
    picker.open(item, std::nullopt);
    picker.complete(first, seasons, {});
    assert(picker.loading);
    picker.complete(picker.generation, seasons, {});
    assert(!picker.loading && picker.selected().empty());
    picker.navigate(ScreenNavigationKey::Activate); // Specials require explicit selection.
    assert(picker.selected() == std::vector<int>{0});
    picker.navigate(ScreenNavigationKey::Up);
    picker.navigate(ScreenNavigationKey::Activate); // Select all missing regular seasons.
    assert((picker.selected() == std::vector<int>{0, 4}));
    assert(picker.allSelected());
    picker.navigate(ScreenNavigationKey::Activate);
    assert(picker.selected().empty());
    picker.selection = 3;
    picker.navigate(ScreenNavigationKey::Activate); // Available season cannot be selected.
    assert(picker.selected().empty());
    for (int i = 0; i < 20; ++i) picker.navigate(ScreenNavigationKey::Down);
    assert(picker.selection == 7);
    picker.navigate(ScreenNavigationKey::Left);
    assert(picker.selection == 0);
    picker.navigate(ScreenNavigationKey::Right);
    assert(picker.selection == 2);
    for (int i = 0; i < 20; ++i) picker.navigate(ScreenNavigationKey::Up);
    assert(picker.selection == 0);
    picker.open(item, std::nullopt);
    picker.complete(picker.generation, {}, "Offline");
    assert(!picker.loading && picker.error == "Offline");
    picker.navigate(ScreenNavigationKey::Down);
    assert(picker.selection == 0);
}
