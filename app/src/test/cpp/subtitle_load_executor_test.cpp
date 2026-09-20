#include "subtitle_load_executor.hpp"

#include <cassert>
#include <filesystem>
#include <fstream>
#include <functional>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

namespace {
constexpr const char* kValidSrt = "1\n00:00:01,000 --> 00:00:02,000\nHello\n";

struct ImmediateTaskRunner {
    bool submit(std::function<void()> task) {
        if (!accept) return false;
        ++submissions;
        task();
        return true;
    }

    bool accept = true;
    int submissions = 0;
};

struct CompletionSink {
    void push(SubtitleLoadCompletion completion) { events.push_back(std::move(completion)); }

    std::vector<SubtitleLoadCompletion> events;
};

struct Epoch {
    bool active(uint64_t generation) const { return generation == activeGeneration; }

    uint64_t activeGeneration = 5;
};

struct FakeClient {
    ApiValueResult<std::string> downloadSubtitleText(const JellyfinSession&, const JellyfinItem& item,
                                                     int subtitleIndex, const std::string&) {
        calls.push_back("text:" + std::to_string(subtitleIndex));
        lastItemId = item.id;
        lastMediaSourceId = item.mediaSourceId;
        return response(textResponses, subtitleIndex);
    }

    ApiValueResult<std::string> downloadSubtitleUrl(const JellyfinSession&, const std::string&) {
        calls.push_back("url");
        return deliveryResponse;
    }

    ApiValueResult<std::string> downloadSubtitleSrt(const JellyfinSession&, const JellyfinItem& item,
                                                    int subtitleIndex) {
        calls.push_back("srt:" + std::to_string(subtitleIndex));
        lastItemId = item.id;
        lastMediaSourceId = item.mediaSourceId;
        return response(srtResponses, subtitleIndex);
    }

    static ApiValueResult<std::string> response(const std::unordered_map<int, ApiValueResult<std::string>>& responses,
                                                int index) {
        const auto found = responses.find(index);
        if (found != responses.end()) return found->second;
        ApiValueResult<std::string> result;
        result.ok = false;
        result.error = "missing";
        return result;
    }

    std::unordered_map<int, ApiValueResult<std::string>> textResponses;
    std::unordered_map<int, ApiValueResult<std::string>> srtResponses;
    ApiValueResult<std::string> deliveryResponse;
    std::vector<std::string> calls;
    std::string lastItemId;
    std::string lastMediaSourceId;
};

ApiValueResult<std::string> success(std::string body) {
    ApiValueResult<std::string> result;
    result.ok = true;
    result.value = std::move(body);
    return result;
}

JellyfinSubtitleStream subtitle(int index, std::string codec, std::string language = "eng") {
    JellyfinSubtitleStream stream;
    stream.index = index;
    stream.codec = std::move(codec);
    stream.language = std::move(language);
    return stream;
}

SubtitleLoadRequest request(int subtitleIndex, std::vector<JellyfinSubtitleStream> candidates,
                            std::string deliveryUrl = {}, std::string dataPath = {}, uint64_t generation = 5) {
    return SubtitleLoadRequest{
        .generation = generation,
        .itemId = "episode-1",
        .mediaSourceId = "source-1",
        .requestedSubtitleIndex = subtitleIndex,
        .candidates = std::move(candidates),
        .deliveryUrl = std::move(deliveryUrl),
        .dataPath = std::move(dataPath),
    };
}
} // namespace

int main() {
    JellyfinSession session;
    session.server = "https://jellyfin.example.nz";
    session.userId = "user-1";

    FakeClient client;
    ImmediateTaskRunner tasks;
    CompletionSink completions;
    Epoch epoch;
    std::vector<SubtitleLoadDiagnostic> diagnostics;
    SubtitleLoadExecutor executor(client, tasks, completions, epoch,
                                  [&](const SubtitleLoadDiagnostic& diagnostic) { diagnostics.push_back(diagnostic); });

    client.textResponses[2] = success(kValidSrt);
    assert(executor.load(session, request(2, {subtitle(2, "srt")})));
    assert(tasks.submissions == 1);
    assert((client.calls == std::vector<std::string>{"text:2"}));
    assert(client.lastItemId == "episode-1");
    assert(client.lastMediaSourceId == "source-1");
    assert(completions.events.back().requestedSubtitleIndex == 2);
    assert(completions.events.back().loadedSubtitle.index == 2);
    assert(completions.events.back().cues.size() == 1);
    assert(diagnostics.empty());

    client.calls.clear();
    client.textResponses.clear();
    client.deliveryResponse = success(kValidSrt);
    assert(executor.load(session, request(3, {subtitle(3, "srt")}, "https://media.example/subtitle")));
    assert((client.calls == std::vector<std::string>{"text:3", "url"}));
    assert(completions.events.back().loadedSubtitle.index == 3);

    client.calls.clear();
    client.deliveryResponse = {};
    client.textResponses[4] = {};
    client.srtResponses[4] = {};
    client.textResponses[5] = success(kValidSrt);
    diagnostics.clear();
    assert(executor.load(session, request(4, {subtitle(4, "srt"), subtitle(5, "srt")})));
    assert(completions.events.back().loadedSubtitle.index == 5);
    assert(diagnostics.size() == 2);
    assert(diagnostics[0].kind == SubtitleLoadDiagnosticKind::Failure);
    assert(diagnostics[0].subtitleIndex == 4);
    assert(diagnostics[1].kind == SubtitleLoadDiagnosticKind::Fallback);
    assert(diagnostics[1].requestedSubtitleIndex == 4);
    assert(diagnostics[1].subtitleIndex == 5);

    const auto cacheRoot = std::filesystem::temp_directory_path() / "sloppatv-subtitle-load-executor-test";
    std::error_code ec;
    std::filesystem::remove_all(cacheRoot, ec);
    std::filesystem::create_directories(cacheRoot / "subtitles");
    {
        std::ofstream cached(cacheRoot / "subtitles" / "episode-1-6.srt", std::ios::binary);
        cached << kValidSrt;
    }
    client.calls.clear();
    assert(executor.load(session, request(6, {subtitle(6, "srt")}, {}, cacheRoot.string())));
    assert(client.calls.empty());
    assert(completions.events.back().loadedSubtitle.index == 6);
    std::filesystem::remove_all(cacheRoot, ec);

    const size_t completionCount = completions.events.size();
    epoch.activeGeneration = 6;
    client.textResponses[7] = success(kValidSrt);
    assert(executor.load(session, request(7, {subtitle(7, "srt")})));
    assert(completions.events.size() == completionCount);

    tasks.accept = false;
    assert(!executor.load(session, request(8, {subtitle(8, "srt")}, {}, {}, 6)));

    return 0;
}
