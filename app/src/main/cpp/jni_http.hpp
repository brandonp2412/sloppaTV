#pragma once

#include <jni.h>

#include <map>
#include <string>

struct HttpResponse {
    int status = 0;
    std::string body;
    std::string error;

    [[nodiscard]] bool ok() const { return status >= 200 && status < 300; }
};

class JniHttpClient {
public:
    explicit JniHttpClient(JavaVM* vm) : vm_(vm) {}

    HttpResponse request(
        const std::string& method,
        const std::string& url,
        const std::map<std::string, std::string>& headers = {},
        const std::string& body = {}
    ) const;

private:
    HttpResponse requestOnce(
        const std::string& method,
        const std::string& url,
        const std::map<std::string, std::string>& headers,
        const std::string& body
    ) const;

    JavaVM* vm_ = nullptr;
};
