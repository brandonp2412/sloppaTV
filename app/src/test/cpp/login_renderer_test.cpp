#include "login_renderer.hpp"

#include <array>
#include <cassert>
#include <string>
#include <string_view>
#include <vector>

namespace {
struct TextCall {
    std::string text;
    int color = 0;
};

struct RectCall {
    float y = 0.0f;
    float height = 0.0f;
    int color = 0;
};

struct FakeRenderer {
    void rect(float, float, float, float, int color) {
        ++rectCalls;
        backgroundColor = color;
    }

    void verticalGradient(float, float, float, float, int top, int bottom) {
        ++gradientCalls;
        gradientTop = top;
        gradientBottom = bottom;
    }

    void roundedRect(float, float y, float, float height, float, int color) {
        roundedRects.push_back({y, height, color});
    }

    void roundedOutline(float, float y, float, float height, float, float, int color) {
        outlines.push_back({y, height, color});
    }

    void textVerticallyCentered(float, float, float, float, std::string_view value, int color, float) {
        verticalText.push_back({std::string(value), color});
    }

    float textWidth(float, std::string_view value) const { return static_cast<float>(value.size()) * 10.0f; }

    void text(float, float, float, std::string_view value, int color, float) {
        textCalls.push_back({std::string(value), color});
    }

    int rectCalls = 0;
    int backgroundColor = 0;
    int gradientCalls = 0;
    int gradientTop = 0;
    int gradientBottom = 0;
    std::vector<RectCall> roundedRects;
    std::vector<RectCall> outlines;
    std::vector<TextCall> verticalText;
    std::vector<TextCall> textCalls;
};

struct SurfaceCall {
    bool focused = false;
    bool primary = false;
    float height = 0.0f;
};

struct RenderHarness {
    FakeRenderer renderer;
    std::vector<TextCall> centered;
    std::vector<TextCall> leftAligned;
    std::vector<float> modalHeights;
    std::vector<SurfaceCall> inputs;
    std::vector<SurfaceCall> buttons;
    std::vector<SurfaceCall> focusedSurfaces;
    std::vector<float> keyboardTops;

    void render(const LoginRenderState& state, const LoginRenderStyle<int>& style) {
        renderLoginScreen(
            renderer, 1920.0f, 1080.0f, state, style,
            [&](float, float, float, float, float, std::string_view value, int color, float, float) {
                centered.push_back({std::string(value), color});
            },
            [&](float, float, float, float height) { modalHeights.push_back(height); },
            [&](float, float, float, float, float, std::string_view value, int color) {
                leftAligned.push_back({std::string(value), color});
            },
            [&](float x, float y, float width, float height, bool focused, float) {
                inputs.push_back({focused, false, height});
                return std::array<float, 4>{x, y, width, height};
            },
            [&](float x, float y, float width, float height, bool focused, bool primary) {
                buttons.push_back({focused, primary, height});
                return std::array<float, 4>{x, y, width, height};
            },
            [&](float x, float y, float width, float height, bool focused) {
                focusedSurfaces.push_back({focused, false, height});
                return std::array<float, 4>{x, y, width, height};
            },
            [](std::string_view value, float, float, int) { return std::string(value); },
            [&](float top) { keyboardTops.push_back(top); });
    }
};

bool containsText(const std::vector<TextCall>& calls, std::string_view text, int color = -1) {
    for (const auto& call : calls) {
        if (call.text == text && (color < 0 || call.color == color)) return true;
    }
    return false;
}
} // namespace

int main() {
    const LoginRenderStyle<int> style{
        .cornerLarge = 28.0f,
        .cornerMedium = 20.0f,
        .displayScale = 4.0f,
        .bodyScale = 2.0f,
        .wideInputFocusScale = 1.04f,
        .background = 1,
        .surfaceContainerHigh = 2,
        .text = 3,
        .muted = 4,
        .secondaryText = 5,
        .panelElevated = 6,
        .outline = 7,
        .panelAlt = 8,
        .focusSoft = 9,
        .tertiary = 10,
        .focus = 11,
    };

    RenderHarness quick;
    quick.render(
        LoginRenderState{
            .quickConnectActive = true,
            .quickConnectCode = "ABC123",
            .loading = true,
            .savedUserCount = 0,
            .keyboardActive = false,
            .loginFocus = AccountScreenState::kServerField,
            .fields = {},
            .discoveryStatus = {},
        },
        style);

    assert(quick.renderer.rectCalls == 1);
    assert(quick.renderer.backgroundColor == style.background);
    assert(quick.renderer.gradientCalls == 1);
    assert(quick.renderer.gradientTop == style.surfaceContainerHigh);
    assert(quick.renderer.gradientBottom == style.background);
    assert(quick.modalHeights.size() == 1);
    assert(quick.modalHeights[0] == 610.0f);
    assert(containsText(quick.centered, "sloppaTV", style.text));
    assert(containsText(quick.centered, "ABC123", style.text));
    assert(containsText(quick.centered, "Starting…", style.text));
    assert(containsText(quick.centered, "Press Back to cancel", style.muted));
    assert(containsText(quick.renderer.verticalText, "Open Jellyfin on another device", style.secondaryText));
    assert(containsText(quick.renderer.verticalText, "Settings > Quick Connect > Enter code", style.secondaryText));
    assert(quick.inputs.empty());
    assert(quick.buttons.empty());
    assert(quick.keyboardTops.empty());

    RenderHarness form;
    form.render(
        LoginRenderState{
            .quickConnectActive = false,
            .quickConnectCode = {},
            .loading = false,
            .savedUserCount = 2,
            .keyboardActive = false,
            .loginFocus = AccountScreenState::kSavedUsersAction,
            .fields = {"", "viewer", "secret"},
            .discoveryStatus = "Found Jellyfin",
        },
        style);

    assert(form.modalHeights.size() == 1);
    assert(form.modalHeights[0] == 680.0f);
    assert(form.leftAligned.size() == 3);
    assert(form.leftAligned[0].text == "Server");
    assert(form.leftAligned[1].text == "Username");
    assert(form.leftAligned[2].text == "Password");
    assert(form.inputs.size() == 3);
    assert(!form.inputs[0].focused);
    assert(!form.inputs[1].focused);
    assert(!form.inputs[2].focused);
    assert(containsText(form.renderer.verticalText, "https://your-jellyfin-server", style.tertiary));
    assert(containsText(form.renderer.verticalText, "viewer", style.text));
    assert(containsText(form.renderer.verticalText, "******", style.text));
    assert(form.buttons.size() == 3);
    assert(!form.buttons[0].focused && form.buttons[0].primary);
    assert(!form.buttons[1].focused && !form.buttons[1].primary);
    assert(!form.buttons[2].focused && !form.buttons[2].primary);
    assert(form.focusedSurfaces.size() == 1);
    assert(form.focusedSurfaces[0].focused);
    assert(containsText(form.centered, "Saved users (2)", style.text));
    assert(containsText(form.renderer.textCalls, "Found Jellyfin", style.focus));
    assert(form.keyboardTops.empty());

    RenderHarness keyboard;
    keyboard.render(
        LoginRenderState{
            .quickConnectActive = false,
            .quickConnectCode = {},
            .loading = false,
            .savedUserCount = 0,
            .keyboardActive = true,
            .loginFocus = AccountScreenState::kUsernameField,
            .fields = {"https://server", "viewer", ""},
            .discoveryStatus = {},
        },
        style);

    assert(keyboard.modalHeights.size() == 1);
    assert(keyboard.modalHeights[0] == 615.0f);
    assert(keyboard.inputs.size() == 3);
    assert(!keyboard.inputs[1].focused);
    assert(keyboard.focusedSurfaces.empty());
    assert(keyboard.renderer.textCalls.empty());
    assert(keyboard.keyboardTops.size() == 1);
    assert(keyboard.keyboardTops[0] == 655.0f);

    return 0;
}
