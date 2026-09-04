#include "home_image_disk_cache.hpp"

#include <cassert>
#include <filesystem>
#include <string>

int main() {
    namespace fs = std::filesystem;
    const fs::path root = fs::temp_directory_path() / "sloppatv-home-image-disk-cache-test";
    std::error_code ec;
    fs::remove_all(root, ec);

    HomeImageDiskCache cache;
    assert(!cache.read("missing"));
    cache.write("ignored", "bytes");

    cache.setDataPath(root.string());
    cache.write("first-key", "abc123");
    const auto first = cache.read("first-key");
    assert(first && *first == "abc123");

    cache.write("first-key", "replacement");
    const auto replaced = cache.read("first-key");
    assert(replaced && *replaced == "replacement");

    cache.write("empty", "");
    assert(!cache.read("empty"));

    cache.erase("first-key");
    assert(!cache.read("first-key"));

    fs::remove_all(root, ec);
    return 0;
}
