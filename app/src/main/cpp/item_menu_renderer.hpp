#pragma once

#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <string>
#include <string_view>
#include <vector>

struct ItemMenuRenderState {
    bool deleteConfirmation = false;
    int deleteConfirmationSelection = 1;
    int itemMenuSelection = 0;
    bool seerrRequest = false;
    std::string_view itemName;
    std::string_view itemType;
    std::string_view externalStatus;
    int externalProgressPercent = -1;
    std::string_view externalProgressLabel;
    std::string_view externalProgressEta;
};

template <typename ColorLike>
struct ItemMenuRenderStyle {
    float cornerLarge = 0.0f;
    ColorLike scrim{};
    ColorLike error{};
    ColorLike text{};
    ColorLike muted{};
    ColorLike tertiary{};
    ColorLike focus{};
    ColorLike secondaryText{};
    ColorLike divider{};
};

template <typename RendererLike, typename ColorLike, typename DrawModal, typename DrawButton, typename DrawCentered,
          typename FitText, typename DrawChip, typename DrawFocusedSurface, typename MaterialLabel>
void renderItemMenuScreen(RendererLike& renderer, float logicalWidth, float logicalHeight,
                          const ItemMenuRenderState& state, const std::vector<std::string>& actions,
                          const ItemMenuRenderStyle<ColorLike>& style, DrawModal&& drawModal,
                          DrawButton&& drawButton, DrawCentered&& drawCentered, FitText&& fitText,
                          DrawChip&& drawChip, DrawFocusedSurface&& drawFocusedSurface,
                          MaterialLabel&& materialLabel) {
    renderer.rect(0, 0, logicalWidth, logicalHeight, style.scrim);

    if (state.deleteConfirmation) {
        drawModal(405.0f, 275.0f, 1110.0f, 520.0f);
        renderer.text(470.0f, 335.0f, 3.25f, state.seerrRequest ? "Delete this request?" : "Delete this media?",
                      style.error, 980.0f);
        renderer.text(
            470.0f, 435.0f, 2.0f,
            state.seerrRequest
                ? "Removes the request from Seerr. Already-sent Sonarr/Radarr downloads may continue."
                : "Jellyfin will delete this item and its media files.\nThis cannot be undone.",
            style.text, 980.0f);

        const std::array<std::string_view, 2> confirmationActions{
            state.seerrRequest ? "Delete request" : "Delete permanently",
            "Cancel",
        };
        for (int index = 0; index < 2; ++index) {
            const float x = index == 0 ? 470.0f : 995.0f;
            const bool focused = state.deleteConfirmationSelection == index;
            const auto bounds = drawButton(x, 610.0f, 450.0f, 92.0f, focused, false, index == 0);
            drawCentered(bounds[0], bounds[1], bounds[2], bounds[3], index == 0 ? 1.75f : 2.0f,
                         confirmationActions[static_cast<std::size_t>(index)],
                         focused || index == 1 ? style.text : style.muted, 20.0f, 8.0f);
        }
        drawCentered(470.0f, 724.0f, 980.0f, 52.0f, 1.55f, "Press Back to cancel", style.tertiary, 12.0f, 4.0f);
        return;
    }

    constexpr float panelX = 1110.0f;
    constexpr float panelWidth = 700.0f;
    constexpr float rowStep = 66.0f;
    const float panelHeaderHeight = state.seerrRequest && !state.externalStatus.empty() ? 194.0f : 170.0f;
    const float panelHeight = panelHeaderHeight + static_cast<float>(actions.size()) * rowStep + 46.0f;
    const float panelY = std::max(72.0f, (logicalHeight - panelHeight) * 0.5f);
    drawModal(panelX, panelY, panelWidth, panelHeight);
    renderer.text(panelX + 38.0f, panelY + 24.0f, 2.35f,
                  fitText(state.itemName.empty() ? std::string_view("Item") : state.itemName, 2.35f,
                          panelWidth - 76.0f, 1),
                  style.text, panelWidth - 76.0f);
    renderer.text(panelX + 40.0f, panelY + 92.0f, 1.35f,
                  fitText(state.itemType.empty() ? std::string_view("Media") : state.itemType, 1.35f,
                          panelWidth - 80.0f, 1),
                  style.muted, panelWidth - 80.0f);
    if (state.seerrRequest && !state.externalStatus.empty()) {
        if (state.externalProgressPercent >= 0 && !state.externalProgressLabel.empty()) {
            const float statusY = panelY + 119.0f;
            const std::string percentLabel = std::to_string(state.externalProgressPercent) + "%";
            const float percentWidth = std::ceil(renderer.textWidth(1.14f, percentLabel));
            const float etaWidth =
                state.externalProgressEta.empty()
                    ? 0.0f
                    : std::round(std::clamp(renderer.textWidth(1.05f, materialLabel(state.externalProgressEta)) + 34.0f,
                                            72.0f, 240.0f));
            constexpr float metadataGap = 14.0f;
            constexpr float statusToMetadataGap = 28.0f;
            const float rightEdge = panelX + panelWidth - 40.0f;
            const float etaX = rightEdge - etaWidth;
            const float percentX =
                state.externalProgressEta.empty() ? rightEdge - percentWidth : etaX - metadataGap - percentWidth;
            const float statusWidth = std::max(120.0f, percentX - (panelX + 40.0f) - statusToMetadataGap);
            renderer.text(panelX + 40.0f, statusY, 1.45f,
                          fitText(state.externalProgressLabel, 1.45f, statusWidth, 1), style.focus, statusWidth);
            renderer.textVerticallyCentered(percentX, panelY + 115.0f, 32.0f, 1.14f, percentLabel,
                                            style.secondaryText, percentWidth);
            if (!state.externalProgressEta.empty()) {
                drawChip(etaX, panelY + 115.0f, state.externalProgressEta, false, 1.05f, 32.0f, 240.0f);
            }
        } else {
            renderer.text(panelX + 40.0f, panelY + 122.0f, 1.55f,
                          fitText(state.externalStatus, 1.55f, panelWidth - 80.0f, 1), style.focus,
                          panelWidth - 80.0f);
        }
    }
    const float dividerY = panelY + (state.seerrRequest && !state.externalStatus.empty() ? 162.0f : 138.0f);
    renderer.rect(panelX + 34.0f, dividerY, panelWidth - 68.0f, 1.0f, style.divider);

    const float firstActionY = dividerY + 16.0f;
    for (std::size_t index = 0; index < actions.size(); ++index) {
        const float y = firstActionY + static_cast<float>(index) * rowStep;
        const bool focused = state.itemMenuSelection == static_cast<int>(index);
        const bool destructive = actions[index] == "DELETE MEDIA" || actions[index] == "DELETE REQUEST";
        const auto bounds =
            drawFocusedSurface(panelX + 30.0f, y, panelWidth - 60.0f, 52.0f, focused, focused && !destructive,
                               destructive);
        renderer.textVerticallyCentered(bounds[0] + 24.0f, bounds[1], bounds[3], 1.70f,
                                        materialLabel(actions[index]), destructive && !focused ? style.muted : style.text,
                                        bounds[2] - 48.0f);
    }
    drawCentered(panelX + 40.0f, panelY + panelHeight - 56.0f, panelWidth - 80.0f, 44.0f, 1.30f,
                 "OK selects   |   Back closes", style.tertiary, 10.0f, 3.0f);
}
