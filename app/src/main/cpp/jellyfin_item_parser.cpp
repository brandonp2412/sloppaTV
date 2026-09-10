#include "jellyfin_item_parser.hpp"

#include <nlohmann/json.hpp>

using nlohmann::json;

namespace {
template <typename T>
T scalarValueOr(const json& value, const char* key, T fallback) {
    const auto match = value.find(key);
    if (match == value.end() || match->is_null()) return fallback;
    try {
        return match->get<T>();
    } catch (const json::exception&) {
        return fallback;
    }
}

std::string stringValueOrFallback(const json& value, const char* primary, const char* fallback) {
    if (const auto match = value.find(primary); match != value.end() && match->is_string()) {
        return match->get<std::string>();
    }
    return scalarValueOr(value, fallback, std::string{});
}

std::string firstStringValue(const json& values) {
    if (!values.is_array()) return {};
    for (const auto& value : values) {
        if (value.is_string()) return value.get<std::string>();
    }
    return {};
}

const json* firstObjectValue(const json& values) {
    if (!values.is_array()) return nullptr;
    for (const auto& value : values) {
        if (value.is_object()) return &value;
    }
    return nullptr;
}
}

JellyfinItem parseJellyfinItem(const json& value) {
    JellyfinItem item;
    item.id = scalarValueOr(value, "Id", std::string{});
    item.name = scalarValueOr(value, "Name", std::string{});
    item.type = scalarValueOr(value, "Type", std::string{});
    item.collectionType = scalarValueOr(value, "CollectionType", std::string{});
    item.seriesId = scalarValueOr(value, "SeriesId", std::string{});
    item.seriesName = scalarValueOr(value, "SeriesName", std::string{});
    item.seriesPrimaryImageTag = scalarValueOr(value, "SeriesPrimaryImageTag", std::string{});
    item.seasonName = scalarValueOr(value, "SeasonName", std::string{});
    item.overview = scalarValueOr(value, "Overview", std::string{});
    item.container = scalarValueOr(value, "Container", std::string{});
    item.officialRating = scalarValueOr(value, "OfficialRating", std::string{});
    item.productionYear = scalarValueOr(value, "ProductionYear", 0);
    if (const auto rating = value.find("CommunityRating"); rating != value.end() && rating->is_number()) {
        item.communityRating = rating->get<float>();
    }
    item.indexNumber = scalarValueOr(value, "IndexNumber", -1);
    item.parentIndexNumber = scalarValueOr(value, "ParentIndexNumber", -1);
    item.runtimeTicks = scalarValueOr(value, "RunTimeTicks", static_cast<int64_t>(0));
    item.canDelete = scalarValueOr(value, "CanDelete", false);

    if (const auto providerIds = value.find("ProviderIds"); providerIds != value.end() && providerIds->is_object()) {
        item.tmdbId = scalarValueOr(*providerIds, "Tmdb", std::string{});
        item.tmdbCollectionId = scalarValueOr(*providerIds, "TmdbCollection", std::string{});
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
            if (!person.is_object() || scalarValueOr(person, "Type", std::string{}) != "Actor") continue;
            JellyfinPerson parsed;
            parsed.id = scalarValueOr(person, "Id", std::string{});
            parsed.name = scalarValueOr(person, "Name", std::string{});
            parsed.imageTag = scalarValueOr(person, "PrimaryImageTag", std::string{});
            parsed.role = scalarValueOr(person, "Role", std::string{});
            if (parsed.name.empty()) continue;
            item.cast.push_back(parsed.name);
            item.people.push_back(std::move(parsed));
            if (item.people.size() >= 12) break;
        }
    }
    if (const auto userData = value.find("UserData"); userData != value.end() && userData->is_object()) {
        item.positionTicks = scalarValueOr(*userData, "PlaybackPositionTicks", static_cast<int64_t>(0));
        item.favorite = scalarValueOr(*userData, "IsFavorite", false);
        item.played = scalarValueOr(*userData, "Played", false);
    }
    if (const auto imageTags = value.find("ImageTags"); imageTags != value.end() && imageTags->is_object()) {
        item.imageTag = scalarValueOr(*imageTags, "Primary", std::string{});
        item.thumbTag = scalarValueOr(*imageTags, "Thumb", std::string{});
        item.logoTag = scalarValueOr(*imageTags, "Logo", std::string{});
        if (!item.logoTag.empty()) item.logoItemId = item.id;
    }
    if (item.logoTag.empty()) {
        item.logoTag = scalarValueOr(value, "ParentLogoImageTag", std::string{});
        item.logoItemId = scalarValueOr(value, "ParentLogoItemId", std::string{});
    }
    if (const auto backdrops = value.find("BackdropImageTags"); backdrops != value.end() && backdrops->is_array()) {
        item.backdropTag = firstStringValue(*backdrops);
        if (!item.backdropTag.empty()) item.backdropItemId = item.id;
    }
    if (item.backdropTag.empty()) {
        const auto backdrops = value.find("ParentBackdropImageTags");
        if (backdrops != value.end() && backdrops->is_array()) {
            item.backdropTag = firstStringValue(*backdrops);
            if (!item.backdropTag.empty()) item.backdropItemId = scalarValueOr(value, "ParentBackdropItemId", std::string{});
        }
    }
    if (const auto mediaSources = value.find("MediaSources"); mediaSources != value.end() && mediaSources->is_array()) {
        const json* source = firstObjectValue(*mediaSources);
        if (source) {
            item.mediaSourceId = scalarValueOr(*source, "Id", std::string{});
            if (item.container.empty()) item.container = scalarValueOr(*source, "Container", std::string{});
            if (const auto streams = source->find("MediaStreams"); streams != source->end() && streams->is_array()) {
                item.audios.reserve(streams->size());
                item.subtitles.reserve(streams->size());
                for (const auto& stream : *streams) {
                    if (!stream.is_object()) continue;
                    const std::string streamType = scalarValueOr(stream, "Type", std::string{});
                    if (streamType == "Video" && item.videoCodec.empty()) {
                        item.videoCodec = scalarValueOr(stream, "Codec", std::string{});
                        item.videoProfile = scalarValueOr(stream, "Profile", std::string{});
                        item.videoRangeType = scalarValueOr(stream, "VideoRangeType", std::string{});
                        item.videoWidth = scalarValueOr(stream, "Width", 0);
                        item.videoHeight = scalarValueOr(stream, "Height", 0);
                        item.videoBitDepth = scalarValueOr(stream, "BitDepth", 0);
                        item.videoLevel = scalarValueOr(stream, "Level", 0);
                        if (const auto frameRate = stream.find("RealFrameRate"); frameRate != stream.end() && frameRate->is_number()) {
                            item.videoFrameRate = scalarValueOr(stream, "RealFrameRate", 0.0f);
                        } else if (const auto frameRate = stream.find("AverageFrameRate"); frameRate != stream.end() && frameRate->is_number()) {
                            item.videoFrameRate = scalarValueOr(stream, "AverageFrameRate", 0.0f);
                        }
                    } else if (streamType == "Audio") {
                        JellyfinAudioStream audio;
                        audio.index = scalarValueOr(stream, "Index", -1);
                        audio.channels = scalarValueOr(stream, "Channels", 0);
                        audio.codec = scalarValueOr(stream, "Codec", std::string{});
                        audio.language = scalarValueOr(stream, "Language", std::string{});
                        audio.title = stringValueOrFallback(stream, "DisplayTitle", "Title");
                        audio.isDefault = scalarValueOr(stream, "IsDefault", false);
                        if (audio.index >= 0) item.audios.push_back(std::move(audio));
                    } else if (streamType == "Subtitle") {
                        JellyfinSubtitleStream subtitle;
                        subtitle.index = scalarValueOr(stream, "Index", -1);
                        subtitle.codec = scalarValueOr(stream, "Codec", std::string{});
                        subtitle.language = scalarValueOr(stream, "Language", std::string{});
                        subtitle.title = stringValueOrFallback(stream, "DisplayTitle", "Title");
                        subtitle.forced = scalarValueOr(stream, "IsForced", false);
                        subtitle.isDefault = scalarValueOr(stream, "IsDefault", false);
                        subtitle.isExternal = scalarValueOr(stream, "IsExternal", false);
                        if (subtitle.index >= 0) item.subtitles.push_back(std::move(subtitle));
                    }
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
                candidate.width = scalarValueOr(valueInfo, "Width", 0);
                candidate.height = scalarValueOr(valueInfo, "Height", 0);
                candidate.tileWidth = scalarValueOr(valueInfo, "TileWidth", 0);
                candidate.tileHeight = scalarValueOr(valueInfo, "TileHeight", 0);
                candidate.thumbnailCount = scalarValueOr(valueInfo, "ThumbnailCount", 0);
                candidate.intervalMs = scalarValueOr(valueInfo, "Interval", 0);
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
