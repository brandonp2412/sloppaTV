#pragma once
#include "third_party/picosha2/picosha2.h"
#include <filesystem>
#include <fstream>
#include <functional>
#include <iterator>
#include <string>

struct SubtitleSpeechModelSpec {
    std::string filename = "ggml-tiny.en-q5_1.bin";
    std::string url = "https://huggingface.co/ggerganov/whisper.cpp/resolve/main/ggml-tiny.en-q5_1.bin";
    size_t bytes = 32166155;
    std::string sha256 = "c77c5766f1cef09b6b7d47f21b546cbddd4157886b3b5d6d4f709e91e66c7c2b";
};

template <typename Http>
std::string ensureSubtitleSpeechModel(Http& http, const std::string& directory, const std::function<bool()>& cancelled,
                                      const SubtitleSpeechModelSpec& spec = {}) {
    if (directory.empty() || (cancelled && cancelled())) return {};
    const auto valid = [&](const std::string& body) {
        return body.size() == spec.bytes && picosha2::hash256_hex_string(body) == spec.sha256;
    };
    const auto path = std::filesystem::path(directory) / spec.filename;
    std::error_code error;
    if (std::filesystem::file_size(path, error) == spec.bytes && !error) {
        std::ifstream input(path, std::ios::binary);
        const std::string body((std::istreambuf_iterator<char>(input)), {});
        if (valid(body)) return path.string();
    }
    if (cancelled && cancelled()) return {};
    const auto response = http.request("GET", spec.url);
    if (!response.ok() || !valid(response.body) || (cancelled && cancelled())) return {};
    std::filesystem::create_directories(path.parent_path(), error);
    if (error) return {};
    const auto partial = path.string() + ".part";
    {
        std::ofstream output(partial, std::ios::binary | std::ios::trunc);
        output.write(response.body.data(), static_cast<std::streamsize>(response.body.size()));
        if (!output) {
            std::filesystem::remove(partial, error);
            return {};
        }
    }
    if (cancelled && cancelled()) {
        std::filesystem::remove(partial, error);
        return {};
    }
    std::filesystem::rename(partial, path, error);
    if (error) {
        std::filesystem::remove(partial, error);
        return {};
    }
    return path.string();
}
