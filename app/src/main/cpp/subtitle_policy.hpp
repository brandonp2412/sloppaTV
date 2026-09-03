#pragma once

#include <algorithm>
#include <charconv>
#include <cctype>
#include <limits>
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

inline int parseSubtitleTimestamp(std::string_view input) {
    while (!input.empty() && std::isspace(static_cast<unsigned char>(input.front()))) input.remove_prefix(1);
    const size_t whitespace = input.find_first_of(" \t\r\n");
    if (whitespace != std::string_view::npos) input = input.substr(0, whitespace);
    if (input.empty()) return -1;

    const size_t firstColon = input.find(':');
    if (firstColon == std::string_view::npos) return -1;
    const size_t secondColon = input.find(':', firstColon + 1);
    const size_t fraction = input.find_first_of(".,", secondColon == std::string_view::npos ? firstColon + 1 : secondColon + 1);
    if (fraction == std::string_view::npos) return -1;

    auto parseUnsigned = [](std::string_view value, int& output) {
        if (value.empty()) return false;
        const char* begin = value.data();
        const char* end = begin + value.size();
        const auto [parsed, error] = std::from_chars(begin, end, output);
        return error == std::errc{} && parsed == end && output >= 0;
    };

    int hours = 0;
    int minutes = 0;
    int seconds = 0;
    if (secondColon == std::string_view::npos) {
        if (!parseUnsigned(input.substr(0, firstColon), minutes)
            || !parseUnsigned(input.substr(firstColon + 1, fraction - firstColon - 1), seconds)) {
            return -1;
        }
    } else {
        if (!parseUnsigned(input.substr(0, firstColon), hours)
            || !parseUnsigned(input.substr(firstColon + 1, secondColon - firstColon - 1), minutes)
            || !parseUnsigned(input.substr(secondColon + 1, fraction - secondColon - 1), seconds)) {
            return -1;
        }
    }
    if (minutes > 59 || seconds > 59) return -1;

    const std::string_view fractionDigits = input.substr(fraction + 1);
    if (fractionDigits.empty()) return -1;
    int rawFraction = 0;
    const size_t usedDigits = std::min<size_t>(3, fractionDigits.size());
    if (!parseUnsigned(fractionDigits.substr(0, usedDigits), rawFraction)) return -1;
    int milliseconds = rawFraction;
    if (usedDigits == 1) milliseconds *= 100;
    else if (usedDigits == 2) milliseconds *= 10;
    for (size_t index = usedDigits; index < fractionDigits.size(); ++index) {
        if (!std::isdigit(static_cast<unsigned char>(fractionDigits[index]))) return -1;
    }

    const int64_t total = (((static_cast<int64_t>(hours) * 60) + minutes) * 60 + seconds) * 1000 + milliseconds;
    if (total > std::numeric_limits<int>::max()) return -1;
    return static_cast<int>(total);
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
