#pragma once

#include "ui_theme.hpp"

// Stateless drawing primitives shared by every native screen. No textures,
// offscreen passes, allocations or persistent animation loops are required.
namespace material_tv {
inline void focusRing(Renderer& renderer, float x, float y, float width, float height, float radius,
                      Color accent = primary) {
    // A single high-contrast ring keeps remote focus clear without a glow.
    renderer.roundedOutline(x - 3.0f, y - 3.0f, width + 6.0f, height + 6.0f, radius + 3.0f, focusOutlineWidth, accent);
}

inline void dialog(Renderer& renderer, float x, float y, float width, float height, float radius = cornerLarge) {
    renderer.roundedRect(x, y, width, height, radius, surfaceContainerHigh);
}
} // namespace material_tv
