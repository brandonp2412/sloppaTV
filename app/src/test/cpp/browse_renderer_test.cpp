#include "browse_renderer.hpp"

#include <array>
#include <cassert>
#include <string>
#include <string_view>
#include <vector>

namespace {
struct RectCall {
    float x = 0.0f;
    float y = 0.0f;
    float width = 0.0f;
    float height = 0.0f;
    int color = 0;
};

struct CenteredCall {
    std::string text;
    float x = 0.0f;
    float y = 0.0f;
    int color = 0;
};

struct ItemCall {
    std::string id;
    float x = 0.0f;
    float y = 0.0f;
    bool focused = false;
};

struct FakeRenderer {
    void roundedRect(float x, float y, float width, float height, float, int color) {
        rects.push_back({x, y, width, height, color});
    }

    void roundedOutline(float x, float y, float width, float height, float, float, int color) {
        outlines.push_back({x, y, width, height, color});
    }

    std::vector<RectCall> rects;
    std::vector<RectCall> outlines;
};

JellyfinItem makeItem(int index, std::string type = "Movie") {
    JellyfinItem item;
    item.id = "item-" + std::to_string(index);
    item.name = "Item " + std::to_string(index);
    item.type = std::move(type);
    return item;
}
} // namespace

int main() {
    BrowseRenderStyle<int> style{
        .cornerLarge = 28.0f,
        .focusSoft = 1,
        .panel = 2,
        .outline = 3,
        .text = 4,
        .muted = 5,
    };

    JellyfinItem library;
    library.id = "movies";
    library.name = "Movies";
    library.collectionType = "movies";

    BrowseScreenState state;
    state.resetForLibrary(library);
    state.setFilterFocused(true);
    state.moveFilter(2);

    std::vector<JellyfinItem> items;
    for (int index = 0; index < 12; ++index)
        items.push_back(makeItem(index, index % 3 == 0 ? "Episode" : "Movie"));
    state.replacePage(std::move(items), 50);
    state.setSelection(11);

    FakeRenderer renderer;
    std::vector<std::string> headers;
    std::vector<std::string> emptyTitles;
    std::vector<CenteredCall> centered;
    std::vector<RectCall> focusedTabs;
    std::vector<ItemCall> textTiles;
    std::vector<ItemCall> artworkCards;

    renderBrowseScreen(
        renderer, state, false, style,
        [&](std::string_view heading) { headers.emplace_back(heading); },
        [&](std::string_view title, std::string_view) { emptyTitles.emplace_back(title); },
        [&](float x, float y, float width, float height, bool focused, bool selected) {
            assert(focused);
            focusedTabs.push_back({x, y, width, height, selected ? 10 : 11});
            return std::array<float, 4>{x - 2.0f, y - 1.0f, width + 4.0f, height + 2.0f};
        },
        [&](float x, float y, float, float, float, std::string_view value, int color, float, float) {
            centered.push_back({std::string(value), x, y, color});
        },
        [&](const JellyfinItem& item, float x, float y, float, float, bool focused) {
            textTiles.push_back({item.id, x, y, focused});
        },
        [&](const JellyfinItem& item, float x, float y, float, bool focused, bool, bool, bool, int) {
            artworkCards.push_back({item.id, x, y, focused});
        });

    assert(headers.size() == 1);
    assert(headers.front() == "Movies");
    assert(emptyTitles.empty());

    assert(focusedTabs.size() == 1);
    assert(focusedTabs.front().x == 460.0f);
    assert(focusedTabs.front().color == 11);
    assert(renderer.rects.size() == 4);
    assert(renderer.outlines.size() == 3);
    assert(renderer.rects.front().color == style.focusSoft);
    assert(centered.size() == 5);
    assert(centered[0].text == "All");
    assert(centered[2].text == "Genres");

    assert(textTiles.empty());
    assert(artworkCards.size() == 7);
    assert(artworkCards.front().id == "item-5");
    assert(artworkCards.front().x == 80.0f);
    assert(artworkCards.front().y == 285.0f);
    assert(artworkCards.back().id == "item-11");
    assert(artworkCards.back().y == 675.0f);
    assert(!artworkCards.back().focused);

    BrowseScreenState emptyState;
    emptyState.resetForLibrary(library);
    std::string emptyMessage;
    renderBrowseScreen(
        renderer, emptyState, true, style,
        [](std::string_view) {},
        [&](std::string_view title, std::string_view message) {
            assert(title == "Loading your library");
            emptyMessage = message;
        },
        [](float x, float y, float width, float height, bool, bool) {
            return std::array<float, 4>{x, y, width, height};
        },
        [](float, float, float, float, float, std::string_view, int, float, float) {},
        [](const JellyfinItem&, float, float, float, float, bool) {},
        [](const JellyfinItem&, float, float, float, bool, bool, bool, bool, int) {});
    assert(emptyMessage == "Fetching titles from Jellyfin");

    BrowseScreenState syntheticState;
    syntheticState.resetForLibrary(library);
    assert(syntheticState.applyFilter(2));
    std::vector<JellyfinItem> genres;
    genres.push_back(makeItem(1, "Genre"));
    genres.push_back(makeItem(2, "Genre"));
    syntheticState.replacePage(std::move(genres), 50);

    std::vector<ItemCall> syntheticTiles;
    renderBrowseScreen(
        renderer, syntheticState, false, style,
        [](std::string_view) {},
        [](std::string_view, std::string_view) {},
        [](float x, float y, float width, float height, bool, bool) {
            return std::array<float, 4>{x, y, width, height};
        },
        [](float, float, float, float, float, std::string_view, int, float, float) {},
        [&](const JellyfinItem& item, float x, float y, float, float, bool focused) {
            syntheticTiles.push_back({item.id, x, y, focused});
        },
        [](const JellyfinItem&, float, float, float, bool, bool, bool, bool, int) {});

    assert(syntheticTiles.size() == 2);
    assert(syntheticTiles.front().id == "item-1");
    assert(syntheticTiles.front().y == 285.0f);
    assert(syntheticTiles.front().focused);

    return 0;
}
