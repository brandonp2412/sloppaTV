#include "seerr_season_picker_renderer.hpp"

#include <cassert>
#include <string_view>

struct FakeUi {
    std::vector<std::string> texts;
    std::vector<float> rowPositions;
    int focusedRows = 0;

    void text(float, float, float, std::string_view value, int, float) { texts.emplace_back(value); }

    void textVerticallyCentered(float, float, float, float, std::string_view value, int, float) {
        texts.emplace_back(value);
    }

    std::string fitTextLines(std::string_view text, float, float, int) { return std::string(text); }

    void renderEmptyState(const std::string& title, const std::string& message) {
        texts.push_back(title);
        texts.push_back(message);
    }

    void drawListItemSurface(float, float y, float, float height, bool focused, float, float) {
        assert(y >= 190 && y + height < 960);
        rowPositions.push_back(y);
        if (focused) ++focusedRows;
    }

    void drawCenteredSingleLineFit(float, float, float, float, float, std::string_view value, int, float, float) {
        texts.emplace_back(value);
    }

    bool contains(std::string_view value) const {
        return std::any_of(texts.begin(), texts.end(),
                           [&](const auto& text) { return text.find(value) != std::string::npos; });
    }
};

int main() {
    SeerrSeasonPickerState state;
    SeerrMediaItem item;
    item.name = "A show with many seasons";
    state.open(item, std::nullopt);
    const SeerrSeasonPickerStyle<int> style;
    FakeUi loading;
    renderSeerrSeasonPicker(loading, loading, state, style);
    assert(loading.contains("Loading seasons") && loading.rowPositions.empty());

    state.complete(state.generation, {}, "HTTP 503");
    FakeUi error;
    renderSeerrSeasonPicker(error, error, state, style);
    assert(error.contains("HTTP 503") && error.contains("OK retries") && error.rowPositions.empty());

    state.open(item, std::nullopt);
    std::vector<SeerrSeason> seasons;
    for (int i = 1; i <= 20; ++i) {
        SeerrSeason season;
        season.number = i;
        season.name = "Season " + std::to_string(i);
        season.episodes = 10;
        seasons.push_back(season);
    }
    state.complete(state.generation, seasons, {});
    state.selection = 21;
    state.navigate(ScreenNavigationKey::Activate);
    FakeUi bottom;
    renderSeerrSeasonPicker(bottom, bottom, state, style);
    assert(bottom.contains("[x] Season 20"));
    assert(bottom.contains("16–20 / 20") && bottom.contains("Request 1 season"));
    assert(bottom.rowPositions.size() == 7 && bottom.focusedRows == 1);
    state.submitting = true;
    FakeUi sending;
    renderSeerrSeasonPicker(sending, sending, state, style);
    assert(sending.contains("Sending request"));
}
