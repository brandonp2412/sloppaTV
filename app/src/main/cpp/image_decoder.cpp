#include "image_decoder.hpp"
#include "jni_env.hpp"

#include <android/bitmap.h>
#include <android/log.h>

#include <cstring>
#include <limits>
#include <new>

namespace {
constexpr const char* kTag = "sloppaTV/image";

using ScopedEnv = ScopedJniEnv;

bool clearException(JNIEnv* env, const char* where, std::string& error) {
    if (!env || !env->ExceptionCheck()) return false;
    __android_log_print(ANDROID_LOG_ERROR, kTag, "Bitmap exception at %s", where);
    env->ExceptionDescribe();
    env->ExceptionClear();
    error = std::string("Image decode failed at ") + where;
    return true;
}

void recycleBitmap(JNIEnv* env, jobject bitmap) {
    if (!env || !bitmap) return;
    jclass bitmapClass = env->GetObjectClass(bitmap);
    if (env->ExceptionCheck()) {
        env->ExceptionClear();
        if (bitmapClass) env->DeleteLocalRef(bitmapClass);
        return;
    }
    jmethodID recycle = bitmapClass ? env->GetMethodID(bitmapClass, "recycle", "()V") : nullptr;
    if (env->ExceptionCheck()) {
        env->ExceptionClear();
        recycle = nullptr;
    }
    if (recycle) {
        env->CallVoidMethod(bitmap, recycle);
        if (env->ExceptionCheck()) env->ExceptionClear();
    }
    if (bitmapClass) env->DeleteLocalRef(bitmapClass);
}
} // namespace

DecodedImage JniImageDecoder::decode(const std::string& encodedBytes, std::string& error) const {
    DecodedImage result;
    if (encodedBytes.empty()) {
        error = "Image response was empty";
        return result;
    }

    ScopedEnv scoped(vm_);
    JNIEnv* env = scoped.get();
    if (!env) {
        error = "Unable to attach image decoder thread to JVM";
        return result;
    }

    jclass factoryClass = env->FindClass("android/graphics/BitmapFactory");
    const bool factoryLookupFailed = clearException(env, "FindClass(BitmapFactory)", error);
    if (factoryLookupFailed || !factoryClass) {
        if (error.empty()) error = "Unable to find Android BitmapFactory";
        return result;
    }
    jmethodID decodeByteArray =
        env->GetStaticMethodID(factoryClass, "decodeByteArray", "([BII)Landroid/graphics/Bitmap;");
    const bool methodLookupFailed = clearException(env, "BitmapFactory.decodeByteArray", error);
    if (methodLookupFailed || !decodeByteArray) {
        env->DeleteLocalRef(factoryClass);
        if (error.empty()) error = "Unable to find Android bitmap decoder";
        return result;
    }

    if (encodedBytes.size() > static_cast<size_t>(std::numeric_limits<jsize>::max())) {
        env->DeleteLocalRef(factoryClass);
        error = "Image payload exceeds the Android decoder size limit";
        return result;
    }

    jbyteArray bytes = env->NewByteArray(static_cast<jsize>(encodedBytes.size()));
    const bool byteArrayFailed = clearException(env, "NewByteArray", error);
    if (byteArrayFailed || !bytes) {
        env->DeleteLocalRef(factoryClass);
        if (error.empty()) error = "Unable to allocate image byte array";
        return result;
    }
    env->SetByteArrayRegion(bytes, 0, static_cast<jsize>(encodedBytes.size()),
                            reinterpret_cast<const jbyte*>(encodedBytes.data()));
    if (clearException(env, "SetByteArrayRegion", error)) {
        env->DeleteLocalRef(bytes);
        env->DeleteLocalRef(factoryClass);
        return result;
    }

    jobject bitmap =
        env->CallStaticObjectMethod(factoryClass, decodeByteArray, bytes, 0, static_cast<jint>(encodedBytes.size()));
    const bool decodeFailed = clearException(env, "BitmapFactory.decodeByteArray", error);
    env->DeleteLocalRef(bytes);
    env->DeleteLocalRef(factoryClass);
    if (decodeFailed || !bitmap) {
        if (bitmap) env->DeleteLocalRef(bitmap);
        if (error.empty()) error = "Android could not decode the image";
        return result;
    }

    AndroidBitmapInfo info{};
    if (AndroidBitmap_getInfo(env, bitmap, &info) != ANDROID_BITMAP_RESULT_SUCCESS || info.width == 0 ||
        info.height == 0) {
        recycleBitmap(env, bitmap);
        env->DeleteLocalRef(bitmap);
        error = "Unable to inspect decoded bitmap";
        return result;
    }
    if (info.width > static_cast<uint32_t>(std::numeric_limits<int>::max()) ||
        info.height > static_cast<uint32_t>(std::numeric_limits<int>::max())) {
        recycleBitmap(env, bitmap);
        env->DeleteLocalRef(bitmap);
        error = "Decoded bitmap dimensions exceed the supported size";
        return result;
    }

    result.width = static_cast<int>(info.width);
    result.height = static_cast<int>(info.height);
    const size_t width = static_cast<size_t>(result.width);
    const size_t height = static_cast<size_t>(result.height);
    const size_t bytesPerSourcePixel =
        info.format == ANDROID_BITMAP_FORMAT_RGBA_8888 ? 4 : (info.format == ANDROID_BITMAP_FORMAT_RGB_565 ? 2 : 0);
    if (bytesPerSourcePixel == 0) {
        recycleBitmap(env, bitmap);
        env->DeleteLocalRef(bitmap);
        result = {};
        error = "Unsupported decoded bitmap pixel format";
        return result;
    }
    if (width > std::numeric_limits<size_t>::max() / 4) {
        recycleBitmap(env, bitmap);
        env->DeleteLocalRef(bitmap);
        result = {};
        error = "Decoded bitmap is too large";
        return result;
    }
    const size_t rgbaRowBytes = width * 4;
    if (height > std::numeric_limits<size_t>::max() / rgbaRowBytes) {
        recycleBitmap(env, bitmap);
        env->DeleteLocalRef(bitmap);
        result = {};
        error = "Decoded bitmap is too large";
        return result;
    }
    const size_t rgbaBytes = rgbaRowBytes * height;
    if (rgbaBytes > result.rgba.max_size() || width > std::numeric_limits<size_t>::max() / bytesPerSourcePixel ||
        static_cast<size_t>(info.stride) < width * bytesPerSourcePixel) {
        recycleBitmap(env, bitmap);
        env->DeleteLocalRef(bitmap);
        result = {};
        error = "Decoded bitmap layout exceeds the supported size";
        return result;
    }
    try {
        result.rgba.resize(rgbaBytes);
    } catch (const std::bad_alloc&) {
        recycleBitmap(env, bitmap);
        env->DeleteLocalRef(bitmap);
        result = {};
        error = "Unable to allocate decoded image pixels";
        return result;
    }

    void* pixels = nullptr;
    if (AndroidBitmap_lockPixels(env, bitmap, &pixels) != ANDROID_BITMAP_RESULT_SUCCESS || !pixels) {
        recycleBitmap(env, bitmap);
        env->DeleteLocalRef(bitmap);
        result = {};
        error = "Unable to lock decoded bitmap pixels";
        return result;
    }

    if (info.format == ANDROID_BITMAP_FORMAT_RGBA_8888) {
        for (int y = 0; y < result.height; ++y) {
            const auto* source = static_cast<const uint8_t*>(pixels) + static_cast<size_t>(y) * info.stride;
            auto* destination = result.rgba.data() + static_cast<size_t>(y) * static_cast<size_t>(result.width) * 4;
            std::memcpy(destination, source, static_cast<size_t>(result.width) * 4);
        }
    } else if (info.format == ANDROID_BITMAP_FORMAT_RGB_565) {
        for (int y = 0; y < result.height; ++y) {
            const auto* source = reinterpret_cast<const uint16_t*>(static_cast<const uint8_t*>(pixels) +
                                                                   static_cast<size_t>(y) * info.stride);
            for (int x = 0; x < result.width; ++x) {
                const uint16_t packed = source[x];
                const size_t out =
                    (static_cast<size_t>(y) * static_cast<size_t>(result.width) + static_cast<size_t>(x)) * 4;
                result.rgba[out] = static_cast<uint8_t>(((packed >> 11) & 0x1f) * 255 / 31);
                result.rgba[out + 1] = static_cast<uint8_t>(((packed >> 5) & 0x3f) * 255 / 63);
                result.rgba[out + 2] = static_cast<uint8_t>((packed & 0x1f) * 255 / 31);
                result.rgba[out + 3] = 255;
            }
        }
    }

    AndroidBitmap_unlockPixels(env, bitmap);
    recycleBitmap(env, bitmap);
    env->DeleteLocalRef(bitmap);
    return result;
}
