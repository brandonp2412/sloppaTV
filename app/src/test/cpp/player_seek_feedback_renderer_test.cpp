#include "player_seek_feedback_renderer.hpp"

#include <cassert>
#include <cmath>
#include <vector>

namespace {
struct TestColor {
    float alpha = 0.0f;
};

struct FakeRenderer {
    void roundedRect(float x, float, float, float, float, TestColor color) {
        ovalX = x;
        washAlpha = color.alpha;
        ++roundedRects;
    }

    void triangle(float, float, float, float, float, float, TestColor color) {
        glyphAlphas.push_back(color.alpha);
        ++triangles;
    }

    float ovalX = 0.0f;
    float washAlpha = 0.0f;
    int roundedRects = 0;
    int triangles = 0;
    std::vector<float> glyphAlphas;
};

TestColor color(float, float, float, float alpha) {
    return {.alpha = alpha};
}
} // namespace

int main() {
    FakeRenderer forward;
    renderPlayerSeekFeedback(forward, PlayerSeekFeedbackRenderState{.seconds = 30, .fade = 0.5f}, color);
    assert(forward.roundedRects == 1);
    assert(forward.triangles == 2);
    assert(forward.ovalX == 1675.0f);
    assert(std::abs(forward.washAlpha - 0.045f) < 0.0001f);
    assert(forward.glyphAlphas.size() == 2);
    assert(std::abs(forward.glyphAlphas[0] - 0.36f) < 0.0001f);

    FakeRenderer backward;
    renderPlayerSeekFeedback(backward, PlayerSeekFeedbackRenderState{.seconds = -10, .fade = 1.0f}, color);
    assert(backward.roundedRects == 1);
    assert(backward.triangles == 2);
    assert(backward.ovalX == -175.0f);
    assert(std::abs(backward.washAlpha - 0.09f) < 0.0001f);
    assert(std::abs(backward.glyphAlphas[1] - 0.72f) < 0.0001f);

    return 0;
}
