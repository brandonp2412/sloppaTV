#pragma once

#include "jellyfin_types.hpp"
#include "seerr_media.hpp"

#include <optional>
#include <string>

struct SeerrDeleteRequest {
    std::string itemId;
    int requestId = 0;
};

inline bool isSeerrItem(const JellyfinItem& item) {
    return item.externalSource == "seerr";
}

inline JellyfinItem jellyfinItemFromSeerrMedia(const SeerrMediaItem& media) {
    JellyfinItem item;
    item.id = media.id;
    item.name = media.name;
    item.type = media.television() ? "Series" : "Movie";
    item.overview = media.overview;
    item.tmdbId = media.tmdbId > 0 ? std::to_string(media.tmdbId) : std::string{};
    item.productionYear = media.productionYear;
    item.externalSource = "seerr";
    item.externalMediaType = media.mediaType;
    item.externalPosterUrl = media.posterUrl;
    item.externalBackdropUrl = media.backdropUrl;
    item.externalJellyfinId = media.jellyfinId;
    item.externalStatus = media.status;
    item.externalProgressLabel = media.progressLabel;
    item.externalProgressEta = media.progressEta;
    item.externalProgressPercent = media.progressPercent;
    item.externalRequestId = media.requestId;
    item.externalMediaStatus = media.mediaStatus;
    item.externalRequested = media.requested;
    item.externalAvailable = media.available;
    return item;
}

inline std::optional<SeerrDeleteRequest> seerrDeleteRequestFromJellyfinItem(const JellyfinItem& item) {
    if (!isSeerrItem(item) || item.externalRequestId <= 0) return std::nullopt;
    return SeerrDeleteRequest{
        .itemId = item.id,
        .requestId = item.externalRequestId,
    };
}

inline std::optional<SeerrMediaItem> seerrMediaFromJellyfinItem(const JellyfinItem& item) {
    if (!isSeerrItem(item)) return std::nullopt;
    int tmdbId = 0;
    try {
        tmdbId = std::stoi(item.tmdbId);
    } catch (...) {
        return std::nullopt;
    }
    SeerrMediaItem media;
    media.id = item.id;
    media.name = item.name;
    media.mediaType = item.externalMediaType;
    media.tmdbId = tmdbId;
    media.overview = item.overview;
    media.posterUrl = item.externalPosterUrl;
    media.backdropUrl = item.externalBackdropUrl;
    media.jellyfinId = item.externalJellyfinId;
    media.status = item.externalStatus;
    media.progressLabel = item.externalProgressLabel;
    media.progressEta = item.externalProgressEta;
    media.productionYear = item.productionYear;
    media.progressPercent = item.externalProgressPercent;
    media.requestId = item.externalRequestId;
    media.mediaStatus = item.externalMediaStatus;
    media.requested = item.externalRequested;
    media.available = item.externalAvailable;
    if (!media.valid()) return std::nullopt;
    return media;
}
