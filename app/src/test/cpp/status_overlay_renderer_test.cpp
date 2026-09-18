#include "status_overlay_renderer.hpp"

#include <cassert>
#include <string>
#include <string_view>
#include <vector>

namespace {
struct RectCall {
    float x = 0.0f;
    float y = 0.0f;
    float width = 0.0f;
    float height = 0.0f;
    int color = 0;
};

struct TextCall {
    float x = 0.0f;
    float y = 0.0f;
    std::string text;
};

struct FakeRenderer {
    void roundedRect(float x, float y, float width, float height, float, int color) {
        rects.push_back({x, y, width, height, color});
    }

    void roundedOutline(float x, float y, float width, float height, float, float, int color) {
        outlines.push_back({x, y, width, height, color});
    }

    void textVerticallyCentered(float x, float y, float, float, std::string_view value, int, float) {
        texts.push_back({x, y, std::string(value)});
    }

    std::vector<RectCall> rects;
    std::vector<RectCall> outlines;
    std::vector<TextCall> texts;
};

bool hasText(const std::vector<TextCall>& calls, std::string_view text, float y) {
    for (const auto& call : calls)
        if (call.text == text && call.y == y) return true;
    return false;
}
} // namespace

int main() {
    const StatusOverlayRenderStyle<int> style{
        .cornerMedium = 20.0f,
        .panelElevated = 1,
        .focus = 2,
        .error = 3,
        .errorOutline = 4,
        .text = 5,
    };

    FakeRenderer loading;
    renderStatusOverlay(
        loading,
        StatusOverlayRenderState{
            .loading = true,
            .playerScreen = false,
            .noticeVisible = false,
            .notice = {},
            .errorVisible = false,
            .error = {},
        },
        style, [](std::string_view value, float, float, int) { return std::string(value); });
    assert(loading.rects.size() == 2);
    assert(loading.rects[0].x == 1600.0f && loading.rects[0].y == 120.0f);
    assert(hasText(loading.texts, "Loading…", 120.0f));

    FakeRenderer player;
    renderStatusOverlay(
        player,
        StatusOverlayRenderState{
            .loading = false,
            .playerScreen = true,
            .noticeVisible = true,
            .notice = "Track changed",
            .errorVisible = true,
            .error = "Playback failed",
        },
        style, [](std::string_view value, float, float, int) { return std::string(value); });
    assert(player.rects.size() == 4);
    assert(player.outlines.size() == 1);
    assert(player.outlines[0].y == 588.0f);
    assert(player.outlines[0].color == style.errorOutline);
    assert(hasText(player.texts, "Track changed", 670.0f));
    assert(hasText(player.texts, "Playback failed", 588.0f));

    FakeRenderer browse;
    renderStatusOverlay(
        browse,
        StatusOverlayRenderState{
            .loading = false,
            .playerScreen = false,
            .noticeVisible = true,
            .notice = "Saved",
            .errorVisible = true,
            .error = "Request failed",
        },
        style, [](std::string_view value, float, float, int) { return std::string(value); });
    assert(hasText(browse.texts, "Saved", 914.0f));
    assert(hasText(browse.texts, "Request failed", 834.0f));

    FakeRenderer errorOnly;
    renderStatusOverlay(
        errorOnly,
        StatusOverlayRenderState{
            .loading = false,
            .playerScreen = true,
            .noticeVisible = false,
            .notice = {},
            .errorVisible = true,
            .error = "Decode failed",
        },
        style, [](std::string_view value, float, float, int) { return std::string(value); });
    assert(hasText(errorOnly.texts, "Decode failed", 670.0f));

    return 0;
}
