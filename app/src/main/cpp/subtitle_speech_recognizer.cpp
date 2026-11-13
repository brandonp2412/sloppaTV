#include "subtitle_speech_recognizer.hpp"
#include "subtitle_speech_model.hpp"
#include <whisper.h>
#include <chrono>
#include <android/log.h>
#include <mutex>
#include <thread>

struct SubtitleSpeechRecognizer::Impl {
    std::mutex mutex;
    whisper_context* context = nullptr;

    ~Impl() {
        if (context) whisper_free(context);
    }
};

SubtitleSpeechRecognizer::SubtitleSpeechRecognizer(JavaVM* vm, jobject activity)
    : impl_(std::make_unique<Impl>()), http_(vm, activity) {}

SubtitleSpeechRecognizer::~SubtitleSpeechRecognizer() = default;

void SubtitleSpeechRecognizer::cancelPending() {
    http_.cancelPending();
}

std::vector<SubtitleSpeechWord> SubtitleSpeechRecognizer::recognize(const std::vector<float>& audio,
                                                                    const std::string& directory,
                                                                    const std::function<bool()>& cancelled) {
    std::scoped_lock lock(impl_->mutex);
    if (audio.empty() || audio.size() > 120U * 16000U || (cancelled && cancelled())) return {};
    if (!impl_->context) {
        const auto model = ensureSubtitleSpeechModel(http_, directory, cancelled);
        if (model.empty() || (cancelled && cancelled())) return {};
        auto options = whisper_context_default_params();
        options.use_gpu = false;
        impl_->context = whisper_init_from_file_with_params(model.c_str(), options);
        if (!impl_->context) return {};
    }
    const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(90);
    std::function<bool()> abort = [&] {
        return (cancelled && cancelled()) || std::chrono::steady_clock::now() >= deadline;
    };
    auto params = whisper_full_default_params(WHISPER_SAMPLING_GREEDY);
    params.n_threads = static_cast<int>(std::clamp(std::thread::hardware_concurrency() / 2U, 1U, 2U));
    params.language = "en";
    params.translate = false;
    params.no_context = true;
    params.print_progress = false;
    params.print_realtime = false;
    params.print_timestamps = false;
    params.print_special = false;
    params.token_timestamps = true;
    params.max_len = 1;
    params.split_on_word = true;
    params.greedy.best_of = 1;
    // No subtitle text is supplied as a prompt: recognition is independent evidence.
    params.initial_prompt = nullptr;
    params.abort_callback = [](void* data) { return (*static_cast<const std::function<bool()>*>(data))(); };
    params.abort_callback_user_data = &abort;
    params.encoder_begin_callback = [](whisper_context*, whisper_state*, void* data) {
        return !(*static_cast<const std::function<bool()>*>(data))();
    };
    params.encoder_begin_callback_user_data = &abort;
    const auto recognitionStart = std::chrono::steady_clock::now();
    const int status = whisper_full(impl_->context, params, audio.data(), static_cast<int>(audio.size()));
    const bool timedOutOrCancelled = abort();
    const auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::steady_clock::now() - recognitionStart).count();
    __android_log_print(ANDROID_LOG_INFO, "sloppaTV/subsync",
                        "Speech recognition inputMs=%zu elapsedMs=%lld status=%d aborted=%d segments=%d",
                        audio.size() / 16U, static_cast<long long>(elapsed), status,
                        timedOutOrCancelled, status == 0 ? whisper_full_n_segments(impl_->context) : 0);
    if (status != 0 || timedOutOrCancelled) return {};
    std::vector<SubtitleSpeechWord> words;
    for (int segment = 0; segment < whisper_full_n_segments(impl_->context); ++segment) {
        const auto* text = whisper_full_get_segment_text(impl_->context, segment);
        if (!text || subtitleDialogueWords(text).empty()) continue;
        double confidence = 0.0;
        int count = 0;
        for (int token = 0; token < whisper_full_n_tokens(impl_->context, segment); ++token) {
            const auto* tokenText = whisper_full_get_token_text(impl_->context, segment, token);
            if (tokenText && !subtitleDialogueWords(tokenText).empty()) {
                confidence += whisper_full_get_token_p(impl_->context, segment, token);
                ++count;
            }
        }
        const auto start = whisper_full_get_segment_t0(impl_->context, segment) * 10;
        const auto end = whisper_full_get_segment_t1(impl_->context, segment) * 10;
        const auto duration = static_cast<int64_t>(audio.size()) * 1000 / 16000;
        if (count == 0 || start < 0 || end < start || end > duration) continue;
        words.push_back({static_cast<int>(start), static_cast<int>(end), text, confidence / count});
    }
    return words;
}
