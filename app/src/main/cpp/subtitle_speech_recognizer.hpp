#pragma once
#include "jni_http.hpp"
#include "subtitle_dialogue_sync.hpp"
#include "subtitle_lapse.hpp"
#include <functional>
#include <memory>
#include <string>
#include <vector>

class SubtitleSpeechRecognizer {
public:
    SubtitleSpeechRecognizer(JavaVM* vm, jobject activity);
    ~SubtitleSpeechRecognizer();
    std::vector<SubtitleSpeechWord> recognize(const std::vector<float>& audio, const std::string& directory,
                                              const std::function<bool()>& cancelled);
    SubtitleSceneProposal proposeScenes(const std::vector<float>& audio, const std::vector<SubtitleCue>& cues,
                                        int startMs, const std::string& directory,
                                        const std::function<bool()>& cancelled);
    void cancelPending();

private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
    JniHttpClient http_;
};
