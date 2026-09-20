#include "seerr_progress.hpp"

#include <cassert>

int main() {
    assert(seerrProgressPercent(1000.0, 500.0) == 50);
    assert(seerrProgressPercent(1000.0, 0.0) == 100);
    assert(seerrProgressPercent(0.0, 0.0) == -1);
    assert(seerrCompactTimeLeft("00:11:42") == "11m left");
    assert(seerrCompactTimeLeft("01:02:03") == "1h 2m left");
    assert(seerrCompactTimeLeft("1.02:03:04") == "1d 2h left");
    assert(seerrCompactTimeLeft("30000.00:00:00") == "30000d left");
    assert(seerrCompactTimeLeft("00:11:42junk").empty());
    assert(seerrCompactTimeLeft("00:1x:42").empty());
    assert(seerrCompactTimeLeft("1x.02:03:04").empty());
    assert(seerrCompactTimeLeft("-1.02:03:04").empty());
    assert(seerrCompactTimeLeft("00:-1:42").empty());
    assert(seerrCompactTimeLeft("00:11:-1").empty());
    assert(seerrProgressLabel("movie", -1, -1, 63) == "Downloading");
    assert(seerrProgressLabel("tv", 1, 1, 63) == "S1E1 downloading");
    assert(seerrProgressLabel("tv", -1, -1, 63) == "Season pack downloading");
    assert(seerrProgressEta(63, "00:11:42") == "11m left");
    assert(seerrProgressEta(100, "00:00:00") == "Waiting for import");
    assert(seerrProgressStatus("movie", -1, -1, 63, "00:11:42") == "Downloading 63%  11m left");
    assert(seerrProgressStatus("movie", -1, -1, 100, "00:00:00") == "Downloaded 100%  Waiting for import");
    assert(seerrProgressStatus("tv", 1, 1, 63, "00:11:42") == "S1E1 downloading 63%  11m left");
    assert(seerrProgressStatus("tv", -1, -1, 63, "00:11:42") == "Season pack downloading 63%  11m left");
    return 0;
}
