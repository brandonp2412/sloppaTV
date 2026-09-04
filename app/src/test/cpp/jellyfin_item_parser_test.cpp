#include "jellyfin_item_parser.hpp"

#include <nlohmann/json.hpp>

#include <cassert>

int main() {
    const nlohmann::json value = {
        {"Id", "episode-1"},
        {"Name", "Pilot"},
        {"Type", "Episode"},
        {"SeriesId", "series-1"},
        {"SeriesName", "Series"},
        {"SeriesPrimaryImageTag", "series-primary"},
        {"SeasonName", "Season 1"},
        {"Overview", "Overview"},
        {"OfficialRating", "TV-14"},
        {"ProductionYear", 2026},
        {"CommunityRating", 8.5},
        {"IndexNumber", 1},
        {"ParentIndexNumber", 1},
        {"RunTimeTicks", 123456789LL},
        {"CanDelete", true},
        {"ProviderIds", {{"Tmdb", "42"}, {"TmdbCollection", "84"}}},
        {"Genres", {"Comedy", "Crime"}},
        {"People", {
            {{"Type", "Director"}, {"Name", "Ignored"}},
            {{"Type", "Actor"}, {"Id", "actor-1"}, {"Name", "Actor"}, {"PrimaryImageTag", "actor-tag"}, {"Role", "Lead"}}
        }},
        {"UserData", {{"PlaybackPositionTicks", 25000000LL}, {"IsFavorite", true}, {"Played", false}}},
        {"ImageTags", {{"Primary", "primary-tag"}, {"Thumb", "thumb-tag"}, {"Logo", "logo-tag"}}},
        {"BackdropImageTags", {"backdrop-tag"}},
        {"MediaSources", {{
            {"Id", "media-source"},
            {"Container", "mkv"},
            {"MediaStreams", {
                {{"Type", "Video"}, {"Codec", "hevc"}, {"Profile", "Main 10"}, {"VideoRangeType", "HDR10"}, {"Width", 3840}, {"Height", 2160}, {"BitDepth", 10}, {"Level", 153}, {"RealFrameRate", 23.976}},
                {{"Type", "Audio"}, {"Index", 1}, {"Channels", 6}, {"Codec", "eac3"}, {"Language", "eng"}, {"DisplayTitle", "English"}, {"IsDefault", true}},
                {{"Type", "Subtitle"}, {"Index", 2}, {"Codec", "subrip"}, {"Language", "eng"}, {"DisplayTitle", "English"}, {"IsForced", false}, {"IsDefault", true}, {"IsExternal", true}}
            }}
        }}},
        {"Trickplay", {{"media-source", {
            {"640", {{"Width", 640}, {"Height", 360}, {"TileWidth", 10}, {"TileHeight", 10}, {"ThumbnailCount", 100}, {"Interval", 10000}}},
            {"320", {{"Width", 320}, {"Height", 180}, {"TileWidth", 10}, {"TileHeight", 10}, {"ThumbnailCount", 100}, {"Interval", 10000}}}
        }}}}
    };

    const JellyfinItem item = parseJellyfinItem(value);
    assert(item.id == "episode-1");
    assert(item.name == "Pilot");
    assert(item.seriesId == "series-1");
    assert(item.tmdbId == "42");
    assert(item.genres.size() == 2);
    assert(item.people.size() == 1 && item.people[0].name == "Actor");
    assert(item.cast.size() == 1 && item.cast[0] == "Actor");
    assert(item.positionTicks == 25000000LL);
    assert(item.favorite && !item.played);
    assert(item.imageTag == "primary-tag");
    assert(item.logoItemId == "episode-1");
    assert(item.backdropItemId == "episode-1");
    assert(item.mediaSourceId == "media-source");
    assert(item.container == "mkv");
    assert(item.videoCodec == "hevc");
    assert(item.videoWidth == 3840 && item.videoHeight == 2160);
    assert(item.videoBitDepth == 10 && item.videoLevel == 153);
    assert(item.audios.size() == 1 && item.audios[0].index == 1);
    assert(item.subtitles.size() == 1 && item.subtitles[0].isExternal);
    assert(item.trickplay.valid());
    assert(item.trickplay.mediaSourceId == "media-source");
    assert(item.trickplay.width == 320);

    const nlohmann::json parentArtwork = {
        {"Id", "episode-2"},
        {"Name", "Episode 2"},
        {"Type", "Episode"},
        {"ParentLogoImageTag", "parent-logo"},
        {"ParentLogoItemId", "series-1"},
        {"ParentBackdropImageTags", {"parent-backdrop"}},
        {"ParentBackdropItemId", "series-1"}
    };
    const JellyfinItem inherited = parseJellyfinItem(parentArtwork);
    assert(inherited.logoTag == "parent-logo" && inherited.logoItemId == "series-1");
    assert(inherited.backdropTag == "parent-backdrop" && inherited.backdropItemId == "series-1");

    const nlohmann::json secondValid = {
        {"Id", "movie-2"},
        {"Name", "Second"},
        {"Type", "Movie"}
    };
    const nlohmann::json malformed = {
        {"Id", "broken"},
        {"ProductionYear", "not-a-number"}
    };
    const auto items = parseJellyfinItems(nlohmann::json::array({
        value,
        nullptr,
        {{"Name", "Missing Id"}},
        malformed,
        secondValid
    }));
    assert(items.size() == 2);
    assert(items[0].id == "episode-1");
    assert(items[1].id == "movie-2");
    assert(parseJellyfinItems(nlohmann::json::object()).empty());
    return 0;
}
