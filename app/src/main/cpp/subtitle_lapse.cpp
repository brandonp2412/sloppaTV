#include "subtitle_lapse.hpp"
#include "third_party/lapse/align.h"
#include "third_party/lapse/correlate.h"
#include "third_party/lapse/cuetext.h"
#include "third_party/lapse/silero.h"
#include "third_party/lapse/srt_parser.h"
#include <algorithm>
#include <cmath>
#include <chrono>
#include <cstdlib>
#include <mutex>
#include <span>

SubtitleSceneProposal proposeSubtitleLapse(const std::vector<float>& audio, const std::vector<SubtitleCue>& cues,
                                           int logicalStartMs, const std::string& model,
                                           const std::function<bool()>& cancelled) {
    static std::mutex engineMutex;
    std::scoped_lock lock(engineMutex);
    const auto deadline = std::chrono::steady_clock::now() + std::chrono::minutes(2);
    const auto stop = [&] { return (cancelled && cancelled()) || std::chrono::steady_clock::now() >= deadline; };
    if (audio.size() < 20U * 16000U || audio.size() > 128U * 60U * 16000U || cues.size() < 10 || cues.size() > 20000 ||
        logicalStartMs < 0 || model.empty() || stop())
        return {};
    // Use upstream's Silero implementation and normalization. Only PCM decoding
    // comes from the player's independent, source-checked libmpv session.
    setenv("LAPSE_VAD_MODEL", model.c_str(), 1);
    if (!silero_open()) return {};

    struct Cleanup {
        ~Cleanup() { silero_close(); }
    } cleanup;

    const auto profilePiece = [&](std::span<const float> audio) -> std::vector<float> {
        std::vector<float> loudness;
        for (size_t at = 0; at + 160 <= audio.size(); at += 160) {
            double energy = 0;
            for (size_t i = at; i < at + 160; ++i) energy += audio[i] * static_cast<double>(audio[i]);
            loudness.push_back(static_cast<float>(std::sqrt(energy / 160)));
        }
        if (loudness.empty()) return {};
        auto levels = loudness;
        const size_t percentile = levels.size() * 9 / 10;
        std::nth_element(levels.begin(), levels.begin() + static_cast<std::ptrdiff_t>(percentile), levels.end());
        const double boost = std::clamp(0.1 / std::max(static_cast<double>(levels[percentile]), 1e-5), 1.0, 12.0);
        const size_t total = audio.size() / SILERO_WINDOW;
        Lanes run;
        const int lanes = silero_begin(run, total >= 8U * 64U ? 8 : 1);
        if (lanes < 1) return std::vector<float>{};
        const size_t per = (total + static_cast<size_t>(lanes) - 1) / static_cast<size_t>(lanes);
        std::vector<float> feed(static_cast<size_t>(lanes) * SILERO_WINDOW), got(static_cast<size_t>(lanes));
        std::vector<float> windows(total);
        for (size_t step = 0; step < per; ++step) {
            if (stop()) return std::vector<float>{};
            for (int lane = 0; lane < lanes; ++lane) {
                const size_t window = static_cast<size_t>(lane) * per + step;
                for (size_t i = 0; i < SILERO_WINDOW; ++i)
                    feed[static_cast<size_t>(lane) * SILERO_WINDOW + i] =
                        window < total
                            ? static_cast<float>(std::clamp(audio[window * SILERO_WINDOW + i] * boost, -1.0, 1.0))
                            : 0.0F;
            }
            if (!silero_step(run, feed.data(), got.data())) return std::vector<float>{};
            for (int lane = 0; lane < lanes; ++lane) {
                const size_t window = static_cast<size_t>(lane) * per + step;
                if (window < total) windows[window] = got[static_cast<size_t>(lane)];
            }
        }
        std::vector<float> probabilities(loudness.size());
        for (size_t i = 0; i < probabilities.size(); ++i) {
            const size_t window = (i * 10 + 5) / 32;
            probabilities[i] = window < windows.size() ? windows[window] : 0.0F;
        }
        return probabilities;
    };
    std::vector<float> probabilities;
    constexpr size_t pieceSamples = 180U * 16000U;
    for (size_t offset = 0; offset + 160 <= audio.size(); offset += pieceSamples) {
        const auto piece =
            profilePiece(std::span(audio).subspan(offset, std::min(pieceSamples, audio.size() - offset)));
        if (piece.empty() || stop()) return {};
        probabilities.insert(probabilities.end(), piece.begin(), piece.end());
    }
    auto [speech, weights] = reference_spans(probabilities);
    lead_in(speech);
    for (auto& span : speech) {
        span.first += logicalStartMs;
        span.second += logicalStartMs;
    }
    return proposeSubtitleLapseAlignment(cues, speech, weights, cancelled);
}
