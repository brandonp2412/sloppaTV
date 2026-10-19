#include "audio_policy.hpp"

#include <algorithm>
#include <cassert>
#include <optional>
#include <string>

namespace {
bool hasCodec(const std::vector<std::string>& codecs, const std::string& codec) {
    return std::find(codecs.begin(), codecs.end(), codec) != codecs.end();
}
} // namespace

int main() {
    assert(serverAudioChannels(true, 2) == 8);
    assert(serverAudioChannels(true, 8) == 8);
    assert(serverAudioChannels(false, 2) == 2);
    assert(serverAudioChannels(false, 6) == 6);
    assert(effectiveAudioChannels(2, 8) == 2);
    assert(effectiveAudioChannels(8, 2) == 2);
    assert(effectiveAudioChannels(8, 6) == 6);
    assert(effectiveAudioChannels(6, 8) == 6);

    // A stereo HDMI route can still direct-play Remux TrueHD/DTS tracks:
    // FFmpeg decodes/downmixes locally even without platform decoders or
    // compressed passthrough. Normal stereo server negotiation stays limited.
    const auto embedded = embeddedMpvAudioCodecs();
    const auto remuxStereo = advertisedAudioCodecs(embedded, serverAudioChannels(true, 2));
    assert(audioStreamCopyAllowed(remuxStereo, "truehd", 6, serverAudioChannels(true, 2)));
    assert(audioStreamCopyAllowed(remuxStereo, "dts", 8, serverAudioChannels(true, 2)));
    const auto jellyfinStereo = advertisedAudioCodecs(embedded, serverAudioChannels(false, 2));
    assert(!audioStreamCopyAllowed(jellyfinStereo, "truehd", 6, 2));

    AudioCodecCapabilities codecs{
        .aac = true,
        .mp3 = true,
        .mp2 = true,
        .pcm = true,
        .ac3 = true,
        .eac3 = true,
        .dts = true,
        .truehd = true,
        .flac = true,
        .opus = true,
        .vorbis = true,
    };

    const auto stereo = advertisedAudioCodecs(codecs, 2);
    assert(hasCodec(stereo, "aac"));
    assert(hasCodec(stereo, "mp3"));
    assert(hasCodec(stereo, "mp2"));
    assert(hasCodec(stereo, "pcm_s16le"));
    assert(!hasCodec(stereo, "ac3"));
    assert(!hasCodec(stereo, "eac3"));
    assert(!hasCodec(stereo, "dts"));
    assert(!hasCodec(stereo, "truehd"));
    assert(!hasCodec(stereo, "flac"));
    assert(!hasCodec(stereo, "opus"));
    assert(audioStreamCopyAllowed(stereo, "AAC", 2, 2));
    assert(!audioStreamCopyAllowed(stereo, "ac3", 2, 2));
    assert(!audioStreamCopyAllowed(stereo, "aac", 6, 2));
    const auto stereoTranscode = transcodingAudioCodecs(codecs, 2);
    assert(hasCodec(stereoTranscode, "aac"));
    assert(hasCodec(stereoTranscode, "mp3"));
    assert(!hasCodec(stereoTranscode, "ac3"));
    assert(!hasCodec(stereoTranscode, "eac3"));

    const auto surround = advertisedAudioCodecs(codecs, 8);
    assert(hasCodec(surround, "ac3"));
    assert(hasCodec(surround, "eac3"));
    assert(hasCodec(surround, "dts"));
    assert(hasCodec(surround, "truehd"));
    assert(hasCodec(surround, "flac"));
    assert(hasCodec(surround, "opus"));
    assert(hasCodec(surround, "mp2"));
    assert(hasCodec(surround, "pcm_s16le"));
    assert(audioStreamCopyAllowed(surround, "ac3", 6, 8));
    assert(audioStreamCopyAllowed(surround, "pcm_s16le", 6, 8));
    assert(!audioStreamCopyAllowed(surround, "ac3", 8, 6));
    const auto surroundTranscode = transcodingAudioCodecs(codecs, 8);
    assert(hasCodec(surroundTranscode, "aac"));
    assert(hasCodec(surroundTranscode, "ac3"));
    assert(hasCodec(surroundTranscode, "eac3"));
    assert(!hasCodec(surroundTranscode, "dts"));
    assert(!hasCodec(surroundTranscode, "truehd"));

    AudioCodecCapabilities passthroughOnly{
        .aac = true,
        .directAc3 = true,
        .directEac3 = true,
        .directDtsHd = true,
        .directTrueHd = true,
    };
    const auto passthrough = advertisedAudioCodecs(passthroughOnly, 8);
    assert(hasCodec(passthrough, "ac3"));
    assert(hasCodec(passthrough, "eac3"));
    assert(hasCodec(passthrough, "dts"));
    assert(hasCodec(passthrough, "truehd"));

    const std::vector<AudioPreferenceCandidate> audioTracks{
        {1, "jpn"},
        {3, "eng"},
        {5, "ENG"},
    };
    assert(audioIndexForQueuePreference(audioTracks, std::nullopt) == -1);
    assert(audioIndexForQueuePreference(audioTracks, std::optional<std::string>{"eng"}) == 3);
    // Three-letter stream tags and regional user preferences must round-trip.
    assert(audioLanguageEquals("ita", "it-IT"));
    assert(audioLanguageEquals("nld", "nl-NL"));
    assert(audioLanguageEquals("ENG", "en_NZ"));
    assert(!audioLanguageEquals("ita", "en-NZ"));
    const std::vector<AudioPreferenceCandidate> locales{{1, "en-US"}, {2, "en-NZ"}, {3, "ita"}};
    assert(audioIndexForQueuePreference(locales, std::optional<std::string>{"en-NZ"}) == 2);
    assert(audioIndexForQueuePreference(locales, std::optional<std::string>{"eng"}) == 1);
    assert(audioIndexForQueuePreference(locales, std::optional<std::string>{"it-IT"}) == 3);
    assert(audioIndexForQueuePreference(audioTracks, std::optional<std::string>{"ENG"}) == 3);
    assert(audioIndexForQueuePreference(audioTracks, std::optional<std::string>{"jpn"}) == 1);
    assert(audioIndexForQueuePreference(audioTracks, std::optional<std::string>{"fra"}) == -1);
    // Server and player settings may alternate ISO-639-1 and ISO-639-2.
    assert(audioLanguageEquals("ko", "kor"));
    assert(audioLanguageEquals("rus", "RU"));
    assert(audioLanguageEquals("eng", "en"));
    assert(audioLanguageEquals("JPN", "ja"));
    assert(audioLanguageEquals("fre", "fr"));
    assert(audioLanguageEquals("ger", "deu"));
    assert(!audioLanguageEquals("kor", "rus"));
    assert(!audioLanguageEquals("en", "ja"));
    const std::vector<AudioPreferenceCandidate> mixedDubTracks{{1, "rus"}, {2, "kor"}};
    assert(audioIndexForQueuePreference(mixedDubTracks, std::optional<std::string>{"ko"}) == 2);
    assert(audioIndexForQueuePreference(mixedDubTracks, std::optional<std::string>{"ru"}) == 1);

    assert(audioIndexForQueuePreference(audioTracks, std::optional<std::string>{""}) == -1);
    return 0;
}
