#include "subtitle_speech_model.hpp"
#include "http_response.hpp"
#include <cassert>
#include <unistd.h>

struct Http {
    int calls = 0;
    HttpResponse response;
    std::function<void()> onRequest;

    HttpResponse request(const std::string& method, const std::string& url) {
        assert(method == "GET" && url == "https://fixture/model");
        ++calls;
        if (onRequest) onRequest();
        return response;
    }
};

int main() {
    const auto directory = std::filesystem::temp_directory_path() / ("sloppa-speech-model-" + std::to_string(getpid()));
    std::filesystem::create_directories(directory);
    const std::string body = "verified model fixture";
    SubtitleSpeechModelSpec spec{"model.bin", "https://fixture/model", body.size(), picosha2::hash256_hex_string(body)};
    Http http;
    http.response = {200, body, {}, {}};
    auto result = ensureSubtitleSpeechModel(http, directory.string(), {}, spec);
    assert(!result.empty() && http.calls == 1);
    assert(ensureSubtitleSpeechModel(http, directory.string(), {}, spec) == result && http.calls == 1);
    {
        std::ofstream output(result, std::ios::binary);
        output << "corrupt";
    }
    http.response.body = std::string(body.size(), 'x');
    assert(ensureSubtitleSpeechModel(http, directory.string(), {}, spec).empty() && http.calls == 2);
    http.response = {503, body, {}, {}};
    assert(ensureSubtitleSpeechModel(http, directory.string(), {}, spec).empty());
    const int calls = http.calls;
    assert(ensureSubtitleSpeechModel(http, directory.string(), [] { return true; }, spec).empty());
    assert(http.calls == calls);
    bool cancelled = false;
    spec.filename = "cancelled.bin";
    http.response = {200, body, {}, {}};
    http.onRequest = [&] { cancelled = true; };
    assert(ensureSubtitleSpeechModel(http, directory.string(), [&] { return cancelled; }, spec).empty());
    assert(!std::filesystem::exists(directory / spec.filename));
    assert(!std::filesystem::exists(directory / (spec.filename + ".part")));
    std::filesystem::remove_all(directory);
}
