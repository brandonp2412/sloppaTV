#pragma once

#include <jni.h>

#include <string>
#include <vector>

struct DeviceCodecSupport {
    bool h264 = false;
    bool h264High10 = false;
    bool hevc = false;
    bool hevcMain10 = false;
    bool vp8 = false;
    bool vp9 = false;
    bool av1 = false;
    bool av1Main10 = false;
    bool mpeg2 = false;
    bool vc1 = false;

    int maxH264Width = 0;
    int maxH264Height = 0;
    int maxHevcWidth = 0;
    int maxHevcHeight = 0;
    int maxAv1Width = 0;
    int maxAv1Height = 0;

    bool displayHdr10 = false;
    bool displayHdr10Plus = false;
    bool displayDolbyVision = false;
    bool displayHlg = false;

    bool aac = false;
    bool mp3 = false;
    bool ac3 = false;
    bool eac3 = false;
    bool flac = false;
    bool opus = false;
    bool vorbis = false;

    [[nodiscard]] std::vector<std::string> jellyfinVideoCodecs() const;
    [[nodiscard]] std::vector<std::string> jellyfinAudioCodecs() const;
};

DeviceCodecSupport queryDeviceCodecSupport(JavaVM* vm, jobject activity);
