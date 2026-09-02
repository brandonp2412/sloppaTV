#include "http_error_policy.hpp"

#include <cassert>

int main() {
    assert(userFacingJavaHttpError(
        "getResponseCode",
        "javax.net.ssl.SSLHandshakeException: java.security.cert.CertPathValidatorException"
    ) == "TLS certificate validation failed");
    assert(userFacingJavaHttpError(
        "getResponseCode",
        "java.net.UnknownHostException: jellyfin.example.invalid"
    ) == "Server hostname could not be resolved");
    assert(userFacingJavaHttpError(
        "getResponseCode",
        "java.net.SocketTimeoutException: Read timed out"
    ) == "Server connection timed out");
    assert(userFacingJavaHttpError(
        "getResponseCode",
        "java.net.ConnectException: Connection refused"
    ) == "Unable to connect to server");
    assert(userFacingJavaHttpError(
        "getResponseCode",
        "javax.net.ssl.SSLException: connection closed"
    ) == "TLS connection failed");
    assert(userFacingJavaHttpError("URL constructor", "bad URL") == "Java exception at URL constructor: bad URL");
    return 0;
}
