#pragma once

#include <algorithm>
#include <cctype>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

struct AudioCodecCapabilities {
    bool aac = false;
    bool mp3 = false;
    bool mp2 = false;
    bool pcm = false;
    bool ac3 = false;
    bool eac3 = false;
    bool dts = false;
    bool truehd = false;
    bool flac = false;
    bool opus = false;
    bool vorbis = false;
    bool directAc3 = false;
    bool directEac3 = false;
    bool directDts = false;
    bool directDtsHd = false;
    bool directTrueHd = false;
};

constexpr int effectiveAudioChannels(int requestedChannels, int routeChannels) {
    const int requested = std::clamp(requestedChannels, 2, 8);
    if (requested <= 2) return 2;
    return std::clamp(routeChannels, 2, requested);
}

inline std::string normalizedAudioCodec(std::string_view codec) {
    std::string value(codec);
    std::transform(value.begin(), value.end(), value.begin(), [](unsigned char c) {
        return static_cast<char>(std::tolower(c));
    });
    return value;
}

struct AudioPreferenceCandidate {
    int index = -1;
    std::string language;
};

inline std::string normalizeAudioLanguage(std::string language) {
    std::transform(language.begin(), language.end(), language.begin(), [](unsigned char c) {
        return static_cast<char>(std::tolower(c));
    });
    return language;
}

inline int audioIndexForQueuePreference(
    const std::vector<AudioPreferenceCandidate>& audios,
    const std::optional<std::string>& languagePreference
) {
    if (!languagePreference.has_value() || languagePreference->empty()) return -1;
    const std::string preferred = normalizeAudioLanguage(*languagePreference);
    const auto match = std::find_if(audios.begin(), audios.end(), [&](const AudioPreferenceCandidate& audio) {
        return audio.index >= 0 && normalizeAudioLanguage(audio.language) == preferred;
    });
    return match == audios.end() ? -1 : match->index;
}

inline bool audioStreamCopyAllowed(
    const std::vector<std::string>& advertisedCodecs,
    std::string_view codec,
    int channels,
    int maxAudioChannels
) {
    const std::string normalized = normalizedAudioCodec(codec);
    const bool codecAllowed = std::find(advertisedCodecs.begin(), advertisedCodecs.end(), normalized) != advertisedCodecs.end();
    return codecAllowed && (channels <= 0 || channels <= std::max(2, maxAudioChannels));
}

inline std::vector<std::string> transcodingAudioCodecs(
    const AudioCodecCapabilities& capabilities,
    int maxAudioChannels
) {
    std::vector<std::string> codecs;
    if (capabilities.aac) codecs.emplace_back("aac");
    if (capabilities.mp3) codecs.emplace_back("mp3");
    if (maxAudioChannels > 2) {
        if (capabilities.ac3 || capabilities.directAc3) codecs.emplace_back("ac3");
        if (capabilities.eac3 || capabilities.directEac3) codecs.emplace_back("eac3");
    }
    if (codecs.empty()) codecs.emplace_back("aac");
    return codecs;
}

inline std::vector<std::string> advertisedAudioCodecs(
    const AudioCodecCapabilities& capabilities,
    int maxAudioChannels
) {
    std::vector<std::string> codecs;
    if (capabilities.aac) codecs.emplace_back("aac");
    if (capabilities.mp3) codecs.emplace_back("mp3");
    if (capabilities.mp2) codecs.emplace_back("mp2");
    if (capabilities.pcm) codecs.emplace_back("pcm_s16le");

    // Jellyfin can transcode/downmix when surround formats are not advertised.
    if (maxAudioChannels <= 2) return codecs;

    if (capabilities.ac3 || capabilities.directAc3) codecs.emplace_back("ac3");
    if (capabilities.eac3 || capabilities.directEac3) codecs.emplace_back("eac3");
    if (capabilities.dts || capabilities.directDts || capabilities.directDtsHd) codecs.emplace_back("dts");
    if (capabilities.truehd || capabilities.directTrueHd) codecs.emplace_back("truehd");
    if (capabilities.flac) codecs.emplace_back("flac");
    if (capabilities.opus) codecs.emplace_back("opus");
    if (capabilities.vorbis) codecs.emplace_back("vorbis");
    return codecs;
}
