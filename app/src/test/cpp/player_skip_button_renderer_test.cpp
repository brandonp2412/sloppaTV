#include "player_skip_button_renderer.hpp"

#include <array>
#include <cassert>
#include <string>
#include <string_view>

namespace {
struct FakeRenderer {
    void triangle(float, float, float, float, float, float, int) { ++triangles; }
    void roundedRect(float, float, float, float, float, int) { ++roundedRects; }
    float textWidth(float scale, std::string_view value) {
        measuredScale = scale;
        measured = std::string(value);
        return 90.0f;
    }
    void textVerticallyCentered(float x, float y, float height, float scale, std::string_view value, int,
                                float maxWidth) {
        textX = x;
        textY = y;
        textHeight = height;
        textScale = scale;
        text = std::string(value);
        textMaxWidth = maxWidth;
    }

    int triangles = 0;
    int roundedRects = 0;
    float measuredScale = 0.0f;
    std::string measured;
    float textX = 0.0f;
    float textY = 0.0f;
    float textHeight = 0.0f;
    float textScale = 0.0f;
    std::string text;
    float textMaxWidth = 0.0f;
};
} // namespace

int main() {
    FakeRenderer renderer;
    int buttonCalls = 0;
    std::string fittedInput;
    renderPlayerSkipButton(
        renderer, PlayerSkipButtonRenderState{.label = "Skip Intro", .y = 730.0f},
        PlayerSkipButtonRenderStyle<int>{.text = 7},
        [&](float x, float y, float width, float height, bool focused, bool primary) {
            ++buttonCalls;
            assert(x == 1480.0f);
            assert(y == 730.0f);
            assert(width == 320.0f);
            assert(height == 74.0f);
            assert(focused);
            assert(primary);
            return std::array<float, 4>{x, y, width, height};
        },
        [&](std::string_view value, float scale, float width, int lines) {
            fittedInput = value;
            assert(scale == 1.82f);
            assert(width == 224.0f);
            assert(lines == 1);
            return std::string("Skip Intro");
        });

    assert(buttonCalls == 1);
    assert(fittedInput == "Skip Intro");
    assert(renderer.triangles == 2);
    assert(renderer.roundedRects == 1);
    assert(renderer.measured == "Skip Intro");
    assert(renderer.measuredScale == 1.82f);
    assert(renderer.text == "Skip Intro");
    assert(renderer.textY == 730.0f);
    assert(renderer.textHeight == 74.0f);
    assert(renderer.textScale == 1.82f);
    assert(renderer.textMaxWidth == 90.0f);

    return 0;
}
