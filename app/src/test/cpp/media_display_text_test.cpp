#include "media_display_text.hpp"
#include <cassert>

int main() {
    JellyfinItem episode;
    episode.type = "Episode";
    episode.seriesName = "Tatami";
    episode.name = "S01E02 - Second episode";
    assert(episodeNumberLabel(episode) == "S1E2");
    assert(episodeLabel(episode) == "Tatami - S1E2");
    episode.parentIndexNumber = 3;
    episode.indexNumber = 8;
    assert(episodeNumberLabel(episode) == "S3E8");
    episode.parentIndexNumber = -1;
    episode.indexNumber = -1;
    episode.name = "Episode 7";
    episode.seasonName = "Season 2";
    assert(episodeNumberLabel(episode) == "S2E7");
    episode.name = "The Show S04E09 - Numbered";
    assert(episodeNumberLabel(episode) == "S4E9");
    episode.name = "Unnumbered";
    episode.seasonName.clear();
    assert(episodeNumberLabel(episode).empty());
    episode.type = "Movie";
    episode.name = "S02E09";
    assert(episodeNumberLabel(episode).empty());
}
