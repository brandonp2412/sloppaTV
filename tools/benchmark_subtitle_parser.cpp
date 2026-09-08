#include "subtitle_policy.hpp"

#include <chrono>
#include <cstdlib>
#include <iomanip>
#include <iostream>
#include <string>

namespace {
std::string makeSrt(int cueCount, bool markup) {
    std::string input;
    input.reserve(static_cast<size_t>(cueCount) * 96);
    for (int index = 0; index < cueCount; ++index) {
        const int second = index % 50;
        input += std::to_string(index + 1) + "\n00:00:";
        if (second < 10) input += '0';
        input += std::to_string(second) + ",000 --> 00:00:";
        if (second + 1 < 10) input += '0';
        input += std::to_string(second + 1) + ",000\n";
        if (markup) input += "<i>";
        input += "Subtitle line " + std::to_string(index) + " with representative dialogue";
        if (markup) input += "</i>";
        input += "\n\n";
    }
    return input;
}

std::string makeAss(int cueCount) {
    std::string input =
        "[Script Info]\nTitle: benchmark\n[Events]\n"
        "Format: Layer, Start, End, Style, Name, MarginL, MarginR, MarginV, Effect, Text\n";
    input.reserve(input.size() + static_cast<size_t>(cueCount) * 128);
    for (int index = 0; index < cueCount; ++index) {
        const int second = index % 50;
        input += "Dialogue: 0,0:00:";
        if (second < 10) input += '0';
        input += std::to_string(second) + ".00,0:00:";
        if (second + 1 < 10) input += '0';
        input += std::to_string(second + 1)
            + ".00,Default,,0,0,0,,{\\an8}<i>Subtitle line " + std::to_string(index)
            + "</i>\\Nwith representative dialogue, and comma\n";
    }
    return input;
}

template <typename Parse>
double benchmark(const std::string& input, int iterations, Parse parse, size_t& parsedCues) {
    const auto started = std::chrono::steady_clock::now();
    size_t count = 0;
    for (int iteration = 0; iteration < iterations; ++iteration) count += parse(input).size();
    const auto elapsed = std::chrono::duration<double, std::milli>(
        std::chrono::steady_clock::now() - started
    ).count();
    parsedCues += count;
    return elapsed;
}
}

int main(int argc, char** argv) {
    const int cueCount = argc > 1 ? std::atoi(argv[1]) : 2000;
    const int iterations = argc > 2 ? std::atoi(argv[2]) : 30;
    const std::string srt = makeSrt(cueCount, true);
    const std::string plainSrt = makeSrt(cueCount, false);
    const std::string ass = makeAss(cueCount);
    size_t parsedCues = 0;
    const double srtMs = benchmark(srt, iterations, [](const std::string& input) {
        return parseTextSubtitleCues(input, "srt");
    }, parsedCues);
    const double plainSrtMs = benchmark(plainSrt, iterations, [](const std::string& input) {
        return parseTextSubtitleCues(input, "srt");
    }, parsedCues);
    const double assMs = benchmark(ass, iterations, [](const std::string& input) {
        return parseTextSubtitleCues(input, "ass");
    }, parsedCues);
    std::cout << std::fixed << std::setprecision(3)
              << "cues=" << cueCount << " iterations=" << iterations
              << " srt_ms=" << srtMs << " plain_srt_ms=" << plainSrtMs << " ass_ms=" << assMs
              << " total_ms=" << (srtMs + plainSrtMs + assMs)
              << " parsed=" << parsedCues << '\n';
    return parsedCues == static_cast<size_t>(cueCount) * static_cast<size_t>(iterations) * 3 ? 0 : 1;
}
