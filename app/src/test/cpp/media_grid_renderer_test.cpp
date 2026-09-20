#include "media_grid_renderer.hpp"

#include <cassert>
#include <string>
#include <string_view>
#include <vector>

namespace {
struct CardCall {
    std::string id;
    MediaGridCardPlacement placement;
};

struct EmptyCall {
    std::string title;
    std::string message;
};
} // namespace

int main() {
    std::vector<JellyfinItem> mixed;
    for (int index = 0; index < 12; ++index) {
        JellyfinItem item;
        item.id = "item-" + std::to_string(index);
        item.type = index == 0 ? "Movie" : "Episode";
        mixed.push_back(std::move(item));
    }

    std::vector<std::string> headers;
    std::vector<EmptyCall> emptyCalls;
    std::vector<CardCall> cards;
    renderMediaGridScreen(
        "Episodes", mixed,
        MediaGridRenderState{
            .loading = false,
            .selection = 11,
            .uiTextSize = 2,
        },
        [&](std::string_view title) { headers.emplace_back(title); },
        [&](std::string_view title, std::string_view message) {
            emptyCalls.push_back({std::string(title), std::string(message)});
        },
        [&](const JellyfinItem& item, const MediaGridCardPlacement& placement) {
            cards.push_back({item.id, placement});
        });

    assert(headers.size() == 1 && headers[0] == "Episodes");
    assert(emptyCalls.empty());
    assert(cards.size() == 7);
    assert(cards.front().id == "item-5");
    assert(cards.front().placement.x == 80.0f);
    assert(cards.front().placement.y == 195.0f);
    assert(cards.front().placement.slotWidth == mediaCardWidth());
    assert(!cards.front().placement.focused);
    assert(cards.front().placement.preferSeriesCover);
    assert(cards.front().placement.alignToPortraitBand);
    assert(cards.front().placement.titleLineLimit == 0);
    assert(cards.back().id == "item-11");
    assert(cards.back().placement.x == 432.0f);
    assert(cards.back().placement.y == 625.0f);
    assert(cards.back().placement.focused);
    assert(cards.back().placement.titleLineLimit == 1);

    std::vector<JellyfinItem> landscape(6);
    for (int index = 0; index < static_cast<int>(landscape.size()); ++index) {
        landscape[static_cast<std::size_t>(index)].id = "episode-" + std::to_string(index);
        landscape[static_cast<std::size_t>(index)].type = "Episode";
    }
    cards.clear();
    renderMediaGridScreen(
        "Landscape", landscape,
        MediaGridRenderState{
            .loading = false,
            .selection = 5,
            .uiTextSize = 2,
        },
        [](std::string_view) {}, [](std::string_view, std::string_view) {},
        [&](const JellyfinItem& item, const MediaGridCardPlacement& placement) {
            cards.push_back({item.id, placement});
        });
    assert(cards.size() == 6);
    assert(!cards.front().placement.preferSeriesCover);
    assert(!cards.front().placement.alignToPortraitBand);
    assert(cards.back().placement.y == 495.0f);
    assert(cards.back().placement.titleLineLimit == 0);

    emptyCalls.clear();
    renderMediaGridScreen(
        "Empty", {},
        MediaGridRenderState{
            .loading = true,
            .selection = 0,
            .uiTextSize = 0,
        },
        [](std::string_view) {},
        [&](std::string_view title, std::string_view message) {
            emptyCalls.push_back({std::string(title), std::string(message)});
        },
        [](const JellyfinItem&, const MediaGridCardPlacement&) {});
    assert(emptyCalls.size() == 1);
    assert(emptyCalls[0].title == "Loading titles");
    assert(emptyCalls[0].message == "Fetching titles from Jellyfin");

    emptyCalls.clear();
    renderMediaGridScreen(
        "Empty", {},
        MediaGridRenderState{
            .loading = false,
            .selection = 0,
            .uiTextSize = 0,
        },
        [](std::string_view) {},
        [&](std::string_view title, std::string_view message) {
            emptyCalls.push_back({std::string(title), std::string(message)});
        },
        [](const JellyfinItem&, const MediaGridCardPlacement&) {});
    assert(emptyCalls.size() == 1);
    assert(emptyCalls[0].title == "No titles available");
    assert(emptyCalls[0].message == "Press Back to return to your library.");

    return 0;
}
