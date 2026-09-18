#include "home_row_renderer.hpp"

#include <array>
#include <cassert>
#include <chrono>
#include <cmath>
#include <string>
#include <string_view>
#include <vector>

namespace {
struct FakeColor {
    int id = 0;
    float a = 1.0f;
};

struct TextCall {
    std::string value;
    FakeColor color;
};

struct RectCall {
    float width = 0.0f;
    FakeColor color;
};

struct FakeRenderer {
    void text(float, float, float, std::string_view value, FakeColor color, float) {
        texts.push_back({std::string(value), color});
    }

    void roundedRect(float, float, float width, float, float, FakeColor color) { rects.push_back({width, color}); }

    float textWidth(float, std::string_view value) const { return static_cast<float>(value.size()) * 8.0f; }

    void textVerticallyCentered(float, float, float, float, std::string_view value, FakeColor color, float) {
        verticalTexts.push_back({std::string(value), color});
    }

    std::vector<TextCall> texts;
    std::vector<TextCall> verticalTexts;
    std::vector<RectCall> rects;
};

HomeRowRenderStyle<FakeColor> style() {
    return HomeRowRenderStyle<FakeColor>{
        .cornerSmall = 12.0f,
        .cardFocusScale = 1.025f,
        .text = {1, 1.0f},
        .secondaryText = {2, 1.0f},
        .muted = {3, 1.0f},
        .track = {4, 1.0f},
        .focus = {5, 1.0f},
    };
}

std::array<float, 4> bounds(float x, float y, float width, float height, bool focused, float focusScale) {
    assert(focusScale == 1.025f);
    if (!focused) return {x, y, width, height};
    return {x - 2.0f, y - 2.0f, width + 4.0f, height + 4.0f};
}

std::string fitText(std::string_view value, float, float, int) {
    return std::string(value);
}

JellyfinItem item(std::string id, std::string name) {
    JellyfinItem value;
    value.id = std::move(id);
    value.name = std::move(name);
    return value;
}

bool near(float actual, float expected) {
    return std::abs(actual - expected) < 0.001f;
}
} // namespace

int main() {
    HomeScreenState mediaState;
    mediaState.reset();
    mediaState.setSelections({1});
    mediaState.setRow(0);

    std::vector<JellyfinItem> mediaItems{
        item("library-1", "Movies"), item("library-2", "Shows"), item("library-3", "Music"),
        item("library-4", "Photos"), item("library-5", "Books"),
    };

    FakeRenderer mediaRenderer;
    int mediaArtworkCalls = 0;
    int mediaPlaceholderCalls = 0;
    int mediaHaloCalls = 0;
    renderHomeRowContent(
        mediaRenderer, "My Media", mediaItems, 0, 150.0f, mediaState, 1, HomeRowFadeState{}, style(), bounds,
        [&](const JellyfinItem&, float, float, float, float, float, float alpha) {
            ++mediaArtworkCalls;
            assert(alpha == 1.0f);
            return mediaArtworkCalls != 2;
        },
        [&](const JellyfinItem& rendered, float, float, float, float, float, float alpha) {
            ++mediaPlaceholderCalls;
            assert(rendered.id == "library-2");
            assert(alpha == 1.0f);
        },
        [&](float, float, float, float, FakeColor color, float) {
            ++mediaHaloCalls;
            assert(color.id == 5);
            assert(color.a == 1.0f);
        },
        fitText, [](float, float, const std::string&, bool, float, float, float) { return 0.0f; },
        [](const JellyfinItem&) { return false; }, [](const JellyfinItem&) { return std::string(); });

    assert(mediaArtworkCalls == 4);
    assert(mediaPlaceholderCalls == 1);
    assert(mediaHaloCalls == 1);
    assert(mediaRenderer.texts.size() == 5);
    assert(mediaRenderer.texts.front().value == "My media");
    assert(mediaRenderer.texts.front().color.id == 1);
    assert(mediaRenderer.texts.back().value == "Photos");

    HomeScreenState nextUpState;
    nextUpState.reset();
    nextUpState.setSelections({0});
    nextUpState.setRow(0);

    JellyfinItem downloading = item("episode-1", "Pilot");
    downloading.type = "Episode";
    downloading.seriesName = "Series";
    downloading.externalStatus = "Downloading";
    downloading.externalProgressPercent = 42;
    downloading.externalProgressLabel = "Downloading";
    downloading.externalProgressEta = "12 min";

    const auto started = std::chrono::steady_clock::time_point{std::chrono::seconds(10)};
    FakeRenderer nextUpRenderer;
    float artworkAlpha = -1.0f;
    float haloAlpha = -1.0f;
    std::string chipLabel;
    renderHomeRowContent(
        nextUpRenderer, "Next Up", std::vector<JellyfinItem>{downloading}, 0, 150.0f, nextUpState, 1,
        HomeRowFadeState{
            .itemIndex = 0,
            .itemId = "episode-1",
            .started = started,
            .now = started + std::chrono::milliseconds(150),
        },
        style(), bounds,
        [&](const JellyfinItem& rendered, float, float, float, float, float, float alpha) {
            assert(rendered.id == "episode-1");
            artworkAlpha = alpha;
            return true;
        },
        [](const JellyfinItem&, float, float, float, float, float, float) { assert(false); },
        [&](float, float, float, float, FakeColor color, float) { haloAlpha = color.a; }, fitText,
        [&](float, float, const std::string& label, bool selected, float scale, float height, float maxWidth) {
            assert(!selected);
            assert(scale == 1.08f);
            assert(height == 30.0f);
            assert(maxWidth == 210.0f);
            chipLabel = label;
            return 100.0f;
        },
        [](const JellyfinItem& rendered) { return rendered.externalStatus == "Downloading"; },
        [](const JellyfinItem&) { return std::string("S1 - E1"); });

    assert(near(artworkAlpha, 0.5f));
    assert(near(haloAlpha, 0.5f));
    assert(chipLabel == "12 min");
    assert(nextUpRenderer.rects.size() == 2);
    assert(near(nextUpRenderer.rects[0].color.a, 0.5f));
    assert(near(nextUpRenderer.rects[1].color.a, 0.5f));
    assert(nextUpRenderer.verticalTexts.size() == 1);
    assert(nextUpRenderer.verticalTexts.front().value == "42%");
    assert(nextUpRenderer.verticalTexts.front().color.id == 2);
    assert(nextUpRenderer.verticalTexts.front().color.a == 1.0f);

    JellyfinItem episode = item("episode-2", "Second");
    episode.type = "Episode";
    episode.seriesName = "Series";

    FakeRenderer episodeRenderer;
    renderHomeRowContent(
        episodeRenderer, "Continue Watching", std::vector<JellyfinItem>{episode}, 0, 150.0f, nextUpState, 1,
        HomeRowFadeState{}, style(), bounds,
        [](const JellyfinItem&, float, float, float, float, float, float) { return true; },
        [](const JellyfinItem&, float, float, float, float, float, float) { assert(false); },
        [](float, float, float, float, FakeColor, float) {}, fitText,
        [](float, float, const std::string&, bool, float, float, float) { return 0.0f; },
        [](const JellyfinItem&) { return false; }, [](const JellyfinItem&) { return std::string("S1 - E2"); });

    bool sawEpisodeLabel = false;
    for (const auto& text : episodeRenderer.texts) {
        if (text.value == "S1 - E2  |  Second") sawEpisodeLabel = true;
    }
    assert(sawEpisodeLabel);

    return 0;
}
