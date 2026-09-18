#include "search_renderer.hpp"

#include <array>
#include <cassert>
#include <chrono>
#include <string>
#include <string_view>
#include <vector>

namespace {
struct TextCall {
    std::string value;
    float x = 0.0f;
    float y = 0.0f;
    int color = 0;
};

struct RectCall {
    float x = 0.0f;
    float y = 0.0f;
    float width = 0.0f;
    float height = 0.0f;
    int color = 0;
};

struct FakeRenderer {
    void text(float x, float y, float, std::string_view value, int color, float) {
        texts.push_back({std::string(value), x, y, color});
    }

    void textVerticallyCentered(float x, float y, float, float, std::string_view value, int color, float) {
        texts.push_back({std::string(value), x, y, color});
    }

    void roundedRect(float x, float y, float width, float height, float, int color) {
        roundedRects.push_back({x, y, width, height, color});
    }

    void rect(float x, float y, float width, float height, int color) {
        rects.push_back({x, y, width, height, color});
    }

    std::vector<TextCall> texts;
    std::vector<RectCall> roundedRects;
    std::vector<RectCall> rects;
};

bool contains(const std::vector<TextCall>& calls, std::string_view value) {
    for (const auto& call : calls)
        if (call.value == value) return true;
    return false;
}

bool contains(const std::vector<std::string>& values, std::string_view value) {
    for (const auto& item : values)
        if (item == value) return true;
    return false;
}
} // namespace

int main() {
    const SearchRenderStyle<int> style{
        .headlineScale = 3.0f,
        .cornerSmall = 16.0f,
        .wideInputFocusScale = 1.015f,
        .cardFocusScale = 1.025f,
        .text = 1,
        .muted = 2,
        .secondaryText = 3,
        .focus = 4,
        .divider = 5,
        .panel = 6,
        .panelAlt = 7,
        .panelElevated = 8,
        .error = 9,
    };

    std::vector<SeerrMediaItem> noSeerr;
    SearchScreenState keyboardState(noSeerr);
    FakeRenderer keyboardRenderer;
    int keyboardCalls = 0;
    int emptyCalls = 0;
    renderSearchScreen(
        keyboardRenderer, keyboardState, false, false, false, "", false, {}, style,
        [](float x, float y, float width, float height, bool, float) {
            return std::array<float, 4>{x, y, width, height};
        },
        [](std::string_view value, float, float, int) { return std::string(value); },
        [](float, float, float, float, float, std::string_view, int) {},
        [](float, float, float, float, float, std::string_view, int, float, float) {},
        [&](float top) {
            ++keyboardCalls;
            assert(top == 270.0f);
        },
        [&](std::string_view, std::string_view) { ++emptyCalls; },
        [](float x, float y, float width, float height, bool, float) {
            return std::array<float, 4>{x, y, width, height};
        },
        [](const JellyfinItem&, float, float, float, float, float) { return true; },
        [](const JellyfinItem&, float, float, float, float, float) {},
        [](float, float, float, float, int, float) {},
        [](float, float, float, std::string_view, float, int, SearchScreenState::Clock::time_point) {},
        [](const JellyfinItem&, float, float, float, bool, bool, bool, bool, int) {},
        [] { return SearchScreenState::Clock::time_point{}; },
        [](int, float) { return 99; });

    assert(keyboardCalls == 1);
    assert(emptyCalls == 0);
    assert(contains(keyboardRenderer.texts, "Search"));

    std::vector<SeerrMediaItem> seerrItems(1);
    seerrItems[0].id = "seerr:movie:2";
    seerrItems[0].mediaType = "movie";
    seerrItems[0].tmdbId = 2;
    seerrItems[0].name = "Remote Movie";

    SearchScreenState state(seerrItems);
    state.setQuery("movie");
    state.setKeyboard(false);
    JellyfinItem local;
    local.id = "local-1";
    local.name = "Local Movie";
    local.type = "Movie";
    local.tmdbId = "1";
    assert(state.finishLibrarySearch("movie", {local}));
    state.refreshSeerrResults();
    assert(state.results().size() == 2);
    state.setSelection(1);

    std::vector<SeerrStorageTarget> storage{
        {.mediaType = "", .serviceName = "", .path = "/media/a", .serverId = -1, .profileId = 0,
         .freeSpace = 10, .totalSpace = 100, .isDefault = false, .is4k = false},
        {.mediaType = "", .serviceName = "", .path = "/media/a", .serverId = -1, .profileId = 0,
         .freeSpace = 10, .totalSpace = 100, .isDefault = false, .is4k = false},
        {.mediaType = "", .serviceName = "", .path = "/media/b", .serverId = -1, .profileId = 0,
         .freeSpace = 100, .totalSpace = 200, .isDefault = false, .is4k = false},
    };

    FakeRenderer renderer;
    std::vector<std::string> centered;
    int mediaCards = 0;
    int placeholders = 0;
    int halos = 0;
    int lingering = 0;
    int emptyStates = 0;
    renderSearchScreen(
        renderer, state, true, false, false, "", false, storage, style,
        [](float x, float y, float width, float height, bool focused, float) {
            assert(!focused);
            return std::array<float, 4>{x, y, width, height};
        },
        [](std::string_view value, float, float, int) { return std::string(value); },
        [](float, float, float, float, float, std::string_view, int) {},
        [&](float, float, float, float, float, std::string_view value, int, float, float) {
            centered.emplace_back(value);
        },
        [](float) { assert(false); },
        [&](std::string_view, std::string_view) { ++emptyStates; },
        [](float x, float y, float width, float height, bool focused, float) {
            if (focused) {
                return std::array<float, 4>{x - 2.0f, y - 1.0f, width + 4.0f, height + 2.0f};
            }
            return std::array<float, 4>{x, y, width, height};
        },
        [](const JellyfinItem&, float, float, float, float, float) { return false; },
        [&](const JellyfinItem& item, float, float, float, float, float) {
            ++placeholders;
            assert(item.id == "seerr:movie:2");
        },
        [&](float, float, float, float, int color, float) {
            ++halos;
            assert(color == style.focus);
        },
        [&](float, float, float, std::string_view value, float, int color, SearchScreenState::Clock::time_point) {
            ++lingering;
            assert(value == "Remote Movie");
            assert(color == style.text);
        },
        [&](const JellyfinItem& item, float, float, float, bool focused, bool, bool, bool, int) {
            ++mediaCards;
            assert(item.id == "local-1");
            assert(!focused);
        },
        [] { return SearchScreenState::Clock::time_point{std::chrono::seconds(1)}; },
        [](int, float) { return 99; });

    assert(emptyStates == 0);
    assert(mediaCards == 1);
    assert(placeholders == 1);
    assert(halos == 1);
    assert(lingering == 1);
    assert(renderer.rects.size() == 1);
    assert(contains(renderer.texts, "In your library"));
    assert(contains(renderer.texts, "Seerr"));
    assert(contains(centered, "90%"));
    assert(contains(centered, "50%"));

    SearchScreenState loadingState(noSeerr);
    loadingState.setQuery("loading");
    loadingState.setKeyboard(false);
    loadingState.setLoading(true);
    FakeRenderer loadingRenderer;
    renderSearchScreen(
        loadingRenderer, loadingState, false, false, false, "", false, {}, style,
        [](float x, float y, float width, float height, bool, float) {
            return std::array<float, 4>{x, y, width, height};
        },
        [](std::string_view value, float, float, int) { return std::string(value); },
        [](float, float, float, float, float, std::string_view, int) {},
        [](float, float, float, float, float, std::string_view, int, float, float) {},
        [](float) {},
        [](std::string_view, std::string_view) {},
        [](float x, float y, float width, float height, bool, float) {
            return std::array<float, 4>{x, y, width, height};
        },
        [](const JellyfinItem&, float, float, float, float, float) { return true; },
        [](const JellyfinItem&, float, float, float, float, float) {},
        [](float, float, float, float, int, float) {},
        [](float, float, float, std::string_view, float, int, SearchScreenState::Clock::time_point) {},
        [](const JellyfinItem&, float, float, float, bool, bool, bool, bool, int) {},
        [] { return SearchScreenState::Clock::time_point{std::chrono::seconds(1)}; },
        [](int, float) { return 99; });

    assert(contains(loadingRenderer.texts, "Searching Jellyfin…"));
    int pulseRects = 0;
    for (const auto& rect : loadingRenderer.roundedRects)
        if (rect.color == 99) ++pulseRects;
    assert(pulseRects == 3);

    SearchScreenState emptyState(noSeerr);
    emptyState.setKeyboard(false);
    std::string emptyTitle;
    renderSearchScreen(
        loadingRenderer, emptyState, false, false, false, "", false, {}, style,
        [](float x, float y, float width, float height, bool, float) {
            return std::array<float, 4>{x, y, width, height};
        },
        [](std::string_view value, float, float, int) { return std::string(value); },
        [](float, float, float, float, float, std::string_view, int) {},
        [](float, float, float, float, float, std::string_view, int, float, float) {},
        [](float) {},
        [&](std::string_view title, std::string_view) { emptyTitle = title; },
        [](float x, float y, float width, float height, bool, float) {
            return std::array<float, 4>{x, y, width, height};
        },
        [](const JellyfinItem&, float, float, float, float, float) { return true; },
        [](const JellyfinItem&, float, float, float, float, float) {},
        [](float, float, float, float, int, float) {},
        [](float, float, float, std::string_view, float, int, SearchScreenState::Clock::time_point) {},
        [](const JellyfinItem&, float, float, float, bool, bool, bool, bool, int) {},
        [] { return SearchScreenState::Clock::time_point{}; },
        [](int, float) { return 99; });

    assert(emptyTitle == "Find your next favorite");

    return 0;
}
