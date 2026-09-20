#include "seerr_download_progress.hpp"

#include <cassert>
#include <vector>

int main() {
    {
        const auto progress =
            seerrDownloadProgressSummary("movie", 3, {{.size = 1000.0, .sizeLeft = 250.0, .timeLeft = "00:10:00"}});
        assert(progress.percent == 75);
        assert(progress.label == "Downloading");
        assert(progress.eta == "10m left");
        assert(progress.status == "Downloading 75%  10m left");
    }

    {
        const std::vector<SeerrDownloadProgressEntry> downloads{
            {.size = 1000.0, .sizeLeft = 200.0, .timeLeft = "00:05:00", .seasonNumber = 2, .episodeNumber = 1},
            {.size = 1000.0, .sizeLeft = 600.0, .timeLeft = "00:12:00", .seasonNumber = 1, .episodeNumber = 4},
        };
        const auto progress = seerrDownloadProgressSummary("tv", 3, downloads);
        assert(progress.percent == 40);
        assert(progress.label == "S1E4 downloading");
        assert(progress.eta == "12m left");
        assert(progress.status == "S1E4 downloading 40%  12m left");
    }

    {
        const std::vector<SeerrDownloadProgressEntry> downloads{
            {.size = 1000.0, .sizeLeft = 100.0, .timeLeft = "", .seasonNumber = 0, .episodeNumber = 1},
            {.size = 1000.0, .sizeLeft = 500.0, .timeLeft = "", .seasonNumber = 1, .episodeNumber = 1},
        };
        const auto progress = seerrDownloadProgressSummary("tv", 3, downloads);
        assert(progress.percent == 50);
        assert(progress.label == "S1E1 downloading");
    }

    {
        const auto progress =
            seerrDownloadProgressSummary("tv", 3, {{.size = 1000.0, .sizeLeft = 700.0, .timeLeft = "01:00:00"}});
        assert(progress.percent == 30);
        assert(progress.label == "Season pack downloading");
        assert(progress.eta == "1h left");
        assert(progress.status == "Season pack downloading 30%  1h left");
    }

    {
        const auto progress = seerrDownloadProgressSummary("tv", 3, {{.size = 0.0, .sizeLeft = 0.0, .timeLeft = ""}});
        assert(progress.percent == -1);
        assert(progress.label.empty());
        assert(progress.eta.empty());
        assert(progress.status == "Series downloading");
    }

    {
        const auto progress = seerrDownloadProgressSummary(
            "tv", 3,
            {{.size = 0.0, .sizeLeft = 0.0, .timeLeft = ""},
             {.size = 0.0, .sizeLeft = 0.0, .timeLeft = "", .seasonNumber = 1, .episodeNumber = 2}});
        assert(progress.percent == -1);
        assert(progress.status == "2 downloads active");
    }

    {
        const auto progress = seerrDownloadProgressSummary("tv", 4, {{.size = 0.0, .sizeLeft = 0.0, .timeLeft = ""}});
        assert(progress.percent == -1);
        assert(progress.status == "Partially available, still downloading");
    }

    {
        const auto progress = seerrDownloadProgressSummary("tv", 3, {});
        assert(progress.percent == -1);
        assert(progress.status.empty());
    }

    return 0;
}
