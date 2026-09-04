#include "launch_intent.hpp"

#include <cassert>

int main() {
    const auto view = launchRequestFromIntentParts(
        "android.intent.action.VIEW",
        "F02C1110-716D-4851-470A-F395136ACB0A",
        "ignored"
    );
    assert(view.itemId == "f02c1110716d4851470af395136acb0a");
    assert(view.searchQuery.empty());

    const auto search = launchRequestFromIntentParts(
        "android.intent.action.SEARCH",
        "ignored",
        "  Brooklyn Nine-Nine  "
    );
    assert(search.itemId.empty());
    assert(search.searchQuery == "Brooklyn Nine-Nine");

    const auto unsupported = launchRequestFromIntentParts("android.intent.action.MAIN", "ignored", "ignored");
    assert(unsupported.itemId.empty());
    assert(unsupported.searchQuery.empty());
    return 0;
}
