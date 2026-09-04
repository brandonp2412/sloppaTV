#pragma once

#include "decoded_image.hpp"

#include <jni.h>

#include <string>

class JniImageDecoder {
public:
    explicit JniImageDecoder(JavaVM* vm) : vm_(vm) {}

    DecodedImage decode(const std::string& encodedBytes, std::string& error) const;

private:
    JavaVM* vm_ = nullptr;
};
