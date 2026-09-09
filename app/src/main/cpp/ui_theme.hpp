#pragma once

#include "renderer.hpp"

// Material 3-inspired tokens for a dark, remote-first television interface.
// Keep these semantic: screens select a role rather than embedding a colour.
namespace material_tv {

inline constexpr Color background{0.031f, 0.043f, 0.067f, 1.0f};
inline constexpr Color surface{0.047f, 0.059f, 0.086f, 0.96f};
inline constexpr Color surfaceContainer{0.078f, 0.090f, 0.122f, 0.98f};
inline constexpr Color surfaceContainerHigh{0.106f, 0.122f, 0.161f, 0.99f};
inline constexpr Color surfaceContainerHighest{0.137f, 0.153f, 0.196f, 1.0f};
inline constexpr Color onSurface{0.937f, 0.941f, 0.973f, 1.0f};
inline constexpr Color onSurfaceSecondary{0.855f, 0.867f, 0.914f, 1.0f};
inline constexpr Color onSurfaceVariant{0.773f, 0.788f, 0.835f, 1.0f};
inline constexpr Color onSurfaceDisabled{0.610f, 0.631f, 0.700f, 1.0f};
inline constexpr Color outline{0.548f, 0.569f, 0.639f, 0.62f};
inline constexpr Color outlineVariant{0.270f, 0.290f, 0.349f, 0.72f};
inline constexpr Color track{0.270f, 0.290f, 0.349f, 0.82f};
inline constexpr Color primary{0.655f, 0.806f, 1.0f, 1.0f};
inline constexpr Color onPrimary{0.012f, 0.118f, 0.212f, 1.0f};
inline constexpr Color primaryContainer{0.082f, 0.269f, 0.448f, 1.0f};
inline constexpr Color onPrimaryContainer{0.804f, 0.909f, 1.0f, 1.0f};
inline constexpr Color secondaryContainer{0.154f, 0.231f, 0.318f, 0.98f};
inline constexpr Color tertiary{0.925f, 0.778f, 0.536f, 1.0f};
inline constexpr Color error{1.0f, 0.706f, 0.706f, 1.0f};
inline constexpr Color errorContainer{0.565f, 0.055f, 0.075f, 1.0f};
inline constexpr Color scrim{0.0f, 0.0f, 0.0f, 0.68f};

constexpr float cornerExtraSmall = 12.0f;
constexpr float cornerSmall = 16.0f;
constexpr float cornerMedium = 24.0f;
constexpr float cornerLarge = 32.0f;
constexpr float focusOutlineWidth = 4.0f;
constexpr float focusHaloWidth = 10.0f;

} // namespace material_tv
