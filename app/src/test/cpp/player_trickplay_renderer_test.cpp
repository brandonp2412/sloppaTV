#include "player_trickplay_renderer.hpp"

#include <cassert>
#include <cmath>
#include <string>
#include <string_view>

namespace {
struct TestColor {
    int value = 0;
};

struct FakeRenderer {
    void roundedRect(float x, float y, float width, float height, float radius, TestColor) {
        ++roundedRects;
        panelX = x;
        panelY = y;
        panelWidth = width;
        panelHeight = height;
        panelRadius = radius;
    }

    void roundedOutline(float, float, float, float, float, float width, TestColor) {
        ++outlines;
        outlineWidth = width;
    }

    void roundedImageRegion(uint32_t texture, float x, float y, float width, float height, float radius, float u0,
                            float v0, float u1, float v1) {
        ++images;
        imageTexture = texture;
        imageX = x;
        imageY = y;
        imageWidth = width;
        imageHeight = height;
        imageRadius = radius;
        imageU0 = u0;
        imageV0 = v0;
        imageU1 = u1;
        imageV1 = v1;
    }

    int roundedRects = 0;
    int outlines = 0;
    int images = 0;
    float panelX = 0.0f;
    float panelY = 0.0f;
    float panelWidth = 0.0f;
    float panelHeight = 0.0f;
    float panelRadius = 0.0f;
    float outlineWidth = 0.0f;
    uint32_t imageTexture = 0;
    float imageX = 0.0f;
    float imageY = 0.0f;
    float imageWidth = 0.0f;
    float imageHeight = 0.0f;
    float imageRadius = 0.0f;
    float imageU0 = 0.0f;
    float imageV0 = 0.0f;
    float imageU1 = 0.0f;
    float imageV1 = 0.0f;
};

JellyfinTrickplayInfo info() {
    return JellyfinTrickplayInfo{
        .mediaSourceId = "source-1",
        .width = 320,
        .height = 180,
        .tileWidth = 2,
        .tileHeight = 2,
        .thumbnailCount = 8,
        .intervalMs = 10'000,
    };
}
} // namespace

int main() {
    const JellyfinTrickplayInfo trickplay = info();
    const TrickplayFrame frame{.thumbnailIndex = 3, .tileIndex = 0, .cellX = 1, .cellY = 1};

    FakeRenderer renderer;
    std::string label;
    float textX = 0.0f;
    float textY = 0.0f;
    const bool rendered = renderPlayerTrickplay(
        renderer,
        PlayerTrickplayRenderState{
            .texture = 42,
            .frame = frame,
            .info = trickplay,
            .decodedWidth = 640,
            .decodedHeight = 360,
            .positionMs = 5000,
            .durationMs = 10'000,
            .logicalWidth = 1920.0f,
            .positionLabel = "00:05",
        },
        PlayerTrickplayRenderStyle<TestColor>{
            .previewRadius = 12.0f,
            .backdrop = {1},
            .focus = {2},
            .text = {3},
        },
        [&](float x, float y, float width, float height, float scale, std::string_view value, TestColor color) {
            textX = x;
            textY = y;
            label = value;
            assert(width == 392.0f);
            assert(height == 44.0f);
            assert(scale == 1.65f);
            assert(color.value == 3);
        });

    assert(rendered);
    assert(renderer.roundedRects == 1);
    assert(renderer.outlines == 1);
    assert(renderer.images == 1);
    assert(renderer.imageTexture == 42);
    assert(std::abs(renderer.imageX - 750.0f) < 0.001f);
    assert(renderer.imageY == 555.0f);
    assert(renderer.imageWidth == 420.0f);
    assert(std::abs(renderer.imageHeight - 236.25f) < 0.001f);
    assert(renderer.imageRadius == 12.0f);
    assert(std::abs(renderer.imageU0 - 0.5f) < 0.001f);
    assert(std::abs(renderer.imageV0 - 0.5f) < 0.001f);
    assert(renderer.imageU1 == 1.0f);
    assert(renderer.imageV1 == 1.0f);
    assert(std::abs(renderer.panelX - 743.0f) < 0.001f);
    assert(renderer.panelY == 548.0f);
    assert(renderer.panelWidth == 434.0f);
    assert(std::abs(renderer.panelHeight - 294.25f) < 0.001f);
    assert(renderer.panelRadius == 19.0f);
    assert(renderer.outlineWidth == 4.0f);
    assert(std::abs(textX - 764.0f) < 0.001f);
    assert(std::abs(textY - 798.25f) < 0.001f);
    assert(label == "00:05");

    FakeRenderer left;
    assert(renderPlayerTrickplay(left,
                                 PlayerTrickplayRenderState{
                                     .texture = 1,
                                     .frame = frame,
                                     .info = trickplay,
                                     .decodedWidth = 640,
                                     .decodedHeight = 360,
                                     .positionMs = 0,
                                     .durationMs = 10'000,
                                     .logicalWidth = 1920.0f,
                                     .positionLabel = "00:00",
                                 },
                                 PlayerTrickplayRenderStyle<TestColor>{},
                                 [](float, float, float, float, float, std::string_view, TestColor) {}));
    assert(left.imageX == 80.0f);

    FakeRenderer right;
    assert(renderPlayerTrickplay(right,
                                 PlayerTrickplayRenderState{
                                     .texture = 1,
                                     .frame = frame,
                                     .info = trickplay,
                                     .decodedWidth = 640,
                                     .decodedHeight = 360,
                                     .positionMs = 20'000,
                                     .durationMs = 10'000,
                                     .logicalWidth = 1920.0f,
                                     .positionLabel = "00:20",
                                 },
                                 PlayerTrickplayRenderStyle<TestColor>{},
                                 [](float, float, float, float, float, std::string_view, TestColor) {}));
    assert(right.imageX == 1420.0f);

    FakeRenderer invalid;
    assert(!renderPlayerTrickplay(invalid,
                                  PlayerTrickplayRenderState{
                                      .texture = 1,
                                      .frame = frame,
                                      .info = trickplay,
                                      .decodedWidth = 0,
                                      .decodedHeight = 0,
                                      .positionMs = 5000,
                                      .durationMs = 10'000,
                                      .logicalWidth = 1920.0f,
                                      .positionLabel = "00:05",
                                  },
                                  PlayerTrickplayRenderStyle<TestColor>{},
                                  [](float, float, float, float, float, std::string_view, TestColor) {}));
    assert(invalid.roundedRects == 0);
    assert(invalid.images == 0);

    return 0;
}
