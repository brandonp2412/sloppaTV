#pragma once

#include "app_settings.hpp"
#include "browse_screen.hpp"
#include "media_display_text.hpp"
#include "renderer.hpp"
#include "screen_chrome_renderer.hpp"
#include "seerr_jellyfin_adapter.hpp"
#include "text_layout.hpp"
#include "ui_components.hpp"
#include "ui_labels.hpp"
#include "ui_policy.hpp"
#include "ui_theme.hpp"

#include <algorithm>
#include <array>
#include <chrono>
#include <cmath>
#include <cctype>
#include <ctime>
#include <iomanip>
#include <sstream>
#include <string>
#include <string_view>

template <typename ArtworkProviderLike> class UiPresentation {
public:
    UiPresentation(Renderer& renderer, ArtworkProviderLike& artwork) : renderer_(renderer), artwork_(artwork) {}

    void beginFrame(const JellyfinSession& session, const AppSettings& settings,
                    std::chrono::steady_clock::time_point lastInteraction, bool homeSelectionPress) {
        session_ = &session;
        uiTextSize_ = settings.uiTextSize;
        backdropMode_ = settings.backdropMode;
        showClock_ = settings.showClock;
        clock24Hour_ = settings.clock24Hour;
        lastInteraction_ = lastInteraction;
        homeSelectionPress_ = homeSelectionPress;
    }

    void drawCoverTexture(const ArtworkEntry& entry, float x, float y, float width, float height, float alpha = 1.0f,
                          float radius = material_tv::cornerSmall) {
        const int sourceWidth = entry.sourceWidth > 0 ? entry.sourceWidth : entry.decoded.width;
        const int sourceHeight = entry.sourceHeight > 0 ? entry.sourceHeight : entry.decoded.height;
        if (entry.texture == 0 || sourceWidth <= 0 || sourceHeight <= 0 || width <= 0.0f || height <= 0.0f) return;
        const float sourceAspect = static_cast<float>(sourceWidth) / static_cast<float>(sourceHeight);
        const float targetAspect = width / height;
        float u0 = 0.0f;
        float v0 = 0.0f;
        float u1 = 1.0f;
        float v1 = 1.0f;
        if (sourceAspect > targetAspect) {
            const float visible = targetAspect / sourceAspect;
            u0 = (1.0f - visible) * 0.5f;
            u1 = u0 + visible;
        } else if (sourceAspect < targetAspect) {
            const float visible = sourceAspect / targetAspect;
            v0 = (1.0f - visible) * 0.5f;
            v1 = v0 + visible;
        }
        renderer_.roundedImageRegion(entry.texture, x, y, width, height, radius, u0, v0, u1, v1, alpha);
    }

    bool drawHomeArtwork(const JellyfinItem& item, float x, float y, float width, float height,
                         float radius = material_tv::cornerSmall, float alpha = 1.0f) {
        if (!session_) return false;
        ArtworkEntry* entry = artwork_.homeTexture(*session_, item, isSeerrItem(item), renderer_);
        if (!entry) return false;
        drawCoverTexture(*entry, x, y, width, height, alpha, radius);
        return true;
    }

    void prefetchHomeWindow(const JellyfinSession& session, const JellyfinHomeData& home, int row, int selection) {
        if (row < 0 || row >= static_cast<int>(home.rows.size())) return;
        const auto& items = home.rows[static_cast<size_t>(row)].items;
        if (items.empty()) return;
        const int begin = std::max(0, selection - 2);
        const int end = std::min(static_cast<int>(items.size()), selection + 7);
        for (int index = begin; index < end; ++index) {
            const auto& item = items[static_cast<size_t>(index)];
            artwork_.requestHome(session, item, isSeerrItem(item), renderer_);
        }
    }

    void prefetchBrowseArtworkAhead(const JellyfinSession& session, const BrowseScreenState& browseState) {
        const auto& items = browseState.items();
        if (items.empty() || browseState.syntheticPage()) return;
        constexpr int columns = mediaGridColumns();
        constexpr int visibleRows = 2;
        constexpr int warmRowsAhead = 3;
        const int selectedRow = std::max(0, browseState.selection() / columns);
        const int firstVisibleRow = std::max(0, selectedRow - 1);
        const int begin = firstVisibleRow * columns;
        const int end =
            std::min(static_cast<int>(items.size()), (firstVisibleRow + visibleRows + warmRowsAhead) * columns);
        for (int index = begin; index < end; ++index) {
            const auto& item = items[static_cast<size_t>(index)];
            if (usesLandscapeMediaCard(item.type))
                artwork_.requestHome(session, item, isSeerrItem(item), renderer_);
            else
                artwork_.requestPoster(session, item, isSeerrItem(item), renderer_);
        }
    }

    bool drawProfileArtwork(const JellyfinSession& saved, float x, float y, float size) {
        ArtworkEntry* entry = artwork_.profileTexture(saved, renderer_);
        if (!entry) return false;
        drawCoverTexture(*entry, x, y, size, size, 1.0f, size * 0.5f);
        return true;
    }

    bool drawArtwork(const JellyfinItem& item, float x, float y, float width, float height, float alpha = 1.0f,
                     float radius = material_tv::cornerSmall) {
        if (!session_) return false;
        ArtworkEntry* entry = artwork_.posterTexture(*session_, item, isSeerrItem(item), renderer_);
        if (!entry) return false;
        drawCoverTexture(*entry, x, y, width, height, alpha, radius);
        return true;
    }

    bool drawBackdrop(const JellyfinItem& item, float alpha = 0.28f) {
        if (!session_) return false;
        ArtworkEntry* entry = artwork_.backdropTexture(*session_, item, backdropMode_, renderer_);
        if (!entry) return false;
        const float effectiveAlpha = backdropMode_ == 1 ? std::max(alpha, 0.34f) : alpha;
        drawCoverTexture(*entry, 0.0f, 0.0f, Renderer::logicalWidth(), Renderer::logicalHeight(), effectiveAlpha, 0.0f);
        renderer_.rect(0, 0, Renderer::logicalWidth(), Renderer::logicalHeight(),
                       Color{0.0f, 0.0f, 0.0f, backdropMode_ == 1 ? 0.26f : 0.12f});
        return true;
    }

    bool drawLogo(const JellyfinItem& item, float x, float y, float maxWidth, float maxHeight) {
        if (!session_) return false;
        ArtworkEntry* entry = artwork_.logoTexture(*session_, item, renderer_);
        if (!entry || entry->sourceWidth <= 0 || entry->sourceHeight <= 0) return false;
        const float aspect = static_cast<float>(entry->sourceWidth) / static_cast<float>(entry->sourceHeight);
        float width = maxWidth;
        float height = width / aspect;
        if (height > maxHeight) {
            height = maxHeight;
            width = height * aspect;
        }
        renderer_.image(entry->texture, x, y + (maxHeight - height) * 0.5f, width, height);
        return true;
    }

    void drawFocusHalo(float x, float y, float width, float height, Color accent = material_tv::primary,
                       float radius = 18.0f) {
        material_tv::focusRing(renderer_, x, y, width, height, radius, accent);
    }

    std::array<float, 4> focusedBounds(float x, float y, float width, float height, bool focused,
                                       float scale = materialCardFocusScale()) const {
        if (!focused) return {x, y, width, height};
        const auto elapsedMs =
            std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now() - lastInteraction_)
                .count();
        float animatedScale = scale;
        if (!homeSelectionPress_ && elapsedMs >= 0 && elapsedMs < 150) {
            const float t = std::clamp(static_cast<float>(elapsedMs) / 150.0f, 0.0f, 1.0f);
            const float remaining = 1.0f - t;
            const float eased = 1.0f - remaining * remaining * remaining;
            animatedScale = 1.0f + (scale - 1.0f) * eased;
        }
        const float scaledWidth = width * animatedScale;
        const float scaledHeight = height * animatedScale;
        return {
            x - (scaledWidth - width) * 0.5f,
            y - (scaledHeight - height) * 0.5f,
            scaledWidth,
            scaledHeight,
        };
    }

    std::array<float, 4> drawFocusedSurface(float x, float y, float width, float height, bool focused,
                                            bool primary = false, bool destructive = false, float focusScale = 1.035f,
                                            float radius = material_tv::cornerMedium, bool outlinedWhenIdle = false) {
        auto bounds = focusedBounds(x, y, width, height, focused, focusScale);
        if (!focused) {
            bounds[0] = std::round(bounds[0]);
            bounds[1] = std::round(bounds[1]);
            bounds[2] = std::round(bounds[2]);
            bounds[3] = std::round(bounds[3]);
        }
        const float radiusScale = height > 0.0f ? bounds[3] / height : 1.0f;
        const float renderedRadius = radius * radiusScale;
        const Color accent = destructive ? material_tv::error : material_tv::primary;
        const Color surface =
            destructive ? (focused ? material_tv::errorContainer
                                   : Color{material_tv::errorContainer.r, material_tv::errorContainer.g,
                                           material_tv::errorContainer.b, 0.72f})
                        : (primary ? material_tv::primaryContainer
                                   : (focused ? material_tv::surfaceContainerHighest : material_tv::surfaceContainer));
        renderer_.roundedRect(bounds[0], bounds[1], bounds[2], bounds[3], renderedRadius, surface);
        if (!focused && outlinedWhenIdle) {
            renderer_.roundedOutline(bounds[0], bounds[1], bounds[2], bounds[3], renderedRadius, 1.5f,
                                     material_tv::outline);
        }
        if (focused) drawFocusHalo(bounds[0], bounds[1], bounds[2], bounds[3], accent, renderedRadius);
        return bounds;
    }

    void drawModalSurface(float x, float y, float width, float height, float radius = material_tv::cornerLarge) {
        material_tv::dialog(renderer_, x, y, width, height, radius);
    }

    std::array<float, 4> drawListItemSurface(float x, float y, float width, float height, bool focused,
                                             float radius = material_tv::cornerMedium,
                                             float focusScale = materialListItemFocusScale()) {
        const auto bounds = focusedBounds(x, y, width, height, focused, focusScale);
        const float radiusScale = height > 0.0f ? bounds[3] / height : 1.0f;
        const float renderedRadius = radius * radiusScale;
        renderer_.roundedRect(bounds[0], bounds[1], bounds[2], bounds[3], renderedRadius,
                              focused ? material_tv::surfaceContainerHigh : material_tv::surface);
        if (focused) drawFocusHalo(bounds[0], bounds[1], bounds[2], bounds[3], material_tv::primary, renderedRadius);
        return bounds;
    }

    void drawDisabledButtonSurface(float x, float y, float width, float height) {
        x = std::round(x);
        y = std::round(y);
        width = std::round(width);
        height = std::round(height);
        const float radius = std::min(material_tv::cornerLarge, height * 0.5f);
        renderer_.roundedRect(x, y, width, height, radius, material_tv::surface);
        renderer_.roundedOutline(x, y, width, height, radius, 1.0f, material_tv::outlineVariant);
    }

    std::array<float, 4> drawButtonSurface(float x, float y, float width, float height, bool focused,
                                           bool primary = false, bool destructive = false) {
        return drawFocusedSurface(x, y, width, height, focused, primary, destructive, materialButtonFocusScale(),
                                  std::min(material_tv::cornerLarge, height * 0.5f), !primary);
    }

    std::array<float, 4> drawTabSurface(float x, float y, float width, float height, bool focused, bool selected) {
        return drawFocusedSurface(x, y, width, height, focused, selected, false, materialTabFocusScale(),
                                  material_tv::cornerLarge, !selected);
    }

    std::array<float, 4> drawInputSurface(float x, float y, float width, float height, bool focused,
                                          float focusScale = materialInputFocusScale()) {
        const auto bounds = focusedBounds(x, y, width, height, focused, focusScale);
        constexpr float radius = material_tv::cornerMedium;
        const float radiusScale = height > 0.0f ? bounds[3] / height : 1.0f;
        const float renderedRadius = radius * radiusScale;
        renderer_.roundedRect(bounds[0], bounds[1], bounds[2], bounds[3], renderedRadius,
                              focused ? material_tv::surfaceContainerHigh : material_tv::surface);
        renderer_.roundedOutline(bounds[0], bounds[1], bounds[2], bounds[3], renderedRadius, focused ? 3.0f : 1.5f,
                                 focused ? material_tv::primary : material_tv::outline);
        return bounds;
    }

    void drawSwitch(float x, float y, bool on, bool focused) {
        constexpr float width = 112.0f;
        constexpr float height = 56.0f;
        constexpr float thumbSize = 36.0f;
        constexpr float inset = 10.0f;
        const Color track = on ? material_tv::primaryContainer
                               : (focused ? material_tv::surfaceContainerHighest : material_tv::surfaceContainer);
        renderer_.roundedRect(x, y, width, height, height * 0.5f, track);
        if (!on)
            renderer_.roundedOutline(x, y, width, height, height * 0.5f, focused ? 2.0f : 1.5f,
                                     focused ? material_tv::primary : material_tv::outline);
        const float thumbX = on ? x + width - thumbSize - inset : x + inset;
        renderer_.roundedRect(thumbX, y + inset, thumbSize, thumbSize, thumbSize * 0.5f,
                              on ? material_tv::onPrimaryContainer : material_tv::onSurfaceSecondary);
    }

    void drawRightAlignedSingleLine(float right, float y, float scale, std::string_view value, Color color,
                                    float maxWidth) {
        float fittedScale = scale;
        float width = renderer_.textWidth(fittedScale, value);
        if (maxWidth > 0.0f && width > maxWidth && width > 0.0f) {
            fittedScale *= maxWidth / width;
            width = renderer_.textWidth(fittedScale, value);
        }
        renderer_.text(right - width, y, fittedScale, value, color);
    }

    float fittedSingleLineScale(float scale, std::string_view value, float width, float height) const {
        float fittedScale = scale;
        const float measuredWidth = renderer_.textWidth(fittedScale, value);
        const float visualHeight = 10.0f * fittedScale * uiTextScale(uiTextSize_);
        float fit = 1.0f;
        if (measuredWidth > width && measuredWidth > 0.0f) fit = std::min(fit, width / measuredWidth);
        if (visualHeight > height && visualHeight > 0.0f) fit = std::min(fit, height / visualHeight);
        return fittedScale * fit;
    }

    void drawCenteredSingleLineFit(float x, float y, float width, float height, float scale, std::string_view value,
                                   Color color, float horizontalPadding = 0.0f, float verticalPadding = 0.0f) {
        const float availableWidth = std::max(1.0f, width - horizontalPadding * 2.0f);
        const float availableHeight = std::max(1.0f, height - verticalPadding * 2.0f);
        const float fittedScale = fittedSingleLineScale(scale, value, availableWidth, availableHeight);
        renderer_.textCentered(x + horizontalPadding, y + verticalPadding, availableWidth, availableHeight, fittedScale,
                               value, color);
    }

    void drawLeftAlignedSingleLineFit(float x, float y, float width, float height, float scale, std::string_view value,
                                      Color color) {
        const float fittedScale = fittedSingleLineScale(scale, value, width, height);
        renderer_.textVerticallyCentered(x, y, height, fittedScale, fitTextLines(value, fittedScale, width, 1), color,
                                         width);
    }

    float drawChip(float x, float y, std::string_view label, bool selected = false, float scale = 1.45f,
                   float height = 42.0f, float maxWidth = 360.0f) {
        const std::string_view displayLabel = materialLabel(label);
        const float width = std::round(std::clamp(renderer_.textWidth(scale, displayLabel) + 34.0f, 72.0f, maxWidth));
        x = std::round(x);
        y = std::round(y);
        renderer_.roundedRect(x, y, width, height, height * 0.5f,
                              selected ? material_tv::primaryContainer : material_tv::surfaceContainer);
        if (!selected) renderer_.roundedOutline(x, y, width, height, height * 0.5f, 1.0f, material_tv::outline);
        drawCenteredSingleLineFit(x, y, width, height, scale, displayLabel,
                                  selected ? material_tv::onSurface : material_tv::onSurfaceSecondary, 14.0f, 4.0f);
        return width;
    }

    void drawArtworkPlaceholder(const JellyfinItem& item, float x, float y, float width, float height,
                                float radius = material_tv::cornerSmall, float alpha = 1.0f) {
        const auto faded = [alpha](Color color) {
            color.a *= alpha;
            return color;
        };
        renderer_.roundedRect(x, y, width, height, radius, faded(material_tv::surfaceContainer));
        renderer_.roundedOutline(x, y, width, height, radius, 1.0f, faded(material_tv::outline));
        const std::string& source = item.type == "Episode" && !item.seriesName.empty() ? item.seriesName : item.name;
        std::string initial = "?";
        const auto first =
            std::find_if(source.begin(), source.end(), [](unsigned char c) { return std::isalnum(c) != 0; });
        if (first != source.end())
            initial.assign(1, static_cast<char>(std::toupper(static_cast<unsigned char>(*first))));
        const float scale = height < 100.0f ? 2.0f : (height < 200.0f ? 3.0f : 4.2f);
        renderer_.textCentered(x, y, width, height, scale, initial, faded(material_tv::onSurfaceVariant));
    }

    std::string fitTextLines(std::string_view value, float scale, float maxWidth, int maxLines) const {
        return fitRenderedTextLines(renderer_, value, scale, maxWidth, maxLines);
    }

    void renderHeader(std::string_view title) {
        renderScreenHeader(
            renderer_, std::string(title), showClock_, clock24Hour_, screenChromeStyle(),
            [this](std::string_view value, float scale, float width, int lines) {
                return fitTextLines(value, scale, width, lines);
            },
            [this](float right, float y, float scale, std::string_view value, Color color, float maxWidth) {
                drawRightAlignedSingleLine(right, y, scale, value, color, maxWidth);
            },
            [](bool clock24Hour) { return formatLocalClock(std::time(nullptr), clock24Hour); });
    }

    void renderEmptyState(std::string_view title, std::string_view message) {
        renderScreenEmptyState(renderer_, std::string(title), std::string(message), screenChromeStyle(),
                               [this](float x, float y, float width, float height, float scale, std::string_view value,
                                      Color color, float horizontalPadding, float verticalPadding) {
                                   drawCenteredSingleLineFit(x, y, width, height, scale, value, color,
                                                             horizontalPadding, verticalPadding);
                               });
    }

private:
    ScreenChromeRenderStyle<Color> screenChromeStyle() const {
        return ScreenChromeRenderStyle<Color>{
            .pageInset = material_tv::layout::pageInset,
            .supportingScale = material_tv::type::supporting,
            .headlineScale = material_tv::type::headline,
            .titleScale = material_tv::type::title,
            .labelScale = material_tv::type::label,
            .cornerLarge = material_tv::cornerLarge,
            .muted = material_tv::onSurfaceVariant,
            .text = material_tv::onSurface,
            .panelAlt = material_tv::surfaceContainer,
        };
    }

    Renderer& renderer_;
    ArtworkProviderLike& artwork_;
    const JellyfinSession* session_ = nullptr;
    int uiTextSize_ = 1;
    int backdropMode_ = 0;
    bool showClock_ = true;
    bool clock24Hour_ = false;
    bool homeSelectionPress_ = false;
    std::chrono::steady_clock::time_point lastInteraction_{};
};
