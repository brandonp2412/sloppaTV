#pragma once

#include <algorithm>
#include <cmath>
#include <iomanip>
#include <sstream>
#include <string>

// Keep 0 at the centre, while allowing fine-grained movement around 0 and
// expanding the visible range if a track needs a large manual correction.
inline float subtitleOffsetDotX(int offsetMs, float centreX, float halfWidth) {
    const float halfRangeMs = static_cast<float>(std::max(10000, std::abs(offsetMs) + 5000));
    return centreX + std::clamp(static_cast<float>(offsetMs) / halfRangeMs, -1.0f, 1.0f) * halfWidth;
}

inline std::string subtitleOffsetLabel(int offsetMs) {
    std::ostringstream value;
    value << std::showpos << std::fixed << std::setprecision(2) << static_cast<double>(offsetMs) / 1000.0;
    return value.str() + " s" + (offsetMs > 0 ? "  Later" : offsetMs < 0 ? "  Earlier" : "  In sync");
}

template <typename RendererLike, typename ColorLike>
void renderSubtitleOffsetOverlay(RendererLike& renderer, int offsetMs, float logicalWidth, float logicalHeight,
                                 ColorLike background, ColorLike track, ColorLike centre, ColorLike active) {
    const float centreX = logicalWidth * 0.5f;
    const float centreY = logicalHeight * 0.50f;
    const float panelWidth = logicalWidth * 0.78f;
    const float panelHeight = 212.0f;
    const float trackHalfWidth = (panelWidth - 130.0f) * 0.5f;
    const float lineY = centreY + 9.0f;
    renderer.roundedRect(centreX - panelWidth * 0.5f, centreY - panelHeight * 0.5f, panelWidth, panelHeight, 24.0f,
                         background);
    renderer.textCentered(centreX - panelWidth * 0.5f, centreY - 85.0f, panelWidth, 38.0f, 1.55f, "Subtitle offset",
                          centre);
    renderer.textCentered(centreX - panelWidth * 0.5f, centreY - 48.0f, panelWidth, 34.0f, 1.35f,
                          subtitleOffsetLabel(offsetMs), active);
    renderer.roundedRect(centreX - trackHalfWidth, lineY - 2.0f, trackHalfWidth * 2.0f, 4.0f, 2.0f, track);
    renderer.roundedRect(centreX - 1.5f, lineY - 15.0f, 3.0f, 30.0f, 1.5f, centre);
    const float dotX = subtitleOffsetDotX(offsetMs, centreX, trackHalfWidth);
    renderer.roundedRect(dotX - 11.0f, lineY - 11.0f, 22.0f, 22.0f, 11.0f, active);
    renderer.textCentered(centreX - panelWidth * 0.5f, centreY + 40.0f, panelWidth, 31.0f, 1.15f,
                          "Left / Right  0.25 s     OK  Reset     Back  Close", centre);
}
