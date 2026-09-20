#include "media_card_renderer.hpp"

#include <array>
#include <cassert>
#include <string>
#include <string_view>
#include <vector>

namespace {
struct RectCall {
    float x = 0.0f;
    float y = 0.0f;
    float width = 0.0f;
    float height = 0.0f;
    float radius = 0.0f;
    int color = 0;
};

struct TextCall {
    float x = 0.0f;
    float y = 0.0f;
    float scale = 0.0f;
    std::string value;
    int color = 0;
    float maxWidth = 0.0f;
};

struct FakeRenderer {
    void roundedRect(float x, float y, float width, float height, float radius, int color) {
        rects.push_back({x, y, width, height, radius, color});
    }

    void roundedOutline(float x, float y, float width, float height, float radius, float, int color) {
        outlines.push_back({x, y, width, height, radius, color});
    }

    void text(float x, float y, float scale, std::string_view value, int color, float maxWidth) {
        texts.push_back({x, y, scale, std::string(value), color, maxWidth});
    }

    std::vector<RectCall> rects;
    std::vector<RectCall> outlines;
    std::vector<TextCall> texts;
};

MediaCardRenderStyle<int> style() {
    return MediaCardRenderStyle<int>{
        .canvasHeight = 1080.0f,
        .cornerSmall = 12.0f,
        .cornerMedium = 20.0f,
        .cardFocusScale = 1.025f,
        .labelScale = 1.8f,
        .panel = 1,
        .panelAlt = 2,
        .panelElevated = 3,
        .tertiary = 4,
        .text = 5,
        .muted = 6,
        .track = 7,
        .focus = 8,
        .focusSoft = 9,
        .outline = 10,
    };
}

std::array<float, 4> bounds(float x, float y, float width, float height, bool focused, float focusScale) {
    assert(focusScale == 1.025f);
    if (!focused) return {x, y, width, height};
    return {x - 2.0f, y - 3.0f, width + 4.0f, height + 6.0f};
}
} // namespace

int main() {
    JellyfinItem episode;
    episode.id = "episode-1";
    episode.name = "Pilot";
    episode.type = "Episode";
    episode.seriesId = "series-1";
    episode.seriesPrimaryImageTag = "series-tag";
    episode.positionTicks = 50;
    episode.runtimeTicks = 100;
    episode.favorite = true;

    FakeRenderer renderer;
    int artworkCalls = 0;
    int placeholderCalls = 0;
    int haloCalls = 0;
    int centeredCalls = 0;
    bool sawSeriesCover = false;
    bool sawLandscape = true;

    renderMediaArtworkCardContent(
        renderer, episode, 80.0f, 200.0f, 320.0f, true,
        MediaArtworkCardRenderOptions{
            .showState = true,
            .preferSeriesCover = true,
            .alignToPortraitBand = true,
            .titleLineLimit = 2,
            .uiTextSize = 1,
            .showWatchedIndicators = true,
        },
        style(), bounds,
        [&](const JellyfinItem& item, bool seriesCover, bool landscape, float x, float y, float width, float height,
            float radius) {
            ++artworkCalls;
            assert(item.id == "episode-1");
            sawSeriesCover = seriesCover;
            sawLandscape = landscape;
            assert(x == 126.0f);
            assert(y == 197.0f);
            assert(width == 228.0f);
            assert(height == 318.0f);
            assert(radius > 12.0f);
            return false;
        },
        [&](const JellyfinItem& item, float, float, float, float, float) {
            ++placeholderCalls;
            assert(item.id == "episode-1");
        },
        [&](float, float, float, float, int color, float) {
            ++haloCalls;
            assert(color == 8);
        },
        [&](float, float, float, float, float scale, std::string_view value, int color, float, float) {
            ++centeredCalls;
            assert(scale == 1.12f);
            assert(value == "Favorite");
            assert(color == 5);
        },
        [](std::string_view value, float, float, int) { return std::string(value); },
        [](const JellyfinItem& item) {
            assert(item.id == "episode-1");
            return std::string("S1 - E1");
        });

    assert(artworkCalls == 1);
    assert(placeholderCalls == 1);
    assert(haloCalls == 1);
    assert(centeredCalls == 1);
    assert(sawSeriesCover);
    assert(!sawLandscape);
    assert(renderer.outlines.size() == 1);
    assert(renderer.rects.size() == 4);
    assert(renderer.rects[1].color == 7);
    assert(renderer.rects[2].color == 8);
    assert(renderer.rects[3].color == 3);
    assert(renderer.texts.size() == 2);
    assert(renderer.texts[0].value == "Pilot");
    assert(renderer.texts[0].scale == 1.8f);
    assert(renderer.texts[0].x == 82.0f);
    assert(renderer.texts[0].maxWidth == 316.0f);
    assert(renderer.texts[1].value == "S1 - E1");
    assert(renderer.texts[1].scale == 1.45f);

    JellyfinItem landscapeItem;
    landscapeItem.id = "episode-2";
    landscapeItem.name = "Next Episode";
    landscapeItem.type = "Episode";
    landscapeItem.played = true;

    FakeRenderer landscapeRenderer;
    bool landscapeArtwork = false;
    int landscapeCentered = 0;
    renderMediaArtworkCardContent(
        landscapeRenderer, landscapeItem, 100.0f, 120.0f, 320.0f, false,
        MediaArtworkCardRenderOptions{
            .showState = true,
            .preferSeriesCover = false,
            .alignToPortraitBand = false,
            .titleLineLimit = 0,
            .uiTextSize = 0,
            .showWatchedIndicators = false,
        },
        style(), bounds,
        [&](const JellyfinItem&, bool seriesCover, bool landscape, float, float, float, float, float) {
            assert(!seriesCover);
            landscapeArtwork = landscape;
            return true;
        },
        [](const JellyfinItem&, float, float, float, float, float) { assert(false); },
        [](float, float, float, float, int, float) { assert(false); },
        [&](float, float, float, float, float, std::string_view, int, float, float) { ++landscapeCentered; },
        [](std::string_view value, float, float, int) { return std::string(value); },
        [](const JellyfinItem&) { return std::string(); });

    assert(landscapeArtwork);
    assert(landscapeCentered == 0);
    assert(landscapeRenderer.rects.size() == 1);
    assert(landscapeRenderer.outlines.empty());
    assert(landscapeRenderer.texts.size() == 1);

    JellyfinItem genre;
    genre.name = "Drama";
    genre.type = "Genre";
    FakeRenderer tileRenderer;
    int tileCentered = 0;
    int tileHalos = 0;
    renderMediaTextTileContent(
        tileRenderer, genre, 80.0f, 285.0f, 320.0f, 160.0f, true, style(), bounds,
        [&](float, float, float, float, float scale, std::string_view value, int, float, float) {
            ++tileCentered;
            if (tileCentered == 1) {
                assert(scale == 1.25f);
                assert(value == "Genre");
            } else {
                assert(scale == 2.55f);
                assert(value == "Drama");
            }
        },
        [&](float, float, float, float, int color, float) {
            ++tileHalos;
            assert(color == 8);
        });

    assert(tileRenderer.rects.size() == 1);
    assert(tileRenderer.rects[0].color == 3);
    assert(tileCentered == 2);
    assert(tileHalos == 1);

    return 0;
}
