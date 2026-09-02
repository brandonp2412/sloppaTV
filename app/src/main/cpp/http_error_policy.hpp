#pragma once

#include <string>
#include <string_view>

inline bool httpErrorContains(std::string_view value, std::string_view token) {
    return value.find(token) != std::string_view::npos;
}

inline std::string userFacingJavaHttpError(std::string_view where, std::string_view detail) {
    // HttpURLConnection wraps the useful network cause in the Throwable text. Keep
    // connection failures actionable in the TV UI instead of presenting raw JNI/Java
    // exception names. The full Throwable remains in logcat for diagnostics.
    if (httpErrorContains(detail, "CertPathValidatorException")
        || httpErrorContains(detail, "CertificateException")
        || httpErrorContains(detail, "SSLPeerUnverifiedException")
        || httpErrorContains(detail, "SSLHandshakeException")) {
        return "TLS certificate validation failed";
    }
    if (httpErrorContains(detail, "UnknownHostException")) {
        return "Server hostname could not be resolved";
    }
    if (httpErrorContains(detail, "SocketTimeoutException")) {
        return "Server connection timed out";
    }
    if (httpErrorContains(detail, "ConnectException") || httpErrorContains(detail, "NoRouteToHostException")) {
        return "Unable to connect to server";
    }
    if (httpErrorContains(detail, "SSLException")) {
        return "TLS connection failed";
    }

    std::string result = "Java exception at ";
    result += where;
    if (!detail.empty()) {
        result += ": ";
        result += detail;
    }
    return result;
}
