#pragma once

#include "seerr_progress.hpp"

#include <cstddef>
#include <string>
#include <tuple>
#include <vector>

struct SeerrDownloadProgressEntry {
    double size = 0.0;
    double sizeLeft = 0.0;
    std::string timeLeft;
    int seasonNumber = -1;
    int episodeNumber = -1;
};

struct SeerrDownloadProgressSummary {
    int percent = -1;
    std::string label;
    std::string eta;
    std::string status;
};

inline SeerrDownloadProgressSummary
seerrDownloadProgressSummary(const std::string& mediaType, int mediaStatus,
                             const std::vector<SeerrDownloadProgressEntry>& downloads) {
    SeerrDownloadProgressSummary summary;
    if (downloads.empty()) return summary;

    const SeerrDownloadProgressEntry* selected = &downloads.front();
    if (mediaType == "tv") {
        const SeerrDownloadProgressEntry* earliestEpisode = nullptr;
        auto earliestRank = std::tuple<int, int, int>{};
        for (const auto& download : downloads) {
            if (download.seasonNumber < 0 || download.episodeNumber < 0) continue;
            const auto rank =
                std::tuple{download.seasonNumber == 0 ? 1 : 0, download.seasonNumber, download.episodeNumber};
            if (!earliestEpisode || rank < earliestRank) {
                earliestEpisode = &download;
                earliestRank = rank;
            }
        }
        if (earliestEpisode) selected = earliestEpisode;
    }

    summary.percent = seerrProgressPercent(selected->size, selected->sizeLeft);
    if (summary.percent >= 0) {
        summary.label = seerrProgressLabel(mediaType, selected->seasonNumber, selected->episodeNumber, summary.percent);
        summary.eta = seerrProgressEta(summary.percent, selected->timeLeft);
        summary.status = seerrProgressStatus(mediaType, selected->seasonNumber, selected->episodeNumber,
                                             summary.percent, selected->timeLeft);
        return summary;
    }

    if (mediaType == "tv") {
        if (mediaStatus == 4) {
            summary.status = "Partially available, still downloading";
        } else if (downloads.size() == 1) {
            summary.status = "Series downloading";
        } else {
            summary.status = std::to_string(downloads.size()) + " downloads active";
        }
    }
    return summary;
}
