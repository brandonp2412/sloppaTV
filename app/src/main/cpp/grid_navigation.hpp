#pragma once

#include <algorithm>

constexpr int gridSelectionAfterMove(int selection, int itemCount, int dx, int dy, int columns) {
    if (itemCount <= 0 || columns <= 0) return 0;

    selection = std::clamp(selection, 0, itemCount - 1);
    const int currentRow = selection / columns;
    const int currentColumn = selection % columns;
    const int rowCount = (itemCount + columns - 1) / columns;
    const int targetRow = std::clamp(currentRow + dy, 0, rowCount - 1);
    const int targetRowStart = targetRow * columns;
    const int targetRowSize = std::min(columns, itemCount - targetRowStart);
    const int targetColumn = std::clamp(currentColumn + dx, 0, targetRowSize - 1);
    return targetRowStart + targetColumn;
}
