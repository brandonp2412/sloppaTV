#pragma once

#include "ui_labels.hpp"
#include "ui_policy.hpp"

#include <cstddef>
#include <string_view>

template <typename ColorLike> struct KeyboardRenderStyle {
    float canvasHeight = 1080.0f;
    float cornerLarge = 0.0f;
    ColorLike surfaceContainerHigh{};
    ColorLike text{};
};

template <typename RendererLike, typename Rows, typename ColorLike, typename DrawButtonSurface, typename DrawCentered>
void renderVirtualKeyboard(RendererLike& renderer, const Rows& rows, int selectedRow, int selectedColumn, float top,
                           const KeyboardRenderStyle<ColorLike>& style, DrawButtonSurface&& drawButtonSurface,
                           DrawCentered&& drawCentered) {
    renderer.roundedRect(110.0f, top - 24.0f, 1700.0f, style.canvasHeight - top + 24.0f, style.cornerLarge,
                         style.surfaceContainerHigh);
    constexpr float startX = 150.0f;
    constexpr float gap = 14.0f;
    const float keyHeight = keyboardKeyHeight(top, static_cast<int>(rows.size()), gap);
    for (std::size_t row = 0; row < rows.size(); ++row) {
        const float y = top + static_cast<float>(row) * (keyHeight + gap);
        const auto& keys = rows[row];
        const float keyWidth = row == rows.size() - 1 ? 310.0f : 145.0f;
        for (std::size_t column = 0; column < keys.size(); ++column) {
            const float x = startX + static_cast<float>(column) * (keyWidth + gap);
            const bool selected = static_cast<int>(row) == selectedRow && static_cast<int>(column) == selectedColumn;
            const auto bounds = drawButtonSurface(x, y, keyWidth, keyHeight, selected, selected);
            const std::string_view label = keys[column].label;
            const float scale = label.size() > 4 ? 2.15f : 2.65f;
            drawCentered(bounds[0], bounds[1], bounds[2], bounds[3], scale, materialLabel(label), style.text, 12.0f,
                         5.0f);
        }
    }
}
