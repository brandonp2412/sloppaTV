#include "queue_overlay_renderer.hpp"

#include <array>
#include <cassert>
#include <string>
#include <string_view>
#include <vector>

namespace {
struct FakeRenderer {
    void rect(float, float, float, float, int) { ++rects; }

    void roundedRect(float, float, float, float, float, int) { ++roundedRects; }

    int rects = 0;
    int roundedRects = 0;
};

struct Harness {
    FakeRenderer renderer;
    int modals = 0;
    int rows = 0;
    int artwork = 0;
    int placeholders = 0;
    int actions = 0;
    int unavailableActions = 0;
    std::vector<std::string> leftText;
    std::vector<std::string> centeredText;

    void render(const QueueOverlayRenderState& state) {
        renderQueueOverlayScreen(
            renderer, state,
            QueueOverlayRenderStyle<int>{
                .artworkCornerRadius = 8.0f,
                .scrim = 1,
                .text = 2,
                .muted = 3,
                .tertiary = 4,
                .focusSoft = 5,
                .panelAlt = 6,
            },
            [&](float, float, float, float) { ++modals; },
            [&](float, float, float, float, float, std::string_view value, int) { leftText.emplace_back(value); },
            [&](float, float, float, float, float, std::string_view value, int, float, float) {
                centeredText.emplace_back(value);
            },
            [&](float x, float y, float width, float height, bool) {
                ++rows;
                return std::array<float, 4>{x, y, width, height};
            },
            [&](const JellyfinItem& item, float, float, float, float, float) {
                ++artwork;
                return item.name != "Episode 3";
            },
            [&](const JellyfinItem&, float, float, float, float, float) { ++placeholders; },
            [](const JellyfinItem& item) { return item.seriesName; },
            [&](float x, float y, float width, float height, bool, bool, bool) {
                ++actions;
                return std::array<float, 4>{x, y, width, height};
            },
            [&](float, float, float, float) { ++unavailableActions; },
            [](std::string_view value) { return std::string(value); });
    }
};

bool has(const std::vector<std::string>& values, std::string_view expected) {
    for (const auto& value : values)
        if (value == expected) return true;
    return false;
}
} // namespace

int main() {
    const std::vector<JellyfinItem> empty;
    Harness noItems;
    noItems.render(QueueOverlayRenderState{.items = empty});
    assert(noItems.renderer.rects == 0);
    assert(noItems.modals == 0);

    std::vector<JellyfinItem> items;
    for (int index = 0; index < 7; ++index) {
        JellyfinItem item;
        item.name = "Episode " + std::to_string(index);
        item.seriesName = "Season 1";
        items.push_back(std::move(item));
    }

    Harness queue;
    queue.render(QueueOverlayRenderState{
        .items = items,
        .currentIndex = 1,
        .selection = 3,
        .actionSelection = 4,
        .repeatMode = QueueRepeatMode::All,
    });
    assert(queue.renderer.rects == 1);
    assert(queue.modals == 1);
    assert(queue.rows == 5);
    assert(queue.artwork == 5);
    assert(queue.placeholders == 1);
    assert(queue.actions == 7);
    assert(queue.unavailableActions == 0);
    assert(has(queue.leftText, "Playback queue"));
    assert(has(queue.leftText, "Episode 3"));
    assert(has(queue.leftText, "Season 1"));
    assert(has(queue.centeredText, "6 remaining"));
    assert(has(queue.centeredText, "Current"));
    assert(has(queue.centeredText, "Next"));
    assert(has(queue.centeredText, "Remove"));
    assert(has(queue.centeredText, "Repeat ALL"));
    assert(has(queue.centeredText, "Back closes queue"));

    Harness atCurrent;
    atCurrent.render(QueueOverlayRenderState{
        .items = items,
        .currentIndex = 1,
        .selection = 1,
        .actionSelection = 0,
        .repeatMode = QueueRepeatMode::Off,
    });
    assert(atCurrent.actions < 7);
    assert(atCurrent.unavailableActions > 0);

    return 0;
}
