#pragma once

#include <string>

struct HttpResponse {
    int status = 0;
    std::string body;
    std::string error;
    std::string setCookie;

    [[nodiscard]] bool ok() const { return status >= 200 && status < 300; }
};
