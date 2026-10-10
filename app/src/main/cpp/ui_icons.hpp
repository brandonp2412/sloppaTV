#pragma once

#include <optional>
#include <string_view>
#include "ui_labels.hpp"

namespace ui_icons {
// Atlas order is shared with tools/generate_icon_atlas.py.
enum class Glyph {
    Audio,
    Subtitles,
    Previous,
    Next,
    Heart,
    Check,
    Play,
    Pause,
    Home,
    Search,
    Settings,
    User,
    Add,
    Trash,
    Back,
    Close,
    Grid,
    Tag,
    Folder,
    List,
    Refresh,
    Clock,
    Display,
    Server,
    Info,
    External,
    Login,
    More,
    Eye,
    Film,
    Download,
    Lock,
    Count
};

// Only app-owned action labels are matched; unknown labels retain their text layout.
inline std::optional<Glyph> forLabel(std::string_view label) {
    label = materialLabel(label);
    if (label.starts_with("Saved users (")) return Glyph::User;
    static constexpr std::pair<std::string_view, Glyph> labels[] = {
        {"Home", Glyph::Home},
        {"Search", Glyph::Search},
        {"Settings", Glyph::Settings},
        {"Favorites", Glyph::Heart},
        {"Favorite", Glyph::Heart},
        {"Unfavorite", Glyph::Heart},
        {"Watched", Glyph::Check},
        {"Mark watched", Glyph::Check},
        {"Mark unwatched", Glyph::Eye},
        {"Play", Glyph::Play},
        {"Play all", Glyph::Play},
        {"Resume", Glyph::Play},
        {"Keep watching", Glyph::Play},
        {"Play next", Glyph::Next},
        {"Play external", Glyph::External},
        {"Episodes", Glyph::List},
        {"View queue", Glyph::List},
        {"Cast", Glyph::User},
        {"More", Glyph::More},
        {"Back", Glyph::Back},
        {"Cancel", Glyph::Close},
        {"All", Glyph::Grid},
        {"Genres", Glyph::Tag},
        {"Collections", Glyph::Folder},
        {"Show on home", Glyph::Home},
        {"Hide from home", Glyph::Home},
        {"Refresh metadata", Glyph::Refresh},
        {"Delete media", Glyph::Trash},
        {"Delete request", Glyph::Trash},
        {"Delete permanently", Glyph::Trash},
        {"Forget", Glyph::Trash},
        {"Use", Glyph::Login},
        {"Log in", Glyph::Login},
        {"Quick Connect", Glyph::Login},
        {"Discover", Glyph::Search},
        {"Saved accounts", Glyph::User},
        {"Switch user", Glyph::User},
        {"Request", Glyph::Add},
        {"Request movie", Glyph::Add},
        {"Request series", Glyph::Add},
        {"Request seasons", Glyph::Add},
        {"Select all", Glyph::Check},
        {"Deselect all", Glyph::Close},
        {"Delete", Glyph::Back},
        {"Done", Glyph::Check},
        {"Clear", Glyph::Close},
        {"Retry", Glyph::Refresh},
        {"Connect with Jellyfin", Glyph::Login},
        {"Diagnostics", Glyph::Info},
        {"Advanced settings", Glyph::Settings},
        {"Show common", Glyph::Settings},
        {"Show technical", Glyph::Settings},
    };
    for (const auto& [text, glyph] : labels)
        if (label == text) return glyph;
    return std::nullopt;
}

inline Glyph forSetting(std::string_view label) {
    if (const auto icon = forLabel(label)) return *icon;
    if (label.find("SUBTITLE") != std::string_view::npos) return Glyph::Subtitles;
    if (label.find("AUDIO") != std::string_view::npos) return Glyph::Audio;
    if (label.find("SEERR") != std::string_view::npos) return Glyph::Server;
    if (label.find("CLOCK") != std::string_view::npos || label.find("TIME") != std::string_view::npos ||
        label.find("WATCHING") != std::string_view::npos)
        return Glyph::Clock;
    if (label.find("SKIP BACK") != std::string_view::npos) return Glyph::Previous;
    if (label.find("SKIP AHEAD") != std::string_view::npos) return Glyph::Next;
    if (label.find("WATCHED") != std::string_view::npos) return Glyph::Check;
    if (label.find("PLAY") != std::string_view::npos || label.find("BUFFER") != std::string_view::npos)
        return Glyph::Play;
    return Glyph::Display;
}

template <typename Renderer, typename Color> void audio(Renderer& r, float x, float y, float size, Color color) {
    r.icon(Glyph::Audio, x, y, size, color);
}

template <typename Renderer, typename Color> void subtitles(Renderer& r, float x, float y, float size, Color color) {
    r.icon(Glyph::Subtitles, x, y, size, color);
}

template <typename Renderer, typename Color>
void skip(Renderer& r, float x, float y, float size, bool forward, Color color) {
    r.icon(forward ? Glyph::Next : Glyph::Previous, x, y, size, color);
}

template <typename Renderer, typename Color> void heart(Renderer& r, float x, float y, float size, Color color) {
    r.icon(Glyph::Heart, x, y, size, color);
}

template <typename Renderer, typename Color> void check(Renderer& r, float x, float y, float size, Color color) {
    r.icon(Glyph::Check, x, y, size, color);
}
} // namespace ui_icons
