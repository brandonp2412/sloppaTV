#pragma once

#include <jni.h>

#include <cstdint>
#include <string>
#include <vector>

struct DecodedImage {
    int width = 0;
    int height = 0;
    std::vector<uint8_t> rgba;

    [[nodiscard]] bool valid() const {
        return width > 0 && height > 0 && rgba.size() == static_cast<size_t>(width) * static_cast<size_t>(height) * 4;
    }
};

class JniImageDecoder {
public:
    explicit JniImageDecoder(JavaVM* vm) : vm_(vm) {}

    DecodedImage decode(const std::string& encodedBytes, std::string& error) const;

private:
    JavaVM* vm_ = nullptr;
};
