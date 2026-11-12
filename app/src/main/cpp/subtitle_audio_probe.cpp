#include "subtitle_audio_probe.hpp"

#include "third_party/libfvad/include/fvad.h"

#include <dlfcn.h>

#include <algorithm>
#include <array>
#include <chrono>
#include <cstdio>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <memory>
#include <mutex>
#include <string>
#include <vector>

namespace {
constexpr int kProbeSeconds = 300;
constexpr int kExtendedProbeSeconds = 540;
constexpr int kProbeTimeoutSeconds = 120;
constexpr int kExtendedProbeTimeoutSeconds = 180;
constexpr uintmax_t kProbeReadyBytes = 270U * 16000U * 2U; // 270 seconds of mono 16-bit PCM
constexpr uintmax_t kExtendedProbeReadyBytes = 500U * 16000U * 2U;
constexpr size_t kMaximumPcmBytes = 20 * 1024 * 1024;
constexpr int kMpvEndFileEvent = 7;

struct MpvEvent {
    int eventId;
    int error;
    uint64_t replyUserdata;
    void* data;
};

struct MpvEventEndFile {
    int reason;
    int error;
};

struct ProbeMpv {
    using Create = void* (*)();
    using Initialize = int (*)(void*);
    using Terminate = void (*)(void*);
    using Option = int (*)(void*, const char*, const char*);
    using Command = int (*)(void*, const char* const*);
    using WaitEvent = MpvEvent* (*)(void*, double);
    Create create = nullptr;
    Initialize initialize = nullptr;
    Terminate terminate = nullptr;
    Option option = nullptr;
    Command command = nullptr;
    WaitEvent waitEvent = nullptr;
};

template <typename T> T loadSymbol(void* handle, const char* name) {
    return reinterpret_cast<T>(dlsym(handle, name));
}

ProbeMpv loadApi(void* handle) {
    return {
        loadSymbol<ProbeMpv::Create>(handle, "mpv_create"),
        loadSymbol<ProbeMpv::Initialize>(handle, "mpv_initialize"),
        loadSymbol<ProbeMpv::Terminate>(handle, "mpv_terminate_destroy"),
        loadSymbol<ProbeMpv::Option>(handle, "mpv_set_option_string"),
        loadSymbol<ProbeMpv::Command>(handle, "mpv_command"),
        loadSymbol<ProbeMpv::WaitEvent>(handle, "mpv_wait_event"),
    };
}

bool completeApi(const ProbeMpv& api) {
    return api.create && api.initialize && api.terminate && api.option && api.command && api.waitEvent;
}

uint16_t le16(const unsigned char* p) {
    return static_cast<uint16_t>(p[0] | (static_cast<uint16_t>(p[1]) << 8));
}

uint32_t le32(const unsigned char* p) {
    return static_cast<uint32_t>(p[0]) | (static_cast<uint32_t>(p[1]) << 8) | (static_cast<uint32_t>(p[2]) << 16) |
           (static_cast<uint32_t>(p[3]) << 24);
}

struct PcmAudio {
    int sampleRate = 0;
    std::vector<int16_t> samples;
};

// libmpv's PCM AO writes a RIFF/WAVE file; parse chunks instead of assuming a 44-byte header.
PcmAudio readPcmWave(const std::filesystem::path& path) {
    std::ifstream input(path, std::ios::binary);
    std::array<unsigned char, 12> header{};
    if (!input.read(reinterpret_cast<char*>(header.data()), header.size()) ||
        std::memcmp(header.data(), "RIFF", 4) != 0 || std::memcmp(header.data() + 8, "WAVE", 4) != 0)
        return {};

    PcmAudio audio;
    bool pcm16Mono = false;
    while (input) {
        std::array<unsigned char, 8> chunk{};
        if (!input.read(reinterpret_cast<char*>(chunk.data()), chunk.size())) break;
        const uint32_t length = le32(chunk.data() + 4);
        if (std::memcmp(chunk.data(), "fmt ", 4) == 0) {
            std::array<unsigned char, 16> fmt{};
            if (length < fmt.size() || !input.read(reinterpret_cast<char*>(fmt.data()), fmt.size())) return {};
            constexpr std::array<unsigned char, 16> kPcmSubtype = {
                1, 0, 0, 0, 0, 0, 0x10, 0, 0x80, 0, 0, 0xaa, 0, 0x38, 0x9b, 0x71,
            };
            const uint16_t format = le16(fmt.data());
            bool isPcm = format == 1;
            if (format == 0xfffe && length >= 40 && length <= 256) {
                std::vector<unsigned char> extension(length - fmt.size());
                if (!input.read(reinterpret_cast<char*>(extension.data()),
                                static_cast<std::streamsize>(extension.size())))
                    return {};
                isPcm = std::equal(kPcmSubtype.begin(), kPcmSubtype.end(), extension.begin() + 8);
            } else {
                input.seekg(static_cast<std::streamoff>(length - fmt.size()), std::ios::cur);
            }
            pcm16Mono = isPcm && le16(fmt.data() + 2) == 1 && le16(fmt.data() + 14) == 16;
            audio.sampleRate = static_cast<int>(le32(fmt.data() + 4));
        } else if (std::memcmp(chunk.data(), "data", 4) == 0) {
            if (!pcm16Mono || length > kMaximumPcmBytes || length % 2 != 0) return {};
            audio.samples.resize(length / 2);
            if (!input.read(reinterpret_cast<char*>(audio.samples.data()), length)) return {};
            return audio;
        } else {
            input.seekg(length, std::ios::cur);
        }
        if (length % 2 != 0) input.seekg(1, std::ios::cur);
    }
    return {};
}

std::vector<uint8_t> detectSpeech(const PcmAudio& audio, int mode) {
    if (audio.sampleRate != 8000 && audio.sampleRate != 16000 && audio.sampleRate != 32000 && audio.sampleRate != 48000)
        return {};
    std::unique_ptr<Fvad, decltype(&fvad_free)> detector(fvad_new(), &fvad_free);
    if (!detector || fvad_set_sample_rate(detector.get(), audio.sampleRate) != 0 ||
        fvad_set_mode(detector.get(), mode) != 0)
        return {};
    const size_t samplesPerFrame = static_cast<size_t>(audio.sampleRate) / 100;
    std::vector<uint8_t> activity;
    activity.reserve(audio.samples.size() / samplesPerFrame);
    for (size_t i = 0; i + samplesPerFrame <= audio.samples.size(); i += samplesPerFrame) {
        const int status = fvad_process(detector.get(), audio.samples.data() + i, samplesPerFrame);
        if (status < 0) return {};
        activity.push_back(status == 1 ? 1 : 0);
    }
    return activity;
}

// Keep the mpv/FFmpeg shared objects mapped: their audio threads have TLS destructors
// which may run after a playback context has been destroyed.
void* openMpvLibrary() {
    static void* handle = [] {
        if (void* library = dlopen("libmpv.so", RTLD_NOW | RTLD_LOCAL)) return library;
        return dlopen("libmpv.so.2", RTLD_NOW | RTLD_LOCAL);
    }();
    return handle;
}

bool capturePcm(const SubtitleAudioSyncRequest& request, const std::filesystem::path& output, std::string& failure) {
    void* library = openMpvLibrary();
    if (!library) {
        failure = "libmpv unavailable";
        return false;
    }
    const ProbeMpv api = loadApi(library);
    if (!completeApi(api)) {
        failure = "libmpv audio probe API incomplete";
        return false;
    }
    void* mpv = api.create();
    if (!mpv) {
        failure = "unable to create audio probe";
        return false;
    }

    struct MpvCleanup {
        ProbeMpv api;
        void* handle = nullptr;

        ~MpvCleanup() { api.terminate(handle); }
    } cleanup{api, mpv};

    auto option = [&](const char* name, const std::string& value) {
        if (api.option(mpv, name, value.c_str()) >= 0) return true;
        failure = std::string("mpv rejected audio probe option ") + name;
        return false;
    };
    const std::filesystem::path certificate = std::filesystem::path(request.privateFilesDir) / "android-ca-bundle.pem";
    if (!option("config", "no") || !option("video", "no") || !option("vo", "null") || !option("ao", "pcm") ||
        !option("ao-pcm-file", output.string()) || !option("audio-samplerate", "16000") ||
        !option("audio-channels", "mono") || !option("audio-format", "s16") ||
        !option("start", std::to_string(request.sampleStartMs / 1000.0)) ||
        !option("length", std::to_string(request.extendedSample ? kExtendedProbeSeconds : kProbeSeconds)) ||
        !option("cache", "no") || !option("demuxer", "lavf") || !option("ytdl", "no") ||
        (std::filesystem::exists(certificate) && !option("tls-ca-file", certificate.string())) ||
        !option("tls-verify", "yes"))
        return false;

    if (api.initialize(mpv) < 0) {
        failure = "unable to initialize audio-only libmpv";
        return false;
    }
    const char* command[] = {"loadfile", request.mediaUrl.c_str(), "replace", nullptr};
    if (api.command(mpv, command) < 0) {
        failure = "audio probe media failed to open";
        return false;
    }
    const auto deadline =
        std::chrono::steady_clock::now() +
        std::chrono::seconds(request.extendedSample ? kExtendedProbeTimeoutSeconds : kProbeTimeoutSeconds);
    while (std::chrono::steady_clock::now() < deadline) {
        MpvEvent* event = api.waitEvent(mpv, 0.25);
        // Live/DirectStream URLs can continue waiting after sufficient PCM is captured.
        std::error_code fileError;
        const auto bytes = std::filesystem::file_size(output, fileError);
        if (!fileError && bytes >= (request.extendedSample ? kExtendedProbeReadyBytes : kProbeReadyBytes)) return true;
        if (event && event->eventId == kMpvEndFileEvent) {
            const auto* end = static_cast<const MpvEventEndFile*>(event->data);
            failure = "audio probe end reason=" + std::to_string(end ? end->reason : -1) +
                      " error=" + std::to_string(end ? end->error : event->error) +
                      " wavBytes=" + std::to_string(fileError ? 0 : bytes);
            return end && end->reason == 0;
        }
    }
    failure = "audio probe deadline exceeded";
    return false;
}
} // namespace

SubtitleAudioSyncResult synchronizeSubtitleAudio(const SubtitleAudioSyncRequest& request) {
    SubtitleAudioSyncResult result;
    if (request.mediaUrl.empty() || request.privateFilesDir.empty() || request.cues.size() < 12) {
        result.failure = "missing audio or subtitle inputs";
        return result;
    }

    // Remove only our own abandoned probe WAVs from earlier crashes/debug builds.
    static std::once_flag staleProbeCleanup;
    std::call_once(staleProbeCleanup, [&] {
        std::error_code directoryError;
        const std::filesystem::path privateDir(request.privateFilesDir);
        for (std::filesystem::directory_iterator it(privateDir, directoryError), end; !directoryError && it != end;
             it.increment(directoryError)) {
            const std::string name = it->path().filename().string();
            if (name.rfind("subtitle-audio-", 0) == 0 && name.ends_with(".wav") && it->is_regular_file()) {
                std::error_code removeError;
                std::filesystem::remove(it->path(), removeError);
            }
        }
    });

    static uint64_t probeCounter = 0;
    const auto serial = __atomic_add_fetch(&probeCounter, static_cast<uint64_t>(1), __ATOMIC_RELAXED);
    const std::filesystem::path output =
        std::filesystem::path(request.privateFilesDir) / ("subtitle-audio-" + std::to_string(serial) + ".wav");
    std::error_code ec;
    // A crashed or reinstalled process may leave an old file with this counter value.
    std::filesystem::remove(output, ec);
    // A live stream can time out after providing a sufficiently long usable sample.
    capturePcm(request, output, result.failure);
    const std::string captureDiagnostic = result.failure;
    const auto wavBytes = std::filesystem::file_size(output, ec);
    PcmAudio audio = readPcmWave(output);
    std::filesystem::remove(output, ec);
    if (!audio.samples.empty()) result.failure.clear();
    if (audio.samples.empty() && result.failure.empty())
        result.failure = "mpv audio probe WAV unreadable (bytes=" +
                         std::to_string(wavBytes == static_cast<uintmax_t>(-1) ? 0 : wavBytes) + ")";
    if (audio.samples.empty()) {
        if (result.failure.empty()) result.failure = "audio probe returned no PCM";
        return result;
    }

    result.audioMs = subtitlePcmDurationMs(audio.samples.size(), audio.sampleRate);
    const auto moderate = detectSpeech(audio, 2);
    const auto aggressive = detectSpeech(audio, 3);
    if (moderate.empty() || aggressive.size() != moderate.size()) {
        result.failure = "speech detector rejected the audio format";
        return result;
    }
    const int subtitleSampleStartMs =
        subtitleSyncLogicalSampleStart(request.sampleStartMs, request.subtitleTimelineBaseMs);
    const int matchingCues =
        static_cast<int>(std::count_if(request.cues.begin(), request.cues.end(), [&](const SubtitleCue& cue) {
            return cue.startMs < subtitleSampleStartMs + result.audioMs && cue.endMs > subtitleSampleStartMs &&
                   cue.endMs - cue.startMs <= 12000 && !subtitleSyncCueIsMetadata(cue);
        }));
    const SubtitleSyncPeak score2 = findSubtitleSyncPeak(moderate, request.cues, subtitleSampleStartMs);
    const SubtitleSyncPeak score3 = findSubtitleSyncPeak(aggressive, request.cues, subtitleSampleStartMs);
    result.decision = decideSubtitleSync(score2, score3, result.audioMs, matchingCues);
    if (!result.decision.confident) {
        result.failure = "subtitle timing match lacked confidence";
        result.failure += " startMs=" + std::to_string(subtitleSampleStartMs) +
                          " audioMs=" + std::to_string(result.audioMs) + " cues=" + std::to_string(matchingCues) +
                          " mode2=" + std::to_string(score2.offsetMs) + ":" + std::to_string(score2.correlation) + ":" +
                          std::to_string(score2.alternativeCorrelation) + " mode3=" + std::to_string(score3.offsetMs) +
                          ":" + std::to_string(score3.correlation) + ":" +
                          std::to_string(score3.alternativeCorrelation);
        result.failure += " capture=" + captureDiagnostic +
                          " wavBytes=" + std::to_string(wavBytes == static_cast<uintmax_t>(-1) ? 0 : wavBytes) +
                          " playerSampleStartMs=" + std::to_string(request.sampleStartMs) +
                          " timelineBaseMs=" + std::to_string(request.subtitleTimelineBaseMs) +
                          " isHls=" + std::to_string(request.mediaUrl.find(".m3u8") != std::string::npos);
    }
    return result;
}
