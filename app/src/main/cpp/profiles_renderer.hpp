#pragma once

#include <algorithm>
#include <cctype>
#include <cstddef>
#include <string>

template <typename ColorLike> struct ProfilesRenderStyle {
    float cornerMedium = 0.0f;
    ColorLike text{};
    ColorLike muted{};
    ColorLike focus{};
    ColorLike panelElevated{};
    ColorLike panelAlt{};
};

template <typename RendererLike, typename ColorLike, typename RenderHeader, typename DrawLeftAligned,
          typename DrawFocusedSurface, typename DrawCentered, typename GetSession, typename DrawProfileArtwork,
          typename DrawButtonSurface>
void renderProfilesScreen(RendererLike& renderer, int savedCount, int profileSelection, int profileAction,
                          const ProfilesRenderStyle<ColorLike>& style, RenderHeader&& renderHeader,
                          DrawLeftAligned&& drawLeftAligned, DrawFocusedSurface&& drawFocusedSurface,
                          DrawCentered&& drawCentered, GetSession&& getSession, DrawProfileArtwork&& drawProfileArtwork,
                          DrawButtonSurface&& drawButtonSurface) {
    renderHeader("Users & servers");
    drawLeftAligned(105.0f, 176.0f, 640.0f, 50.0f, 2.15f, "Choose who is watching", style.muted);

    const int totalRows = savedCount + 1;
    constexpr int visibleRows = 5;
    const int maxFirst = std::max(0, totalRows - visibleRows);
    const int first = std::clamp(profileSelection - visibleRows + 1, 0, maxFirst);
    for (int slot = 0; slot < visibleRows; ++slot) {
        const int index = first + slot;
        if (index >= totalRows) break;
        const float y = 245.0f + static_cast<float>(slot) * 138.0f;
        const bool focused = index == profileSelection;
        if (index == savedCount) {
            const auto bounds = drawFocusedSurface(250.0f, y, 1420.0f, 108.0f, focused, false);
            drawCentered(bounds[0] + 30.0f, bounds[1], 90.0f, bounds[3], 3.0f, "+", style.focus, 8.0f, 8.0f);
            renderer.textVerticallyCentered(bounds[0] + 120.0f, bounds[1], bounds[3], 2.35f, "Add another account",
                                            style.text, bounds[2] - 160.0f);
            continue;
        }

        renderer.roundedRect(250.0f, y, 1420.0f, 108.0f, style.cornerMedium,
                             focused ? style.panelElevated : style.panelAlt);
        const auto* savedSession = getSession(index);
        if (!savedSession) continue;
        const auto& saved = *savedSession;
        if (!drawProfileArtwork(saved, 280.0f, y + 12.0f, 84.0f)) {
            renderer.roundedRect(280.0f, y + 12.0f, 84.0f, 84.0f, style.cornerMedium, style.panelAlt);
            std::string initial =
                saved.username.empty()
                    ? "?"
                    : std::string(1,
                                  static_cast<char>(std::toupper(static_cast<unsigned char>(saved.username.front()))));
            drawCentered(280.0f, y + 12.0f, 84.0f, 84.0f, 3.0f, initial, style.text, 8.0f, 8.0f);
        }

        const std::string profileName = saved.username.empty() ? "User" : saved.username;
        drawLeftAligned(395.0f, y + 6.0f, 650.0f, 54.0f, 2.35f, profileName, style.text);
        drawLeftAligned(395.0f, y + 64.0f, 650.0f, 34.0f, 1.35f, saved.server, style.muted);

        const bool useFocused = focused && profileAction == 0;
        const bool forgetFocused = focused && profileAction == 1;
        const auto useBounds = drawButtonSurface(1110.0f, y + 18.0f, 210.0f, 72.0f, useFocused, true, false);
        drawCentered(useBounds[0], useBounds[1], useBounds[2], useBounds[3], 1.85f, "Use", style.text, 16.0f, 6.0f);
        const auto forgetBounds = drawButtonSurface(1340.0f, y + 18.0f, 270.0f, 72.0f, forgetFocused, false, true);
        drawCentered(forgetBounds[0], forgetBounds[1], forgetBounds[2], forgetBounds[3], 1.75f, "Forget",
                     forgetFocused ? style.text : style.muted, 16.0f, 6.0f);
    }

    drawCentered(270.0f, 950.0f, 1380.0f, 58.0f, 1.72f, "Up / Down chooses account   |   Left / Right chooses action",
                 style.muted, 18.0f, 5.0f);
}
