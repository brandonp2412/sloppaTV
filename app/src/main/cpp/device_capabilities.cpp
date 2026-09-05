#include "device_capabilities.hpp"
#include "audio_policy.hpp"
#include "jni_env.hpp"

#include <android/log.h>

#include <algorithm>
#include <string>
#include <unordered_set>

namespace {
constexpr const char* kTag = "sloppaTV/capabilities";

using ScopedEnv = ScopedJniEnv;

bool has(const std::unordered_set<std::string>& types, const char* mime) {
    return types.contains(mime);
}

jint staticInt(JNIEnv* env, jclass clazz, const char* name) {
    if (!env || !clazz) return -1;
    jfieldID field = env->GetStaticFieldID(clazz, name, "I");
    if (!field || env->ExceptionCheck()) {
        if (env->ExceptionCheck()) env->ExceptionClear();
        return -1;
    }
    return env->GetStaticIntField(clazz, field);
}

bool supportsVideoFormat(JNIEnv* env, jobject codecList, const char* mime, jint profile, int width, int height) {
    if (!env || !codecList || !mime) return false;
    jclass formatClass = env->FindClass("android/media/MediaFormat");
    jclass listClass = env->FindClass("android/media/MediaCodecList");
    if (!formatClass || !listClass) {
        if (env->ExceptionCheck()) env->ExceptionClear();
        if (formatClass) env->DeleteLocalRef(formatClass);
        if (listClass) env->DeleteLocalRef(listClass);
        return false;
    }
    jmethodID createVideoFormat = env->GetStaticMethodID(
        formatClass, "createVideoFormat", "(Ljava/lang/String;II)Landroid/media/MediaFormat;"
    );
    jmethodID setInteger = env->GetMethodID(formatClass, "setInteger", "(Ljava/lang/String;I)V");
    jmethodID findDecoder = env->GetMethodID(listClass, "findDecoderForFormat", "(Landroid/media/MediaFormat;)Ljava/lang/String;");
    if (!createVideoFormat || !setInteger || !findDecoder || env->ExceptionCheck()) {
        if (env->ExceptionCheck()) env->ExceptionClear();
        env->DeleteLocalRef(formatClass);
        env->DeleteLocalRef(listClass);
        return false;
    }
    jstring jMime = env->NewStringUTF(mime);
    jobject format = env->CallStaticObjectMethod(formatClass, createVideoFormat, jMime, width, height);
    env->DeleteLocalRef(jMime);
    if (profile > 0 && format) {
        jstring key = env->NewStringUTF("profile");
        env->CallVoidMethod(format, setInteger, key, profile);
        env->DeleteLocalRef(key);
    }
    auto decoder = format ? static_cast<jstring>(env->CallObjectMethod(codecList, findDecoder, format)) : nullptr;
    const bool supported = decoder && !env->ExceptionCheck();
    if (env->ExceptionCheck()) env->ExceptionClear();
    if (decoder) env->DeleteLocalRef(decoder);
    if (format) env->DeleteLocalRef(format);
    env->DeleteLocalRef(formatClass);
    env->DeleteLocalRef(listClass);
    return supported;
}

bool supportsAudioFormat(JNIEnv* env, jobject codecList, const char* mime, int sampleRate = 48000, int channels = 2) {
    if (!env || !codecList || !mime) return false;
    jclass formatClass = env->FindClass("android/media/MediaFormat");
    jclass listClass = env->FindClass("android/media/MediaCodecList");
    if (!formatClass || !listClass) {
        if (env->ExceptionCheck()) env->ExceptionClear();
        if (formatClass) env->DeleteLocalRef(formatClass);
        if (listClass) env->DeleteLocalRef(listClass);
        return false;
    }
    jmethodID createAudioFormat = env->GetStaticMethodID(
        formatClass, "createAudioFormat", "(Ljava/lang/String;II)Landroid/media/MediaFormat;"
    );
    jmethodID findDecoder = env->GetMethodID(listClass, "findDecoderForFormat", "(Landroid/media/MediaFormat;)Ljava/lang/String;");
    if (!createAudioFormat || !findDecoder || env->ExceptionCheck()) {
        if (env->ExceptionCheck()) env->ExceptionClear();
        env->DeleteLocalRef(formatClass);
        env->DeleteLocalRef(listClass);
        return false;
    }
    jstring jMime = env->NewStringUTF(mime);
    jobject format = env->CallStaticObjectMethod(formatClass, createAudioFormat, jMime, sampleRate, channels);
    env->DeleteLocalRef(jMime);
    auto decoder = format ? static_cast<jstring>(env->CallObjectMethod(codecList, findDecoder, format)) : nullptr;
    const bool supported = decoder && !env->ExceptionCheck();
    if (env->ExceptionCheck()) env->ExceptionClear();
    if (decoder) env->DeleteLocalRef(decoder);
    if (format) env->DeleteLocalRef(format);
    env->DeleteLocalRef(formatClass);
    env->DeleteLocalRef(listClass);
    return supported;
}

void queryDisplayHdr(JNIEnv* env, jobject activity, DeviceCodecSupport& result) {
    if (!env || !activity) return;
    jclass activityClass = env->GetObjectClass(activity);
    jmethodID getWindowManager = activityClass
        ? env->GetMethodID(activityClass, "getWindowManager", "()Landroid/view/WindowManager;")
        : nullptr;
    jobject windowManager = getWindowManager ? env->CallObjectMethod(activity, getWindowManager) : nullptr;
    if (env->ExceptionCheck()) { env->ExceptionClear(); windowManager = nullptr; }
    jclass wmClass = env->FindClass("android/view/WindowManager");
    jmethodID getDefaultDisplay = wmClass ? env->GetMethodID(wmClass, "getDefaultDisplay", "()Landroid/view/Display;") : nullptr;
    jobject display = windowManager && getDefaultDisplay ? env->CallObjectMethod(windowManager, getDefaultDisplay) : nullptr;
    if (env->ExceptionCheck()) { env->ExceptionClear(); display = nullptr; }
    jclass displayClass = env->FindClass("android/view/Display");
    jmethodID getHdrCapabilities = displayClass
        ? env->GetMethodID(displayClass, "getHdrCapabilities", "()Landroid/view/Display$HdrCapabilities;")
        : nullptr;
    jobject hdrCaps = display && getHdrCapabilities ? env->CallObjectMethod(display, getHdrCapabilities) : nullptr;
    if (env->ExceptionCheck()) { env->ExceptionClear(); hdrCaps = nullptr; }
    jclass hdrClass = env->FindClass("android/view/Display$HdrCapabilities");
    jmethodID getSupportedTypes = hdrClass ? env->GetMethodID(hdrClass, "getSupportedHdrTypes", "()[I") : nullptr;
    auto types = hdrCaps && getSupportedTypes ? static_cast<jintArray>(env->CallObjectMethod(hdrCaps, getSupportedTypes)) : nullptr;
    if (env->ExceptionCheck()) { env->ExceptionClear(); types = nullptr; }
    const jint hdr10 = staticInt(env, hdrClass, "HDR_TYPE_HDR10");
    const jint hdr10Plus = staticInt(env, hdrClass, "HDR_TYPE_HDR10_PLUS");
    const jint dolbyVision = staticInt(env, hdrClass, "HDR_TYPE_DOLBY_VISION");
    const jint hlg = staticInt(env, hdrClass, "HDR_TYPE_HLG");
    if (types) {
        const jsize count = env->GetArrayLength(types);
        std::vector<jint> values(static_cast<size_t>(count));
        env->GetIntArrayRegion(types, 0, count, values.data());
        if (env->ExceptionCheck()) env->ExceptionClear();
        for (const jint type : values) {
            if (type == hdr10) result.displayHdr10 = true;
            if (type == hdr10Plus) result.displayHdr10Plus = true;
            if (type == dolbyVision) result.displayDolbyVision = true;
            if (type == hlg) result.displayHlg = true;
        }
        env->DeleteLocalRef(types);
    }
    if (hdrClass) env->DeleteLocalRef(hdrClass);
    if (hdrCaps) env->DeleteLocalRef(hdrCaps);
    if (displayClass) env->DeleteLocalRef(displayClass);
    if (display) env->DeleteLocalRef(display);
    if (wmClass) env->DeleteLocalRef(wmClass);
    if (windowManager) env->DeleteLocalRef(windowManager);
    if (activityClass) env->DeleteLocalRef(activityClass);
}
}

std::vector<std::string> DeviceCodecSupport::jellyfinVideoCodecs() const {
    std::vector<std::string> codecs;
    if (h264) codecs.emplace_back("h264");
    if (hevc) codecs.emplace_back("hevc");
    if (vp8) codecs.emplace_back("vp8");
    if (vp9) codecs.emplace_back("vp9");
    if (av1) codecs.emplace_back("av1");
    if (mpeg2) codecs.emplace_back("mpeg2video");
    if (mpeg4) codecs.emplace_back("mpeg4");
    if (vc1) codecs.emplace_back("vc1");
    return codecs;
}

std::vector<std::string> DeviceCodecSupport::jellyfinAudioCodecs(int maxAudioChannels) const {
    return advertisedAudioCodecs(
        AudioCodecCapabilities{
            .aac = aac,
            .mp3 = mp3,
            .mp2 = mp2,
            .pcm = pcm,
            .ac3 = ac3,
            .eac3 = eac3,
            .dts = dts,
            .truehd = truehd,
            .flac = flac,
            .opus = opus,
            .vorbis = vorbis,
            .directAc3 = directAc3,
            .directEac3 = directEac3,
            .directDts = directDts,
            .directDtsHd = directDtsHd,
            .directTrueHd = directTrueHd,
        },
        maxAudioChannels
    );
}

std::vector<std::string> DeviceCodecSupport::jellyfinTranscodingAudioCodecs(int maxAudioChannels) const {
    return transcodingAudioCodecs(
        AudioCodecCapabilities{
            .aac = aac,
            .mp3 = mp3,
            .mp2 = mp2,
            .pcm = pcm,
            .ac3 = ac3,
            .eac3 = eac3,
            .dts = dts,
            .truehd = truehd,
            .flac = flac,
            .opus = opus,
            .vorbis = vorbis,
            .directAc3 = directAc3,
            .directEac3 = directEac3,
            .directDts = directDts,
            .directDtsHd = directDtsHd,
            .directTrueHd = directTrueHd,
        },
        maxAudioChannels
    );
}

DeviceCodecSupport queryDeviceCodecSupport(JavaVM* vm, jobject activity) {
    DeviceCodecSupport result;
    ScopedEnv scoped(vm);
    JNIEnv* env = scoped.get();
    if (!env) return result;

    jclass listClass = env->FindClass("android/media/MediaCodecList");
    if (!listClass) {
        if (env->ExceptionCheck()) env->ExceptionClear();
        return result;
    }
    jfieldID regularField = env->GetStaticFieldID(listClass, "REGULAR_CODECS", "I");
    jmethodID ctor = env->GetMethodID(listClass, "<init>", "(I)V");
    jmethodID getCodecInfos = env->GetMethodID(listClass, "getCodecInfos", "()[Landroid/media/MediaCodecInfo;");
    if (!regularField || !ctor || !getCodecInfos || env->ExceptionCheck()) {
        if (env->ExceptionCheck()) env->ExceptionClear();
        env->DeleteLocalRef(listClass);
        return result;
    }

    const jint regularCodecs = env->GetStaticIntField(listClass, regularField);
    jobject list = env->NewObject(listClass, ctor, regularCodecs);
    auto infos = list ? static_cast<jobjectArray>(env->CallObjectMethod(list, getCodecInfos)) : nullptr;
    if (env->ExceptionCheck()) {
        env->ExceptionClear();
        infos = nullptr;
    }

    std::unordered_set<std::string> decoderTypes;
    if (infos) {
        jclass infoClass = env->FindClass("android/media/MediaCodecInfo");
        jmethodID isEncoder = infoClass ? env->GetMethodID(infoClass, "isEncoder", "()Z") : nullptr;
        jmethodID getSupportedTypes = infoClass ? env->GetMethodID(infoClass, "getSupportedTypes", "()[Ljava/lang/String;") : nullptr;
        if (env->ExceptionCheck()) env->ExceptionClear();

        const jsize infoCount = env->GetArrayLength(infos);
        for (jsize index = 0; index < infoCount; ++index) {
            jobject info = env->GetObjectArrayElement(infos, index);
            if (!info) continue;
            const bool encoder = isEncoder && env->CallBooleanMethod(info, isEncoder);
            if (env->ExceptionCheck()) env->ExceptionClear();
            if (!encoder && getSupportedTypes) {
                auto types = static_cast<jobjectArray>(env->CallObjectMethod(info, getSupportedTypes));
                if (env->ExceptionCheck()) {
                    env->ExceptionClear();
                    types = nullptr;
                }
                if (types) {
                    const jsize count = env->GetArrayLength(types);
                    for (jsize typeIndex = 0; typeIndex < count; ++typeIndex) {
                        auto value = static_cast<jstring>(env->GetObjectArrayElement(types, typeIndex));
                        std::string mime = jniString(env, value);
                        std::transform(mime.begin(), mime.end(), mime.begin(), [](unsigned char c) {
                            return static_cast<char>(std::tolower(c));
                        });
                        if (!mime.empty()) decoderTypes.insert(std::move(mime));
                        if (value) env->DeleteLocalRef(value);
                    }
                    env->DeleteLocalRef(types);
                }
            }
            env->DeleteLocalRef(info);
        }
        if (infoClass) env->DeleteLocalRef(infoClass);
        env->DeleteLocalRef(infos);
    }

    jclass profileLevelClass = env->FindClass("android/media/MediaCodecInfo$CodecProfileLevel");
    if (env->ExceptionCheck()) { env->ExceptionClear(); profileLevelClass = nullptr; }
    if (list && profileLevelClass) {
        result.h264High10 = supportsVideoFormat(
            env, list, "video/avc", staticInt(env, profileLevelClass, "AVCProfileHigh10"), 1920, 1080
        );
        const jint hevcMain10 = staticInt(env, profileLevelClass, "HEVCProfileMain10");
        const jint hevcMain10Hdr10 = staticInt(env, profileLevelClass, "HEVCProfileMain10HDR10");
        const jint hevcMain10Hdr10Plus = staticInt(env, profileLevelClass, "HEVCProfileMain10HDR10Plus");
        result.hevcMain10 = supportsVideoFormat(env, list, "video/hevc", hevcMain10, 1920, 1080)
            || supportsVideoFormat(env, list, "video/hevc", hevcMain10Hdr10, 1920, 1080)
            || supportsVideoFormat(env, list, "video/hevc", hevcMain10Hdr10Plus, 1920, 1080);
        const jint av1Main10 = staticInt(env, profileLevelClass, "AV1ProfileMain10");
        const jint av1Main10Hdr10 = staticInt(env, profileLevelClass, "AV1ProfileMain10HDR10");
        const jint av1Main10Hdr10Plus = staticInt(env, profileLevelClass, "AV1ProfileMain10HDR10Plus");
        result.av1Main10 = supportsVideoFormat(env, list, "video/av01", av1Main10, 1920, 1080)
            || supportsVideoFormat(env, list, "video/av01", av1Main10Hdr10, 1920, 1080)
            || supportsVideoFormat(env, list, "video/av01", av1Main10Hdr10Plus, 1920, 1080);

        auto maxResolution = [&](const char* mime, int& width, int& height) {
            if (supportsVideoFormat(env, list, mime, 0, 7680, 4320)) { width = 7680; height = 4320; }
            else if (supportsVideoFormat(env, list, mime, 0, 3840, 2160)) { width = 3840; height = 2160; }
            else if (supportsVideoFormat(env, list, mime, 0, 1920, 1080)) { width = 1920; height = 1080; }
        };
        maxResolution("video/avc", result.maxH264Width, result.maxH264Height);
        maxResolution("video/hevc", result.maxHevcWidth, result.maxHevcHeight);
        maxResolution("video/av01", result.maxAv1Width, result.maxAv1Height);
    }
    if (profileLevelClass) env->DeleteLocalRef(profileLevelClass);

    result.h264 = has(decoderTypes, "video/avc");
    result.hevc = has(decoderTypes, "video/hevc");
    result.vp8 = has(decoderTypes, "video/x-vnd.on2.vp8");
    result.vp9 = has(decoderTypes, "video/x-vnd.on2.vp9");
    result.av1 = has(decoderTypes, "video/av01");
    result.mpeg2 = has(decoderTypes, "video/mpeg2");
    result.mpeg4 = has(decoderTypes, "video/mp4v-es");
    result.vc1 = has(decoderTypes, "video/wvc1") || has(decoderTypes, "video/vc1");

    result.aac = has(decoderTypes, "audio/mp4a-latm");
    result.mp3 = has(decoderTypes, "audio/mpeg");
    result.mp2 = has(decoderTypes, "audio/mpeg-l2") || supportsAudioFormat(env, list, "audio/mpeg-L2");
    result.pcm = has(decoderTypes, "audio/raw");
    result.ac3 = has(decoderTypes, "audio/ac3");
    result.eac3 = has(decoderTypes, "audio/eac3") || has(decoderTypes, "audio/eac3-joc");
    result.dts = has(decoderTypes, "audio/vnd.dts") || has(decoderTypes, "audio/vnd.dts.hd");
    result.truehd = has(decoderTypes, "audio/true-hd") || has(decoderTypes, "audio/vnd.dolby.mlp");
    result.flac = has(decoderTypes, "audio/flac");
    result.opus = has(decoderTypes, "audio/opus");
    result.vorbis = has(decoderTypes, "audio/vorbis");

    if (list) env->DeleteLocalRef(list);
    env->DeleteLocalRef(listClass);

    queryDisplayHdr(env, activity, result);

    if (activity) {
        jclass activityClass = env->GetObjectClass(activity);
        jmethodID queryAudio = activityClass
            ? env->GetMethodID(activityClass, "queryAudioOutputCapabilities", "()[I")
            : nullptr;
        auto audioCaps = queryAudio
            ? static_cast<jintArray>(env->CallObjectMethod(activity, queryAudio))
            : nullptr;
        if (env->ExceptionCheck()) {
            env->ExceptionClear();
            audioCaps = nullptr;
        }
        if (audioCaps && env->GetArrayLength(audioCaps) >= 2) {
            jint values[2]{};
            env->GetIntArrayRegion(audioCaps, 0, 2, values);
            if (!env->ExceptionCheck()) {
                constexpr int kDirectAc3 = 1;
                constexpr int kDirectEac3 = 1 << 1;
                constexpr int kDirectDts = 1 << 2;
                constexpr int kDirectDtsHd = 1 << 3;
                constexpr int kDirectTrueHd = 1 << 4;
                result.maxAudioOutputChannels = std::clamp(static_cast<int>(values[0]), 2, 8);
                result.directAc3 = (values[1] & kDirectAc3) != 0;
                result.directEac3 = (values[1] & kDirectEac3) != 0;
                result.directDts = (values[1] & kDirectDts) != 0;
                result.directDtsHd = (values[1] & kDirectDtsHd) != 0;
                result.directTrueHd = (values[1] & kDirectTrueHd) != 0;
            } else {
                env->ExceptionClear();
            }
            env->DeleteLocalRef(audioCaps);
        }
        if (activityClass) env->DeleteLocalRef(activityClass);
    }

    const auto videos = result.jellyfinVideoCodecs();
    const auto audios = result.jellyfinAudioCodecs(result.maxAudioOutputChannels);
    __android_log_print(
        ANDROID_LOG_INFO,
        kTag,
        "Detected %zu video/%zu audio; MPEG4=%d MP2=%d PCM=%d; H264 High10=%d HEVC Main10=%d AV1 Main10=%d; max H264=%dx%d HEVC=%dx%d AV1=%dx%d; HDR10=%d HDR10+=%d DV=%d HLG=%d; audioOut=%dch direct(ac3=%d eac3=%d dts=%d dtshd=%d truehd=%d)",
        videos.size(), audios.size(),
        result.mpeg4, result.mp2, result.pcm,
        result.h264High10, result.hevcMain10, result.av1Main10,
        result.maxH264Width, result.maxH264Height,
        result.maxHevcWidth, result.maxHevcHeight,
        result.maxAv1Width, result.maxAv1Height,
        result.displayHdr10, result.displayHdr10Plus, result.displayDolbyVision, result.displayHlg,
        result.maxAudioOutputChannels,
        result.directAc3, result.directEac3, result.directDts, result.directDtsHd, result.directTrueHd
    );
    return result;
}
