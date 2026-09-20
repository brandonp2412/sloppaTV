#include "cast_renderer.hpp"

#include <array>
#include <cassert>
#include <cmath>
#include <string>
#include <string_view>
#include <vector>

namespace {
struct RectCall {
    float x = 0.0f;
    float y = 0.0f;
    float width = 0.0f;
    float height = 0.0f;
    float radius = 0.0f;
    int color = 0;
};

struct TextCall {
    std::string text;
    float x = 0.0f;
    float y = 0.0f;
    float scale = 0.0f;
    int color = 0;
};

struct ArtworkCall {
    std::string personId;
    float x = 0.0f;
    float y = 0.0f;
    float radius = 0.0f;
};

struct FakeRenderer {
    void roundedRect(float x, float y, float width, float height, float radius, int color) {
        rects.push_back({x, y, width, height, radius, color});
    }

    void text(float x, float y, float scale, std::string_view value, int color, float) {
        textCalls.push_back({std::string(value), x, y, scale, color});
    }

    std::vector<RectCall> rects;
    std::vector<TextCall> textCalls;
};
} // namespace

int main() {
    const CastRenderStyle<int> style{
        .cornerSmall = 16.0f,
        .labelScale = 1.8f,
        .supportingScale = 1.55f,
        .panelAlt = 1,
        .focus = 2,
        .text = 3,
        .muted = 4,
        .tertiary = 5,
    };

    std::vector<JellyfinPerson> people;
    for (int index = 0; index < 12; ++index) {
        people.push_back({
            .id = "person-" + std::to_string(index),
            .name = "Person " + std::to_string(index),
            .imageTag = "tag-" + std::to_string(index),
            .role = index % 2 == 0 ? "Role " + std::to_string(index) : "",
        });
    }

    FakeRenderer renderer;
    std::vector<std::string> headers;
    std::vector<ArtworkCall> artwork;
    std::vector<RectCall> halos;
    std::vector<TextCall> centered;
    int emptyCalls = 0;

    renderCastScreen(
        renderer, "Example", people, 10, 2, style, [&](std::string_view heading) { headers.emplace_back(heading); },
        [&](std::string_view, std::string_view) { ++emptyCalls; },
        [](float x, float y, float width, float height, bool focused) {
            if (!focused) return std::array<float, 4>{x, y, width, height};
            return std::array<float, 4>{x - 4.75f, y - 7.125f, width + 9.5f, height + 14.25f};
        },
        [&](const JellyfinPerson& person, float x, float y, float, float, float radius) {
            artwork.push_back({person.id, x, y, radius});
        },
        [&](float x, float y, float width, float height, int color, float radius) {
            halos.push_back({x, y, width, height, radius, color});
        },
        [](std::string_view value, float, float, int) { return std::string(value); },
        [&](float x, float y, float, float, float scale, std::string_view value, int color, float, float) {
            centered.push_back({std::string(value), x, y, scale, color});
        });

    assert(emptyCalls == 0);
    assert(headers.size() == 1);
    assert(headers[0] == "Example | Cast");

    assert(renderer.rects.size() == 7);
    assert(artwork.size() == 7);
    assert(artwork.front().personId == "person-5");
    assert(artwork.back().personId == "person-11");
    assert(renderer.rects.front().x == 145.0f);
    assert(renderer.rects.front().y == 195.0f);
    assert(renderer.rects[5].y == 617.875f);
    assert(renderer.rects[5].width == 199.5f);
    assert(renderer.rects[5].height == 299.25f);
    assert(artwork[5].personId == "person-10");

    assert(halos.size() == 1);
    assert(halos[0].x == renderer.rects[5].x);
    assert(halos[0].y == renderer.rects[5].y);
    assert(halos[0].color == style.focus);

    assert(renderer.textCalls.size() == 10);
    assert(renderer.textCalls.front().text == "Person 5");
    assert(renderer.textCalls.front().x == 145.0f);
    assert(renderer.textCalls.front().y == 504.0f);
    bool sawRole10 = false;
    for (const auto& call : renderer.textCalls) {
        if (call.text == "Role 10") {
            sawRole10 = true;
            assert(std::abs(call.y - 985.52f) < 0.001f);
            assert(call.color == style.muted);
        }
    }
    assert(sawRole10);

    assert(centered.size() == 1);
    assert(centered[0].text == "Press OK to explore titles featuring this person");
    assert(centered[0].x == 500.0f);
    assert(centered[0].y == 1032.0f);
    assert(centered[0].color == style.tertiary);

    FakeRenderer emptyRenderer;
    int renderedEmpty = 0;
    renderCastScreen(
        emptyRenderer, "", {}, 0, 1, style, [&](std::string_view heading) { assert(heading == "Cast"); },
        [&](std::string_view title, std::string_view message) {
            ++renderedEmpty;
            assert(title == "No cast information");
            assert(message == "Jellyfin has no cast information for this title.");
        },
        [](float x, float y, float width, float height, bool) { return std::array<float, 4>{x, y, width, height}; },
        [](const JellyfinPerson&, float, float, float, float, float) {}, [](float, float, float, float, int, float) {},
        [](std::string_view value, float, float, int) { return std::string(value); },
        [](float, float, float, float, float, std::string_view, int, float, float) {});

    assert(renderedEmpty == 1);
    assert(emptyRenderer.rects.empty());
    assert(emptyRenderer.textCalls.empty());

    return 0;
}
