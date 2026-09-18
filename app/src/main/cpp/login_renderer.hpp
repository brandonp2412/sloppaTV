#pragma once

#include "account_screen.hpp"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <string>
#include <string_view>

struct LoginRenderState {
    bool quickConnectActive = false;
    std::string_view quickConnectCode;
    bool loading = false;
    int savedUserCount = 0;
    bool keyboardActive = false;
    int loginFocus = AccountScreenState::kServerField;
    std::array<std::string_view, 3> fields{};
    std::string_view discoveryStatus;
};

template <typename ColorLike>
struct LoginRenderStyle {
    float cornerLarge = 0.0f;
    float cornerMedium = 0.0f;
    float displayScale = 0.0f;
    float bodyScale = 0.0f;
    float wideInputFocusScale = 1.0f;
    ColorLike background{};
    ColorLike surfaceContainerHigh{};
    ColorLike text{};
    ColorLike muted{};
    ColorLike secondaryText{};
    ColorLike panelElevated{};
    ColorLike outline{};
    ColorLike panelAlt{};
    ColorLike focusSoft{};
    ColorLike tertiary{};
    ColorLike focus{};
};

template <typename RendererLike, typename ColorLike, typename DrawCentered, typename DrawModal, typename DrawLeftAligned,
          typename DrawInputSurface, typename DrawButtonSurface, typename DrawFocusedSurface, typename FitText,
          typename RenderKeyboard>
void renderLoginScreen(RendererLike& renderer, float logicalWidth, float logicalHeight, const LoginRenderState& state,
                       const LoginRenderStyle<ColorLike>& style, DrawCentered&& drawCentered, DrawModal&& drawModal,
                       DrawLeftAligned&& drawLeftAligned, DrawInputSurface&& drawInputSurface,
                       DrawButtonSurface&& drawButtonSurface, DrawFocusedSurface&& drawFocusedSurface,
                       FitText&& fitText, RenderKeyboard&& renderKeyboard) {
    renderer.rect(0, 0, logicalWidth, logicalHeight, style.background);
    renderer.verticalGradient(0.0f, 0.0f, 1920.0f, 1080.0f, style.surfaceContainerHigh, style.background);
    drawCentered(410.0f, 15.0f, 1100.0f, 145.0f, style.displayScale, "sloppaTV", style.text, 20.0f, 8.0f);
    drawCentered(410.0f, 165.0f, 1100.0f, 50.0f, style.bodyScale, "Connect to your Jellyfin server", style.muted,
                 20.0f, 4.0f);

    if (state.quickConnectActive) {
        drawModal(465.0f, 230.0f, 990.0f, 610.0f);
        drawCentered(610.0f, 266.0f, 700.0f, 62.0f, 2.5f, "Quick Connect", style.secondaryText, 12.0f, 4.0f);

        renderer.roundedRect(610.0f, 345.0f, 700.0f, 150.0f, style.cornerLarge, style.panelElevated);
        renderer.roundedOutline(610.0f, 345.0f, 700.0f, 150.0f, style.cornerLarge, 1.0f, style.outline);
        drawCentered(610.0f, 345.0f, 700.0f, 150.0f, 7.6f, state.quickConnectCode, style.text, 24.0f, 10.0f);

        const std::array<std::string_view, 2> steps{
            "Open Jellyfin on another device",
            "Settings > Quick Connect > Enter code",
        };
        for (std::size_t index = 0; index < steps.size(); ++index) {
            const float rowY = 535.0f + static_cast<float>(index) * 74.0f;
            renderer.roundedRect(510.0f, rowY, 900.0f, 58.0f, style.cornerMedium, style.panelAlt);
            renderer.roundedRect(528.0f, rowY + 9.0f, 40.0f, 40.0f, 20.0f, style.focusSoft);
            drawCentered(528.0f, rowY + 9.0f, 40.0f, 40.0f, 1.45f, std::to_string(index + 1), style.text, 4.0f,
                         3.0f);
            renderer.textVerticallyCentered(595.0f, rowY, 58.0f, 1.45f, fitText(steps[index], 1.45f, 790.0f, 1),
                                            style.secondaryText, 790.0f);
        }

        const std::string status = state.loading ? "Starting…" : "Waiting for authorization…";
        const float statusWidth = std::round(std::clamp(renderer.textWidth(1.55f, status) + 54.0f, 260.0f, 560.0f));
        const float statusX = std::round(960.0f - statusWidth * 0.5f);
        renderer.roundedRect(statusX, 706.0f, statusWidth, 48.0f, 24.0f, style.focusSoft);
        drawCentered(statusX, 706.0f, statusWidth, 48.0f, 1.55f, status, style.text, 16.0f, 4.0f);
        drawCentered(610.0f, 776.0f, 700.0f, 48.0f, 1.55f, "Press Back to cancel", style.muted, 12.0f, 4.0f);
        return;
    }

    const bool hasSavedUsers = state.savedUserCount > 0;
    drawModal(410.0f, 225.0f, 1100.0f, hasSavedUsers ? 680.0f : 615.0f);
    static constexpr std::array<std::string_view, 3> labels{"Server", "Username", "Password"};
    for (int index = 0; index < 3; ++index) {
        const float y = 290.0f + static_cast<float>(index) * 128.0f;
        drawLeftAligned(495.0f, y - 45.0f, 420.0f, 32.0f, 1.35f, labels[static_cast<std::size_t>(index)],
                        style.muted);
        const bool focused = !state.keyboardActive && state.loginFocus == index;
        const auto bounds = drawInputSurface(490.0f, y, 940.0f, 70.0f, focused, style.wideInputFocusScale);
        std::string value(state.fields[static_cast<std::size_t>(index)]);
        if (index == AccountScreenState::kPasswordField && !value.empty()) value.assign(value.size(), '*');
        const bool placeholder = value.empty() && index == AccountScreenState::kServerField;
        if (placeholder) value = "https://your-jellyfin-server";
        renderer.textVerticallyCentered(520.0f, bounds[1], bounds[3], 2.35f, fitText(value, 2.35f, 880.0f, 1),
                                        placeholder ? style.tertiary : style.text, 880.0f);
    }

    const bool loginFocused = state.loginFocus == AccountScreenState::kLoginAction && !state.keyboardActive;
    const auto loginBounds = drawButtonSurface(490.0f, 690.0f, 330.0f, 72.0f, loginFocused, true);
    drawCentered(loginBounds[0], loginBounds[1], loginBounds[2], loginBounds[3], 2.15f, "Log in", style.text, 18.0f,
                 6.0f);
    const bool quickFocused = state.loginFocus == AccountScreenState::kQuickConnectAction && !state.keyboardActive;
    const auto quickBounds = drawButtonSurface(840.0f, 690.0f, 310.0f, 72.0f, quickFocused, false);
    drawCentered(quickBounds[0], quickBounds[1], quickBounds[2], quickBounds[3], 1.65f, "Quick Connect", style.text,
                 18.0f, 6.0f);
    const bool discoverFocused = state.loginFocus == AccountScreenState::kDiscoverAction && !state.keyboardActive;
    const auto discoverBounds = drawButtonSurface(1170.0f, 690.0f, 260.0f, 72.0f, discoverFocused, false);
    drawCentered(discoverBounds[0], discoverBounds[1], discoverBounds[2], discoverBounds[3], 1.8f, "Discover",
                 style.text, 18.0f, 6.0f);

    if (hasSavedUsers) {
        const bool savedFocused = state.loginFocus == AccountScreenState::kSavedUsersAction && !state.keyboardActive;
        const auto savedBounds = drawFocusedSurface(650.0f, 785.0f, 620.0f, 58.0f, savedFocused);
        drawCentered(savedBounds[0], savedBounds[1], savedBounds[2], savedBounds[3], 1.65f,
                     "Saved users (" + std::to_string(state.savedUserCount) + ")",
                     savedFocused ? style.text : style.muted, 16.0f, 5.0f);
    }
    if (!state.keyboardActive) {
        const std::string hint =
            state.discoveryStatus.empty() ? "Discover searches your local network" : std::string(state.discoveryStatus);
        renderer.text(555.0f, 870.0f, 1.65f, fitText(hint, 1.65f, 810.0f, 1),
                      state.discoveryStatus.empty() ? style.tertiary : style.focus, 810.0f);
    }
    if (state.keyboardActive) renderKeyboard(655.0f);
}
