#include "player_subtitle_renderer.hpp"

#include <cassert>
#include <cmath>
#include <string>
#include <string_view>
#include <vector>

namespace {
struct FakeRenderer {
    void roundedRect(float x, float y, float width, float height, float, int) {
        ++backgrounds;
        backgroundX = x;
        backgroundY = y;
        backgroundWidth = width;
        backgroundHeight = height;
    }

    void outlinedText(float x, float y, float, std::string_view value, int, int, float) {
        textX.push_back(x);
        textY.push_back(y);
        lines.emplace_back(value);
    }

    float textWidth(float, std::string_view value) {
        return static_cast<float>(value.size()) * 10.0f;
    }

    int backgrounds = 0;
    float backgroundX = 0.0f;
    float backgroundY = 0.0f;
    float backgroundWidth = 0.0f;
    float backgroundHeight = 0.0f;
    std::vector<float> textX;
    std::vector<float> textY;
    std::vector<std::string> lines;
};
} // namespace

int main() {
    assert(normalizePlayerSubtitleText("Hello   !  world") == "Hello! world");
    assert(normalizePlayerSubtitleText("wait \xE2\x80\xA6") == "wait...");
    assert(normalizePlayerSubtitleText("\xE3\x81\x82").empty());

    FakeRenderer empty;
    renderPlayerSubtitle(
        empty,
        PlayerSubtitleRenderState{
            .text = "",
            .boxMaxWidth = 1000.0f,
            .textScale = 2.0f,
            .lineHeight = 24.0f,
            .logicalWidth = 1920.0f,
            .bottomY = 900.0f,
            .showBackground = true,
        },
        PlayerSubtitleRenderStyle<int>{.cornerRadius = 24.0f, .text = 1, .background = 2, .outline = 3},
        [](std::string value, float, float, int) { return value; });
    assert(empty.backgrounds == 0);
    assert(empty.lines.empty());

    FakeRenderer renderer;
    std::string fittedInput;
    renderPlayerSubtitle(
        renderer,
        PlayerSubtitleRenderState{
            .text = "Hello   !  world",
            .boxMaxWidth = 1000.0f,
            .textScale = 2.0f,
            .lineHeight = 30.0f,
            .logicalWidth = 1920.0f,
            .bottomY = 900.0f,
            .showBackground = true,
        },
        PlayerSubtitleRenderStyle<int>{.cornerRadius = 24.0f, .text = 1, .background = 2, .outline = 3},
        [&](std::string value, float scale, float maxWidth, int maxLines) {
            fittedInput = value;
            assert(scale == 2.0f);
            assert(maxWidth == 936.0f);
            assert(maxLines == 3);
            return std::string("Hello!\nworld");
        });

    assert(fittedInput == "Hello! world");
    assert(renderer.backgrounds == 1);
    assert(renderer.lines.size() == 2);
    assert(renderer.lines[0] == "Hello!");
    assert(renderer.lines[1] == "world");
    assert(renderer.backgroundWidth == 320.0f);
    assert(renderer.backgroundHeight == 100.0f);
    assert(renderer.backgroundX == 800.0f);
    assert(renderer.backgroundY == 800.0f);
    assert(renderer.textX[0] == 930.0f);
    assert(renderer.textY[0] == 820.0f);
    assert(renderer.textY[1] == 850.0f);

    FakeRenderer noBackground;
    renderPlayerSubtitle(
        noBackground,
        PlayerSubtitleRenderState{
            .text = "One line",
            .boxMaxWidth = 1000.0f,
            .textScale = 2.0f,
            .lineHeight = 30.0f,
            .logicalWidth = 1920.0f,
            .bottomY = 900.0f,
            .showBackground = false,
        },
        PlayerSubtitleRenderStyle<int>{.cornerRadius = 24.0f, .text = 1, .background = 2, .outline = 3},
        [](std::string value, float, float, int) { return value; });
    assert(noBackground.backgrounds == 0);
    assert(noBackground.lines.size() == 1);

    return 0;
}
