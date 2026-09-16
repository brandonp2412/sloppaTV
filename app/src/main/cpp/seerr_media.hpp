#pragma once

#include <string>

struct SeerrMediaItem {
    std::string id;
    std::string name;
    std::string mediaType;
    int tmdbId = 0;
    std::string overview;
    std::string posterUrl;
    std::string backdropUrl;
    std::string jellyfinId;
    std::string status;
    std::string progressLabel;
    std::string progressEta;
    int productionYear = 0;
    int progressPercent = -1;
    int requestId = 0;
    int mediaStatus = 0;
    bool requested = false;
    bool available = false;

    [[nodiscard]] bool valid() const {
        return !id.empty() && tmdbId > 0 && (mediaType == "movie" || mediaType == "tv");
    }

    [[nodiscard]] bool television() const { return mediaType == "tv"; }
};

inline std::string seerrMediaId(const std::string& mediaType, int tmdbId) {
    if ((mediaType != "movie" && mediaType != "tv") || tmdbId <= 0) return {};
    return "seerr:" + mediaType + ":" + std::to_string(tmdbId);
}
