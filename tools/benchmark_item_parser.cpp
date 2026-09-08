#include "jellyfin_item_parser.hpp"

#include <nlohmann/json.hpp>

#include <chrono>
#include <cstdlib>
#include <iomanip>
#include <iostream>

int main(int argc, char** argv) {
    const int iterations = argc > 1 ? std::atoi(argv[1]) : 100000;
    const nlohmann::json value = {
        {"Id", "episode-1"}, {"Name", "Pilot"}, {"Type", "Episode"}, {"SeriesId", "series-1"},
        {"Overview", "Representative overview"}, {"ProductionYear", 2026}, {"CommunityRating", 8.5},
        {"ProviderIds", {{"Tmdb", "42"}, {"TmdbCollection", "84"}}},
        {"Genres", {"Comedy", "Crime", "Drama"}},
        {"People", {{{"Type", "Actor"}, {"Id", "actor-1"}, {"Name", "Actor"}, {"PrimaryImageTag", "actor-tag"}, {"Role", "Lead"}}}},
        {"UserData", {{"PlaybackPositionTicks", 25000000LL}, {"IsFavorite", true}, {"Played", false}}},
        {"ImageTags", {{"Primary", "primary-tag"}, {"Thumb", "thumb-tag"}, {"Logo", "logo-tag"}}},
        {"BackdropImageTags", {"backdrop-tag"}},
        {"MediaSources", {{{"Id", "media-source"}, {"Container", "mkv"}, {"MediaStreams", {
            {{"Type", "Video"}, {"Codec", "hevc"}, {"Width", 3840}, {"Height", 2160}, {"BitDepth", 10}, {"RealFrameRate", 23.976}},
            {{"Type", "Audio"}, {"Index", 1}, {"Channels", 6}, {"Codec", "eac3"}, {"Language", "eng"}, {"DisplayTitle", "English"}},
            {{"Type", "Subtitle"}, {"Index", 2}, {"Codec", "subrip"}, {"Language", "eng"}, {"DisplayTitle", "English"}, {"IsExternal", true}}
        }}}}},
        {"Trickplay", {{"media-source", {{"320", {{"Width", 320}, {"Height", 180}, {"TileWidth", 10}, {"TileHeight", 10}, {"ThumbnailCount", 100}, {"Interval", 10000}}}}}}}
    };

    size_t checksum = 0;
    const auto started = std::chrono::steady_clock::now();
    for (int iteration = 0; iteration < iterations; ++iteration) {
        const auto item = parseJellyfinItem(value);
        checksum += item.audios.size() + item.subtitles.size() + item.genres.size() + item.people.size();
    }
    const double elapsedMs = std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - started).count();
    std::cout << std::fixed << std::setprecision(3) << "iterations=" << iterations << " elapsed_ms=" << elapsedMs << " checksum=" << checksum << '\n';
    return checksum == static_cast<size_t>(iterations) * 6 ? 0 : 1;
}
