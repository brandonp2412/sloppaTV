#include "http_retry_policy.hpp"

#include <cassert>

int main() {
    assert(transientHttpRetryCount("GET") == 2);
    assert(transientHttpRetryCount("HEAD") == 2);
    assert(transientHttpRetryCount("POST") == 0);
    assert(transientHttpRetryCount("DELETE") == 0);
    assert(transientHttpRetryCount("PUT") == 0);

    assert(transientHttpStatus(500));
    assert(transientHttpStatus(502));
    assert(transientHttpStatus(503));
    assert(transientHttpStatus(504));
    assert(!transientHttpStatus(400));
    assert(!transientHttpStatus(401));
    assert(!transientHttpStatus(404));
    assert(!transientHttpStatus(429));

    assert(shouldRetryTransientHttpResponse("GET", 0, true));
    assert(shouldRetryTransientHttpResponse("HEAD", 503, false));
    assert(shouldRetryTransientHttpResponse("GET", 500, false));
    assert(!shouldRetryTransientHttpResponse("GET", 404, false));
    assert(!shouldRetryTransientHttpResponse("GET", 0, false));
    assert(!shouldRetryTransientHttpResponse("POST", 503, false));
    assert(!shouldRetryTransientHttpResponse("DELETE", 0, true));
    return 0;
}
