#pragma once

#include "settings_screen.hpp"
#include "ui_policy.hpp"

#include <algorithm>
#include <cmath>
#include <string>
#include <string_view>

template <typename ColorLike> struct SettingsRenderStyle {
    float headlineScale = 0.0f;
    float cornerMedium = 0.0f;
    float cornerSmall = 0.0f;
    float wideInputFocusScale = 1.0f;
    float wideListItemFocusScale = 1.0f;
    float listItemFocusScale = 1.0f;
    ColorLike text{};
    ColorLike secondaryText{};
    ColorLike muted{};
    ColorLike focus{};
    ColorLike focusSoft{};
    ColorLike panelAlt{};
    ColorLike outline{};
    ColorLike scrim{};
};

struct SettingsRenderState {
    const SettingsScreenState& screen;
    const AppSettings& settings;
    int maxAudioOutputChannels = 0;
    std::string_view externalPlayer;
    std::string_view username;
    bool systemSettingsInputActive = false;
    bool seerrApiKeyTyping = false;
};

template <typename RendererLike, typename ColorLike, typename DrawInputSurface, typename DrawCentered, typename FitText,
          typename RenderEmptyState, typename DrawListItemSurface, typename MaterialLabel, typename DrawSwitch,
          typename DrawChip, typename DrawModal>
void renderSettingsScreen(RendererLike& renderer, const SettingsRenderState& state,
                          const SettingsRenderStyle<ColorLike>& style, DrawInputSurface&& drawInputSurface,
                          DrawCentered&& drawCentered, FitText&& fitText, RenderEmptyState&& renderEmptyState,
                          DrawListItemSurface&& drawListItemSurface, MaterialLabel&& materialLabel,
                          DrawSwitch&& drawSwitch, DrawChip&& drawChip, DrawModal&& drawModal) {
    renderer.text(80.0f, 58.0f, style.headlineScale, "Settings", style.text, 560.0f);

    const auto presentation = settingsScreenPresentation(state.screen, state.systemSettingsInputActive);
    const auto settingsSearchBounds =
        drawInputSurface(1070.0f, 52.0f, 760.0f, 58.0f, state.screen.searchFocused(), style.wideInputFocusScale);
    constexpr float settingsSearchTextWidth = 520.0f;
    renderer.textVerticallyCentered(1102.0f, settingsSearchBounds[1], settingsSearchBounds[3], 2.20f,
                                    fitText(presentation.searchText, 2.20f, settingsSearchTextWidth, 1),
                                    presentation.searchPlaceholder ? style.muted : style.text, settingsSearchTextWidth);
    drawCentered(1640.0f, settingsSearchBounds[1], 170.0f, settingsSearchBounds[3], 1.60f,
                 presentation.searchActionLabel, state.screen.searchFocused() ? style.focus : style.muted, 10.0f, 4.0f);

    if (state.screen.matches().empty()) {
        if (presentation.showTypingFilterHint) {
            drawCentered(480.0f, 300.0f, 960.0f, 64.0f, 1.75f, "Type to filter settings", style.muted, 16.0f, 5.0f);
        } else {
            renderEmptyState("No matching settings", "Press OK or Search to change your filter.");
        }
        return;
    }

    renderer.text(120.0f, 165.0f, 2.10f, presentation.sectionTitle, style.secondaryText, 620.0f);
    const float descriptionY = settingsDescriptionY(state.settings.uiTextSize);
    renderer.text(120.0f, descriptionY, 1.40f, fitText(presentation.sectionDescription, 1.40f, 920.0f, 1), style.muted,
                  920.0f);

    constexpr int visibleRows = 6;
    const float rowsTop = settingsRowsTop(state.settings.uiTextSize);
    for (int slot = 0; slot < visibleRows; ++slot) {
        const auto row =
            settingsScreenRow(state.screen, state.settings, state.maxAudioOutputChannels, state.externalPlayer,
                              state.username, state.seerrApiKeyTyping, slot, visibleRows);
        if (!row) break;

        const float y = rowsTop + static_cast<float>(slot) * 112.0f;
        const bool focused = row->focused;
        constexpr float rowX = 110.0f;
        constexpr float rowWidth = 1700.0f;
        const auto rowBounds = drawListItemSurface(rowX, y - 8.0f, rowWidth, 88.0f, focused, style.cornerMedium,
                                                   style.wideListItemFocusScale);
        renderer.textVerticallyCentered(rowX + 35.0f, rowBounds[1], rowBounds[3], 2.20f,
                                        fitText(materialLabel(row->label), 2.20f, 900.0f, 1),
                                        focused ? style.text : style.secondaryText, 900.0f);

        const std::string& value = row->value;
        constexpr float valueRightInset = 45.0f;
        const float valueRight = std::round(rowX + rowWidth - valueRightInset);
        switch (row->kind) {
        case SettingKind::Action: {
            constexpr float valueScale = 1.70f;
            const std::string displayValue = fitText(materialLabel(value), valueScale, 570.0f, 1);
            const float valueWidth = renderer.textWidth(valueScale, displayValue);
            renderer.textVerticallyCentered(std::max(1190.0f, valueRight - valueWidth), rowBounds[1], rowBounds[3],
                                            valueScale, displayValue, focused ? style.focus : style.text, 570.0f);
            break;
        }
        case SettingKind::Boolean: {
            constexpr float switchWidth = 112.0f;
            drawSwitch(valueRight - switchWidth, y + 8.0f, value == "ON", focused);
            break;
        }
        case SettingKind::Value: {
            const std::string displayValue(materialLabel(value));
            const float chipWidth =
                std::round(std::clamp(renderer.textWidth(1.65f, displayValue) + 44.0f, 112.0f, 570.0f));
            const float chipX = std::round(valueRight - chipWidth);
            renderer.roundedRect(chipX, y + 8.0f, chipWidth, 56.0f, 28.0f, focused ? style.focusSoft : style.panelAlt);
            if (!focused) renderer.roundedOutline(chipX, y + 8.0f, chipWidth, 56.0f, 28.0f, 1.0f, style.outline);
            drawCentered(chipX, y + 8.0f, chipWidth, 56.0f, 1.65f, displayValue,
                         focused ? style.text : style.secondaryText, 12.0f, 4.0f);
            break;
        }
        }
    }

    const float footerY = settingsFooterY(state.settings.uiTextSize);
    drawCentered(330.0f, footerY, 1260.0f, 58.0f, 1.65f,
                 "Left / Right changes   |   OK opens options   |   Up searches", style.muted, 14.0f, 5.0f);

    if (!state.screen.subtitleLanguagePicker()) return;

    renderer.rect(0.0f, 0.0f, 1920.0f, 1080.0f, style.scrim);
    drawModal(430.0f, 92.0f, 1060.0f, 896.0f);
    renderer.text(490.0f, 118.0f, 3.15f, "Subtitle languages", style.text, 820.0f);
    renderer.text(490.0f, 204.0f, 1.50f,
                  fitText("Only selected languages will appear during playback", 1.50f, 920.0f, 1), style.muted,
                  920.0f);
    constexpr int visibleLanguageRows = 8;
    for (int slot = 0; slot < visibleLanguageRows; ++slot) {
        const auto row = subtitleLanguageScreenRow(state.screen, state.settings, slot, visibleLanguageRows);
        if (!row) break;

        const float y = 258.0f + static_cast<float>(slot) * 82.0f;
        const bool focused = row->focused;
        const auto languageBounds =
            drawListItemSurface(478.0f, y - 8.0f, 964.0f, 68.0f, focused, style.cornerSmall, style.listItemFocusScale);
        renderer.textVerticallyCentered(languageBounds[0] + 32.0f, languageBounds[1], languageBounds[3], 2.05f,
                                        fitText(materialLabel(row->label), 2.05f, 620.0f, 1),
                                        focused ? style.text : style.secondaryText, 620.0f);
        drawChip(1280.0f, y + 5.0f, row->selected ? "ON" : "OFF", row->selected, 1.45f, 42.0f, 116.0f);
    }
    drawCentered(540.0f, 918.0f, 840.0f, 52.0f, 1.62f, "OK toggles selection   |   Back returns to settings",
                 style.muted, 12.0f, 4.0f);
}
