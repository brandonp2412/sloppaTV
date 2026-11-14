#include "subtitle_audio_probe.hpp"
#include "subtitle_scene_sync.hpp"

#include "third_party/libfvad/include/fvad.h"

#include <dlfcn.h>
#ifdef __ANDROID__
#include <android/log.h>
#endif

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
#include <thread>

namespace {
constexpr int kProbeSeconds = 300;
constexpr int kExtendedProbeSeconds = 540;
constexpr int kProbeTimeoutSeconds = 120;
constexpr int kExtendedProbeTimeoutSeconds = 180;
constexpr uintmax_t kProbeReadyBytes = 270U * 16000U * 2U; // 270 seconds of mono 16-bit PCM
constexpr uintmax_t kExtendedProbeReadyBytes = 500U * 16000U * 2U;
constexpr size_t kMaximumPcmBytes = 20 * 1024 * 1024;
constexpr int kMpvEndFileEvent = 7;
constexpr int kMpvLogMessageEvent = 2;

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

struct ProbeLogMessage {
    const char* prefix;
    const char* level;
    const char* text;
    int logLevel;
};

struct ProbeMpv {
    using Create = void* (*)();
    using Initialize = int (*)(void*);
    using Terminate = void (*)(void*);
    using Option = int (*)(void*, const char*, const char*);
    using Command = int (*)(void*, const char* const*);
    using WaitEvent = MpvEvent* (*)(void*, double);
    using RequestLogMessages = int (*)(void*, const char*);
    using GetProperty = int (*)(void*, const char*, int, void*);
    Create create = nullptr;
    Initialize initialize = nullptr;
    Terminate terminate = nullptr;
    Option option = nullptr;
    Command command = nullptr;
    WaitEvent waitEvent = nullptr;
    RequestLogMessages requestLogMessages = nullptr;
    GetProperty getProperty = nullptr;
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
        loadSymbol<ProbeMpv::RequestLogMessages>(handle, "mpv_request_log_messages"),
        loadSymbol<ProbeMpv::GetProperty>(handle, "mpv_get_property"),
    };
}

bool completeApi(const ProbeMpv& api) {
    return api.create && api.initialize && api.terminate && api.option && api.command && api.waitEvent &&
           api.requestLogMessages && api.getProperty;
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
PcmAudio readPcmWave(const std::filesystem::path& path, size_t maximumBytes = kMaximumPcmBytes) {
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
            if (!pcm16Mono || length > maximumBytes || length % 2 != 0) return {};
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

std::optional<std::uint64_t> pcmDataOffset(const std::filesystem::path& path) {
    std::ifstream input(path, std::ios::binary);
    input.seekg(12);
    while (input && input.tellg() < 4096) {
        std::array<unsigned char, 8> chunk{};
        if (!input.read(reinterpret_cast<char*>(chunk.data()), chunk.size())) return std::nullopt;
        if (std::memcmp(chunk.data(), "data", 4) == 0) return static_cast<std::uint64_t>(input.tellg());
        const auto size = le32(chunk.data() + 4);
        if (size > 4096) return std::nullopt;
        input.seekg(static_cast<std::streamoff>(size) + static_cast<std::streamoff>(size % 2), std::ios::cur);
    }
    return std::nullopt;
}

bool capturePcm(const SubtitleAudioSyncRequest& request, const std::filesystem::path& output, std::string& failure,
                int& actualStartMs, std::optional<int>* probeOriginMs = nullptr) {
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
    const bool shortSpeechProbe = request.recognizeSpeech && request.shortSpeechProbe;
    const int sampleSeconds = request.sceneProfileProbe ? std::clamp(request.sourceDurationMs / 1000 + 2, 20, 7680)
                              : request.sceneProbe      ? 65
                              : shortSpeechProbe        ? 120
                              : request.extendedSample  ? kExtendedProbeSeconds
                                                        : kProbeSeconds;
    const uintmax_t readyBytes = request.sceneProfileProbe ? static_cast<uintmax_t>(sampleSeconds) * 16000U * 2U
                                 : request.sceneProbe      ? 60U * 16000U * 2U
                                 : shortSpeechProbe        ? 90U * 16000U * 2U
                                 : request.extendedSample  ? kExtendedProbeReadyBytes
                                                           : kProbeReadyBytes;
    const std::filesystem::path certificate = std::filesystem::path(request.privateFilesDir) / "android-ca-bundle.pem";
    if (!option("config", "no") || !option("video", "no") || !option("vo", "null") || !option("ao", "pcm") ||
        !option("ao-pcm-file", output.string()) || !option("audio-samplerate", "16000") ||
        !option("audio-channels", "mono") || !option("audio-format", "s16") ||
        !option("af", request.sceneProfileProbe && request.centerSpeechChannel ? "lavfi=[pan=mono|c0=FC,ashowinfo]"
                                                                               : "lavfi=[ashowinfo]") ||
        (request.audioOrdinal >= 0 && !option("aid", std::to_string(request.audioOrdinal + 1))) ||
        !option("start", std::to_string(request.sampleStartMs / 1000.0)) ||
        !option("length", std::to_string(sampleSeconds)) || !option("cache", "no") || !option("demuxer", "lavf") ||
        !option("ytdl", "no") ||
        (std::filesystem::exists(certificate) && !option("tls-ca-file", certificate.string())) ||
        !option("tls-verify", "yes"))
        return false;

    if (api.initialize(mpv) < 0) {
        failure = "unable to initialize audio-only libmpv";
        return false;
    }
    api.requestLogMessages(mpv, "v");
    const char* command[] = {"loadfile", request.mediaUrl.c_str(), "replace", nullptr};
    if (api.command(mpv, command) < 0) {
        failure = "audio probe media failed to open";
        return false;
    }
    const auto deadline =
        std::chrono::steady_clock::now() + std::chrono::seconds(request.sceneProfileProbe || request.extendedSample
                                                                    ? kExtendedProbeTimeoutSeconds
                                                                    : kProbeTimeoutSeconds);
    SubtitleProbeClock clock;
    int firstDecodedFrameMs = -1;
    std::optional<std::uint64_t> dataOffset;
    const auto observeFirstFrame = [&](const MpvEvent* event) {
        if (event && event->eventId == kMpvLogMessageEvent && firstDecodedFrameMs < 0) {
            const auto* log = static_cast<const ProbeLogMessage*>(event->data);
            if (log && log->text) {
                if (const auto firstFrameMs = subtitleProbeFirstFrameMs(log->text)) {
                    firstDecodedFrameMs = *firstFrameMs;
                    api.requestLogMessages(mpv, "error");
                }
            }
        }
    };
    const auto updateActualStart = [&] {
        actualStartMs = -1;
        if (clock.inconsistent()) return;
        if (const auto start = clock.startMs()) {
            actualStartMs = subtitleProbeActualStartMs(request.sampleStartMs, *start);
            if (firstDecodedFrameMs >= 0) {
                const int frameStart = subtitleProbeActualStartMs(request.sampleStartMs, firstDecodedFrameMs);
                // Prefer exact frame PTS when independently corroborated. A
                // different mpv context can otherwise misroute FFmpeg logs.
                if (std::abs(frameStart - actualStartMs) <= 250) actualStartMs = frameStart;
            }
        } else if (firstDecodedFrameMs >= 0) {
            actualStartMs = subtitleProbeActualStartMs(request.sampleStartMs, firstDecodedFrameMs);
        }
    };
    while (std::chrono::steady_clock::now() < deadline) {
        if (request.cancelled && request.cancelled()) {
            failure = "audio probe cancelled";
            return false;
        }
        if (probeOriginMs && !*probeOriginMs) {
            double origin = 0.0;
            if (api.getProperty(mpv, "demuxer-start-time", 5, &origin) >= 0 && std::isfinite(origin) &&
                std::abs(origin) <= std::numeric_limits<int>::max() / 1000.0)
                *probeOriginMs = static_cast<int>(std::llround(origin * 1000.0));
        }
        MpvEvent* event = api.waitEvent(mpv, 0.25);
        observeFirstFrame(event);
        // Freeze only this probe while measuring clock and PCM. Unthrottled
        // decoding can advance seconds between otherwise adjacent property reads.
        std::error_code fileError;
        const auto previewBytes = std::filesystem::file_size(output, fileError);
        const char* pauseCommand[] = {"set", "pause", "yes", nullptr};
        const char* resumeCommand[] = {"set", "pause", "no", nullptr};
        const bool measurementPaused = !fileError && previewBytes >= 32000 && api.command(mpv, pauseCommand) >= 0;
        // The pause command can complete before the player refreshes its output
        // clock. Let that refresh finish; all consistency checks still apply.
        if (measurementPaused) std::this_thread::sleep_for(std::chrono::milliseconds(20));
        double beforeSeconds = 0.0;
        const bool beforeValid = api.getProperty(mpv, "time-pos", 5, &beforeSeconds) >= 0;
        const auto bytes = std::filesystem::file_size(output, fileError);
        double afterSeconds = 0.0;
        const bool afterValid = api.getProperty(mpv, "time-pos", 5, &afterSeconds) >= 0;
        if (measurementPaused) static_cast<void>(api.command(mpv, resumeCommand));
        if (!fileError && bytes > 128) {
            if (!dataOffset) dataOffset = pcmDataOffset(output);
            if (measurementPaused && beforeValid && afterValid && dataOffset && bytes >= *dataOffset) {
                clock.observe(beforeSeconds, afterSeconds, bytes - *dataOffset);
            }
        }
        updateActualStart();
        if (!fileError && actualStartMs >= 0 && bytes >= readyBytes) return true;
        if (event && event->eventId == kMpvEndFileEvent) {
            const auto* end = static_cast<const MpvEventEndFile*>(event->data);
            failure = "audio probe end reason=" + std::to_string(end ? end->reason : -1) +
                      " error=" + std::to_string(end ? end->error : event->error) +
                      " wavBytes=" + std::to_string(fileError ? 0 : bytes);
            const bool cleanEnd = end && end->reason == 0;
            // Log events have their own queue and can follow END_FILE for fast
            // local inputs. Drain the first-frame timestamp before returning.
            const auto logDeadline = std::chrono::steady_clock::now() + std::chrono::milliseconds(250);
            while (actualStartMs < 0 && std::chrono::steady_clock::now() < logDeadline) {
                observeFirstFrame(api.waitEvent(mpv, 0.01));
                updateActualStart();
            }
            if (actualStartMs < 0)
                failure += " clockSamples=" + std::to_string(clock.observationCount()) +
                           " spreadMs=" + std::to_string(clock.spreadMs()) +
                           " rawClockMs=" + clock.diagnosticClockSamples() +
                           " firstFrameMs=" + std::to_string(firstDecodedFrameMs) +
                           " startMs=" + std::to_string(request.sampleStartMs);
            return cleanEnd;
        }
    }
    failure = "audio probe deadline exceeded";
    if (actualStartMs < 0)
        failure += " clockSamples=" + std::to_string(clock.observationCount()) +
                   " spreadMs=" + std::to_string(clock.spreadMs()) +
                   " firstFrameMs=" + std::to_string(firstDecodedFrameMs);
    return false;
}
} // namespace

static SubtitleAudioSyncResult captureSubtitleAudioSync(const SubtitleAudioSyncRequest& request,
                                                        SubtitleSyncWindow& window,
                                                        SubtitleDialogueWindow* dialogueWindow = nullptr) {
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
    int actualStartMs = -1;
    std::optional<int> probeOriginMs;
    capturePcm(request, output, result.failure, actualStartMs, &probeOriginMs);
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

    if (request.cancelled && request.cancelled()) {
        result.failure = "audio probe cancelled";
        return result;
    }
    if (actualStartMs < 0) {
        result.failure = "audio probe did not report its decoded starting timestamp: " + captureDiagnostic +
                         " wavBytes=" + std::to_string(wavBytes == static_cast<uintmax_t>(-1) ? 0 : wavBytes) +
                         " sampleStartMs=" + std::to_string(request.sampleStartMs);
        return result;
    }
#ifdef __ANDROID__
    __android_log_print(ANDROID_LOG_INFO, "sloppaTV/subsync",
                        "Audio probe timeline requestedMs=%d actualMs=%d baseMs=%d", request.sampleStartMs,
                        actualStartMs, request.subtitleTimelineBaseMs);
#endif

    result.audioMs = subtitlePcmDurationMs(audio.samples.size(), audio.sampleRate);
    const auto moderate = detectSpeech(audio, 2);
    const auto aggressive = detectSpeech(audio, 3);
    if (moderate.empty() || aggressive.size() != moderate.size()) {
        result.failure = "speech detector rejected the audio format";
        return result;
    }
    int subtitleSampleStartMs = subtitleSyncLogicalSampleStart(actualStartMs, request.subtitleTimelineBaseMs);
    if (request.playbackTimelineOriginMs) {
        if (!probeOriginMs) {
            result.failure = "audio probe did not report its timeline origin";
            return result;
        }
        const auto logical =
            request.playerTimelineBaseMs
                ? subtitleProbePlayerClockStartMs(actualStartMs, *probeOriginMs, *request.playerTimelineBaseMs,
                                                  *request.playbackTimelineOriginMs, request.subtitleTimelineBaseMs,
                                                  request.sampleStartMs)
                : subtitleProbeLogicalStartMs(actualStartMs, request.subtitleTimelineBaseMs, *probeOriginMs,
                                              *request.playbackTimelineOriginMs);
        if (!logical) {
            result.failure = "independent HLS timestamp epoch is incompatible with playback";
            return result;
        }
        subtitleSampleStartMs = *logical;
        result.playbackTimelineOriginMs = request.playbackTimelineOriginMs;
    }
#ifdef __ANDROID__
    __android_log_print(ANDROID_LOG_INFO, "sloppaTV/subsync",
                        "Audio probe origins probeMs=%d playbackMs=%d logicalStartMs=%d", probeOriginMs.value_or(-1),
                        request.playbackTimelineOriginMs.value_or(-1), subtitleSampleStartMs);
#endif
    window = SubtitleSyncWindow{subtitleSampleStartMs, moderate, aggressive};
    const auto evaluation = evaluateSubtitleTimeWarp(moderate, aggressive, request.cues, subtitleSampleStartMs);
    const int matchingCues = evaluation.matchingCues;
    const SubtitleSyncPeak& score2 = evaluation.moderate;
    const SubtitleSyncPeak& score3 = evaluation.aggressive;
    result.decision = evaluation.decision;
    if (!result.decision.confident && subtitleStrongVadGlobalCandidate(score2, score3, result.audioMs, matchingCues))
        result.evidence = "vad-global-candidate";
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
                          " actualSampleStartMs=" + std::to_string(actualStartMs) +
                          " timelineBaseMs=" + std::to_string(request.subtitleTimelineBaseMs) +
                          " isHls=" + std::to_string(request.mediaUrl.find(".m3u8") != std::string::npos);
    }
    if (request.recognizeSpeech && dialogueWindow && audio.sampleRate == 16000) {
        int recognizedWords = 0;
        std::vector<SubtitleDialogueAnchor> allAnchors;
        // Keep independent Whisper work bounded on software-rendered emulators.
        // A 90-second segment can exceed the recognizer's deadline and discard
        // otherwise useful dialogue. Thirty seconds still spans several cues.
        constexpr size_t clipSamples = 30U * 16000U;
        for (size_t begin = 0; begin < audio.samples.size(); begin += clipSamples) {
            if (request.cancelled && request.cancelled()) {
                result.decision.confident = false;
                result.failure = "audio probe cancelled";
                return result;
            }
            const size_t end = std::min(begin + clipSamples, audio.samples.size());
            if (end - begin < 20U * 16000U) break;
            std::vector<float> samples;
            samples.reserve(end - begin);
            for (size_t i = begin; i < end; ++i) samples.push_back(static_cast<float>(audio.samples[i]) / 32768.0F);
            auto words = request.recognizeSpeech(samples);
            // Silence and opening music do not invalidate a later spoken window.
            if (words.empty()) continue;
            const int clipStartMs = static_cast<int>(begin / 16U);
            const int clipDurationMs = static_cast<int>((end - begin) / 16U);
            const int count = static_cast<int>(std::count_if(words.begin(), words.end(), [](const auto& word) {
                return word.confidence >= 0.65 && !subtitleDialogueWords(word.text).empty();
            }));
            recognizedWords = std::max(recognizedWords, count);
            *dialogueWindow = {
                subtitleSampleStartMs + clipStartMs, clipDurationMs,
                matchSubtitleDialogue(words, request.cues, subtitleSampleStartMs + clipStartMs, clipDurationMs),
                recognizedWords};
            allAnchors.insert(allAnchors.end(), dialogueWindow->anchors.begin(), dialogueWindow->anchors.end());
            result.dialogueAnchors = static_cast<int>(dialogueWindow->anchors.size());
            const auto dialogue = evaluateSubtitleDialogue({*dialogueWindow}, false);
            if (!request.sceneProbe && dialogue.confident &&
                result.dialogueAnchors >= request.requiredDialogueAnchors) {
                result.decision = dialogue;
                result.evidence = "dialogue";
                result.failure.clear();
                break;
            }
            if (!request.sceneProbe && request.sourceDurationMs >= 900000 &&
                result.dialogueAnchors >= std::max(6, request.requiredDialogueAnchors)) {
                result.evidence = "dialogue-candidate";
                break; // Only distant windows may authorize a speed correction.
            }
        }
        if (request.sceneProbe) {
            *dialogueWindow = {subtitleSampleStartMs, result.audioMs, std::move(allAnchors), recognizedWords};
            result.dialogueAnchors = static_cast<int>(dialogueWindow->anchors.size());
            result.decision = evaluateSubtitleDialogue({*dialogueWindow}, false);
            result.evidence = "dialogue-scene-sample";
            return result;
        }
        if (result.evidence != "dialogue" && result.evidence != "dialogue-candidate" &&
            static_cast<int>(allAnchors.size()) >= request.requiredDialogueAnchors) {
            *dialogueWindow = {subtitleSampleStartMs, result.audioMs, std::move(allAnchors), recognizedWords};
            result.dialogueAnchors = static_cast<int>(dialogueWindow->anchors.size());
            const auto combined = evaluateSubtitleDialogue({*dialogueWindow}, false);
            if (combined.confident) {
                result.decision = combined;
                result.evidence = "dialogue";
                result.failure.clear();
            } else if (request.sourceDurationMs >= 900000)
                result.evidence = "dialogue-candidate";
        }
#ifdef __ANDROID__
        __android_log_print(ANDROID_LOG_INFO, "sloppaTV/subsync",
                            "Dialogue probe startMs=%d anchors=%d confident=%d offsetMs=%d", subtitleSampleStartMs,
                            result.dialogueAnchors, result.evidence == "dialogue", result.decision.offsetMs);
#endif
        if (result.evidence != "dialogue" && result.evidence != "dialogue-candidate" && request.shortSpeechProbe &&
            !(request.cancelled && request.cancelled())) {
            const int64_t nextLogical = static_cast<int64_t>(subtitleSampleStartMs) + 90000;
            if (recognizedWords > 0 && request.speechWindowRetries < 2 &&
                nextLogical <= std::numeric_limits<int>::max() &&
                (request.sourceDurationMs <= 0 || nextLogical + 20000 < request.sourceDurationMs)) {
                auto next = request;
                ++next.speechWindowRetries;
                if (request.verificationMediaUrl) {
                    const auto url =
                        request.verificationMediaUrl(static_cast<int>(nextLogical) - request.subtitleTimelineBaseMs);
                    if (!url) {
                        result.decision.confident = false;
                        result.failure = "speech retry release negotiation failed";
                        return result;
                    }
                    next.mediaUrl = *url;
                    next.sampleStartMs = 0;
                    // The server restart shifted this independent stream to
                    // nextLogical. Keeping the old base would mislabel 01:30
                    // of audio as 00:00 and manufacture a dialogue conflict.
                    if (!subtitleRebaseHlsRetry(next, request, static_cast<int>(nextLogical))) {
                        result.decision.confident = false;
                        result.failure = "speech retry HLS timeline base overflow";
                        return result;
                    }
                } else {
                    const int64_t position = static_cast<int64_t>(request.sampleStartMs) + 90000;
                    if (position > std::numeric_limits<int>::max()) return result;
                    next.sampleStartMs = static_cast<int>(position);
                }
                auto retried = captureSubtitleAudioSync(next, window, dialogueWindow);
                retried.audioMs += result.audioMs;
                return retried;
            }
            auto longer = request;
            longer.shortSpeechProbe = false;
            // Once speech recognition is available, two further minutes suffice;
            // retain the legacy longer VAD sample when the model is unavailable.
            if (recognizedWords > 0) longer.extendedSample = false;
            auto retried = captureSubtitleAudioSync(longer, window, dialogueWindow);
            retried.audioMs += result.audioMs;
            return retried;
        }
        if (result.evidence != "dialogue" && subtitleHasDialogueEvidence({*dialogueWindow})) {
            result.decision.confident = false;
            result.failure = "recognized speech did not establish a stable subtitle dialogue match";
        }
    }
    return result;
}

static SubtitleSceneProposal captureSubtitleSceneProfile(const SubtitleAudioSyncRequest& request,
                                                         SubtitleAudioSyncResult& result) {
    if (!request.proposeScenes || !request.playerTimelineBaseMs || !request.playbackTimelineOriginMs ||
        request.sourceDurationMs < 20000 || request.sourceDurationMs > 128 * 60000)
        return {};
    static uint64_t serial = 0;
    const auto file = std::filesystem::path(request.privateFilesDir) /
                      ("subtitle-audio-profile-" + std::to_string(++serial) + ".wav");
    int actual = -1;
    std::optional<int> origin;
    std::string failure;
    capturePcm(request, file, failure, actual, &origin);
    auto audio = readPcmWave(file, 128U * 60U * 16000U * 2U);
    std::error_code error;
    std::filesystem::remove(file, error);
    const int duration = subtitlePcmDurationMs(audio.samples.size(), audio.sampleRate);
    if (audio.sampleRate != 16000 ||
        static_cast<int64_t>(duration) * 100 < static_cast<int64_t>(request.sourceDurationMs) * 98 || !origin ||
        actual < 0 || (request.cancelled && request.cancelled())) {
#ifdef __ANDROID__
        __android_log_print(ANDROID_LOG_WARN, "sloppaTV/subsync",
                            "LAPSE profile incomplete durationMs=%d expectedMs=%d actualMs=%d origin=%d", duration,
                            request.sourceDurationMs, actual, origin.has_value());
#endif
        return {};
    }
    const auto start = subtitleProbePlayerClockStartMs(actual, *origin, *request.playerTimelineBaseMs,
                                                       *request.playbackTimelineOriginMs, 0);
    if (!start) return {};
    std::vector<float> samples;
    samples.reserve(audio.samples.size());
    for (const auto sample : audio.samples) samples.push_back(static_cast<float>(sample) / 32768.0F);
    audio.samples.clear();
    audio.samples.shrink_to_fit();
    result.audioMs += duration;
#ifdef __ANDROID__
    __android_log_print(ANDROID_LOG_INFO, "sloppaTV/subsync",
                        "LAPSE profile actualMs=%d originMs=%d logicalMs=%d durationMs=%d center=%d", actual, *origin,
                        *start, duration, request.centerSpeechChannel);
#endif
    return request.proposeScenes(samples, request.cues, *start);
}

SubtitleAudioSyncResult synchronizeSubtitleAudio(const SubtitleAudioSyncRequest& originalRequest) {
    auto request = originalRequest;
    if (request.playbackTimelineOrigin) {
        const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(kProbeTimeoutSeconds);
        while (!request.playbackTimelineOriginMs && std::chrono::steady_clock::now() < deadline) {
            if (request.cancelled && request.cancelled()) return {{}, 0, {}, "audio probe cancelled"};
            request.playbackTimelineOriginMs = request.playbackTimelineOrigin();
            if (!request.playbackTimelineOriginMs) std::this_thread::sleep_for(std::chrono::milliseconds(50));
        }
        if (!request.playbackTimelineOriginMs) return {{}, 0, {}, "playback timeline origin unavailable"};
    }
    SubtitleSyncWindow first;
    SubtitleDialogueWindow firstDialogue;
    const auto remembered =
        request.recallSourceScenes ? request.recallSourceScenes() : std::vector<SubtitleTimelineRegion>{};
    auto firstRequest = request;
    firstRequest.sceneProbe = !remembered.empty();
    auto result = captureSubtitleAudioSync(firstRequest, first, &firstDialogue);
    if (!remembered.empty() && request.playerTimelineBaseMs && request.playbackTimelineOriginMs) {
        const int64_t shift = static_cast<int64_t>(*request.playerTimelineBaseMs) - *request.playbackTimelineOriginMs;
        if (shift >= std::numeric_limits<int>::min() && shift <= std::numeric_limits<int>::max()) {
            auto reused = revalidateSubtitleScenes(remembered, firstDialogue, static_cast<int>(shift));
            if (!reused.empty() && !(request.cancelled && request.cancelled())) {
                result.decision = {true, 0, 0.8, 1.0, std::move(reused)};
                result.evidence = "scene-dialogue-revalidated";
                result.failure.clear();
                return result;
            }
        }
    }
    // Quiet/currently unverified dialogue may not validate a cache hit. Try one
    // bounded, previously supported region before doing any distant analysis.
    if (!remembered.empty() && request.verificationMediaUrl && request.playerTimelineBaseMs &&
        request.playbackTimelineOriginMs && !(request.cancelled && request.cancelled())) {
        const auto best = std::max_element(remembered.begin(), remembered.end(), [](const auto& a, const auto& b) {
            const auto density = [](const auto& region) {
                const int span = region.lastSubtitleMs - region.firstSubtitleMs;
                return span >= 60000 ? static_cast<double>(region.anchors) / span : 0.0;
            };
            return density(a) < density(b);
        });
        const auto choices = subtitleSceneVerificationStarts(*best, request.cues);
        const int start =
            choices.empty() ? static_cast<int>(std::max(0.0, best->spoken(best->firstSubtitleMs))) : choices.front();
        const int64_t relative = static_cast<int64_t>(start) - request.subtitleTimelineBaseMs;
        const int64_t shift = static_cast<int64_t>(*request.playerTimelineBaseMs) - *request.playbackTimelineOriginMs;
        if (relative >= std::numeric_limits<int>::min() && relative <= std::numeric_limits<int>::max() &&
            shift >= std::numeric_limits<int>::min() && shift <= std::numeric_limits<int>::max()) {
            const auto media = request.verificationMediaUrl(static_cast<int>(relative));
            if (media) {
                auto check = request;
                check.mediaUrl = *media;
                check.sampleStartMs = 0;
                check.subtitleTimelineBaseMs = start;
                check.sceneProbe = true;
                check.extendedSample = false;
                check.playbackTimelineOrigin = {};
                SubtitleSyncWindow activity;
                SubtitleDialogueWindow fresh;
                auto captured = captureSubtitleAudioSync(check, activity, &fresh);
                auto reused = revalidateSubtitleScenes(remembered, fresh, static_cast<int>(shift));
                if (!reused.empty() && !(request.cancelled && request.cancelled())) {
                    captured.decision = {true, 0, 0.8, 1.0, std::move(reused)};
                    captured.evidence = "scene-dialogue-revalidated";
                    captured.failure.clear();
                    return captured;
                }
            }
        }
    }
    // Short media cannot establish a speed correction across distant windows.
    const bool candidateVadGlobal = result.evidence == "vad-global-candidate";
    if ((!result.decision.confident && firstDialogue.anchors.size() < 4 && !candidateVadGlobal) ||
        request.sourceDurationMs < 900000)
        return result;
    std::vector<SubtitleSyncWindow> windows;
    windows.push_back(std::move(first));
    std::vector<SubtitleDialogueWindow> dialogueWindows;
    dialogueWindows.push_back(std::move(firstDialogue));
    // With a late HLS restart, negotiate earlier windows of the same release.
    const int scopeBaseMs = request.verificationMediaUrl ? 0 : request.subtitleTimelineBaseMs;
    const int relativeBaseMs = scopeBaseMs - request.subtitleTimelineBaseMs;
    const int lastStartMs = request.sourceDurationMs - scopeBaseMs - 300000;
    const std::vector<int> starts{relativeBaseMs, relativeBaseMs + lastStartMs / 2, relativeBaseMs + lastStartMs};
    // Remux can give each independent verification HLS session an unrelated
    // transport PTS epoch. Map it to the requested episode time instead. This
    // also fixes the case where a 00:00 probe was mislabelled as 39:27.
    const auto retargetWindow = [&](SubtitleAudioSyncRequest& window, int relativePositionMs) {
        const int64_t logicalBase = static_cast<int64_t>(request.subtitleTimelineBaseMs) + relativePositionMs;
        if (logicalBase < 0 || logicalBase > std::numeric_limits<int>::max()) return false;
        window.subtitleTimelineBaseMs = static_cast<int>(logicalBase);
        window.playbackTimelineOrigin = {};
        if (!window.playerTimelineBaseMs) window.playbackTimelineOriginMs.reset();
        if (request.verificationMediaUrl) {
            const auto verify = request.verificationMediaUrl;
            window.verificationMediaUrl = [verify, relativePositionMs](int nextMs) {
                const int64_t offset = static_cast<int64_t>(relativePositionMs) + nextMs;
                if (offset < std::numeric_limits<int>::min() || offset > std::numeric_limits<int>::max())
                    return std::optional<std::string>{};
                return verify(static_cast<int>(offset));
            };
        }
        return true;
    };
    for (int count = 0; count < 2; ++count) {
        std::optional<int> selectedStartMs;
        int greatestDistanceMs = 0;
        for (int start : starts) {
            int distanceMs = std::numeric_limits<int>::max();
            for (const auto& window : windows)
                distanceMs = std::min(distanceMs, std::abs(start + request.subtitleTimelineBaseMs - window.startMs));
            if (distanceMs > greatestDistanceMs) {
                greatestDistanceMs = distanceMs;
                selectedStartMs = start;
            }
        }
        if (!selectedStartMs || greatestDistanceMs < 180000) break;
        auto verification = request;
        verification.sampleStartMs = *selectedStartMs;
        if (request.verificationMediaUrl) {
            const auto url = request.verificationMediaUrl(*selectedStartMs);
            if (!url) {
                result.decision.confident = false;
                result.failure = "distant subtitle probe release negotiation failed";
                return result;
            }
            verification.mediaUrl = *url;
            verification.sampleStartMs = 0;
        }
        if (request.verificationMediaUrl && !retargetWindow(verification, *selectedStartMs)) {
            result.decision.confident = false;
            result.failure = "distant subtitle verification timeline base overflow";
            return result;
        }
        verification.extendedSample = false;
        if (candidateVadGlobal) {
            // The initial recognition already found no reliable caption text.
            // Verify VAD only; do not rerun a costly speech model on two more
            // sections just to adjudicate a global fixed-offset candidate.
            verification.recognizeSpeech = {};
            verification.shortSpeechProbe = false;
        }
        SubtitleSyncWindow window;
        SubtitleDialogueWindow dialogueWindow;
        const auto captured = captureSubtitleAudioSync(verification, window, &dialogueWindow);
        if (window.moderate.empty()) {
            result.decision.confident = false;
            result.failure = "distant subtitle verification unavailable: " + captured.failure;
            return result;
        }
        result.audioMs += captured.audioMs;
        windows.push_back(std::move(window));
        result.dialogueAnchors += captured.dialogueAnchors;
        dialogueWindows.push_back(std::move(dialogueWindow));
    }
    auto dialogue = evaluateSubtitleDialogue(dialogueWindows);
    // Speed verification needs six independent phrases in every window. Gather
    // more speech where a shorter fixed-offset sample did not supply enough.
    if (!dialogue.confident && result.dialogueAnchors >= 12) {
        for (size_t i = 0; i < dialogueWindows.size(); ++i) {
            if (dialogueWindows[i].anchors.size() >= 6) continue;
            auto verification = request;
            verification.shortSpeechProbe = false;
            verification.extendedSample = false;
            verification.requiredDialogueAnchors = 6;
            verification.sampleStartMs = dialogueWindows[i].startMs - request.subtitleTimelineBaseMs;
            if (request.verificationMediaUrl) {
                const auto url = request.verificationMediaUrl(verification.sampleStartMs);
                if (!url) continue;
                verification.mediaUrl = *url;
                verification.sampleStartMs = 0;
            } else if (verification.sampleStartMs < 0)
                continue;
            if (request.verificationMediaUrl &&
                !retargetWindow(verification, dialogueWindows[i].startMs - request.subtitleTimelineBaseMs))
                continue;
            SubtitleSyncWindow extra;
            SubtitleDialogueWindow extraDialogue;
            const auto captured = captureSubtitleAudioSync(verification, extra, &extraDialogue);
            if (extraDialogue.anchors.size() > dialogueWindows[i].anchors.size()) {
                result.dialogueAnchors -= static_cast<int>(dialogueWindows[i].anchors.size());
                result.dialogueAnchors += captured.dialogueAnchors;
                dialogueWindows[i] = std::move(extraDialogue);
                if (!extra.moderate.empty()) windows[i] = std::move(extra);
            }
            result.audioMs += captured.audioMs;
        }
        dialogue = evaluateSubtitleDialogue(dialogueWindows);
    }
    // A freely estimated speed needs more evidence than a known PAL/NTSC
    // conversion: four sparse anchors could otherwise hide a cut scene. Verify
    // the same slope in TWO previously unseen quarters of the episode before
    // changing caption playback speed. Never alter the active HLS session.
    const std::vector<double> knownRates{1.0,         1000.0 / 1001.0,  1001.0 / 1000.0, 24.0 / 25.0, 24000.0 / 25025.0,
                                         25.0 / 24.0, 25025.0 / 24000.0};
    const bool novelRate = dialogue.confident && std::all_of(knownRates.begin(), knownRates.end(), [&](double rate) {
                               return std::abs(dialogue.timeScale - rate) > 0.001;
                           });
    if (novelRate) {
        const SubtitleSyncDecision initialFit = dialogue;
        const int holdoutScopeBase = request.verificationMediaUrl ? 0 : request.subtitleTimelineBaseMs;
        const int lastHoldoutStart = request.sourceDurationMs - holdoutScopeBase - 300000;
        for (int quarter : {1, 3}) {
            if (lastHoldoutStart <= 0 || (request.cancelled && request.cancelled())) {
                dialogue.confident = false;
                break;
            }
            const int64_t position = static_cast<int64_t>(holdoutScopeBase - request.subtitleTimelineBaseMs) +
                                     static_cast<int64_t>(lastHoldoutStart) * quarter / 4;
            if (position < std::numeric_limits<int>::min() || position > std::numeric_limits<int>::max()) {
                dialogue.confident = false;
                break;
            }
            auto holdout = request;
            holdout.sampleStartMs = static_cast<int>(position);
            if (request.verificationMediaUrl) {
                const auto media = request.verificationMediaUrl(holdout.sampleStartMs);
                if (!media) {
                    dialogue.confident = false;
                    break;
                }
                holdout.mediaUrl = *media;
                holdout.sampleStartMs = 0;
            } else if (holdout.sampleStartMs < 0) {
                dialogue.confident = false;
                break;
            }
            if (request.verificationMediaUrl && !retargetWindow(holdout, static_cast<int>(position))) {
                dialogue.confident = false;
                break;
            }
            holdout.extendedSample = false;
            holdout.requiredDialogueAnchors = 6;
            SubtitleSyncWindow speechHoldout;
            SubtitleDialogueWindow dialogueHoldout;
            const auto captured = captureSubtitleAudioSync(holdout, speechHoldout, &dialogueHoldout);
            result.audioMs += captured.audioMs;
            result.dialogueAnchors += captured.dialogueAnchors;
            if (dialogueHoldout.anchors.size() < 6) {
                dialogue.confident = false;
                break;
            }
            dialogueWindows.push_back(std::move(dialogueHoldout));
            dialogue = evaluateSubtitleDialogue(dialogueWindows);
            if (!dialogue.confident || std::abs(dialogue.timeScale - initialFit.timeScale) > 0.00025 ||
                std::abs(dialogue.offsetMs - initialFit.offsetMs) > 500) {
                dialogue.confident = false;
                break;
            }
        }
        if (!dialogue.confident) {
            result.decision.confident = false;
            result.failure = "nonstandard subtitle speed failed independent holdout verification";
        }
    }
    if (dialogue.confident) {
        result.decision = dialogue;
        result.evidence = "dialogue";
        result.failure.clear();
        return result;
    }
    // LAPSE proposes scene boundaries from the full Silero speech profile.
    // Verify each proposed region against fresh recognized dialogue; a VAD
    // coincidence alone never authorizes a playback correction.
    if (request.recognizeSpeech && request.proposeScenes && request.verificationMediaUrl &&
        subtitleHasDialogueEvidence(dialogueWindows)) {
        const auto sceneDeadline = std::chrono::steady_clock::now() + std::chrono::minutes(10);
        auto scene = request;
        scene.sceneProfileProbe = true;
        scene.sampleStartMs = 0;
        scene.subtitleTimelineBaseMs = 0;
        scene.cancelled = [cancelled = request.cancelled, sceneDeadline] {
            return (cancelled && cancelled()) || std::chrono::steady_clock::now() >= sceneDeadline;
        };
        if (!request.profileMediaUrl.empty()) {
            scene.mediaUrl = request.profileMediaUrl;
            scene.audioOrdinal = request.profileAudioOrdinal;
        } else {
            const auto media = request.verificationMediaUrl(-request.subtitleTimelineBaseMs);
            if (!media) return result;
            scene.mediaUrl = *media;
        }
        const auto proposal = captureSubtitleSceneProfile(scene, result);
        std::vector<SubtitleTimelineRegion> regions;
        for (auto candidate : proposal.regions) {
            int passed = 0;
            int anchors = 0;
            const double firstSource = candidate.spoken(candidate.firstSubtitleMs);
            const double lastSource = candidate.spoken(candidate.lastSubtitleMs);
            const int required = lastSource - firstSource >= 180000 ? 2 : 1;
            std::vector<int> verifiedStarts;
            int attempts = 0;
            const double separation = std::max(60000.0, (lastSource - firstSource) / 4);
            for (const int start : subtitleSceneVerificationStarts(candidate, request.cues)) {
                if (scene.cancelled() || attempts >= 4) break;
                if (std::any_of(verifiedStarts.begin(), verifiedStarts.end(),
                                [&](int previous) { return std::abs(start - previous) < separation; }))
                    continue;
                ++attempts;
                auto verify = request;
                verify.sceneProbe = true;
                verify.speechWindowRetries = 2;
                verify.cancelled = scene.cancelled;
                verify.sampleStartMs = 0;
                const int relative = start - request.subtitleTimelineBaseMs;
                const auto url = request.verificationMediaUrl(relative);
                if (!url || !retargetWindow(verify, relative)) break;
                verify.mediaUrl = *url;
                SubtitleSyncWindow activity;
                SubtitleDialogueWindow fresh;
                const auto captured = captureSubtitleAudioSync(verify, activity, &fresh);
                result.audioMs += captured.audioMs;
                result.dialogueAnchors += captured.dialogueAnchors;
                if (!revalidateSubtitleScenes({candidate}, fresh, 0).empty()) {
                    ++passed;
                    verifiedStarts.push_back(start);
                    anchors += static_cast<int>(fresh.anchors.size());
                }
#ifdef __ANDROID__
                __android_log_print(
                    ANDROID_LOG_INFO, "sloppaTV/subsync",
                    "LAPSE verification firstMs=%d lastMs=%d offsetMs=%d startMs=%d anchors=%d passed=%d required=%d",
                    candidate.firstSubtitleMs, candidate.lastSubtitleMs, candidate.offsetMs, start,
                    captured.dialogueAnchors, passed, required);
#endif
                if (passed >= required) break;
            }
            if (passed >= required) {
                candidate.anchors = anchors;
                regions.push_back(candidate);
            }
        }
        if (!subtitleTimelineValid(regions) || scene.cancelled()) regions.clear();
        if (!regions.empty() && !(request.cancelled && request.cancelled())) {
            if (request.rememberSourceScenes && request.playerTimelineBaseMs && request.playbackTimelineOriginMs) {
                auto source = regions;
                const int64_t shift =
                    static_cast<int64_t>(*request.playerTimelineBaseMs) - *request.playbackTimelineOriginMs;
                bool valid = true;
                for (auto& region : source) {
                    const int64_t offset = static_cast<int64_t>(region.offsetMs) - shift;
                    if (std::abs(offset) > 120000) {
                        valid = false;
                        break;
                    }
                    region.offsetMs = static_cast<int>(offset);
                }
                if (valid && subtitleTimelineValid(source)) request.rememberSourceScenes(source);
            }
            result.decision = {true, 0, 0.8, 1.0, std::move(regions)};
            result.evidence = "lapse-scene-dialogue";
            result.failure.clear();
            return result;
        }
    }
    // Conflicting recognized dialogue must not be overruled by a VAD coincidence.
    if (subtitleHasDialogueEvidence(dialogueWindows) &&
        (!candidateVadGlobal || std::any_of(dialogueWindows.begin(), dialogueWindows.end(),
                                            [](const auto& window) { return !window.anchors.empty(); }))) {
        result.decision.confident = false;
        result.failure = "recognized dialogue did not establish one consistent episode timeline";
        return result;
    }
    result.evidence = "vad";
    result.decision = evaluateSubtitleSyncWindows(windows, request.cues);
    if (!result.decision.confident)
        result.failure = "subtitle windows did not establish one consistent episode timeline";
    return result;
}
