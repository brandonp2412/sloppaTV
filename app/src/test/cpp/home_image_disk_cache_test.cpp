#include "home_image_disk_cache.hpp"

#include <cassert>
#include <filesystem>
#include <fstream>
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

    cache.write("preserve-key", "original");
    fs::path preservePath;
    for (const auto& entry : fs::directory_iterator(root / "home-image-cache")) {
        if (!entry.is_regular_file()) continue;
        std::ifstream input(entry.path(), std::ios::binary);
        std::string value((std::istreambuf_iterator<char>(input)), std::istreambuf_iterator<char>());
        if (value == "original") {
            preservePath = entry.path();
            break;
        }
    }
    assert(!preservePath.empty());
    const fs::path blockedTemporaryPath = preservePath.string() + ".tmp";
    fs::create_directories(blockedTemporaryPath / "child", ec);
    assert(!ec);
    cache.write("preserve-key", "should-not-replace");
    const auto preserved = cache.read("preserve-key");
    assert(preserved && *preserved == "original");
    fs::remove_all(blockedTemporaryPath, ec);

    cache.write("empty", "");
    assert(!cache.read("empty"));

    cache.erase("first-key");
    assert(!cache.read("first-key"));

    for (int index = 0; index < 260; ++index) {
        cache.write("bounded-" + std::to_string(index), "x");
    }
    size_t cachedFiles = 0;
    for (const auto& entry : fs::directory_iterator(root / "home-image-cache")) {
        if (entry.is_regular_file()) ++cachedFiles;
    }
    assert(cachedFiles <= 256);
    assert(cache.read("bounded-259"));

    fs::remove_all(root, ec);
    return 0;
}
