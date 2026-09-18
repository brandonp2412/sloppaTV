#include "player_header_renderer.hpp"

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
    void text(float x, float y, float scale, std::string value, TestColor color, float maxWidth) {
        calls.push_back({x, y, scale, std::move(value), color.value, maxWidth});
    }

    std::vector<TextCall> calls;
};

std::string fit(std::string_view value, float, float maxWidth, int maxLines) {
    assert(maxWidth > 0.0f);
    assert(maxLines == 1);
    return std::string(value);
}
} // namespace

int main() {
    FakeRenderer regular;
    renderPlayerHeader(
        regular,
        PlayerHeaderRenderState{
            .heading = "Series",
            .secondary = "S1 E2  |  Episode",
            .showNextUp = false,
            .headlineScale = 3.0f,
            .secondaryY = 92.0f,
        },
        PlayerHeaderRenderStyle<TestColor>{
            .text = {1},
            .muted = {2},
        },
        fit);
    assert(regular.calls.size() == 2);
    assert(regular.calls[0].x == 80.0f);
    assert(regular.calls[0].y == 42.0f);
    assert(regular.calls[0].scale == 3.0f);
    assert(regular.calls[0].value == "Series");
    assert(regular.calls[0].color == 1);
    assert(regular.calls[0].maxWidth == 1460.0f);
    assert(regular.calls[1].y == 92.0f);
    assert(regular.calls[1].scale == 2.6f);
    assert(regular.calls[1].value == "S1 E2  |  Episode");
    assert(regular.calls[1].color == 2);
    assert(regular.calls[1].maxWidth == 1500.0f);

    FakeRenderer nextUp;
    renderPlayerHeader(
        nextUp,
        PlayerHeaderRenderState{
            .heading = "Series",
            .secondary = "Episode",
            .showNextUp = true,
            .headlineScale = 3.0f,
            .secondaryY = 92.0f,
        },
        PlayerHeaderRenderStyle<TestColor>{},
        fit);
    assert(nextUp.calls.size() == 2);
    assert(nextUp.calls[0].maxWidth == 1040.0f);
    assert(nextUp.calls[1].maxWidth == 1040.0f);

    FakeRenderer fallback;
    renderPlayerHeader(
        fallback,
        PlayerHeaderRenderState{
            .heading = "",
            .secondary = "",
            .showNextUp = false,
            .headlineScale = 3.0f,
            .secondaryY = 92.0f,
        },
        PlayerHeaderRenderStyle<TestColor>{},
        fit);
    assert(fallback.calls.size() == 1);
    assert(fallback.calls[0].value == "Playback");

    FakeRenderer duplicateSecondary;
    renderPlayerHeader(
        duplicateSecondary,
        PlayerHeaderRenderState{
            .heading = "Movie",
            .secondary = "Movie",
            .showNextUp = false,
            .headlineScale = 3.0f,
            .secondaryY = 92.0f,
        },
        PlayerHeaderRenderStyle<TestColor>{},
        fit);
    assert(duplicateSecondary.calls.size() == 1);

    return 0;
}
