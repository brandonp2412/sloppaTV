#include "jellyfin_item_parser.hpp"

#include <nlohmann/json.hpp>

#include <string_view>

using nlohmann::json;

namespace {
std::string stringValue(const json& value, const char* key) {
    const auto match = value.find(key);
    return match == value.end() ? std::string{} : match->get<std::string>();
}

std::string_view stringView(const json& value, const char* key) {
    const auto match = value.find(key);
    return match != value.end() && match->is_string()
        ? std::string_view(match->get_ref<const std::string&>())
        : std::string_view{};
}

std::string stringValueOrFallback(const json& value, const char* primary, const char* fallback) {
    if (const auto match = value.find(primary); match != value.end() && match->is_string()) {
        return match->get<std::string>();
    }
    return value.value(fallback, std::string{});
}
}

JellyfinItem parseJellyfinItem(const json& value) {
    JellyfinItem item;
    item.id = stringValue(value, "Id");
    item.name = stringValue(value, "Name");
    item.type = stringValue(value, "Type");
    item.collectionType = stringValue(value, "CollectionType");
    item.seriesId = stringValue(value, "SeriesId");
    item.seriesName = stringValue(value, "SeriesName");
    item.seriesPrimaryImageTag = stringValue(value, "SeriesPrimaryImageTag");
    item.seasonName = stringValue(value, "SeasonName");
    item.overview = stringValue(value, "Overview");
    item.container = stringValue(value, "Container");
    item.officialRating = stringValue(value, "OfficialRating");
    item.productionYear = value.value("ProductionYear", 0);
    if (const auto rating = value.find("CommunityRating"); rating != value.end() && rating->is_number()) {
        item.communityRating = rating->get<float>();
    }
    item.indexNumber = value.value("IndexNumber", -1);
    item.parentIndexNumber = value.value("ParentIndexNumber", -1);
    item.runtimeTicks = value.value("RunTimeTicks", static_cast<int64_t>(0));
    item.canDelete = value.value("CanDelete", false);

    if (const auto providerIds = value.find("ProviderIds"); providerIds != value.end() && providerIds->is_object()) {
        item.tmdbId = stringValue(*providerIds, "Tmdb");
        item.tmdbCollectionId = stringValue(*providerIds, "TmdbCollection");
    }
    if (const auto genres = value.find("Genres"); genres != value.end() && genres->is_array()) {
        item.genres.reserve(genres->size());
        for (const auto& genre : *genres) {
            if (genre.is_string()) item.genres.push_back(genre.get<std::string>());
        }
    }
    if (const auto people = value.find("People"); people != value.end() && people->is_array()) {
        item.cast.reserve(std::min<size_t>(people->size(), 12));
        item.people.reserve(std::min<size_t>(people->size(), 12));
        for (const auto& person : *people) {
            if (!person.is_object() || stringView(person, "Type") != "Actor") continue;
            JellyfinPerson parsed;
            parsed.id = stringValue(person, "Id");
            parsed.name = stringValue(person, "Name");
            parsed.imageTag = stringValue(person, "PrimaryImageTag");
            parsed.role = stringValue(person, "Role");
            if (parsed.name.empty()) continue;
            item.cast.push_back(parsed.name);
            item.people.push_back(std::move(parsed));
            if (item.people.size() >= 12) break;
        }
    }
    if (const auto userData = value.find("UserData"); userData != value.end() && userData->is_object()) {
        item.positionTicks = userData->value("PlaybackPositionTicks", static_cast<int64_t>(0));
        item.favorite = userData->value("IsFavorite", false);
        item.played = userData->value("Played", false);
    }
    if (const auto imageTags = value.find("ImageTags"); imageTags != value.end() && imageTags->is_object()) {
        item.imageTag = stringValue(*imageTags, "Primary");
        item.thumbTag = stringValue(*imageTags, "Thumb");
        item.logoTag = stringValue(*imageTags, "Logo");
        if (!item.logoTag.empty()) item.logoItemId = item.id;
    }
    if (item.logoTag.empty()) {
        item.logoTag = stringValue(value, "ParentLogoImageTag");
        item.logoItemId = stringValue(value, "ParentLogoItemId");
    }
    if (const auto backdrops = value.find("BackdropImageTags"); backdrops != value.end() && backdrops->is_array() && !backdrops->empty()) {
        item.backdropTag = backdrops->front().get<std::string>();
        item.backdropItemId = item.id;
    }
    if (item.backdropTag.empty()) {
        const auto backdrops = value.find("ParentBackdropImageTags");
        if (backdrops != value.end() && backdrops->is_array() && !backdrops->empty()) {
            item.backdropTag = backdrops->front().get<std::string>();
            item.backdropItemId = stringValue(value, "ParentBackdropItemId");
        }
    }
    if (const auto mediaSources = value.find("MediaSources"); mediaSources != value.end() && mediaSources->is_array() && !mediaSources->empty()) {
        const auto& source = mediaSources->front();
        item.mediaSourceId = stringValue(source, "Id");
        if (item.container.empty()) item.container = stringValue(source, "Container");
        if (const auto streams = source.find("MediaStreams"); streams != source.end() && streams->is_array()) {
            item.audios.reserve(streams->size());
            item.subtitles.reserve(streams->size());
            for (const auto& stream : *streams) {
                if (!stream.is_object()) continue;
                const std::string_view streamType = stringView(stream, "Type");
                if (streamType == "Video" && item.videoCodec.empty()) {
                    item.videoCodec = stringValue(stream, "Codec");
                    item.videoProfile = stringValue(stream, "Profile");
                    item.videoRangeType = stringValue(stream, "VideoRangeType");
                    item.videoWidth = stream.value("Width", 0);
                    item.videoHeight = stream.value("Height", 0);
                    item.videoBitDepth = stream.value("BitDepth", 0);
                    if (const auto level = stream.find("Level"); level != stream.end() && level->is_number_integer()) {
                        item.videoLevel = level->get<int>();
                    }
                    if (const auto frameRate = stream.find("RealFrameRate"); frameRate != stream.end() && frameRate->is_number()) {
                        item.videoFrameRate = frameRate->get<float>();
                    } else if (const auto frameRate = stream.find("AverageFrameRate"); frameRate != stream.end() && frameRate->is_number()) {
                        item.videoFrameRate = frameRate->get<float>();
                    }
                } else if (streamType == "Audio") {
                    JellyfinAudioStream audio;
                    audio.index = stream.value("Index", -1);
                    audio.channels = stream.value("Channels", 0);
                    audio.codec = stringValue(stream, "Codec");
                    audio.language = stringValue(stream, "Language");
                    audio.title = stringValueOrFallback(stream, "DisplayTitle", "Title");
                    audio.isDefault = stream.value("IsDefault", false);
                    if (audio.index >= 0) item.audios.push_back(std::move(audio));
                } else if (streamType == "Subtitle") {
                    JellyfinSubtitleStream subtitle;
                    subtitle.index = stream.value("Index", -1);
                    subtitle.codec = stringValue(stream, "Codec");
                    subtitle.language = stringValue(stream, "Language");
                    subtitle.title = stringValueOrFallback(stream, "DisplayTitle", "Title");
                    subtitle.forced = stream.value("IsForced", false);
                    subtitle.isDefault = stream.value("IsDefault", false);
                    subtitle.isExternal = stream.value("IsExternal", false);
                    if (subtitle.index >= 0) item.subtitles.push_back(std::move(subtitle));
                }
            }
        }
    }
    if (const auto trickplayIt = value.find("Trickplay"); trickplayIt != value.end() && trickplayIt->is_object()) {
        const auto& trickplay = *trickplayIt;
        const json* resolutions = nullptr;
        std::string trickplaySourceId = item.mediaSourceId;
        if (!trickplaySourceId.empty() && trickplay.contains(trickplaySourceId) && trickplay[trickplaySourceId].is_object()) {
            resolutions = &trickplay[trickplaySourceId];
        } else {
            for (const auto& [sourceId, candidate] : trickplay.items()) {
                if (!candidate.is_object()) continue;
                trickplaySourceId = sourceId;
                resolutions = &candidate;
                break;
            }
        }
        if (resolutions) {
            for (const auto& [widthKey, valueInfo] : resolutions->items()) {
                (void)widthKey;
                if (!valueInfo.is_object()) continue;
                JellyfinTrickplayInfo candidate;
                candidate.mediaSourceId = trickplaySourceId;
                candidate.width = valueInfo.value("Width", 0);
                candidate.height = valueInfo.value("Height", 0);
                candidate.tileWidth = valueInfo.value("TileWidth", 0);
                candidate.tileHeight = valueInfo.value("TileHeight", 0);
                candidate.thumbnailCount = valueInfo.value("ThumbnailCount", 0);
                candidate.intervalMs = valueInfo.value("Interval", 0);
                if (!candidate.valid()) continue;
                if (!item.trickplay.valid() || candidate.width < item.trickplay.width) {
                    item.trickplay = std::move(candidate);
                }
            }
        }
    }
    return item;
}

std::vector<JellyfinItem> parseJellyfinItems(const json& values) {
    std::vector<JellyfinItem> items;
    if (!values.is_array()) return items;
    items.reserve(values.size());
    for (const auto& value : values) {
        if (!value.is_object()) continue;
        try {
            JellyfinItem item = parseJellyfinItem(value);
            if (!item.id.empty()) items.push_back(std::move(item));
        } catch (const json::exception&) {
        }
    }
    return items;
}
