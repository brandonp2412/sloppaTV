#include "details_renderer.hpp"

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
    int color = 0;
};

struct TextCall {
    std::string value;
    float x = 0.0f;
    float y = 0.0f;
    int color = 0;
};

struct FakeRenderer {
    void rect(float x, float y, float width, float height, int color) { rects.push_back({x, y, width, height, color}); }

    void roundedRect(float x, float y, float width, float height, float, int color) {
        roundedRects.push_back({x, y, width, height, color});
    }

    void roundedOutline(float x, float y, float width, float height, float, float, int color) {
        outlines.push_back({x, y, width, height, color});
    }

    void horizontalGradient(float, float, float, float, int start, int end) {
        horizontalGradients.push_back({0.0f, 0.0f, 0.0f, 0.0f, start});
        horizontalGradientEnd = end;
    }

    void verticalGradient(float, float, float, float, int start, int end) {
        verticalGradients.push_back({0.0f, 0.0f, 0.0f, 0.0f, start});
        verticalGradientEnd = end;
    }

    void text(float x, float y, float, std::string_view value, int color, float) {
        texts.push_back({std::string(value), x, y, color});
    }

    float textWidth(float, std::string_view value) const { return static_cast<float>(value.size()) * 12.0f; }

    std::vector<RectCall> rects;
    std::vector<RectCall> roundedRects;
    std::vector<RectCall> outlines;
    std::vector<RectCall> horizontalGradients;
    std::vector<RectCall> verticalGradients;
    std::vector<TextCall> texts;
    int horizontalGradientEnd = 0;
    int verticalGradientEnd = 0;
};

bool contains(const std::vector<std::string>& values, std::string_view value) {
    for (const auto& item : values)
        if (item == value) return true;
    return false;
}

bool containsText(const std::vector<TextCall>& values, std::string_view value) {
    for (const auto& item : values)
        if (item.value == value) return true;
    return false;
}

struct Harness {
    FakeRenderer renderer;
    bool backdrop = false;
    bool logo = false;
    int backdropCalls = 0;
    int artworkCalls = 0;
    int placeholderCalls = 0;
    int haloCalls = 0;
    std::vector<std::string> centered;
    std::vector<std::string> chips;
    std::vector<bool> buttonFocus;
    std::vector<bool> buttonPrimary;

    void render(const JellyfinItem& detail, const DetailsScreenState& state, std::span<const std::string> actions,
                const DetailsRenderConfig& config) {
        renderDetailsScreen(
            renderer, detail, state, actions, config,
            DetailsRenderStyle<int>{
                .canvasWidth = 1920.0f,
                .canvasHeight = 1080.0f,
                .cornerLarge = 28.0f,
                .cornerExtraSmall = 8.0f,
                .cardFocusScale = 1.025f,
                .labelScale = 1.5f,
                .background = 1,
                .text = 2,
                .muted = 3,
                .secondaryText = 4,
                .focus = 5,
                .panelElevated = 6,
                .outline = 7,
                .track = 8,
                .backdropHorizontalStart = 9,
                .backdropHorizontalEnd = 10,
                .backdropVerticalStart = 11,
                .backdropVerticalEnd = 12,
            },
            [&](const JellyfinItem& item, float alpha) {
                ++backdropCalls;
                assert(item.id == detail.id);
                assert(alpha == 0.84f);
                return backdrop;
            },
            [](float right, float y, float scale, std::string_view value, int color, float maxWidth) {
                assert(right == 1840.0f);
                assert(y == 50.0f);
                assert(scale == 2.10f);
                assert(value == "21:45");
                assert(color == 3);
                assert(maxWidth == 210.0f);
            },
            [](bool clock24Hour) {
                assert(clock24Hour);
                return std::string("21:45");
            },
            [&](float, float, float, float, float, std::string_view value, int, float, float) {
                centered.emplace_back(value);
            },
            [&](const JellyfinItem& item, float, float, float, float) {
                assert(item.id == detail.id);
                return logo;
            },
            [](std::string_view value, float, float, int) { return std::string(value); },
            [](const JellyfinItem& item) {
                if (item.type != "Episode") return std::string();
                return std::string("S1E2");
            },
            [](const JellyfinItem& item) {
                if (item.type == "Episode") return item.seriesName + " - S1E2";
                return std::string();
            },
            [](int milliseconds) {
                assert(milliseconds >= 0);
                return std::string("42:00");
            },
            [&](float, float, std::string_view value, bool, float, float, float available) {
                chips.emplace_back(value);
                return std::min(120.0f, available);
            },
            [](std::string_view value) { return std::string(value); },
            [&](float x, float y, float width, float height, bool focused, bool primary) {
                buttonFocus.push_back(focused);
                buttonPrimary.push_back(primary);
                return std::array<float, 4>{x, y, width, height};
            },
            [](float x, float y, float width, float height, bool focused, float) {
                if (focused) return std::array<float, 4>{x - 2.0f, y - 1.0f, width + 4.0f, height + 2.0f};
                return std::array<float, 4>{x, y, width, height};
            },
            [&](const JellyfinItem&, float, float, float, float, float) {
                ++artworkCalls;
                return false;
            },
            [&](const JellyfinItem&, float, float, float, float, float) { ++placeholderCalls; },
            [&](float, float, float, float, int color, float) {
                ++haloCalls;
                assert(color == 5);
            });
    }
};
} // namespace

int main() {
    JellyfinItem movie;
    movie.id = "movie";
    movie.name = "Example Movie";
    movie.type = "Movie";
    movie.productionYear = 2026;
    movie.officialRating = "M";
    movie.runtimeTicks = 25200000000LL;
    movie.positionTicks = movie.runtimeTicks / 2;
    movie.communityRating = 8.4f;
    movie.genres = {"Drama"};
    movie.overview = "An example overview.";
    movie.favorite = true;
    movie.played = true;

    DetailsScreenState movieState;
    movieState.beginDetails();
    JellyfinItem similar;
    similar.id = "similar";
    similar.name = "Similar Movie";
    similar.type = "Movie";
    movieState.setSimilar({similar});
    movieState.setSimilarFocused(true);

    const std::vector<std::string> movieActions{"PLAY", "UNFAVORITE", "MARK UNWATCHED", "MORE", "BACK"};

    Harness movieHarness;
    movieHarness.backdrop = false;
    movieHarness.render(movie, movieState, movieActions,
                        DetailsRenderConfig{
                            .showClock = true,
                            .clock24Hour = true,
                            .showWatchedIndicators = true,
                            .uiTextSize = 1,
                            .stillWatchingPrompt = true,
                            .overlayOpen = false,
                        });

    assert(movieHarness.backdropCalls == 1);
    assert(movieHarness.renderer.rects.size() == 1);
    assert(movieHarness.renderer.rects.front().color == 1);
    assert(movieHarness.renderer.outlines.size() == 1);
    assert(containsText(movieHarness.renderer.texts, "Example Movie"));
    assert(containsText(movieHarness.renderer.texts, "More like this"));
    assert(contains(movieHarness.centered, "Still watching? Press OK to continue"));
    assert(contains(movieHarness.centered, "PLAY"));
    assert(contains(movieHarness.chips, "2026"));
    assert(contains(movieHarness.chips, "M"));
    assert(contains(movieHarness.chips, "42:00"));
    assert(contains(movieHarness.chips, "8.4/10"));
    assert(contains(movieHarness.chips, "Drama"));
    assert(contains(movieHarness.chips, "Favorite"));
    assert(contains(movieHarness.chips, "Watched"));
    assert(movieHarness.artworkCalls == 1);
    assert(movieHarness.placeholderCalls == 1);
    assert(movieHarness.haloCalls == 1);
    assert(movieHarness.renderer.roundedRects.size() >= 3);
    bool sawHalfProgress = false;
    for (const auto& rect : movieHarness.renderer.roundedRects) {
        if (rect.width == 280.0f && rect.color == 5) sawHalfProgress = true;
    }
    assert(sawHalfProgress);

    JellyfinItem series;
    series.id = "series";
    series.name = "Example Series";
    series.type = "Series";

    JellyfinItem season;
    season.id = "season-1";
    season.name = "Season One";

    JellyfinItem episode;
    episode.id = "episode";
    episode.name = "Pilot";
    episode.type = "Episode";
    episode.seriesId = "series";
    episode.seriesName = "Example Series";
    episode.parentIndexNumber = 1;
    episode.indexNumber = 2;

    DetailsScreenState episodeState;
    episodeState.beginDetails();
    episodeState.setEpisodeSeriesContext(series, {season});
    episodeState.setEpisodeContextFocused(true);
    episodeState.moveEpisodeContext(1);

    const std::vector<std::string> episodeActions{"PLAY", "FAVORITE", "MARK WATCHED", "MORE", "BACK"};

    Harness episodeHarness;
    episodeHarness.backdrop = true;
    episodeHarness.logo = true;
    episodeHarness.render(episode, episodeState, episodeActions,
                          DetailsRenderConfig{
                              .showClock = false,
                              .clock24Hour = true,
                              .showWatchedIndicators = true,
                              .uiTextSize = 1,
                              .stillWatchingPrompt = false,
                              .overlayOpen = false,
                          });

    assert(episodeHarness.renderer.rects.empty());
    assert(episodeHarness.renderer.horizontalGradients.size() == 1);
    assert(episodeHarness.renderer.verticalGradients.size() == 1);
    assert(episodeHarness.renderer.horizontalGradients.front().color == 9);
    assert(episodeHarness.renderer.horizontalGradientEnd == 10);
    assert(episodeHarness.renderer.verticalGradients.front().color == 11);
    assert(episodeHarness.renderer.verticalGradientEnd == 12);
    assert(containsText(episodeHarness.renderer.texts, "S1E2  |  Pilot"));
    assert(containsText(episodeHarness.renderer.texts, "Show & seasons"));
    assert(contains(episodeHarness.centered, "GO TO SHOW"));
    assert(contains(episodeHarness.centered, "Season One"));
    assert(episodeHarness.artworkCalls == 0);
    assert(episodeHarness.placeholderCalls == 0);

    int focusedContextButtons = 0;
    for (std::size_t index = episodeActions.size(); index < episodeHarness.buttonFocus.size(); ++index)
        if (episodeHarness.buttonFocus[index]) ++focusedContextButtons;
    assert(focusedContextButtons == 1);

    return 0;
}
