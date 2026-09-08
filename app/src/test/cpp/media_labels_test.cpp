#include "media_labels.hpp"

#include <cassert>

int main() {
    JellyfinItem episode;
    episode.name = "The Episode";
    episode.seriesName = "The Series";
    episode.parentIndexNumber = 2;
    episode.indexNumber = 7;
    assert(episodeNumberLabel(episode) == "S2E7");
    assert(episodeLabel(episode) == "The Series - S2E7");

    PlaybackLabels labels;
    labels.update(episode);
    assert(labels.heading == "The Series");
    assert(labels.secondary == "S2E7  |  The Episode");

    JellyfinItem movie;
    movie.name = "Movie";
    labels.update(movie);
    assert(labels.heading == "Movie");
    assert(labels.secondary.empty());
    labels.clear();
    assert(labels.heading.empty());
    assert(labels.secondary.empty());
    return 0;
}
