#include "player_subtitle_offset_renderer.hpp"

#include <cassert>
#include <cmath>
#include <string>
#include <string_view>
#include <vector>

struct FakeRenderer {
    struct Rect {
        float x;
        float y;
        float width;
        float height;
    };

    std::vector<Rect> rectangles;
    std::vector<std::string> texts;

    void roundedRect(float x, float y, float width, float height, float, int) {
        rectangles.push_back({x, y, width, height});
    }

    void textCentered(float, float, float, float, float, std::string_view text, int) { texts.emplace_back(text); }
};

int main() {
    assert(subtitleOffsetLabel(0) == "+0.00 s  In sync");
    assert(subtitleOffsetLabel(250) == "+0.25 s  Later");
    assert(subtitleOffsetLabel(-750) == "-0.75 s  Earlier");
    assert(subtitleOffsetDotX(0, 960.0f, 600.0f) == 960.0f);
    assert(subtitleOffsetDotX(250, 960.0f, 600.0f) > 960.0f);
    assert(subtitleOffsetDotX(-250, 960.0f, 600.0f) < 960.0f);
    assert(subtitleOffsetDotX(120000, 960.0f, 600.0f) <= 1560.0f);
    FakeRenderer renderer;
    renderSubtitleOffsetOverlay(renderer, -250, 1920.0f, 1080.0f, 1, 2, 3, 4);
    assert(renderer.rectangles.size() == 4);
    assert(renderer.rectangles[3].x < 960.0f);
    assert(renderer.texts.size() == 3);
    assert(renderer.texts[0] == "Subtitle offset");
    assert(renderer.texts[1] == "-0.25 s  Earlier");
}
