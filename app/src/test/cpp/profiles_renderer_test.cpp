#include "profiles_renderer.hpp"

#include <array>
#include <cassert>
#include <string>
#include <string_view>
#include <vector>

namespace {
struct FakeSession {
    std::string username;
    std::string server;
};

struct RoundedRectCall {
    float y = 0.0f;
    int color = 0;
};

struct TextCall {
    std::string text;
    float x = 0.0f;
    float y = 0.0f;
    int color = 0;
};

struct SurfaceCall {
    float y = 0.0f;
    bool focused = false;
    bool primary = false;
    bool destructive = false;
};

struct FakeRenderer {
    void roundedRect(float, float y, float, float, float, int color) { roundedRects.push_back({y, color}); }

    void textVerticallyCentered(float x, float y, float, float, std::string_view value, int color, float) {
        text.push_back({std::string(value), x, y, color});
    }

    std::vector<RoundedRectCall> roundedRects;
    std::vector<TextCall> text;
};
} // namespace

int main() {
    const ProfilesRenderStyle<int> style{
        .cornerMedium = 24.0f,
        .text = 1,
        .muted = 2,
        .focus = 3,
        .panelElevated = 4,
        .panelAlt = 5,
    };

    std::vector<FakeSession> sessions = {
        {"user0", "https://server0"}, {"user1", "https://server1"}, {"user2", "https://server2"},
        {"", "https://server3"},      {"user4", "https://server4"}, {"user5", "https://server5"},
    };

    FakeRenderer renderer;
    std::vector<std::string> headers;
    std::vector<TextCall> leftText;
    std::vector<TextCall> centeredText;
    std::vector<SurfaceCall> focusedSurfaces;
    std::vector<SurfaceCall> buttons;
    std::vector<std::string> artworkUsers;

    renderProfilesScreen(
        renderer, static_cast<int>(sessions.size()), 5, 1, style,
        [&](std::string_view title) { headers.emplace_back(title); },
        [&](float x, float y, float, float, float, std::string_view value, int color) {
            leftText.push_back({std::string(value), x, y, color});
        },
        [&](float, float y, float, float, bool focused, bool primary) {
            focusedSurfaces.push_back({y, focused, primary, false});
            return std::array<float, 4>{250.0f, y, 1420.0f, 108.0f};
        },
        [&](float x, float y, float, float, float, std::string_view value, int color, float, float) {
            centeredText.push_back({std::string(value), x, y, color});
        },
        [&](int index) -> const FakeSession* {
            if (index < 0 || index >= static_cast<int>(sessions.size())) return nullptr;
            return &sessions[static_cast<std::size_t>(index)];
        },
        [&](const FakeSession& session, float, float, float) {
            artworkUsers.push_back(session.username);
            return !session.username.empty();
        },
        [&](float, float y, float width, float height, bool focused, bool primary, bool destructive) {
            buttons.push_back({y, focused, primary, destructive});
            return std::array<float, 4>{primary ? 1110.0f : 1340.0f, y, width, height};
        });

    assert(headers.size() == 1);
    assert(headers[0] == "Users & servers");
    assert(leftText.front().text == "Choose who is watching");

    assert(renderer.roundedRects.size() == 6);
    assert(renderer.roundedRects[0].y == 245.0f);
    assert(renderer.roundedRects[0].color == style.panelAlt);
    assert(renderer.roundedRects[2].y == 521.0f);
    assert(renderer.roundedRects[3].y == 533.0f);
    assert(renderer.roundedRects[3].color == style.panelAlt);
    assert(renderer.roundedRects[4].y == 659.0f);
    assert(renderer.roundedRects[4].color == style.panelAlt);
    assert(renderer.roundedRects[5].y == 797.0f);
    assert(renderer.roundedRects[5].color == style.panelElevated);

    assert(artworkUsers.size() == 5);
    assert(artworkUsers[0] == "user1");
    assert(artworkUsers[2].empty());
    assert(artworkUsers[4] == "user5");

    bool sawFallbackInitial = false;
    bool sawFallbackName = false;
    for (const auto& call : centeredText) {
        if (call.text == "?") sawFallbackInitial = true;
    }
    for (const auto& call : leftText) {
        if (call.text == "User") sawFallbackName = true;
    }
    assert(sawFallbackInitial);
    assert(sawFallbackName);

    assert(buttons.size() == 10);
    assert(!buttons[8].focused);
    assert(buttons[8].primary);
    assert(buttons[9].focused);
    assert(buttons[9].destructive);

    assert(centeredText.back().text == "Up / Down chooses account   |   Left / Right chooses action");
    assert(centeredText.back().y == 950.0f);
    assert(centeredText.back().color == style.muted);
    assert(focusedSurfaces.empty());

    FakeRenderer addRenderer;
    std::vector<std::string> addCentered;
    int addSurfaceCalls = 0;
    renderProfilesScreen(
        addRenderer, static_cast<int>(sessions.size()), 6, 0, style, [](std::string_view) {},
        [](float, float, float, float, float, std::string_view, int) {},
        [&](float, float y, float, float, bool focused, bool primary) {
            ++addSurfaceCalls;
            assert(y == 797.0f);
            assert(focused);
            assert(!primary);
            return std::array<float, 4>{250.0f, y, 1420.0f, 108.0f};
        },
        [&](float, float, float, float, float, std::string_view value, int, float, float) {
            addCentered.emplace_back(value);
        },
        [&](int index) -> const FakeSession* { return &sessions[static_cast<std::size_t>(index)]; },
        [](const FakeSession&, float, float, float) { return true; },
        [](float, float y, float width, float height, bool, bool primary, bool) {
            return std::array<float, 4>{primary ? 1110.0f : 1340.0f, y, width, height};
        });

    assert(addSurfaceCalls == 1);
    assert(addRenderer.text.size() == 1);
    assert(addRenderer.text[0].text == "Add another account");
    assert(addRenderer.text[0].y == 797.0f);
    assert(addCentered.size() == 10);
    assert(addCentered[8] == "+");
    assert(addCentered[9] == "Up / Down chooses account   |   Left / Right chooses action");

    return 0;
}
