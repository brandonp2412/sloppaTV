#include "text_layout.hpp"

#include <cassert>
#include <chrono>
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
    bool bounded = false;
};

struct ClipCall {
    float x = 0.0f;
    float y = 0.0f;
    float width = 0.0f;
    float height = 0.0f;
};

struct FakeRenderer {
    float textWidth(float scale, std::string_view value) const {
        return static_cast<float>(value.size()) * scale * 10.0f;
    }

    void text(float x, float y, float scale, std::string_view value, int color, float maxWidth) {
        texts.push_back({x, y, scale, std::string(value), color, maxWidth, true});
    }

    void text(float x, float y, float scale, std::string_view value, int color) {
        texts.push_back({x, y, scale, std::string(value), color, 0.0f, false});
    }

    void beginClipRect(float x, float y, float width, float height) {
        clips.push_back({x, y, width, height});
    }

    void endClipRect() {
        ++clipEnds;
    }

    std::vector<TextCall> texts;
    std::vector<ClipCall> clips;
    int clipEnds = 0;
};
} // namespace

int main() {
    FakeRenderer renderer;

    assert(fitRenderedTextLines(renderer, "", 1.0f, 100.0f, 1).empty());
    assert(fitRenderedTextLines(renderer, "Alpha Beta", 1.0f, 90.0f, 2) == "Alpha\nBeta");
    assert(fitRenderedTextLines(renderer, "ABCDEFGHIJ", 1.0f, 70.0f, 1) == "ABCD...");
    assert(fitRenderedTextLines(renderer, "caf\xC3\xA9", 1.0f, 100.0f, 1) == "cafe");

    const auto start = std::chrono::steady_clock::time_point{} + std::chrono::seconds(10);
    renderLingeringTitle(renderer, 20.0f, 30.0f, 1.0f, "Short", 80.0f, 4, start, start);
    assert(renderer.texts.size() == 1);
    assert(renderer.texts.back().value == "Short");
    assert(renderer.texts.back().x == 20.0f);
    assert(renderer.texts.back().maxWidth == 80.0f);
    assert(renderer.clips.empty());

    renderer.texts.clear();
    renderLingeringTitle(renderer, 20.0f, 30.0f, 1.0f, "Long title", 60.0f, 4, start,
                         start + std::chrono::milliseconds(1000));
    assert(renderer.texts.size() == 1);
    assert(renderer.texts.back().value == "Lon...");
    assert(renderer.texts.back().bounded);
    assert(renderer.clips.empty());

    renderer.texts.clear();
    renderLingeringTitle(renderer, 20.0f, 30.0f, 1.0f, "Long title", 60.0f, 4, start,
                         start + std::chrono::milliseconds(2200));
    assert(renderer.texts.size() == 1);
    assert(renderer.texts.back().value == "Long title      Long title");
    assert(!renderer.texts.back().bounded);
    assert(renderer.texts.back().x < 20.0f);
    assert(renderer.clips.size() == 1);
    assert(renderer.clips.front().x == 20.0f);
    assert(renderer.clips.front().y == 26.0f);
    assert(renderer.clips.front().width == 60.0f);
    assert(renderer.clips.front().height == 64.0f);
    assert(renderer.clipEnds == 1);

    return 0;
}
