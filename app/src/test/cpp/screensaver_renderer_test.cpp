#include "screensaver_renderer.hpp"

#include <cassert>
#include <string>
#include <string_view>
#include <vector>

namespace {
struct RectCall {
    float width = 0.0f;
    float height = 0.0f;
    int color = 0;
};

struct TextCall {
    std::string text;
    float x = 0.0f;
    float y = 0.0f;
    float scale = 0.0f;
    int color = 0;
};

struct FakeRenderer {
    void rect(float, float, float width, float height, int color) { rects.push_back({width, height, color}); }

    std::vector<RectCall> rects;
};
} // namespace

int main() {
    const ScreensaverRenderStyle<int> style{
        .background = 1,
        .primary = 2,
        .text = 3,
        .tertiary = 4,
    };

    FakeRenderer renderer;
    std::vector<TextCall> leftText;
    std::vector<TextCall> centeredText;
    renderScreensaverScreen(
        renderer, 30, "22:45", 1920.0f, 1080.0f, style,
        [&](float x, float y, float, float, float scale, std::string_view value, int color) {
            leftText.push_back({std::string(value), x, y, scale, color});
        },
        [&](float x, float y, float, float, float scale, std::string_view value, int color, float, float) {
            centeredText.push_back({std::string(value), x, y, scale, color});
        });

    assert(renderer.rects.size() == 1);
    assert(renderer.rects[0].width == 1920.0f);
    assert(renderer.rects[0].height == 1080.0f);
    assert(renderer.rects[0].color == style.background);

    assert(leftText.size() == 2);
    assert(leftText[0].text == "sloppaTV");
    assert(leftText[0].x == 1120.0f);
    assert(leftText[0].y == 170.0f);
    assert(leftText[0].scale == 4.2f);
    assert(leftText[0].color == style.primary);
    assert(leftText[1].text == "22:45");
    assert(leftText[1].x == 1120.0f);
    assert(leftText[1].y == 282.0f);
    assert(leftText[1].scale == 9.0f);
    assert(leftText[1].color == style.text);

    assert(centeredText.size() == 1);
    assert(centeredText[0].text == "Press any button to return");
    assert(centeredText[0].x == 600.0f);
    assert(centeredText[0].y == 1008.0f);
    assert(centeredText[0].scale == 1.45f);
    assert(centeredText[0].color == style.tertiary);

    leftText.clear();
    centeredText.clear();
    renderer.rects.clear();
    renderScreensaverScreen(
        renderer, -100, "10:00", 1920.0f, 1080.0f, style,
        [&](float x, float y, float, float, float scale, std::string_view value, int color) {
            leftText.push_back({std::string(value), x, y, scale, color});
        },
        [&](float x, float y, float, float, float scale, std::string_view value, int color, float, float) {
            centeredText.push_back({std::string(value), x, y, scale, color});
        });

    assert(leftText[0].x == 170.0f);
    assert(leftText[0].y == 170.0f);
    assert(leftText[1].text == "10:00");

    return 0;
}
