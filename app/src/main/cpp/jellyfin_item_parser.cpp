#include "jellyfin_item_parser.hpp"

#include <nlohmann/json.hpp>

using nlohmann::json;

JellyfinItem parseJellyfinItem(const json& value) {
    JellyfinItem item;
    item.id = value.value("Id", std::string{});
    item.name = value.value("Name", std::string{});
    item.type = value.value("Type", std::string{});
    item.collectionType = value.value("CollectionType", std::string{});
    item.seriesId = value.value("SeriesId", std::string{});
    item.seriesName = value.value("SeriesName", std::string{});
    item.seriesPrimaryImageTag = value.value("SeriesPrimaryImageTag", std::string{});
    item.seasonName = value.value("SeasonName", std::string{});
    item.overview = value.value("Overview", std::string{});
    item.container = value.value("Container", std::string{});
    item.officialRating = value.value("OfficialRating", std::string{});
    item.productionYear = value.value("ProductionYear", 0);
    if (value.contains("CommunityRating") && value["CommunityRating"].is_number()) {
        item.communityRating = value["CommunityRating"].get<float>();
    }
    item.indexNumber = value.value("IndexNumber", -1);
    item.parentIndexNumber = value.value("ParentIndexNumber", -1);
    item.runtimeTicks = value.value("RunTimeTicks", static_cast<int64_t>(0));
    item.canDelete = value.value("CanDelete", false);

    if (value.contains("ProviderIds") && value["ProviderIds"].is_object()) {
        item.tmdbId = value["ProviderIds"].value("Tmdb", std::string{});
        item.tmdbCollectionId = value["ProviderIds"].value("TmdbCollection", std::string{});
    }
    if (value.contains("Genres") && value["Genres"].is_array()) {
        for (const auto& genre : value["Genres"]) {
            if (genre.is_string()) item.genres.push_back(genre.get<std::string>());
        }
    }
    if (value.contains("People") && value["People"].is_array()) {
        for (const auto& person : value["People"]) {
            if (!person.is_object() || person.value("Type", std::string{}) != "Actor") continue;
            JellyfinPerson parsed;
            parsed.id = person.value("Id", std::string{});
            parsed.name = person.value("Name", std::string{});
            parsed.imageTag = person.value("PrimaryImageTag", std::string{});
            parsed.role = person.value("Role", std::string{});
            if (parsed.name.empty()) continue;
            item.cast.push_back(parsed.name);
            item.people.push_back(std::move(parsed));
            if (item.people.size() >= 12) break;
        }
    }
    if (value.contains("UserData") && value["UserData"].is_object()) {
        item.positionTicks = value["UserData"].value("PlaybackPositionTicks", static_cast<int64_t>(0));
        item.favorite = value["UserData"].value("IsFavorite", false);
        item.played = value["UserData"].value("Played", false);
    }
    if (value.contains("ImageTags") && value["ImageTags"].is_object()) {
        item.imageTag = value["ImageTags"].value("Primary", std::string{});
        item.thumbTag = value["ImageTags"].value("Thumb", std::string{});
        item.logoTag = value["ImageTags"].value("Logo", std::string{});
        if (!item.logoTag.empty()) item.logoItemId = item.id;
    }
    if (item.logoTag.empty()) {
        item.logoTag = value.value("ParentLogoImageTag", std::string{});
        item.logoItemId = value.value("ParentLogoItemId", std::string{});
    }
    if (value.contains("BackdropImageTags") && value["BackdropImageTags"].is_array() && !value["BackdropImageTags"].empty()) {
        item.backdropTag = value["BackdropImageTags"][0].get<std::string>();
        item.backdropItemId = item.id;
    }
    if (item.backdropTag.empty() && value.contains("ParentBackdropImageTags")
        && value["ParentBackdropImageTags"].is_array() && !value["ParentBackdropImageTags"].empty()) {
        item.backdropTag = value["ParentBackdropImageTags"][0].get<std::string>();
        item.backdropItemId = value.value("ParentBackdropItemId", std::string{});
    }
    if (value.contains("MediaSources") && value["MediaSources"].is_array() && !value["MediaSources"].empty()) {
        const auto& source = value["MediaSources"][0];
        item.mediaSourceId = source.value("Id", std::string{});
        if (item.container.empty()) item.container = source.value("Container", std::string{});
        if (source.contains("MediaStreams") && source["MediaStreams"].is_array()) {
            for (const auto& stream : source["MediaStreams"]) {
                if (!stream.is_object()) continue;
                const std::string streamType = stream.value("Type", std::string{});
                if (streamType == "Video" && item.videoCodec.empty()) {
                    item.videoCodec = stream.value("Codec", std::string{});
                    item.videoProfile = stream.value("Profile", std::string{});
                    item.videoRangeType = stream.value("VideoRangeType", std::string{});
                    item.videoWidth = stream.value("Width", 0);
                    item.videoHeight = stream.value("Height", 0);
                    item.videoBitDepth = stream.value("BitDepth", 0);
                    if (stream.contains("Level") && stream["Level"].is_number_integer()) {
                        item.videoLevel = stream["Level"].get<int>();
                    }
                    if (stream.contains("RealFrameRate") && stream["RealFrameRate"].is_number()) {
                        item.videoFrameRate = stream["RealFrameRate"].get<float>();
                    } else if (stream.contains("AverageFrameRate") && stream["AverageFrameRate"].is_number()) {
                        item.videoFrameRate = stream["AverageFrameRate"].get<float>();
                    }
                } else if (streamType == "Audio") {
                    JellyfinAudioStream audio;
                    audio.index = stream.value("Index", -1);
                    audio.channels = stream.value("Channels", 0);
                    audio.codec = stream.value("Codec", std::string{});
                    audio.language = stream.value("Language", std::string{});
                    audio.title = stream.value("DisplayTitle", stream.value("Title", std::string{}));
                    audio.isDefault = stream.value("IsDefault", false);
                    if (audio.index >= 0) item.audios.push_back(std::move(audio));
                } else if (streamType == "Subtitle") {
                    JellyfinSubtitleStream subtitle;
                    subtitle.index = stream.value("Index", -1);
                    subtitle.codec = stream.value("Codec", std::string{});
                    subtitle.language = stream.value("Language", std::string{});
                    subtitle.title = stream.value("DisplayTitle", stream.value("Title", std::string{}));
                    subtitle.forced = stream.value("IsForced", false);
                    subtitle.isDefault = stream.value("IsDefault", false);
                    subtitle.isExternal = stream.value("IsExternal", false);
                    if (subtitle.index >= 0) item.subtitles.push_back(std::move(subtitle));
                }
            }
        }
    }
    if (value.contains("Trickplay") && value["Trickplay"].is_object()) {
        const auto& trickplay = value["Trickplay"];
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
            // A malformed library entry should not discard the rest of a valid page.
        }
    }
    return items;
}
