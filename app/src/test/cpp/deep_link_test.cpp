#include "deep_link.hpp"

#include <cassert>

int main() {
    assert(normalizeJellyfinItemId("becd2d781967b8036ee6f144014fb009") == "becd2d781967b8036ee6f144014fb009");
    assert(normalizeJellyfinItemId("BECD2D78-1967-B803-6EE6-F144014FB009") == "becd2d781967b8036ee6f144014fb009");
    assert(normalizeJellyfinItemId("  becd2d781967b8036ee6f144014fb009\n") == "becd2d781967b8036ee6f144014fb009");
    assert(normalizeJellyfinItemId("not-an-item-id").empty());
    assert(normalizeJellyfinItemId("https://example.invalid/item/becd2d781967b8036ee6f144014fb009").empty());
    assert(normalizeJellyfinItemId("becd2d781967b8036ee6f144014fb00").empty());

    assert(normalizeExternalSearchQuery("  Friends  ") == "Friends");
    assert(normalizeExternalSearchQuery("\n\t").empty());
    assert(normalizeExternalSearchQuery("Planet\nEarth") == "Planet Earth");
    assert(normalizeExternalSearchQuery(std::string(200, 'A')).size() == 120);
    return 0;
}
