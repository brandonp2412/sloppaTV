#include "item_menu_renderer.hpp"

#include <array>
#include <cassert>
#include <string>
#include <string_view>
#include <vector>

namespace {
struct FakeRenderer {
    void rect(float, float, float, float, int color) { rectColors.push_back(color); }
    void text(float, float, float, std::string_view value, int color, float) {
        texts.emplace_back(value);
        textColors.push_back(color);
    }
    void textVerticallyCentered(float, float, float, float, std::string_view value, int color, float) {
        verticalTexts.emplace_back(value);
        verticalColors.push_back(color);
    }
    float textWidth(float scale, std::string_view value) const {
        return scale * static_cast<float>(value.size()) * 5.0f;
    }
    std::vector<int> rectColors;
    std::vector<std::string> texts;
    std::vector<int> textColors;
    std::vector<std::string> verticalTexts;
    std::vector<int> verticalColors;
};

struct Surface {
    bool focused = false;
    bool primary = false;
    bool destructive = false;
};

struct Harness {
    FakeRenderer renderer;
    std::vector<float> modalHeights;
    std::vector<Surface> buttons;
    std::vector<Surface> surfaces;
    std::vector<std::string> centered;
    std::vector<std::string> chips;

    void render(const ItemMenuRenderState& state, const std::vector<std::string>& actions,
                const ItemMenuRenderStyle<int>& style) {
        renderItemMenuScreen(
            renderer, 1920.0f, 1080.0f, state, actions, style,
            [&](float, float, float, float height) { modalHeights.push_back(height); },
            [&](float x, float y, float width, float height, bool focused, bool primary, bool destructive) {
                buttons.push_back({focused, primary, destructive});
                return std::array<float, 4>{x, y, width, height};
            },
            [&](float, float, float, float, float, std::string_view value, int, float, float) {
                centered.emplace_back(value);
            },
            [](std::string_view value, float, float, int) { return std::string(value); },
            [&](float, float, std::string_view label, bool, float, float, float) {
                chips.emplace_back(label);
                return 100.0f;
            },
            [&](float x, float y, float width, float height, bool focused, bool primary, bool destructive) {
                surfaces.push_back({focused, primary, destructive});
                return std::array<float, 4>{x, y, width, height};
            },
            [](std::string_view value) { return value; });
    }
};

bool has(const std::vector<std::string>& values, std::string_view expected) {
    for (const auto& value : values)
        if (value == expected) return true;
    return false;
}
} // namespace

int main() {
    const ItemMenuRenderStyle<int> style{
        .cornerLarge = 28.0f, .scrim = 1, .error = 2, .text = 3, .muted = 4,
        .tertiary = 5, .focus = 6, .secondaryText = 7, .divider = 8,
    };

    Harness confirmation;
    confirmation.render(
        ItemMenuRenderState{
            .deleteConfirmation = true, .deleteConfirmationSelection = 0, .itemMenuSelection = 0,
            .seerrRequest = true, .itemName = "Requested Show", .itemType = "Series",
            .externalStatus = "Downloading", .externalProgressPercent = 42,
            .externalProgressLabel = "Episode 2 of 8", .externalProgressEta = "18 min",
        },
        {}, style);
    assert(confirmation.renderer.rectColors.size() == 1);
    assert(confirmation.renderer.rectColors[0] == style.scrim);
    assert(confirmation.modalHeights.size() == 1 && confirmation.modalHeights[0] == 520.0f);
    assert(has(confirmation.renderer.texts, "Delete this request?"));
    assert(confirmation.buttons.size() == 2);
    assert(confirmation.buttons[0].focused && confirmation.buttons[0].destructive);
    assert(has(confirmation.centered, "Delete request"));
    assert(has(confirmation.centered, "Press Back to cancel"));

    Harness menu;
    const std::vector<std::string> actions{"MARK WATCHED", "DELETE REQUEST", "BACK"};
    menu.render(
        ItemMenuRenderState{
            .deleteConfirmation = false, .deleteConfirmationSelection = 1, .itemMenuSelection = 1,
            .seerrRequest = true, .itemName = "Requested Show", .itemType = "Series",
            .externalStatus = "Downloading", .externalProgressPercent = 42,
            .externalProgressLabel = "Episode 2 of 8", .externalProgressEta = "18 MIN",
        },
        actions, style);
    assert(menu.modalHeights.size() == 1 && menu.modalHeights[0] == 438.0f);
    assert(has(menu.renderer.texts, "Requested Show"));
    assert(has(menu.renderer.texts, "Episode 2 of 8"));
    assert(has(menu.renderer.verticalTexts, "42%"));
    assert(menu.chips.size() == 1 && menu.chips[0] == "18 MIN");
    assert(menu.surfaces.size() == actions.size());
    assert(menu.surfaces[1].focused && menu.surfaces[1].destructive && !menu.surfaces[1].primary);
    assert(has(menu.renderer.verticalTexts, "DELETE REQUEST"));
    assert(has(menu.centered, "OK selects   |   Back closes"));

    Harness fallback;
    fallback.render(
        ItemMenuRenderState{
            .deleteConfirmation = false, .deleteConfirmationSelection = 1, .itemMenuSelection = 0,
            .seerrRequest = false, .itemName = {}, .itemType = {}, .externalStatus = {},
            .externalProgressPercent = -1, .externalProgressLabel = {}, .externalProgressEta = {},
        },
        {"BACK"}, style);
    assert(fallback.modalHeights.size() == 1 && fallback.modalHeights[0] == 282.0f);
    assert(has(fallback.renderer.texts, "Item"));
    assert(has(fallback.renderer.texts, "Media"));
    assert(fallback.surfaces.size() == 1 && fallback.surfaces[0].focused && fallback.surfaces[0].primary);

    return 0;
}
