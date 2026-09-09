#pragma once

#include "ui_theme.hpp"

// Stateless drawing primitives shared by every native screen. No textures,
// offscreen passes, allocations or persistent animation loops are required.
namespace material_tv {
inline void focusRing(Renderer& renderer, float x, float y, float width, float height,
                      float radius, Color accent = primary) {
    renderer.roundedOutline(x - focusHaloWidth, y - focusHaloWidth,
        width + focusHaloWidth * 2.0f, height + focusHaloWidth * 2.0f,
        radius + focusHaloWidth, focusHaloWidth,
        Color{accent.r, accent.g, accent.b, 0.18f});
    renderer.roundedOutline(x - 3.0f, y - 3.0f, width + 6.0f, height + 6.0f,
        radius + 3.0f, focusOutlineWidth, accent);
}

inline void dialog(Renderer& renderer, float x, float y, float width, float height,
                   float radius = cornerLarge) {
    renderer.roundedRect(x, y, width, height, radius, surfaceContainerHigh);
    renderer.roundedOutline(x, y, width, height, radius, 1.5f, outlineVariant);
}
}
