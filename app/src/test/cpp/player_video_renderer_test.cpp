#include "ambient_video_color.hpp"
#include "player_video_renderer.hpp"

#include <array>
#include <cassert>
#include <chrono>
#include <cmath>
#include <cstdint>

namespace {
struct FakeRenderer {
    void externalImage(uint32_t texture, float x, float y, float width, float height,
                       const std::array<float, 16>& transform) {
        ++calls;
        lastTexture = texture;
        lastX = x;
        lastY = y;
        lastWidth = width;
        lastHeight = height;
        lastTransform = transform;
    }

    void rect(float x, float y, float width, float height, int) {
        assert(rectCalls < static_cast<int>(rects.size()));
        rects[static_cast<size_t>(rectCalls++)] = {x, y, width, height};
    }

    int calls = 0;
    int rectCalls = 0;
    std::array<std::array<float, 4>, 4> rects{};
    uint32_t lastTexture = 0;
    float lastX = 0.0f;
    float lastY = 0.0f;
    float lastWidth = 0.0f;
    float lastHeight = 0.0f;
    std::array<float, 16> lastTransform{};
};

bool near(float left, float right) {
    return std::abs(left - right) < 0.001f;
}
} // namespace

int main() {
    const PlayerVideoRenderState fit{
        .texture = 7,
        .sourceWidth = 1440,
        .sourceHeight = 1080,
        .zoomMode = VideoZoomMode::Fit,
        .logicalWidth = 1920.0f,
        .logicalHeight = 1080.0f,
    };
    const PlayerVideoBounds fitBounds = playerVideoBounds(fit);
    assert(near(fitBounds.x, 240.0f));
    assert(near(fitBounds.y, 0.0f));
    assert(near(fitBounds.width, 1440.0f));
    assert(near(fitBounds.height, 1080.0f));

    PlayerVideoRenderState fill = fit;
    fill.zoomMode = VideoZoomMode::Fill;
    const PlayerVideoBounds fillBounds = playerVideoBounds(fill);
    assert(near(fillBounds.x, 0.0f));
    assert(near(fillBounds.y, -180.0f));
    assert(near(fillBounds.width, 1920.0f));
    assert(near(fillBounds.height, 1440.0f));

    PlayerVideoRenderState stretch = fit;
    stretch.zoomMode = VideoZoomMode::Stretch;
    const PlayerVideoBounds stretchBounds = playerVideoBounds(stretch);
    assert(near(stretchBounds.x, 0.0f));
    assert(near(stretchBounds.y, 0.0f));
    assert(near(stretchBounds.width, 1920.0f));
    assert(near(stretchBounds.height, 1080.0f));

    PlayerVideoRenderState invalid = fit;
    invalid.sourceWidth = 0;
    const PlayerVideoBounds invalidBounds = playerVideoBounds(invalid);
    assert(near(invalidBounds.width, 1920.0f));
    assert(near(invalidBounds.height, 1080.0f));

    std::array<float, 16> transform{};
    transform[0] = 1.0f;
    transform[5] = 1.0f;
    transform[10] = 1.0f;
    transform[15] = 1.0f;
    FakeRenderer renderer;
    renderPlayerVideo(renderer, fit, transform);
    assert(renderer.calls == 1);
    assert(renderer.lastTexture == 7);
    assert(near(renderer.lastX, 240.0f));
    assert(near(renderer.lastY, 0.0f));
    assert(near(renderer.lastWidth, 1440.0f));
    assert(near(renderer.lastHeight, 1080.0f));
    assert(renderer.lastTransform == transform);

    assert(playerVideoHasBars(fit));
    renderPlayerAmbientBars(renderer, fit, 1);
    assert(renderer.rectCalls == 2);
    assert(near(renderer.rects[0][0], 0.0f));
    assert(near(renderer.rects[0][2], 240.0f));
    assert(near(renderer.rects[1][0], 1680.0f));
    assert(near(renderer.rects[1][2], 240.0f));
    assert(!playerVideoHasBars(fill));
    assert(!playerVideoHasBars(stretch));

    PlayerVideoRenderState wide = fit;
    wide.sourceWidth = 1920;
    wide.sourceHeight = 800;
    const PlayerVideoBounds wideBounds = playerVideoBounds(wide);
    assert(near(wideBounds.y, 140.0f));
    FakeRenderer wideRenderer;
    renderPlayerAmbientBars(wideRenderer, wide, 1);
    assert(wideRenderer.rectCalls == 2);
    assert(near(wideRenderer.rects[0][1], 0.0f));
    assert(near(wideRenderer.rects[0][3], 140.0f));
    assert(near(wideRenderer.rects[1][1], 940.0f));
    assert(near(wideRenderer.rects[1][3], 140.0f));

    using namespace std::chrono_literals;
    AmbientVideoColorState ambient;
    const auto sampleTime = AmbientVideoColorState::Clock::now();
    assert(ambient.sampleDue(sampleTime));
    ambient.submitSample(AmbientVideoColor{.r = 1.0f, .g = 0.5f, .b = 0.25f}, sampleTime);
    assert(!ambient.sampleDue(sampleTime + 1500ms));
    assert(ambient.sampleDue(sampleTime + 2s));
    const AmbientVideoColor initialAmbient = ambient.color(sampleTime);
    assert(near(initialAmbient.r, 0.0f));
    const AmbientVideoColor transitionedAmbient = ambient.color(sampleTime + 1s);
    assert(transitionedAmbient.r > 0.0f);
    assert(transitionedAmbient.r < 1.0f);
    const AmbientVideoColor cappedWhite = ambientBarTarget(AmbientVideoColor{.r = 1.0f, .g = 1.0f, .b = 1.0f});
    assert(cappedWhite.r <= 0.261f);
    ambient.reset();
    assert(ambient.sampleDue(sampleTime + 2s));
    assert(near(ambient.color(sampleTime + 2s).r, 0.0f));

    return 0;
}
