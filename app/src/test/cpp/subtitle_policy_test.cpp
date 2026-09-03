#include "subtitle_policy.hpp"

#include <cassert>
#include <optional>
#include <string>
#include <vector>

int main() {
    assert(resolvedSubtitleIndex(kSubtitleServerDefaultIndex, 4) == 4);
    assert(resolvedSubtitleIndex(kSubtitleServerDefaultIndex, kSubtitleOffIndex) == kSubtitleOffIndex);
    assert(resolvedSubtitleIndex(kSubtitleOffIndex, 4) == kSubtitleOffIndex);
    assert(resolvedSubtitleIndex(7, 4) == 7);
    assert(shouldRetryFailedSubtitleTranscode(true, 2));
    assert(!shouldRetryFailedSubtitleTranscode(false, 2));
    assert(!shouldRetryFailedSubtitleTranscode(true, kSubtitleOffIndex));

    const std::vector<SubtitlePreferenceCandidate> subtitles{
        {2, "eng"},
        {4, "jpn"},
        {7, "ENG"},
    };

    // Playback begins without subtitles. This prevents release-group adverts and
    // forced/default tracks from appearing until the viewer explicitly enables one.
    assert(subtitleIndexForQueuePreference(subtitles, std::nullopt) == kSubtitleOffIndex);
    assert(subtitleIndexForQueuePreference(subtitles, std::optional<std::string>{""}) == kSubtitleOffIndex);
    assert(subtitleIndexForQueuePreference(subtitles, std::optional<std::string>{"eng"}) == 2);
    assert(subtitleIndexForQueuePreference(subtitles, std::optional<std::string>{"ENG"}) == 2);
    assert(subtitleIndexForQueuePreference(subtitles, std::optional<std::string>{"jpn"}) == 4);
    assert(subtitleIndexForQueuePreference(subtitles, std::optional<std::string>{"fra"}) == kSubtitleOffIndex);
    assert(isLikelySignsOnlySubtitle("English [Signs/Songs]"));
    assert(isLikelySignsOnlySubtitle("Signs & Songs"));
    assert(!isLikelySignsOnlySubtitle("English"));
    assert(!isLikelySignsOnlySubtitle("English Dialogue"));
    return 0;
}
