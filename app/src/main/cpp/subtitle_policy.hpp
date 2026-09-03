#pragma once

#include <algorithm>
#include <cctype>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

constexpr int kSubtitleServerDefaultIndex = -2;
constexpr int kSubtitleOffIndex = -1;

constexpr int resolvedSubtitleIndex(int requestedIndex, int serverDefaultIndex) {
    return requestedIndex == kSubtitleServerDefaultIndex ? serverDefaultIndex : requestedIndex;
}

constexpr bool shouldRetryFailedSubtitleTranscode(bool isTranscode, int selectedSubtitleIndex) {
    return isTranscode && selectedSubtitleIndex >= 0;
}

struct SubtitlePreferenceCandidate {
    int index = -1;
    std::string language;
};

inline std::string normalizeSubtitleLanguage(std::string language) {
    std::transform(language.begin(), language.end(), language.begin(), [](unsigned char c) {
        return static_cast<char>(std::tolower(c));
    });
    return language;
}

inline bool isLikelySignsOnlySubtitle(std::string label) {
    std::transform(label.begin(), label.end(), label.begin(), [](unsigned char c) {
        return static_cast<char>(std::tolower(c));
    });
    return label.find("sign") != std::string::npos && label.find("song") != std::string::npos;
}

// nullopt: no explicit viewer choice yet, begin with subtitles off.
// empty string: user explicitly disabled subtitles for this playback chain.
// non-empty: user selected a language; carry it forward when the next item exposes it.
inline int subtitleIndexForQueuePreference(
    const std::vector<SubtitlePreferenceCandidate>& subtitles,
    const std::optional<std::string>& languagePreference
) {
    if (!languagePreference.has_value()) return kSubtitleOffIndex;
    if (languagePreference->empty()) return kSubtitleOffIndex;

    const std::string preferred = normalizeSubtitleLanguage(*languagePreference);
    const auto match = std::find_if(subtitles.begin(), subtitles.end(), [&](const SubtitlePreferenceCandidate& subtitle) {
        return subtitle.index >= 0 && normalizeSubtitleLanguage(subtitle.language) == preferred;
    });
    return match == subtitles.end() ? kSubtitleOffIndex : match->index;
}
