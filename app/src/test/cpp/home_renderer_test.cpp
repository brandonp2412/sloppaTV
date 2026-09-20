#include "home_renderer.hpp"

#include <array>
#include <cassert>
#include <chrono>
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

struct TextCall {
    std::string value;
    float x = 0.0f;
    float y = 0.0f;
    int color = 0;
};

struct RowCall {
    std::string title;
    int row = -1;
    float top = 0.0f;
};

struct FakeRenderer {
    void rect(float x, float y, float width, float height, int color) {
        rects.push_back({x, y, width, height, color});
    }

    void roundedRect(float x, float y, float width, float height, float, int color) {
        roundedRects.push_back({x, y, width, height, color});
    }

    void text(float x, float y, float, std::string_view value, int color, float) {
        texts.push_back({std::string(value), x, y, color});
    }

    float textWidth(float, std::string_view value) const {
        return static_cast<float>(value.size()) * 10.0f;
    }

    std::vector<RectCall> rects;
    std::vector<RectCall> roundedRects;
    std::vector<TextCall> texts;
};

JellyfinHomeRow row(std::string title, std::string id) {
    JellyfinHomeRow result;
    result.title = std::move(title);
    JellyfinItem item;
    item.id = std::move(id);
    result.items.push_back(std::move(item));
    return result;
}
} // namespace

int main() {
    const HomeRenderStyle<int> style{
        .canvasWidth = 1920.0f,
        .canvasHeight = 1080.0f,
        .cornerLarge = 28.0f,
        .buttonFocusScale = 1.025f,
        .background = 1,
        .backdropScrim = 2,
        .text = 3,
        .muted = 4,
        .clockMuted = 5,
        .brandGold = 6,
        .focusSoft = 7,
        .panelElevated = 8,
        .panelAlt = 9,
        .focus = 10,
    };

    std::vector<JellyfinHomeRow> rows{
        row("Continue Watching", "episode-1"),
        row("Next Up", "episode-2"),
        row("Latest", "movie-1"),
    };

    HomeScreenState state;
    state.reset();
    state.setSelections({0, 0, 0});
    state.setRow(1);
    state.setFirstVisibleRow(0);

    JellyfinSession session;
    session.username = "brandon";

    FakeRenderer renderer;
    std::vector<std::string> backdrops;
    std::vector<std::string> centered;
    std::vector<RowCall> renderedRows;
    int emptyCalls = 0;
    int profileArtworkCalls = 0;
    int tabSurfaceCalls = 0;

    renderHomeScreen(
        renderer, rows, state, session,
        HomeRenderConfig{
            .backdropMode = 1,
            .showClock = true,
            .clock24Hour = true,
            .uiTextSize = 1,
            .loading = false,
        },
        HomeSlideState{}, style,
        [&](const JellyfinItem& item, float alpha) {
            backdrops.push_back(item.id);
            assert(alpha == 0.24f);
            return true;
        },
        [](float x, float y, float size) {
            assert(x == 72.0f && y == 27.0f && size == 72.0f);
            return true;
        },
        [&](float x, float y, float width, float height, bool focused, bool) {
            ++tabSurfaceCalls;
            assert(focused);
            return std::array<float, 4>{x, y, width, height};
        },
        [&](float, float, float, float, float, std::string_view value, int, float, float) {
            centered.emplace_back(value);
        },
        [](float x, float y, float width, float height, bool, float) {
            return std::array<float, 4>{x, y, width, height};
        },
        [&](const JellyfinSession& saved, float, float, float) {
            ++profileArtworkCalls;
            assert(saved.username == "brandon");
            return true;
        },
        [](float, float, float, float, int, float) { assert(false); },
        [&](float right, float y, float scale, std::string_view value, int color, float maxWidth) {
            assert(right == 1900.0f);
            assert(y == 53.0f);
            assert(scale == 2.10f);
            assert(value == "21:45");
            assert(color == style.clockMuted);
            assert(maxWidth == 150.0f);
        },
        [](bool clock24Hour) {
            assert(clock24Hour);
            return std::string("21:45");
        },
        [&](std::string_view, std::string_view) { ++emptyCalls; },
        [&](std::string_view title, const std::vector<JellyfinItem>& items, int rowIndex, float top) {
            assert(items.size() == 1);
            renderedRows.push_back({std::string(title), rowIndex, top});
        },
        [] { return std::chrono::steady_clock::time_point{std::chrono::seconds(10)}; });

    assert(backdrops.size() == 1);
    assert(backdrops.front() == "episode-2");
    assert(renderer.rects.size() == 1);
    assert(renderer.rects.front().color == style.backdropScrim);
    assert(renderer.texts.size() == 1);
    assert(renderer.texts.front().value == "sloppaTV");
    assert(renderer.texts.front().x == 160.0f);
    assert(tabSurfaceCalls == 0);
    assert(profileArtworkCalls == 1);
    assert(emptyCalls == 0);
    assert(renderedRows.size() == 2);
    assert(renderedRows[0].title == "Continue Watching");
    assert(renderedRows[0].row == 0);
    assert(renderedRows[0].top == 150.0f);
    assert(renderedRows[1].title == "Next Up");
    assert(renderedRows[1].top == 555.0f);

    HomeScreenState toolbarState;
    toolbarState.reset();
    toolbarState.setSelections({0, 0, 0});
    toolbarState.focusToolbar(2);

    FakeRenderer toolbarRenderer;
    int focusedTab = 0;
    int profileHalos = 0;
    renderHomeScreen(
        toolbarRenderer, rows, toolbarState, session, HomeRenderConfig{}, HomeSlideState{}, style,
        [](const JellyfinItem&, float) { return false; },
        [](float, float, float) { return false; },
        [&](float x, float y, float width, float height, bool focused, bool selected) {
            ++focusedTab;
            assert(focused);
            assert(!selected);
            assert(x > 960.0f);
            return std::array<float, 4>{x, y, width, height};
        },
        [](float, float, float, float, float, std::string_view, int, float, float) {},
        [](float x, float y, float width, float height, bool, float) {
            return std::array<float, 4>{x, y, width, height};
        },
        [](const JellyfinSession&, float, float, float) { return false; },
        [&](float, float, float, float, int, float) { ++profileHalos; },
        [](float, float, float, std::string_view, int, float) {},
        [](bool) { return std::string(); },
        [](std::string_view, std::string_view) {},
        [](std::string_view, const std::vector<JellyfinItem>&, int, float) {},
        [] { return std::chrono::steady_clock::time_point{std::chrono::seconds(10)}; });

    assert(focusedTab == 1);
    assert(profileHalos == 0);

    HomeScreenState profileState;
    profileState.reset();
    profileState.setSelections({0, 0, 0});
    profileState.focusToolbar(0);
    JellyfinSession noArtworkSession;
    noArtworkSession.username = "alice";

    std::vector<std::string> profileCentered;
    int halos = 0;
    renderHomeScreen(
        toolbarRenderer, rows, profileState, noArtworkSession, HomeRenderConfig{}, HomeSlideState{}, style,
        [](const JellyfinItem&, float) { return false; },
        [](float, float, float) { return false; },
        [](float x, float y, float width, float height, bool, bool) {
            return std::array<float, 4>{x, y, width, height};
        },
        [&](float, float, float, float, float, std::string_view value, int, float, float) {
            profileCentered.emplace_back(value);
        },
        [](float x, float y, float width, float height, bool focused, float) {
            if (focused) return std::array<float, 4>{x - 1.0f, y - 1.0f, width + 2.0f, height + 2.0f};
            return std::array<float, 4>{x, y, width, height};
        },
        [](const JellyfinSession&, float, float, float) { return false; },
        [&](float, float, float, float, int color, float) {
            ++halos;
            assert(color == style.focus);
        },
        [](float, float, float, std::string_view, int, float) {},
        [](bool) { return std::string(); },
        [](std::string_view, std::string_view) {},
        [](std::string_view, const std::vector<JellyfinItem>&, int, float) {},
        [] { return std::chrono::steady_clock::time_point{std::chrono::seconds(10)}; });

    assert(halos == 1);
    bool sawInitial = false;
    for (const auto& value : profileCentered)
        if (value == "A") sawInitial = true;
    assert(sawInitial);

    HomeScreenState emptyState;
    emptyState.reset();
    std::string emptyTitle;
    renderHomeScreen(
        toolbarRenderer, {}, emptyState, session,
        HomeRenderConfig{.backdropMode = 0, .showClock = false, .clock24Hour = true, .uiTextSize = 1, .loading = true},
        HomeSlideState{}, style,
        [](const JellyfinItem&, float) { return false; },
        [](float, float, float) { return false; },
        [](float x, float y, float width, float height, bool, bool) {
            return std::array<float, 4>{x, y, width, height};
        },
        [](float, float, float, float, float, std::string_view, int, float, float) {},
        [](float x, float y, float width, float height, bool, float) {
            return std::array<float, 4>{x, y, width, height};
        },
        [](const JellyfinSession&, float, float, float) { return true; },
        [](float, float, float, float, int, float) {},
        [](float, float, float, std::string_view, int, float) {},
        [](bool) { return std::string(); },
        [&](std::string_view title, std::string_view message) {
            emptyTitle = title;
            assert(message == "Connecting to your Jellyfin server");
        },
        [](std::string_view, const std::vector<JellyfinItem>&, int, float) { assert(false); },
        [] { return std::chrono::steady_clock::time_point{std::chrono::seconds(10)}; });
    assert(emptyTitle == "Loading your library");

    HomeScreenState slideState;
    slideState.reset();
    slideState.setSelections({0, 0, 0});
    slideState.setRow(2);
    slideState.setFirstVisibleRow(0);

    std::vector<RowCall> slidingRows;
    const auto slideStarted = std::chrono::steady_clock::time_point{std::chrono::seconds(10)};
    renderHomeScreen(
        toolbarRenderer, rows, slideState, session, HomeRenderConfig{.uiTextSize = 1},
        HomeSlideState{.fromFirst = 0, .toFirst = 1, .started = slideStarted}, style,
        [](const JellyfinItem&, float) { return false; },
        [](float, float, float) { return false; },
        [](float x, float y, float width, float height, bool, bool) {
            return std::array<float, 4>{x, y, width, height};
        },
        [](float, float, float, float, float, std::string_view, int, float, float) {},
        [](float x, float y, float width, float height, bool, float) {
            return std::array<float, 4>{x, y, width, height};
        },
        [](const JellyfinSession&, float, float, float) { return true; },
        [](float, float, float, float, int, float) {},
        [](float, float, float, std::string_view, int, float) {},
        [](bool) { return std::string(); },
        [](std::string_view, std::string_view) {},
        [&](std::string_view title, const std::vector<JellyfinItem>&, int rowIndex, float top) {
            slidingRows.push_back({std::string(title), rowIndex, top});
        },
        [slideStarted] { return slideStarted + std::chrono::milliseconds(110); });

    assert(slidingRows.size() == 3);
    assert(slidingRows[0].row == 0);
    assert(slidingRows[0].top == -52.5f);
    assert(slidingRows[1].row == 1);
    assert(slidingRows[1].top == 352.5f);
    assert(slidingRows[2].row == 2);
    assert(slidingRows[2].top == 757.5f);

    return 0;
}
