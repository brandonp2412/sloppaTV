#pragma once

#include "seerr_seasons.hpp"

template <typename ColorLike> struct SeerrSeasonPickerStyle {
    float headlineScale = 0.0f;
    float cornerRadius = 0.0f;
    float focusScale = 1.0f;
    ColorLike text{};
    ColorLike muted{};
};

template <typename RendererLike, typename UiLike, typename ColorLike>
void renderSeerrSeasonPicker(RendererLike& renderer, UiLike& ui, const SeerrSeasonPickerState& state,
                             const SeerrSeasonPickerStyle<ColorLike>& style) {
    renderer.text(80.0f, 56.0f, style.headlineScale, "Choose seasons", style.text, 1500.0f);
    std::string subtitle = state.item.name;
    if (state.target) subtitle += " · " + state.target->serviceName + (state.target->is4k ? " · 4K" : "");
    renderer.text(82.0f, 125.0f, 1.55f, ui.fitTextLines(subtitle, 1.55f, 1720.0f, 1), style.muted, 1720.0f);
    if (state.loading || !state.error.empty() || state.seasons.empty()) {
        ui.renderEmptyState(state.loading ? "Loading seasons..."
                                          : (!state.error.empty() ? "Could not load seasons" : "No seasons available"),
                            state.loading ? "Back cancels."
                                          : (!state.error.empty()
                                                 ? state.error + " · OK retries · Back cancels"
                                                 : "This show has no seasons to request yet. Back returns to search."));
        return;
    }
    const auto selected = state.selected();
    const bool canRequest =
        std::any_of(state.seasons.begin(), state.seasons.end(), [](const auto& season) { return season.selectable(); });
    auto row = [&](float y, const std::string& title, const std::string& detail, bool focused, bool enabled) {
        ui.drawListItemSurface(120.0f, y, 1680.0f, 88.0f, focused, style.cornerRadius, style.focusScale);
        renderer.textVerticallyCentered(150.0f, y, 88.0f, 1.85f, ui.fitTextLines(title, 1.85f, 1030.0f, 1),
                                        enabled ? style.text : style.muted, 1030.0f);
        ui.drawCenteredSingleLineFit(1210.0f, y, 550.0f, 88.0f, 1.45f, detail, style.muted, 12.0f, 4.0f);
    };
    row(190.0f,
        state.submitting ? "Sending request..."
                         : (selected.empty() ? (canRequest ? "Select seasons below" : "No missing seasons to request")
                                             : "Request " + std::to_string(selected.size()) +
                                                   (selected.size() == 1 ? " season" : " seasons")),
        state.submitting ? "Please wait" : (selected.empty() ? "" : "OK confirms request"), state.selection == 0,
        !selected.empty() && !state.submitting);
    row(290.0f, state.allSelected() ? "Clear selection" : "Select all missing seasons",
        "Specials are selected separately", state.selection == 1, true);
    constexpr int visible = 5;
    const int count = static_cast<int>(state.seasons.size());
    const int first = std::clamp(state.selection - 4, 0, std::max(0, count - visible));
    for (int index = first; index < std::min(first + visible, count); ++index) {
        const auto& season = state.seasons[static_cast<size_t>(index)];
        std::string title = (season.selected ? "[x] " : "[ ] ") + season.name;
        if (!season.selectable()) title = season.name;
        std::string detail = std::to_string(season.episodes) + " episodes · " + season.statusLabel();
        if (!season.airDate.empty()) detail += " · " + season.airDate.substr(0, 4);
        row(400.0f + static_cast<float>(index - first) * 104.0f, title, detail, state.selection == index + 2,
            season.selectable());
    }
    ui.drawCenteredSingleLineFit(
        120.0f, 960.0f, 1680.0f, 48.0f, 1.45f,
        state.submitting ? "Request in progress · Back returns to search; your request will continue"
                         : "OK toggles · Left goes to Request · Back cancels   " + std::to_string(first + 1) + "–" +
                               std::to_string(std::min(first + visible, count)) + " / " + std::to_string(count),
        style.muted, 12.0f, 4.0f);
}
