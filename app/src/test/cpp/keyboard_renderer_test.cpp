#include "keyboard_renderer.hpp"

#include <array>
#include <cassert>
#include <string>
#include <string_view>
#include <vector>

namespace {
struct Key {
    std::string label;
};

struct RectCall {
    float x = 0.0f;
    float y = 0.0f;
    float width = 0.0f;
    float height = 0.0f;
    float radius = 0.0f;
    int color = 0;
};

struct ButtonCall {
    float x = 0.0f;
    float y = 0.0f;
    float width = 0.0f;
    float height = 0.0f;
    bool focused = false;
    bool primary = false;
};

struct TextCall {
    std::string value;
    float scale = 0.0f;
};

struct FakeRenderer {
    void roundedRect(float x, float y, float width, float height, float radius, int color) {
        rects.push_back({x, y, width, height, radius, color});
    }

    std::vector<RectCall> rects;
};
} // namespace

int main() {
    const std::vector<std::vector<Key>> rows{
        {Key{"A"}, Key{"LONGER"}},
        {Key{"SPACE"}, Key{"DONE"}},
    };
    const KeyboardRenderStyle<int> style{
        .canvasHeight = 1080.0f,
        .cornerLarge = 28.0f,
        .surfaceContainerHigh = 1,
        .text = 2,
    };

    FakeRenderer renderer;
    std::vector<ButtonCall> buttons;
    std::vector<TextCall> labels;
    renderVirtualKeyboard(
        renderer, rows, 1, 0, 610.0f, style,
        [&](float x, float y, float width, float height, bool focused, bool primary) {
            buttons.push_back({x, y, width, height, focused, primary});
            return std::array<float, 4>{x, y, width, height};
        },
        [&](float, float, float, float, float scale, std::string_view value, int color, float horizontalPadding,
            float verticalPadding) {
            assert(color == style.text);
            assert(horizontalPadding == 12.0f);
            assert(verticalPadding == 5.0f);
            labels.push_back({std::string(value), scale});
        });

    assert(renderer.rects.size() == 1);
    assert(renderer.rects[0].x == 110.0f);
    assert(renderer.rects[0].y == 586.0f);
    assert(renderer.rects[0].width == 1700.0f);
    assert(renderer.rects[0].height == 494.0f);
    assert(renderer.rects[0].radius == style.cornerLarge);
    assert(renderer.rects[0].color == style.surfaceContainerHigh);

    assert(buttons.size() == 4);
    assert(buttons[0].x == 150.0f);
    assert(buttons[0].width == 145.0f);
    assert(!buttons[0].focused);
    assert(buttons[2].x == 150.0f);
    assert(buttons[2].width == 310.0f);
    assert(buttons[2].focused);
    assert(buttons[2].primary);
    assert(buttons[3].x == 474.0f);

    assert(labels.size() == 4);
    assert(labels[0].value == "A");
    assert(labels[0].scale == 2.65f);
    assert(labels[1].value == "LONGER");
    assert(labels[1].scale == 2.15f);
    assert(labels[2].value == "Space");
    assert(labels[2].scale == 2.15f);
    assert(labels[3].value == "Done");
    assert(labels[3].scale == 2.65f);

    return 0;
}
