#include "screen_chrome_renderer.hpp"

#include <cassert>
#include <string>
#include <string_view>
#include <vector>

namespace {
struct TextCall {
    float x = 0.0f;
    float y = 0.0f;
    float scale = 0.0f;
    std::string value;
    int color = 0;
    float maxWidth = 0.0f;
};

struct RectCall {
    float x = 0.0f;
    float y = 0.0f;
    float width = 0.0f;
    float height = 0.0f;
    float radius = 0.0f;
    int color = 0;
};

struct FakeRenderer {
    void text(float x, float y, float scale, std::string_view value, int color) {
        texts.push_back({x, y, scale, std::string(value), color, 0.0f});
    }

    void text(float x, float y, float scale, std::string_view value, int color, float maxWidth) {
        texts.push_back({x, y, scale, std::string(value), color, maxWidth});
    }

    void roundedRect(float x, float y, float width, float height, float radius, int color) {
        rects.push_back({x, y, width, height, radius, color});
    }

    std::vector<TextCall> texts;
    std::vector<RectCall> rects;
};
} // namespace

int main() {
    const ScreenChromeRenderStyle<int> style{
        .pageInset = 72.0f,
        .supportingScale = 1.55f,
        .headlineScale = 3.8f,
        .titleScale = 2.8f,
        .labelScale = 1.8f,
        .cornerLarge = 28.0f,
        .muted = 1,
        .text = 2,
        .panelAlt = 3,
    };

    FakeRenderer renderer;
    int clockCalls = 0;
    renderScreenHeader(
        renderer, "Library", true, true, style,
        [](std::string_view value, float scale, float width, int lines) {
            assert(value == "Library");
            assert(scale == 3.8f);
            assert(width == 1480.0f);
            assert(lines == 1);
            return std::string(value);
        },
        [&](float right, float y, float scale, std::string_view value, int color, float maxWidth) {
            ++clockCalls;
            assert(right == 1840.0f);
            assert(y == 52.0f);
            assert(scale == 2.05f);
            assert(value == "21:45");
            assert(color == style.muted);
            assert(maxWidth == 210.0f);
        },
        [](bool clock24Hour) {
            assert(clock24Hour);
            return std::string("21:45");
        });

    assert(renderer.texts.size() == 2);
    assert(renderer.texts[0].value == "sloppaTV");
    assert(renderer.texts[0].x == 72.0f);
    assert(renderer.texts[0].y == 28.0f);
    assert(renderer.texts[0].scale == 1.55f);
    assert(renderer.texts[0].color == style.muted);
    assert(renderer.texts[1].value == "Library");
    assert(renderer.texts[1].y == 76.0f);
    assert(renderer.texts[1].scale == 3.8f);
    assert(renderer.texts[1].maxWidth == 1480.0f);
    assert(clockCalls == 1);

    FakeRenderer noClockRenderer;
    renderScreenHeader(
        noClockRenderer, "Search", false, false, style,
        [](std::string_view value, float, float, int) { return std::string(value); },
        [](float, float, float, std::string_view, int, float) { assert(false); },
        [](bool) {
            assert(false);
            return std::string();
        });
    assert(noClockRenderer.texts.size() == 2);

    std::vector<TextCall> centered;
    renderScreenEmptyState(
        renderer, "Nothing here", "Try another filter", style,
        [&](float x, float y, float width, float height, float scale, std::string_view value, int color, float, float) {
            centered.push_back({x, y, scale, std::string(value), color, width + height});
        });
    assert(renderer.rects.size() == 1);
    assert(renderer.rects[0].x == 440.0f);
    assert(renderer.rects[0].y == 350.0f);
    assert(renderer.rects[0].width == 1040.0f);
    assert(renderer.rects[0].height == 250.0f);
    assert(renderer.rects[0].radius == style.cornerLarge);
    assert(renderer.rects[0].color == style.panelAlt);
    assert(centered.size() == 2);
    assert(centered[0].value == "Nothing here");
    assert(centered[0].scale == style.titleScale);
    assert(centered[0].color == style.text);
    assert(centered[1].value == "Try another filter");
    assert(centered[1].scale == style.labelScale);
    assert(centered[1].color == style.muted);

    return 0;
}
