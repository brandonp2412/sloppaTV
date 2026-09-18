#pragma once

#include "jellyfin_types.hpp"
#include "subtitle_policy.hpp"

#include <cstdint>
#include <filesystem>
#include <fstream>
#include <functional>
#include <iterator>
#include <string>
#include <utility>
#include <vector>

struct SubtitleLoadRequest {
    uint64_t generation = 0;
    std::string itemId;
    std::string mediaSourceId;
    int requestedSubtitleIndex = -1;
    std::vector<JellyfinSubtitleStream> candidates;
    std::string deliveryUrl;
    std::string dataPath;
};

struct SubtitleLoadCompletion {
    uint64_t generation = 0;
    std::string itemId;
    int requestedSubtitleIndex = -1;
    JellyfinSubtitleStream loadedSubtitle;
    std::vector<SubtitleCue> cues;
};

enum class SubtitleLoadDiagnosticKind {
    Fallback,
    Failure,
};

struct SubtitleLoadDiagnostic {
    SubtitleLoadDiagnosticKind kind = SubtitleLoadDiagnosticKind::Failure;
    std::string itemId;
    int requestedSubtitleIndex = -1;
    int subtitleIndex = -1;
    std::string codec;
    std::string reason;
};

template <typename Client, typename TaskRunner, typename CompletionSink, typename Epoch> class SubtitleLoadExecutor {
public:
    using DiagnosticSink = std::function<void(const SubtitleLoadDiagnostic&)>;

    SubtitleLoadExecutor(Client& client, TaskRunner& tasks, CompletionSink& completions, const Epoch& epoch,
                         DiagnosticSink diagnostics = {})
        : client_(client), tasks_(tasks), completions_(completions), epoch_(epoch),
          diagnostics_(std::move(diagnostics)) {}

    bool load(JellyfinSession session, SubtitleLoadRequest request) {
        return tasks_.submit([this, session = std::move(session), request = std::move(request)] {
            JellyfinItem subtitleItem;
            subtitleItem.id = request.itemId;
            subtitleItem.mediaSourceId = request.mediaSourceId;

            JellyfinSubtitleStream loadedSubtitle;
            std::vector<SubtitleCue> loadedCues;

            for (size_t candidateIndex = 0; candidateIndex < request.candidates.size(); ++candidateIndex) {
                const JellyfinSubtitleStream& candidate = request.candidates[candidateIndex];
                std::string subtitleBody;
                std::filesystem::path cacheFile;
                bool fromCache = false;
                if (!request.dataPath.empty()) {
                    const std::filesystem::path directory = std::filesystem::path(request.dataPath) / "subtitles";
                    const std::string format = subtitleTextFormat(candidate.codec);
                    cacheFile = directory / (request.itemId + "-" + std::to_string(candidate.index) + "." +
                                             (format.empty() ? "txt" : format));
                    std::ifstream cached(cacheFile, std::ios::binary);
                    if (cached) {
                        subtitleBody.assign(std::istreambuf_iterator<char>(cached), std::istreambuf_iterator<char>());
                        fromCache = !subtitleBody.empty();
                    }
                }

                const auto fetchSubtitle = [&] {
                    auto response =
                        client_.downloadSubtitleText(session, subtitleItem, candidate.index, candidate.codec);
                    if ((!response.ok || response.value.empty()) && candidateIndex == 0 &&
                        !request.deliveryUrl.empty()) {
                        response = client_.downloadSubtitleUrl(session, request.deliveryUrl);
                    }
                    if ((!response.ok || response.value.empty()) && subtitleMayFallbackToSrt(candidate.codec)) {
                        response = client_.downloadSubtitleSrt(session, subtitleItem, candidate.index);
                    }
                    return response;
                };

                std::string subtitleFailure;
                if (subtitleBody.empty()) {
                    auto response = fetchSubtitle();
                    if (response.ok && !response.value.empty()) {
                        subtitleBody = std::move(response.value);
                        if (!cacheFile.empty()) {
                            std::error_code ec;
                            std::filesystem::create_directories(cacheFile.parent_path(), ec);
                            if (!ec) {
                                std::ofstream output(cacheFile, std::ios::binary | std::ios::trunc);
                                if (output)
                                    output.write(subtitleBody.data(), static_cast<std::streamsize>(subtitleBody.size()));
                            }
                        }
                    } else {
                        subtitleFailure = response.error.empty() ? "empty subtitle response" : response.error;
                    }
                }

                std::vector<SubtitleCue> cues = parseTextSubtitleCues(subtitleBody, candidate.codec);
                if (cues.empty() && fromCache) {
                    std::error_code ec;
                    std::filesystem::remove(cacheFile, ec);
                    auto response = fetchSubtitle();
                    if (response.ok) {
                        subtitleBody = std::move(response.value);
                        cues = parseTextSubtitleCues(subtitleBody, candidate.codec);
                    } else {
                        subtitleFailure =
                            response.error.empty() ? "subtitle cache refresh failed" : response.error;
                    }
                }

                if (!cues.empty()) {
                    loadedSubtitle = candidate;
                    loadedCues = std::move(cues);
                    if (candidate.index != request.requestedSubtitleIndex && diagnostics_) {
                        diagnostics_(SubtitleLoadDiagnostic{
                            .kind = SubtitleLoadDiagnosticKind::Fallback,
                            .itemId = request.itemId,
                            .requestedSubtitleIndex = request.requestedSubtitleIndex,
                            .subtitleIndex = candidate.index,
                            .codec = candidate.codec,
                            .reason = {},
                        });
                    }
                    break;
                }

                if (subtitleFailure.empty()) {
                    subtitleFailure =
                        subtitleBody.empty() ? "subtitle body was empty" : "subtitle contained no parseable text cues";
                }
                if (diagnostics_) {
                    diagnostics_(SubtitleLoadDiagnostic{
                        .kind = SubtitleLoadDiagnosticKind::Failure,
                        .itemId = request.itemId,
                        .requestedSubtitleIndex = request.requestedSubtitleIndex,
                        .subtitleIndex = candidate.index,
                        .codec = candidate.codec,
                        .reason = std::move(subtitleFailure),
                    });
                }
            }

            if (!epoch_.active(request.generation)) return;
            completions_.push(SubtitleLoadCompletion{
                .generation = request.generation,
                .itemId = request.itemId,
                .requestedSubtitleIndex = request.requestedSubtitleIndex,
                .loadedSubtitle = std::move(loadedSubtitle),
                .cues = std::move(loadedCues),
            });
        });
    }

private:
    Client& client_;
    TaskRunner& tasks_;
    CompletionSink& completions_;
    const Epoch& epoch_;
    DiagnosticSink diagnostics_;
};
