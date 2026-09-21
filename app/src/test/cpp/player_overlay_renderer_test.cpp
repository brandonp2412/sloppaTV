#include "player_overlay_renderer.hpp"

#include <array>
#include <cassert>
#include <string>
#include <string_view>
#include <vector>

namespace {
struct TestColor {
    float alpha = 0.0f;
};

struct FakeRenderer {
    void verticalGradient(float x, float y, float width, float height, TestColor top, TestColor bottom) {
        assert(x == 0.0f);
        assert(width == 1920.0f);
        gradientY.push_back(y);
        gradientHeight.push_back(height);
        gradientTopAlpha.push_back(top.alpha);
        gradientBottomAlpha.push_back(bottom.alpha);
    }

    void roundedRect(float, float, float, float, float, TestColor) {
        ++roundedRects;
    }

    void outlinedText(float, float, float, std::string_view value, TestColor, TestColor, float) {
        ++outlinedTexts;
        subtitle = std::string(value);
    }

    float textWidth(float, std::string_view value) {
        return static_cast<float>(value.size()) * 10.0f;
    }

    void triangle(float, float, float, float, float, float, TestColor) {
        ++triangles;
    }

    void textVerticallyCentered(float, float, float, float, std::string_view value, TestColor, float) {
        ++centeredTexts;
        buttonLabel = std::string(value);
    }

    std::vector<float> gradientY;
    std::vector<float> gradientHeight;
    std::vector<float> gradientTopAlpha;
    std::vector<float> gradientBottomAlpha;
    int roundedRects = 0;
    int outlinedTexts = 0;
    int triangles = 0;
    int centeredTexts = 0;
    std::string subtitle;
    std::string buttonLabel;
};

TestColor color(float, float, float, float alpha) {
    return {.alpha = alpha};
}
} // namespace

int main() {
    FakeRenderer renderer;
    int fitCalls = 0;
    int buttonCalls = 0;

    renderPlayerOverlay(
        renderer,
        PlayerOverlayRenderState{
            .showOverlay = true,
            .subtitleText = "Hello",
            .subtitleBoxMaxWidth = 800.0f,
            .subtitleTextScale = 2.0f,
            .subtitleLineHeight = 30.0f,
            .logicalWidth = 1920.0f,
            .subtitleBottomY = 900.0f,
            .subtitleBackground = true,
            .skipButtonVisible = true,
            .skipLabel = "Skip Intro",
            .skipButtonY = 730.0f,
            .seekFeedbackVisible = true,
            .seekFeedbackSeconds = 10,
            .seekFeedbackAlpha = 0.5f,
        },
        PlayerOverlayRenderStyle<TestColor>{
            .subtitleCornerRadius = 24.0f,
            .text = color(1.0f, 1.0f, 1.0f, 1.0f),
        },
        [&](std::string_view value, float, float, int) {
            ++fitCalls;
            return std::string(value);
        },
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
        color);

    assert(renderer.gradientY.size() == 2);
    assert(renderer.gradientY[0] == 0.0f);
    assert(renderer.gradientHeight[0] == 250.0f);
    assert(renderer.gradientTopAlpha[0] == 0.74f);
    assert(renderer.gradientBottomAlpha[0] == 0.0f);
    assert(renderer.gradientY[1] == 650.0f);
    assert(renderer.gradientHeight[1] == 430.0f);
    assert(renderer.gradientTopAlpha[1] == 0.0f);
    assert(renderer.gradientBottomAlpha[1] == 0.90f);
    assert(renderer.outlinedTexts == 1);
    assert(renderer.subtitle == "Hello");
    assert(buttonCalls == 1);
    assert(renderer.buttonLabel == "Skip Intro");
    assert(renderer.triangles == 4);
    assert(renderer.roundedRects == 3);
    assert(fitCalls == 2);

    FakeRenderer quiet;
    renderPlayerOverlay(
        quiet, PlayerOverlayRenderState{}, PlayerOverlayRenderStyle<TestColor>{.text = color(1.0f, 1.0f, 1.0f, 1.0f)},
        [](std::string_view value, float, float, int) { return std::string(value); },
        [](float, float, float, float, bool, bool) {
            assert(false);
            return std::array<float, 4>{};
        },
        color);
    assert(quiet.gradientY.empty());
    assert(quiet.roundedRects == 0);
    assert(quiet.outlinedTexts == 0);
    assert(quiet.triangles == 0);

    return 0;
}
