#include "player_progress_renderer.hpp"

#include <cassert>
#include <string>
#include <string_view>
#include <vector>

namespace {
struct TestColor {
    int value = 0;
};

struct Rect {
    float x = 0.0f;
    float y = 0.0f;
    float width = 0.0f;
    float height = 0.0f;
    float radius = 0.0f;
    int color = 0;
};

struct FakeRenderer {
    void text(float x, float y, float scale, std::string_view value, TestColor color) {
        textX = x;
        textY = y;
        textScale = scale;
        textValue = value;
        textColor = color.value;
    }

    void roundedRect(float x, float y, float width, float height, float radius, TestColor color) {
        rects.push_back({x, y, width, height, radius, color.value});
    }

    float textX = 0.0f;
    float textY = 0.0f;
    float textScale = 0.0f;
    std::string textValue;
    int textColor = 0;
    std::vector<Rect> rects;
};
} // namespace

int main() {
    FakeRenderer renderer;
    float durationRight = 0.0f;
    std::string durationText;
    renderPlayerProgress(
        renderer,
        PlayerProgressRenderState{
            .positionMs = 5000,
            .durationMs = 10'000,
            .skipButtonVisible = false,
            .positionText = "00:05",
            .durationText = "00:10",
        },
        PlayerProgressRenderStyle<TestColor>{
            .text = {1},
            .track = {2},
            .focus = {3},
        },
        [&](float right, float y, float scale, std::string_view value, TestColor color, float maxWidth) {
            durationRight = right;
            durationText = value;
            assert(y == 834.0f);
            assert(scale == 2.0f);
            assert(color.value == 1);
            assert(maxWidth == 220.0f);
        });

    assert(renderer.textX == 150.0f);
    assert(renderer.textY == 834.0f);
    assert(renderer.textScale == 2.0f);
    assert(renderer.textValue == "00:05");
    assert(renderer.textColor == 1);
    assert(durationRight == 1770.0f);
    assert(durationText == "00:10");
    assert(renderer.rects.size() == 3);
    assert(renderer.rects[0].x == 150.0f);
    assert(renderer.rects[0].y == 890.0f);
    assert(renderer.rects[0].width == 1620.0f);
    assert(renderer.rects[0].height == 7.0f);
    assert(renderer.rects[0].radius == 3.5f);
    assert(renderer.rects[0].color == 2);
    assert(renderer.rects[1].width == 810.0f);
    assert(renderer.rects[1].color == 3);
    assert(renderer.rects[2].x == 951.0f);
    assert(renderer.rects[2].y == 884.5f);
    assert(renderer.rects[2].width == 18.0f);
    assert(renderer.rects[2].height == 18.0f);
    assert(renderer.rects[2].radius == 9.0f);
    assert(renderer.rects[2].color == 1);

    FakeRenderer skipped;
    renderPlayerProgress(
        skipped,
        PlayerProgressRenderState{
            .positionMs = 20'000,
            .durationMs = 10'000,
            .skipButtonVisible = true,
            .positionText = "00:20",
            .durationText = "00:10",
        },
        PlayerProgressRenderStyle<TestColor>{},
        [&](float right, float, float, std::string_view, TestColor, float) { durationRight = right; });
    assert(durationRight == 1430.0f);
    assert(skipped.rects.size() == 3);
    assert(skipped.rects[1].width == 1620.0f);
    assert(skipped.rects[2].x == 1752.0f);

    FakeRenderer unknownDuration;
    renderPlayerProgress(
        unknownDuration,
        PlayerProgressRenderState{
            .positionMs = 1000,
            .durationMs = 0,
            .skipButtonVisible = false,
            .positionText = "00:01",
            .durationText = "00:00",
        },
        PlayerProgressRenderStyle<TestColor>{},
        [](float, float, float, std::string_view, TestColor, float) {});
    assert(unknownDuration.rects.size() == 1);

    return 0;
}
