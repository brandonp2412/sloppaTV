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
    renderPlayerStatus(renderer,
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
    renderPlayerStatus(clamped,
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
    renderPlayerStatus(empty, PlayerStatusRenderState{}, PlayerStatusRenderStyle<TestColor>{},
                       [&](float, float, float, std::string_view, TestColor, float) { ++emptyClockCalls; });
    assert(empty.calls.empty());
    assert(emptyClockCalls == 0);

    // Playback controls may time out while a subtitle download or media cache
    // stall remains pending, so the compact status must independently persist.
    PlayerBufferIndicator buffer;
    using Clock = PlayerBufferIndicator::Clock;
    const auto start = Clock::time_point{std::chrono::seconds(10)};
    buffer.observe(true, false, false, false, false, 0, 0, true, start);
    assert(playerWaitStatus(true, false, false, buffer, start).find("Resolving Remux") != std::string::npos);
    buffer.observe(false, true, false, false, false, 0, 0, true, start);
    assert(playerWaitStatus(false, true, false, buffer, start).find("Opening Remux") != std::string::npos);
    buffer.observe(false, false, true, false, false, 150000, 10000, true, start + std::chrono::seconds(1));
    assert(playerWaitStatus(false, false, true, buffer, start + std::chrono::seconds(2)) == "Loading subtitles");
    assert(playerWaitStatus(false, false, false, buffer, start + std::chrono::seconds(2)).empty());
    buffer.observe(false, false, true, false, true, 150000, 0, true, start + std::chrono::seconds(5));
    assert(playerWaitStatus(false, false, true, buffer, start + std::chrono::seconds(5)).find("Buffering from Remux") != std::string::npos);
    assert(playerWaitStatus(false, false, false, buffer, start + std::chrono::seconds(5)) == "Buffering from Remux");
    return 0;
}
