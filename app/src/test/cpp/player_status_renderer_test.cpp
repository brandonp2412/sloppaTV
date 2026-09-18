#include "player_status_renderer.hpp"

#include <cassert>
#include <string>
#include <string_view>
#include <vector>

namespace {
struct TestColor {
    int value = 0;
};

struct TextCall {
    float x = 0.0f;
    float y = 0.0f;
    float scale = 0.0f;
    std::string value;
    int color = 0;
    float maxWidth = 0.0f;
};

struct FakeRenderer {
    float textWidth(float scale, std::string_view value) {
        assert(scale == 1.75f);
        return value == "Ends 10:30 PM" ? finishWidth : 0.0f;
    }

    void text(float x, float y, float scale, std::string_view value, TestColor color, float maxWidth) {
        calls.push_back({x, y, scale, std::string(value), color.value, maxWidth});
    }

    float finishWidth = 180.0f;
    std::vector<TextCall> calls;
};
} // namespace

int main() {
    FakeRenderer renderer;
    int clockCalls = 0;
    renderPlayerStatus(
        renderer,
        PlayerStatusRenderState{
            .clockText = "10:15 PM",
            .finishText = "Ends 10:30 PM",
            .statusText = "Switching track",
        },
        PlayerStatusRenderStyle<TestColor>{
            .muted = {1},
            .secondary = {2},
        },
        [&](float right, float y, float scale, std::string_view value, TestColor color, float maxWidth) {
            ++clockCalls;
            assert(right == 1840.0f);
            assert(y == 46.0f);
            assert(scale == 2.05f);
            assert(value == "10:15 PM");
            assert(color.value == 1);
            assert(maxWidth == 210.0f);
        });

    assert(clockCalls == 1);
    assert(renderer.calls.size() == 2);
    assert(renderer.calls[0].x == 1590.0f);
    assert(renderer.calls[0].y == 772.0f);
    assert(renderer.calls[0].scale == 1.75f);
    assert(renderer.calls[0].value == "Ends 10:30 PM");
    assert(renderer.calls[0].color == 2);
    assert(renderer.calls[0].maxWidth == 590.0f);
    assert(renderer.calls[1].x == 80.0f);
    assert(renderer.calls[1].y == 772.0f);
    assert(renderer.calls[1].scale == 2.0f);
    assert(renderer.calls[1].value == "Switching track");
    assert(renderer.calls[1].maxWidth == 580.0f);

    FakeRenderer clamped;
    clamped.finishWidth = 900.0f;
    renderPlayerStatus(
        clamped,
        PlayerStatusRenderState{
            .clockText = {},
            .finishText = "Ends 10:30 PM",
            .statusText = {},
        },
        PlayerStatusRenderStyle<TestColor>{},
        [](float, float, float, std::string_view, TestColor, float) {});
    assert(clamped.calls.size() == 1);
    assert(clamped.calls[0].x == 1180.0f);

    FakeRenderer empty;
    int emptyClockCalls = 0;
    renderPlayerStatus(
        empty, PlayerStatusRenderState{}, PlayerStatusRenderStyle<TestColor>{},
        [&](float, float, float, std::string_view, TestColor, float) { ++emptyClockCalls; });
    assert(empty.calls.empty());
    assert(emptyClockCalls == 0);

    return 0;
}
