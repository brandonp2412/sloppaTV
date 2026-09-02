#include "http_retry_policy.hpp"

#include <cassert>

int main() {
    assert(transientHttpRetryCount("GET") == 2);
    assert(transientHttpRetryCount("HEAD") == 2);
    assert(transientHttpRetryCount("POST") == 0);
    assert(transientHttpRetryCount("DELETE") == 0);
    assert(transientHttpRetryCount("PUT") == 0);
    return 0;
}
