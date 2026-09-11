#include "deep_link.hpp"

#include <cassert>

int main() {
    assert(normalizeJellyfinItemId("becd2d781967b8036ee6f144014fb009") == "becd2d781967b8036ee6f144014fb009");
    assert(normalizeJellyfinItemId("BECD2D78-1967-B803-6EE6-F144014FB009") == "becd2d781967b8036ee6f144014fb009");
    assert(normalizeJellyfinItemId("  becd2d781967b8036ee6f144014fb009\n") == "becd2d781967b8036ee6f144014fb009");
    assert(normalizeJellyfinItemId("sloppatv://item/becd2d781967b8036ee6f144014fb009") == "becd2d781967b8036ee6f144014fb009");
    assert(normalizeJellyfinItemId("sloppatv://item/BECD2D78-1967-B803-6EE6-F144014FB009") == "becd2d781967b8036ee6f144014fb009");
    assert(normalizeJellyfinItemId("sloppatv://item/becd2d781967b8036ee6f144014fb009?from=browser").empty());
    assert(normalizeJellyfinItemId("not-an-item-id").empty());
    assert(normalizeJellyfinItemId("https://example.invalid/item/becd2d781967b8036ee6f144014fb009").empty());
    assert(normalizeJellyfinItemId("becd2d781967b8036ee6f144014fb00").empty());

    assert(normalizeExternalSearchQuery("  Friends  ") == "Friends");
    assert(normalizeExternalSearchQuery("\n\t").empty());
    assert(normalizeExternalSearchQuery("Planet\nEarth") == "Planet Earth");
    assert(normalizeExternalSearchQuery(std::string(200, 'A')).size() == 120);

    std::string splitUtf8(119, 'A');
    splitUtf8 += "\xC4\x81";
    const std::string truncatedSplitUtf8 = normalizeExternalSearchQuery(splitUtf8);
    assert(truncatedSplitUtf8 == std::string(119, 'A'));

    std::string exactUtf8(118, 'A');
    exactUtf8 += "\xC4\x81";
    exactUtf8 += "B";
    const std::string truncatedExactUtf8 = normalizeExternalSearchQuery(exactUtf8);
    assert(truncatedExactUtf8.size() == 120);
    assert(truncatedExactUtf8.ends_with("\xC4\x81"));
    return 0;
}
