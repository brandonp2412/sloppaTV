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

bool clearPendingException(JNIEnv* env) {
    if (!env || !env->ExceptionCheck()) return false;
    env->ExceptionClear();
    return true;
}

jclass findClass(JNIEnv* env, const char* name) {
    if (!env || !name) return nullptr;
    jclass clazz = env->FindClass(name);
    if (!clearPendingException(env)) return clazz;
    if (clazz) env->DeleteLocalRef(clazz);
    return nullptr;
}

jclass objectClass(JNIEnv* env, jobject object) {
    if (!env || !object) return nullptr;
    jclass clazz = env->GetObjectClass(object);
    if (!clearPendingException(env)) return clazz;
    if (clazz) env->DeleteLocalRef(clazz);
    return nullptr;
}

jmethodID method(JNIEnv* env, jclass clazz, const char* name, const char* signature) {
    if (!env || !clazz || !name || !signature) return nullptr;
    jmethodID value = env->GetMethodID(clazz, name, signature);
    if (clearPendingException(env)) return nullptr;
    return value;
}

jmethodID staticMethod(JNIEnv* env, jclass clazz, const char* name, const char* signature) {
    if (!env || !clazz || !name || !signature) return nullptr;
    jmethodID value = env->GetStaticMethodID(clazz, name, signature);
    if (clearPendingException(env)) return nullptr;
    return value;
}

jfieldID staticField(JNIEnv* env, jclass clazz, const char* name, const char* signature) {
    if (!env || !clazz || !name || !signature) return nullptr;
    jfieldID value = env->GetStaticFieldID(clazz, name, signature);
    if (clearPendingException(env)) return nullptr;
    return value;
}

bool has(const std::unordered_set<std::string>& types, const char* mime) {
    return types.contains(mime);
}

jint staticInt(JNIEnv* env, jclass clazz, const char* name) {
    if (!env || !clazz) return -1;
    jfieldID field = staticField(env, clazz, name, "I");
    if (!field) return -1;
    const jint value = env->GetStaticIntField(clazz, field);
    if (env->ExceptionCheck()) {
        env->ExceptionClear();
        return -1;
    }
    return value;
}

bool supportsVideoFormat(JNIEnv* env, jobject codecList, const char* mime, jint profile, int width, int height) {
    if (!env || !codecList || !mime || profile < 0) return false;
    jclass formatClass = findClass(env, "android/media/MediaFormat");
    jclass listClass = findClass(env, "android/media/MediaCodecList");
    if (!formatClass || !listClass) {
        if (formatClass) env->DeleteLocalRef(formatClass);
        if (listClass) env->DeleteLocalRef(listClass);
        return false;
    }
    jmethodID createVideoFormat =
        staticMethod(env, formatClass, "createVideoFormat", "(Ljava/lang/String;II)Landroid/media/MediaFormat;");
    jmethodID setInteger = method(env, formatClass, "setInteger", "(Ljava/lang/String;I)V");
    jmethodID findDecoder =
        method(env, listClass, "findDecoderForFormat", "(Landroid/media/MediaFormat;)Ljava/lang/String;");
    if (!createVideoFormat || !setInteger || !findDecoder) {
        env->DeleteLocalRef(formatClass);
        env->DeleteLocalRef(listClass);
        return false;
    }
    jstring jMime = jniNewString(env, mime);
    if (!jMime || env->ExceptionCheck()) {
        if (env->ExceptionCheck()) env->ExceptionClear();
        if (jMime) env->DeleteLocalRef(jMime);
        env->DeleteLocalRef(formatClass);
        env->DeleteLocalRef(listClass);
        return false;
    }
    jobject format = env->CallStaticObjectMethod(formatClass, createVideoFormat, jMime, width, height);
    env->DeleteLocalRef(jMime);
    if (!format || env->ExceptionCheck()) {
        if (env->ExceptionCheck()) env->ExceptionClear();
        if (format) env->DeleteLocalRef(format);
        env->DeleteLocalRef(formatClass);
        env->DeleteLocalRef(listClass);
        return false;
    }
    if (profile > 0) {
        jstring key = jniNewString(env, "profile");
        if (!key || env->ExceptionCheck()) {
            if (env->ExceptionCheck()) env->ExceptionClear();
            if (key) env->DeleteLocalRef(key);
            env->DeleteLocalRef(format);
            env->DeleteLocalRef(formatClass);
            env->DeleteLocalRef(listClass);
            return false;
        }
        env->CallVoidMethod(format, setInteger, key, profile);
        env->DeleteLocalRef(key);
        if (env->ExceptionCheck()) {
            env->ExceptionClear();
            env->DeleteLocalRef(format);
            env->DeleteLocalRef(formatClass);
            env->DeleteLocalRef(listClass);
            return false;
        }
    }
    auto decoder = static_cast<jstring>(env->CallObjectMethod(codecList, findDecoder, format));
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
    jclass formatClass = findClass(env, "android/media/MediaFormat");
    jclass listClass = findClass(env, "android/media/MediaCodecList");
    if (!formatClass || !listClass) {
        if (formatClass) env->DeleteLocalRef(formatClass);
        if (listClass) env->DeleteLocalRef(listClass);
        return false;
    }
    jmethodID createAudioFormat =
        staticMethod(env, formatClass, "createAudioFormat", "(Ljava/lang/String;II)Landroid/media/MediaFormat;");
    jmethodID findDecoder =
        method(env, listClass, "findDecoderForFormat", "(Landroid/media/MediaFormat;)Ljava/lang/String;");
    if (!createAudioFormat || !findDecoder) {
        env->DeleteLocalRef(formatClass);
        env->DeleteLocalRef(listClass);
        return false;
    }
    jstring jMime = jniNewString(env, mime);
    if (!jMime || env->ExceptionCheck()) {
        if (env->ExceptionCheck()) env->ExceptionClear();
        if (jMime) env->DeleteLocalRef(jMime);
        env->DeleteLocalRef(formatClass);
        env->DeleteLocalRef(listClass);
        return false;
    }
    jobject format = env->CallStaticObjectMethod(formatClass, createAudioFormat, jMime, sampleRate, channels);
    env->DeleteLocalRef(jMime);
    if (!format || env->ExceptionCheck()) {
        if (env->ExceptionCheck()) env->ExceptionClear();
        if (format) env->DeleteLocalRef(format);
        env->DeleteLocalRef(formatClass);
        env->DeleteLocalRef(listClass);
        return false;
    }
    auto decoder = static_cast<jstring>(env->CallObjectMethod(codecList, findDecoder, format));
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
    jclass activityClass = objectClass(env, activity);
    if (!activityClass) return;
    jmethodID getWindowManager = method(env, activityClass, "getWindowManager", "()Landroid/view/WindowManager;");
    jobject windowManager = getWindowManager ? env->CallObjectMethod(activity, getWindowManager) : nullptr;
    if (env->ExceptionCheck()) {
        env->ExceptionClear();
        if (windowManager) env->DeleteLocalRef(windowManager);
        windowManager = nullptr;
    }
    jclass wmClass = findClass(env, "android/view/WindowManager");
    jmethodID getDefaultDisplay = method(env, wmClass, "getDefaultDisplay", "()Landroid/view/Display;");
    jobject display =
        windowManager && getDefaultDisplay ? env->CallObjectMethod(windowManager, getDefaultDisplay) : nullptr;
    if (env->ExceptionCheck()) {
        env->ExceptionClear();
        if (display) env->DeleteLocalRef(display);
        display = nullptr;
    }
    jclass displayClass = findClass(env, "android/view/Display");
    jmethodID getHdrCapabilities =
        method(env, displayClass, "getHdrCapabilities", "()Landroid/view/Display$HdrCapabilities;");
    jobject hdrCaps = display && getHdrCapabilities ? env->CallObjectMethod(display, getHdrCapabilities) : nullptr;
    if (env->ExceptionCheck()) {
        env->ExceptionClear();
        if (hdrCaps) env->DeleteLocalRef(hdrCaps);
        hdrCaps = nullptr;
    }
    jclass hdrClass = findClass(env, "android/view/Display$HdrCapabilities");
    jmethodID getSupportedTypes = method(env, hdrClass, "getSupportedHdrTypes", "()[I");
    auto types = hdrCaps && getSupportedTypes
                     ? static_cast<jintArray>(env->CallObjectMethod(hdrCaps, getSupportedTypes))
                     : nullptr;
    if (env->ExceptionCheck()) {
        env->ExceptionClear();
        if (types) env->DeleteLocalRef(types);
        types = nullptr;
    }
    const jint hdr10 = staticInt(env, hdrClass, "HDR_TYPE_HDR10");
    const jint hdr10Plus = staticInt(env, hdrClass, "HDR_TYPE_HDR10_PLUS");
    const jint dolbyVision = staticInt(env, hdrClass, "HDR_TYPE_DOLBY_VISION");
    const jint hlg = staticInt(env, hdrClass, "HDR_TYPE_HLG");
    if (types) {
        const jsize count = env->GetArrayLength(types);
        if (!env->ExceptionCheck() && count > 0) {
            std::vector<jint> values(static_cast<size_t>(count));
            env->GetIntArrayRegion(types, 0, count, values.data());
            if (!env->ExceptionCheck()) {
                for (const jint type : values) {
                    if (type == hdr10) result.displayHdr10 = true;
                    if (type == hdr10Plus) result.displayHdr10Plus = true;
                    if (type == dolbyVision) result.displayDolbyVision = true;
                    if (type == hlg) result.displayHlg = true;
                }
            }
        }
        if (env->ExceptionCheck()) env->ExceptionClear();
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
} // namespace

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
        maxAudioChannels);
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
        maxAudioChannels);
}

DeviceCodecSupport queryDeviceCodecSupport(JavaVM* vm, jobject activity) {
    DeviceCodecSupport result;
    ScopedEnv scoped(vm);
    JNIEnv* env = scoped.get();
    if (!env) return result;

    jclass listClass = findClass(env, "android/media/MediaCodecList");
    if (!listClass) return result;
    jfieldID regularField = staticField(env, listClass, "REGULAR_CODECS", "I");
    jmethodID ctor = method(env, listClass, "<init>", "(I)V");
    jmethodID getCodecInfos = method(env, listClass, "getCodecInfos", "()[Landroid/media/MediaCodecInfo;");
    if (!regularField || !ctor || !getCodecInfos) {
        env->DeleteLocalRef(listClass);
        return result;
    }

    const jint regularCodecs = env->GetStaticIntField(listClass, regularField);
    if (env->ExceptionCheck()) {
        env->ExceptionClear();
        env->DeleteLocalRef(listClass);
        return result;
    }
    jobject list = env->NewObject(listClass, ctor, regularCodecs);
    if (clearPendingException(env)) {
        if (list) env->DeleteLocalRef(list);
        list = nullptr;
    }
    auto infos = list ? static_cast<jobjectArray>(env->CallObjectMethod(list, getCodecInfos)) : nullptr;
    if (clearPendingException(env)) {
        if (infos) env->DeleteLocalRef(infos);
        infos = nullptr;
    }

    std::unordered_set<std::string> decoderTypes;
    if (infos) {
        jclass infoClass = findClass(env, "android/media/MediaCodecInfo");
        jmethodID isEncoder = method(env, infoClass, "isEncoder", "()Z");
        jmethodID getSupportedTypes = method(env, infoClass, "getSupportedTypes", "()[Ljava/lang/String;");

        jsize infoCount = env->GetArrayLength(infos);
        if (env->ExceptionCheck()) {
            env->ExceptionClear();
            infoCount = 0;
        }
        for (jsize index = 0; index < infoCount; ++index) {
            jobject info = env->GetObjectArrayElement(infos, index);
            if (env->ExceptionCheck()) {
                env->ExceptionClear();
                if (info) env->DeleteLocalRef(info);
                continue;
            }
            if (!info) continue;
            const bool encoder = isEncoder && env->CallBooleanMethod(info, isEncoder);
            if (env->ExceptionCheck()) {
                env->ExceptionClear();
                env->DeleteLocalRef(info);
                continue;
            }
            if (!encoder && getSupportedTypes) {
                auto types = static_cast<jobjectArray>(env->CallObjectMethod(info, getSupportedTypes));
                if (clearPendingException(env)) {
                    if (types) env->DeleteLocalRef(types);
                    types = nullptr;
                }
                if (types) {
                    jsize count = env->GetArrayLength(types);
                    if (env->ExceptionCheck()) {
                        env->ExceptionClear();
                        count = 0;
                    }
                    for (jsize typeIndex = 0; typeIndex < count; ++typeIndex) {
                        auto value = static_cast<jstring>(env->GetObjectArrayElement(types, typeIndex));
                        if (env->ExceptionCheck()) {
                            env->ExceptionClear();
                            if (value) env->DeleteLocalRef(value);
                            continue;
                        }
                        std::string mime = jniString(env, value);
                        std::transform(mime.begin(), mime.end(), mime.begin(),
                                       [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
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

    jclass profileLevelClass = findClass(env, "android/media/MediaCodecInfo$CodecProfileLevel");
    if (list && profileLevelClass) {
        result.h264High10 = supportsVideoFormat(env, list, "video/avc",
                                                staticInt(env, profileLevelClass, "AVCProfileHigh10"), 1920, 1080);
        const jint hevcMain10 = staticInt(env, profileLevelClass, "HEVCProfileMain10");
        const jint hevcMain10Hdr10 = staticInt(env, profileLevelClass, "HEVCProfileMain10HDR10");
        const jint hevcMain10Hdr10Plus = staticInt(env, profileLevelClass, "HEVCProfileMain10HDR10Plus");
        result.hevcMain10 = supportsVideoFormat(env, list, "video/hevc", hevcMain10, 1920, 1080) ||
                            supportsVideoFormat(env, list, "video/hevc", hevcMain10Hdr10, 1920, 1080) ||
                            supportsVideoFormat(env, list, "video/hevc", hevcMain10Hdr10Plus, 1920, 1080);
        const jint av1Main10 = staticInt(env, profileLevelClass, "AV1ProfileMain10");
        const jint av1Main10Hdr10 = staticInt(env, profileLevelClass, "AV1ProfileMain10HDR10");
        const jint av1Main10Hdr10Plus = staticInt(env, profileLevelClass, "AV1ProfileMain10HDR10Plus");
        result.av1Main10 = supportsVideoFormat(env, list, "video/av01", av1Main10, 1920, 1080) ||
                           supportsVideoFormat(env, list, "video/av01", av1Main10Hdr10, 1920, 1080) ||
                           supportsVideoFormat(env, list, "video/av01", av1Main10Hdr10Plus, 1920, 1080);

        auto maxResolution = [&](const char* mime, int& width, int& height) {
            if (supportsVideoFormat(env, list, mime, 0, 7680, 4320)) {
                width = 7680;
                height = 4320;
            } else if (supportsVideoFormat(env, list, mime, 0, 3840, 2160)) {
                width = 3840;
                height = 2160;
            } else if (supportsVideoFormat(env, list, mime, 0, 1920, 1080)) {
                width = 1920;
                height = 1080;
            }
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
        jclass activityClass = objectClass(env, activity);
        jmethodID queryAudio = method(env, activityClass, "queryAudioOutputCapabilities", "()[I");
        auto audioCaps = queryAudio ? static_cast<jintArray>(env->CallObjectMethod(activity, queryAudio)) : nullptr;
        if (clearPendingException(env)) {
            if (audioCaps) env->DeleteLocalRef(audioCaps);
            audioCaps = nullptr;
        }
        if (audioCaps) {
            const jsize count = env->GetArrayLength(audioCaps);
            if (!env->ExceptionCheck() && count >= 2) {
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
                }
            }
            if (env->ExceptionCheck()) env->ExceptionClear();
            env->DeleteLocalRef(audioCaps);
        }
        if (activityClass) env->DeleteLocalRef(activityClass);
    }

    const auto videos = result.jellyfinVideoCodecs();
    const auto audios = result.jellyfinAudioCodecs(result.maxAudioOutputChannels);
    __android_log_print(ANDROID_LOG_INFO, kTag,
                        "Detected %zu video/%zu audio; MPEG4=%d MP2=%d PCM=%d; H264 High10=%d HEVC Main10=%d AV1 "
                        "Main10=%d; max H264=%dx%d HEVC=%dx%d AV1=%dx%d; HDR10=%d HDR10+=%d DV=%d HLG=%d; "
                        "audioOut=%dch direct(ac3=%d eac3=%d dts=%d dtshd=%d truehd=%d)",
                        videos.size(), audios.size(), result.mpeg4, result.mp2, result.pcm, result.h264High10,
                        result.hevcMain10, result.av1Main10, result.maxH264Width, result.maxH264Height,
                        result.maxHevcWidth, result.maxHevcHeight, result.maxAv1Width, result.maxAv1Height,
                        result.displayHdr10, result.displayHdr10Plus, result.displayDolbyVision, result.displayHlg,
                        result.maxAudioOutputChannels, result.directAc3, result.directEac3, result.directDts,
                        result.directDtsHd, result.directTrueHd);
    return result;
}
