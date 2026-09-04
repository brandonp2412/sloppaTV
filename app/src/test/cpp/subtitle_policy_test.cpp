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
    assert(shouldApplyLoadedSubtitle("item", "item", 3, 3, true));
    assert(!shouldApplyLoadedSubtitle("item", "other", 3, 3, true));
    assert(!shouldApplyLoadedSubtitle("item", "item", 3, 4, true));
    assert(!shouldApplyLoadedSubtitle("item", "item", 3, 3, false));

    assert(parseSubtitleTimestamp("00:00:01,250") == 1250);
    assert(parseSubtitleTimestamp("01:02:03.004") == 3723004);
    assert(parseSubtitleTimestamp("01:02.500") == 62500);
    assert(parseSubtitleTimestamp("00:01.5") == 1500);
    assert(parseSubtitleTimestamp("00:01.05") == 1050);
    assert(parseSubtitleTimestamp("00:01.005 align:start position:50%") == 1005);
    assert(parseSubtitleTimestamp("00:60.000") == -1);
    assert(parseSubtitleTimestamp("not-a-timestamp") == -1);

    assert(sanitizeSubtitleText("<i>Hello</i>") == "Hello");
    assert(sanitizeSubtitleText("&lt;b&gt;Hello&lt;/b&gt;") == "Hello");
    assert(sanitizeSubtitleText("A<br>B") == "A B");
    assert(sanitizeSubtitleText("I <3 TV") == "I <3 TV");
    assert(sanitizeSubtitleText("2 < 3") == "2 < 3");
    assert(sanitizeSubtitleText("{\\an8}Top") == "Top");
    assert(sanitizeSubtitleText("<v Roger>Hello</v>") == "Hello");

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
