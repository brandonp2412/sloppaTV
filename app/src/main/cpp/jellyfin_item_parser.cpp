#include "jellyfin_item_parser.hpp"

#include <nlohmann/json.hpp>

using nlohmann::json;

namespace {
template <typename T>
T valueOr(const json& value, const char* key, T fallback) {
    if (!value.is_object()) return fallback;
    const auto match = value.find(key);
    if (match == value.end() || match->is_null()) return fallback;
    try {
        return match->get<T>();
    } catch (const json::exception&) {
        return fallback;
    }
}

std::string stringValueOrFallback(const json& value, const char* primary, const char* fallback) {
    const std::string primaryValue = valueOr<std::string>(value, primary, {});
    return primaryValue.empty() ? valueOr<std::string>(value, fallback, {}) : primaryValue;
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
    item.id = valueOr<std::string>(value, "Id", {});
    item.name = valueOr<std::string>(value, "Name", {});
    item.type = valueOr<std::string>(value, "Type", {});
    item.collectionType = valueOr<std::string>(value, "CollectionType", {});
    item.seriesId = valueOr<std::string>(value, "SeriesId", {});
    item.seriesName = valueOr<std::string>(value, "SeriesName", {});
    item.seriesPrimaryImageTag = valueOr<std::string>(value, "SeriesPrimaryImageTag", {});
    item.seasonName = valueOr<std::string>(value, "SeasonName", {});
    item.overview = valueOr<std::string>(value, "Overview", {});
    item.container = valueOr<std::string>(value, "Container", {});
    item.officialRating = valueOr<std::string>(value, "OfficialRating", {});
    item.productionYear = valueOr(value, "ProductionYear", 0);
    if (const auto rating = value.find("CommunityRating"); rating != value.end() && rating->is_number()) {
        item.communityRating = rating->get<float>();
    }
    item.indexNumber = valueOr(value, "IndexNumber", -1);
    item.parentIndexNumber = valueOr(value, "ParentIndexNumber", -1);
    item.runtimeTicks = valueOr(value, "RunTimeTicks", static_cast<int64_t>(0));
    item.canDelete = valueOr(value, "CanDelete", false);

    if (const auto providerIds = value.find("ProviderIds"); providerIds != value.end() && providerIds->is_object()) {
        item.tmdbId = valueOr<std::string>(*providerIds, "Tmdb", {});
        item.tmdbCollectionId = valueOr<std::string>(*providerIds, "TmdbCollection", {});
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
            if (!person.is_object() || valueOr<std::string>(person, "Type", {}) != "Actor") continue;
            JellyfinPerson parsed;
            parsed.id = valueOr<std::string>(person, "Id", {});
            parsed.name = valueOr<std::string>(person, "Name", {});
            parsed.imageTag = valueOr<std::string>(person, "PrimaryImageTag", {});
            parsed.role = valueOr<std::string>(person, "Role", {});
            if (parsed.name.empty()) continue;
            item.cast.push_back(parsed.name);
            item.people.push_back(std::move(parsed));
            if (item.people.size() >= 12) break;
        }
    }
    if (const auto userData = value.find("UserData"); userData != value.end() && userData->is_object()) {
        item.positionTicks = valueOr(*userData, "PlaybackPositionTicks", static_cast<int64_t>(0));
        item.favorite = valueOr(*userData, "IsFavorite", false);
        item.played = valueOr(*userData, "Played", false);
    }
    if (const auto imageTags = value.find("ImageTags"); imageTags != value.end() && imageTags->is_object()) {
        item.imageTag = valueOr<std::string>(*imageTags, "Primary", {});
        item.thumbTag = valueOr<std::string>(*imageTags, "Thumb", {});
        item.logoTag = valueOr<std::string>(*imageTags, "Logo", {});
        if (!item.logoTag.empty()) item.logoItemId = item.id;
    }
    if (item.logoTag.empty()) {
        item.logoTag = valueOr<std::string>(value, "ParentLogoImageTag", {});
        item.logoItemId = valueOr<std::string>(value, "ParentLogoItemId", {});
    }
    if (const auto backdrops = value.find("BackdropImageTags"); backdrops != value.end() && backdrops->is_array()) {
        item.backdropTag = firstStringValue(*backdrops);
        if (!item.backdropTag.empty()) item.backdropItemId = item.id;
    }
    if (item.backdropTag.empty()) {
        const auto backdrops = value.find("ParentBackdropImageTags");
        if (backdrops != value.end() && backdrops->is_array()) {
            item.backdropTag = firstStringValue(*backdrops);
            if (!item.backdropTag.empty()) item.backdropItemId = valueOr<std::string>(value, "ParentBackdropItemId", {});
        }
    }
    if (const auto mediaSources = value.find("MediaSources"); mediaSources != value.end() && mediaSources->is_array()) {
        const json* source = firstObjectValue(*mediaSources);
        if (source) {
            item.mediaSourceId = valueOr<std::string>(*source, "Id", {});
            if (item.container.empty()) item.container = valueOr<std::string>(*source, "Container", {});
            if (const auto streams = source->find("MediaStreams"); streams != source->end() && streams->is_array()) {
                item.audios.reserve(streams->size());
                item.subtitles.reserve(streams->size());
                for (const auto& stream : *streams) {
                    if (!stream.is_object()) continue;
                    const std::string streamType = valueOr<std::string>(stream, "Type", {});
                    if (streamType == "Video" && item.videoCodec.empty()) {
                        item.videoCodec = valueOr<std::string>(stream, "Codec", {});
                        item.videoProfile = valueOr<std::string>(stream, "Profile", {});
                        item.videoRangeType = valueOr<std::string>(stream, "VideoRangeType", {});
                        item.videoWidth = valueOr(stream, "Width", 0);
                        item.videoHeight = valueOr(stream, "Height", 0);
                        item.videoBitDepth = valueOr(stream, "BitDepth", 0);
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
                        audio.index = valueOr(stream, "Index", -1);
                        audio.channels = valueOr(stream, "Channels", 0);
                        audio.codec = valueOr<std::string>(stream, "Codec", {});
                        audio.language = valueOr<std::string>(stream, "Language", {});
                        audio.title = stringValueOrFallback(stream, "DisplayTitle", "Title");
                        audio.isDefault = valueOr(stream, "IsDefault", false);
                        if (audio.index >= 0) item.audios.push_back(std::move(audio));
                    } else if (streamType == "Subtitle") {
                        JellyfinSubtitleStream subtitle;
                        subtitle.index = valueOr(stream, "Index", -1);
                        subtitle.codec = valueOr<std::string>(stream, "Codec", {});
                        subtitle.language = valueOr<std::string>(stream, "Language", {});
                        subtitle.title = stringValueOrFallback(stream, "DisplayTitle", "Title");
                        subtitle.forced = valueOr(stream, "IsForced", false);
                        subtitle.isDefault = valueOr(stream, "IsDefault", false);
                        subtitle.isExternal = valueOr(stream, "IsExternal", false);
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
                candidate.width = valueOr(valueInfo, "Width", 0);
                candidate.height = valueOr(valueInfo, "Height", 0);
                candidate.tileWidth = valueOr(valueInfo, "TileWidth", 0);
                candidate.tileHeight = valueOr(valueInfo, "TileHeight", 0);
                candidate.thumbnailCount = valueOr(valueInfo, "ThumbnailCount", 0);
                candidate.intervalMs = valueOr(valueInfo, "Interval", 0);
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

std::vector<JellyfinMediaSegment> parseJellyfinMediaSegments(const json& values) {
    std::vector<JellyfinMediaSegment> segments;
    if (!values.is_array()) return segments;
    segments.reserve(values.size());
    for (const auto& value : values) {
        if (!value.is_object()) continue;
        const auto type = value.find("Type");
        const auto start = value.find("StartTicks");
        const auto end = value.find("EndTicks");
        if (type == value.end() || !type->is_string()
            || start == value.end() || !start->is_number_integer()
            || end == value.end() || !end->is_number_integer()) {
            continue;
        }
        JellyfinMediaSegment segment;
        try {
            segment.type = type->get<std::string>();
            segment.startTicks = start->get<int64_t>();
            segment.endTicks = end->get<int64_t>();
        } catch (const json::exception&) {
            continue;
        }
        if (!segment.type.empty() && segment.endTicks > segment.startTicks) {
            segments.push_back(std::move(segment));
        }
    }
    std::sort(segments.begin(), segments.end(), [](const auto& left, const auto& right) {
        return left.startTicks < right.startTicks;
    });
    return segments;
}
