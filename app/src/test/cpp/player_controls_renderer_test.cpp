#include "player_controls_renderer.hpp"

#include <array>
#include <cassert>
#include <string>
#include <string_view>
#include <vector>

namespace {
struct FakeRenderer {
    void triangle(float, float, float, float, float, float, int) { ++triangles; }
    void roundedRect(float, float, float, float, float, int) { ++roundedRects; }
    void roundedOutline(float, float, float, float, float, float, int) { ++roundedOutlines; }
    void textCentered(float, float, float, float, float, std::string_view value, int) {
        centered.emplace_back(value);
    }
    void textVerticallyCentered(float, float, float, float, std::string_view value, int) {
        vertical.emplace_back(value);
    }
    float textWidth(float, std::string_view value) { return static_cast<float>(value.size()) * 10.0f; }

    int triangles = 0;
    int roundedRects = 0;
    int roundedOutlines = 0;
    std::vector<std::string> centered;
    std::vector<std::string> vertical;
};

struct Harness {
    FakeRenderer renderer;
    std::vector<std::size_t> focusedButtons;
    std::vector<std::size_t> primaryButtons;
    int fitCalls = 0;
    int labelCalls = 0;

    void render(const PlayerControlsRenderState& state) {
        std::size_t buttonIndex = 0;
        renderPlayerControls(
            renderer, state, PlayerControlsRenderStyle<int>{.text = 1},
            [&](float x, float y, float width, float height, bool focused, bool primary) {
                if (focused) focusedButtons.push_back(buttonIndex);
                if (primary) primaryButtons.push_back(buttonIndex);
                ++buttonIndex;
                return std::array<float, 4>{x, y, width, height};
            },
            [&](float scale, std::string_view, float, float) {
                ++fitCalls;
                return scale;
            },
            [&](std::string_view value) {
                ++labelCalls;
                return std::string(value);
            });
    }
};
} // namespace

int main() {
    Harness paused;
    paused.render(PlayerControlsRenderState{
        .paused = true,
        .selection = PlayerControl::AudioTrack,
        .logicalWidth = 1920.0f,
        .audioTrackLabel = "English",
        .subtitleTrackLabel = "Off",
    });
    assert((paused.focusedButtons == std::vector<std::size_t>{3}));
    assert((paused.primaryButtons == std::vector<std::size_t>{1}));
    assert(paused.fitCalls == 2);
    assert(paused.labelCalls == 2);
    assert(paused.renderer.triangles == 4);
    assert(paused.renderer.roundedOutlines == 1);
    assert((paused.renderer.centered == std::vector<std::string>{"CC"}));
    assert((paused.renderer.vertical == std::vector<std::string>{"Audio  English", "Subtitles  Off"}));

    Harness playing;
    playing.render(PlayerControlsRenderState{
        .paused = false,
        .selection = PlayerControl::SubtitleTrack,
        .logicalWidth = 1920.0f,
        .audioTrackLabel = "Director",
        .subtitleTrackLabel = "English",
    });
    assert((playing.focusedButtons == std::vector<std::size_t>{4}));
    assert((playing.primaryButtons == std::vector<std::size_t>{1}));
    assert(playing.renderer.triangles == 3);
    assert(playing.renderer.roundedRects > paused.renderer.roundedRects);
    assert((playing.renderer.vertical == std::vector<std::string>{"Audio  Director", "Subtitles  English"}));

    return 0;
}
