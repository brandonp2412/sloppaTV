#pragma once

#include <string_view>

struct StatusOverlayRenderState {
    bool loading = false;
    bool playerScreen = false;
    bool noticeVisible = false;
    std::string_view notice;
    bool errorVisible = false;
    std::string_view error;
};

template <typename ColorLike>
struct StatusOverlayRenderStyle {
    float cornerMedium = 0.0f;
    ColorLike panelElevated{};
    ColorLike focus{};
    ColorLike error{};
    ColorLike errorOutline{};
    ColorLike text{};
};

template <typename RendererLike, typename ColorLike, typename FitText>
void renderStatusOverlay(RendererLike& renderer, const StatusOverlayRenderState& state,
                         const StatusOverlayRenderStyle<ColorLike>& style, FitText&& fitText) {
    if (state.loading) {
        constexpr float loadingX = 1600.0f;
        constexpr float loadingY = 120.0f;
        constexpr float loadingWidth = 210.0f;
        constexpr float loadingHeight = 48.0f;
        renderer.roundedRect(loadingX, loadingY, loadingWidth, loadingHeight, loadingHeight * 0.5f,
                             style.panelElevated);
        renderer.roundedRect(loadingX + 18.0f, loadingY + 16.0f, 16.0f, 16.0f, 8.0f, style.focus);
        renderer.textVerticallyCentered(loadingX + 52.0f, loadingY, loadingHeight, 1.60f, "Loading…", style.text,
                                       loadingWidth - 70.0f);
    }

    if (state.noticeVisible) {
        const float noticeY = state.playerScreen ? 670.0f : 914.0f;
        const float noticeWidth = state.playerScreen ? 1160.0f : 1320.0f;
        const float noticeX = state.playerScreen ? 80.0f : (1920.0f - noticeWidth) * 0.5f;
        renderer.roundedRect(noticeX, noticeY, noticeWidth, 68.0f, style.cornerMedium, style.panelElevated);
        renderer.roundedRect(noticeX + 18.0f, noticeY + 18.0f, 7.0f, 32.0f, 3.5f, style.focus);
        renderer.textVerticallyCentered(noticeX + 46.0f, noticeY, 68.0f, 1.65f,
                                       fitText(state.notice, 1.65f, noticeWidth - 74.0f, 1), style.text,
                                       noticeWidth - 74.0f);
    }

    if (state.errorVisible) {
        const float errorY =
            state.playerScreen ? (state.noticeVisible ? 588.0f : 670.0f) : (state.noticeVisible ? 834.0f : 914.0f);
        const float errorWidth = state.playerScreen ? 1160.0f : 1320.0f;
        const float errorX = state.playerScreen ? 80.0f : (1920.0f - errorWidth) * 0.5f;
        renderer.roundedRect(errorX, errorY, errorWidth, 68.0f, style.cornerMedium, style.panelElevated);
        renderer.roundedRect(errorX + 18.0f, errorY + 18.0f, 7.0f, 32.0f, 3.5f, style.error);
        renderer.roundedOutline(errorX, errorY, errorWidth, 68.0f, style.cornerMedium, 1.5f, style.errorOutline);
        renderer.textVerticallyCentered(errorX + 46.0f, errorY, 68.0f, 1.55f,
                                       fitText(state.error, 1.55f, errorWidth - 74.0f, 1), style.text,
                                       errorWidth - 74.0f);
    }
}
